#include "VideoExport.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace video_export {
namespace {
std::mutex state_mutex;
Settings current_settings;
bool settings_loaded = false;
std::atomic_bool toggle_flag{false};

struct Frame { std::vector<std::uint8_t> pixels; double time = 0; };
struct Session {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<Frame> queue;
    bool stop = false;
    std::thread worker;
    unsigned width = 0, height = 0;
    bool bgra = true;
    int fps = 60;
    std::uint64_t frames = 0, duplicated = 0, dropped = 0;
    double first_time = -1, last_time = 0;
    HANDLE process = nullptr, pipe = nullptr;
    std::string output;
    std::string summary;
};
std::shared_ptr<Session> session;               // the export being captured, if any
std::atomic<int> finalizing{0};                 // sessions whose ffmpeg is still finishing
std::string last_output, last_error;
std::chrono::steady_clock::time_point finished_at{};
bool finished_recent_flag = false;
std::string ffmpeg_cached;

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
std::filesystem::path settings_path() {
    wchar_t dir[32768]{};
    auto n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, 32768);
    if (!n || n >= 32768) return {};
    return std::filesystem::path(dir) / L"EldenRingTheaterMode" / L"export.ini";
}
std::filesystem::path default_folder() {
    wchar_t dir[32768]{};
    auto n = GetEnvironmentVariableW(L"USERPROFILE", dir, 32768);
    std::filesystem::path base = (n && n < 32768) ? std::filesystem::path(dir) / L"Videos" : std::filesystem::temp_directory_path();
    return base / L"EldenRingTheaterMode";
}
HMODULE this_module() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&this_module), &module);
    return module;
}
std::string stamp() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    char text[64];
    snprintf(text, sizeof(text), "%04d%02d%02d_%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return text;
}
std::wstring quote(const std::wstring& s) { return L"\"" + s + L"\""; }

// `scale` is the picture-size filter chain (may be empty). Pieces are joined into one -vf argument.
std::string vf_arg(const std::string& scale, const std::string& tail) {
    std::string chain = scale;
    if (!tail.empty()) chain += (chain.empty() ? "" : ",") + tail;
    return chain.empty() ? std::string() : "-vf " + chain + " ";
}
std::string build_codec_args(const Settings& s, bool sequence, bool jpeg, const std::string& scale) {
    const int q = std::clamp(s.quality, 1, 100);
    char text[512];
    if (sequence) {
        if (jpeg) { snprintf(text, sizeof(text), "-c:v mjpeg -q:v %d -pix_fmt yuvj420p", std::clamp(int(std::lround(31 - q * 0.29)), 2, 31)); return vf_arg(scale, "") + text; }
        return vf_arg(scale, "") + "-c:v png -compression_level 3";
    }
    switch (Codec(s.codec)) {
    case Codec::H264Nvenc:
        snprintf(text, sizeof(text), "-c:v h264_nvenc -preset p5 -rc vbr -cq %d -b:v 0", std::clamp(int(std::lround(40 - q * 0.28)), 10, 40));
        return vf_arg(scale, "format=yuv420p") + text;
    case Codec::H264Cpu:
        snprintf(text, sizeof(text), "-c:v libx264 -preset medium -crf %d", std::clamp(int(std::lround(40 - q * 0.28)), 10, 40));
        return vf_arg(scale, "format=yuv420p") + text;
    case Codec::Mjpeg:
        snprintf(text, sizeof(text), "-c:v mjpeg -q:v %d -pix_fmt yuvj420p", std::clamp(int(std::lround(31 - q * 0.29)), 2, 31));
        return vf_arg(scale, "") + text;
    default:
        return vf_arg(scale, "") + "-c:v ffv1 -level 3 -pix_fmt bgr0";
    }
}

void finish_session(std::shared_ptr<Session> s) {
    // Runs on the session's worker thread after the capture side stopped.
    for (;;) {
        Frame f;
        {
            std::unique_lock lock(s->mutex);
            s->cv.wait(lock, [&] { return s->stop || !s->queue.empty(); });
            if (s->queue.empty()) { if (s->stop) break; continue; }
            f = std::move(s->queue.front());
            s->queue.pop_front();
        }
        if (s->first_time < 0) s->first_time = f.time;
        const double position = (f.time - s->first_time) * s->fps;
        const std::uint64_t need = (std::uint64_t)std::floor(std::max(0.0, position) + 0.5) + 1;
        if (need <= s->frames) { ++s->dropped; continue; }
        const std::uint64_t copies = need - s->frames;
        bool broken = false;
        for (std::uint64_t i = 0; i < copies && !broken; ++i) {
            DWORD wrote = 0; const std::uint8_t* data = f.pixels.data(); std::size_t left = f.pixels.size();
            while (left) {
                DWORD chunk = (DWORD)std::min<std::size_t>(left, 1u << 20);
                if (!WriteFile(s->pipe, data, chunk, &wrote, nullptr) || !wrote) { broken = true; break; }
                data += wrote; left -= wrote;
            }
            if (!broken) { ++s->frames; if (i) ++s->duplicated; }
        }
        s->last_time = f.time;
        if (broken) {
            std::lock_guard lock(state_mutex);
            last_error = "ffmpeg stopped accepting frames (see ffmpeg_log.txt next to the output)";
            break;
        }
    }
    if (s->pipe) { CloseHandle(s->pipe); s->pipe = nullptr; }
    DWORD exit_code = 0;
    if (s->process) {
        if (WaitForSingleObject(s->process, 120000) == WAIT_TIMEOUT) TerminateProcess(s->process, 1);
        GetExitCodeProcess(s->process, &exit_code);
        CloseHandle(s->process); s->process = nullptr;
    }
    {
        std::lock_guard lock(state_mutex);
        if (exit_code != 0 && last_error.empty()) last_error = "ffmpeg exited with code " + std::to_string(exit_code) + " (see ffmpeg_log.txt next to the output)";
        else if (exit_code == 0) { last_output = s->output; }
        finished_at = std::chrono::steady_clock::now();
        finished_recent_flag = true;
    }
    --finalizing;
}
}

std::string find_ffmpeg() {
    std::lock_guard lock(state_mutex);
    auto exists = [](const std::filesystem::path& p) { std::error_code ec; return !p.empty() && std::filesystem::is_regular_file(p, ec); };
    if (!current_settings.ffmpeg.empty()) {
        std::filesystem::path p = widen(current_settings.ffmpeg);
        return exists(p) ? narrow(p.wstring()) : std::string{};
    }
    wchar_t module_path[MAX_PATH]{};
    if (GetModuleFileNameW(this_module(), module_path, MAX_PATH)) {
        const auto dir = std::filesystem::path(module_path).parent_path();
        for (auto candidate : { dir / L"ffmpeg.exe", dir / L"ffmpeg" / L"ffmpeg.exe", dir / L"ffmpeg" / L"bin" / L"ffmpeg.exe" })
            if (exists(candidate)) return narrow(candidate.wstring());
    }
    wchar_t found[MAX_PATH]{};
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr)) return narrow(found);
    return {};
}

Settings settings() { std::lock_guard lock(state_mutex); return current_settings; }

void configure(const Settings& value) {
    {
        std::lock_guard lock(state_mutex);
        current_settings = value;
        current_settings.container = std::clamp(current_settings.container, 0, 2);
        current_settings.codec = std::clamp(current_settings.codec, 0, 3);
        current_settings.fps = std::clamp(current_settings.fps, 1, 240);
        current_settings.quality = std::clamp(current_settings.quality, 1, 100);
        current_settings.out_width = current_settings.out_width > 0 ? std::clamp(current_settings.out_width & ~1, 64, 7680) : 0;
        current_settings.out_height = current_settings.out_height > 0 ? std::clamp(current_settings.out_height & ~1, 64, 4320) : 0;
        settings_loaded = true;
    }
    try {
        auto p = settings_path();
        if (p.empty()) return;
        std::filesystem::create_directories(p.parent_path());
        std::ofstream f(p, std::ios::trunc);
        Settings s = settings();
        f << "container=" << s.container << "\ncodec=" << s.codec << "\nfps=" << s.fps << "\nquality=" << s.quality << "\nout_width=" << s.out_width << "\nout_height=" << s.out_height << "\nfolder=" << s.folder << "\nffmpeg=" << s.ffmpeg << "\n";
    } catch (...) {}
}

void load_settings() {
    Settings s;
    try {
        auto p = settings_path();
        std::ifstream f(p);
        std::string line;
        while (std::getline(f, line)) {
            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            const auto key = line.substr(0, eq), value = line.substr(eq + 1);
            if (key == "container") s.container = atoi(value.c_str());
            else if (key == "codec") s.codec = atoi(value.c_str());
            else if (key == "fps") s.fps = atoi(value.c_str());
            else if (key == "quality") s.quality = atoi(value.c_str());
            else if (key == "out_width") s.out_width = atoi(value.c_str());
            else if (key == "out_height") s.out_height = atoi(value.c_str());
            else if (key == "folder") s.folder = value;
            else if (key == "ffmpeg") s.ffmpeg = value;
        }
    } catch (...) {}
    std::lock_guard lock(state_mutex);
    current_settings = s;
    current_settings.container = std::clamp(current_settings.container, 0, 2);
    current_settings.codec = std::clamp(current_settings.codec, 0, 3);
    current_settings.fps = std::clamp(current_settings.fps, 1, 240);
    current_settings.quality = std::clamp(current_settings.quality, 1, 100);
    settings_loaded = true;
}

void request_toggle() { toggle_flag = true; }
bool toggle_requested() { return toggle_flag.exchange(false); }
bool toggle_pending() { return toggle_flag.load(); }
bool banner_visible() {
    std::lock_guard lock(state_mutex);
    if (session || finalizing.load() > 0) return true;
    if (finished_recent_flag) return std::chrono::duration<double>(std::chrono::steady_clock::now() - finished_at).count() < 8.0;
    return false;
}
bool active() { std::lock_guard lock(state_mutex); return session != nullptr; }

Status status() {
    Status out;
    const std::string ffmpeg = find_ffmpeg();
    std::lock_guard lock(state_mutex);
    out.ffmpeg_found = !ffmpeg.empty();
    out.ffmpeg_path = ffmpeg;
    out.output = last_output;
    out.error = last_error;
    out.active = session != nullptr;
    if (session) {
        std::lock_guard sl(session->mutex);
        out.frames = session->frames; out.duplicated = session->duplicated; out.dropped = session->dropped;
        out.seconds = double(session->frames) / std::max(1, session->fps);
        char text[256];
        snprintf(text, sizeof(text), "EXPORTING  %02d:%02d  %llu frames  %ux%u @ %d fps%s", int(out.seconds) / 60, int(out.seconds) % 60,
                 (unsigned long long)session->frames, session->width, session->height, session->fps,
                 session->dropped ? "  (some frames dropped: encoder is behind)" : "");
        out.text = text;
    } else if (finalizing.load() > 0) {
        out.text = "EXPORT: finishing the file...";
    } else if (finished_recent_flag) {
        const auto age = std::chrono::duration<double>(std::chrono::steady_clock::now() - finished_at).count();
        if (age < 8.0) {
            out.recently_finished = true;
            out.text = last_error.empty() ? "EXPORT SAVED: " + last_output : "EXPORT FAILED: " + last_error;
        } else finished_recent_flag = false;
    }
    return out;
}

bool begin(unsigned width, unsigned height, bool bgra) {
    if (width < 16 || height < 16) { fail("the game picture is too small to export"); return false; }
    if (finalizing.load() > 0) { fail("the previous export is still finishing; try again in a moment"); return false; }
    Settings s;
    {
        std::lock_guard lock(state_mutex);
        if (!settings_loaded) { s = Settings{}; } else s = current_settings;
        last_error.clear();
    }
    const std::string ffmpeg = find_ffmpeg();
    if (ffmpeg.empty()) {
        fail("ffmpeg.exe not found. Put ffmpeg.exe next to TheaterMode.dll (or in a 'ffmpeg' folder there), or set its path in the Export tab.");
        return false;
    }
    const bool sequence = Container(s.container) != Container::Avi;
    const bool jpeg = Container(s.container) == Container::JpegSequence;
    std::filesystem::path root = s.folder.empty() ? default_folder() : std::filesystem::path(widen(s.folder));
    std::filesystem::path out_path;
    std::filesystem::path log_path;
    try {
        std::filesystem::create_directories(root);
        if (sequence) {
            auto dir = root / widen("Theater_" + stamp());
            std::filesystem::create_directories(dir);
            out_path = dir / (jpeg ? L"frame_%06d.jpg" : L"frame_%06d.png");
            log_path = dir / L"ffmpeg_log.txt";
        } else {
            out_path = root / widen("Theater_" + stamp() + ".avi");
            log_path = root / widen("Theater_" + stamp() + "_ffmpeg_log.txt");
        }
    } catch (const std::exception& e) {
        fail(std::string("cannot create the output folder: ") + e.what());
        return false;
    }
    // Output picture size: native (rounded down to even numbers for the video codecs) or a chosen size with the aspect ratio kept.
    std::string scale;
    if (s.out_width > 0 && s.out_height > 0) {
        const std::string w = std::to_string(s.out_width & ~1), h = std::to_string(s.out_height & ~1);
        scale = "scale=" + w + ":" + h + ":force_original_aspect_ratio=decrease:flags=lanczos,pad=" + w + ":" + h + ":(ow-iw)/2:(oh-ih)/2:color=black";
    } else if (!sequence) {
        scale = "scale=trunc(iw/2)*2:trunc(ih/2)*2";
    }
    std::wstring command = quote(widen(ffmpeg)) + L" -y -hide_banner -loglevel warning -f rawvideo -pix_fmt " + (bgra ? L"bgr0" : L"rgb0") +
        L" -video_size " + std::to_wstring(width) + L"x" + std::to_wstring(height) + L" -framerate " + std::to_wstring(std::clamp(s.fps, 1, 240)) +
        L" -i - -an " + widen(build_codec_args(s, sequence, jpeg, scale)) + L" " + quote(out_path.wstring());
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE read_end = nullptr, write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &sa, 8u << 20)) { fail("could not create the ffmpeg pipe"); return false; }
    SetHandleInformation(write_end, HANDLE_FLAG_INHERIT, 0);
    HANDLE log = CreateFileW(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = read_end; si.hStdOutput = log != INVALID_HANDLE_VALUE ? log : nullptr; si.hStdError = si.hStdOutput;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutable_command(command.begin(), command.end()); mutable_command.push_back(L'\0');
    const BOOL ok = CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(read_end);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!ok) { CloseHandle(write_end); fail("could not start ffmpeg (error " + std::to_string(GetLastError()) + ")"); return false; }
    CloseHandle(pi.hThread);

    auto s2 = std::make_shared<Session>();
    s2->width = width; s2->height = height; s2->bgra = bgra; s2->fps = std::clamp(s.fps, 1, 240);
    s2->process = pi.hProcess; s2->pipe = write_end;
    s2->output = narrow(sequence ? out_path.parent_path().wstring() : out_path.wstring());
    ++finalizing;
    s2->worker = std::thread(finish_session, s2);
    s2->worker.detach();
    std::lock_guard lock(state_mutex);
    session = s2;
    last_output.clear();
    return true;
}

void end(const char* reason) {
    std::shared_ptr<Session> s;
    {
        std::lock_guard lock(state_mutex);
        s = session;
        session.reset();
        if (reason && last_error.empty()) last_error = reason;
    }
    if (!s) return;
    {
        std::lock_guard lock(s->mutex);
        s->stop = true;
    }
    s->cv.notify_all();
}

void fail(const std::string& message) {
    end(nullptr);
    std::lock_guard lock(state_mutex);
    last_error = message;
    finished_at = std::chrono::steady_clock::now();
    finished_recent_flag = true;
}

void submit(const std::uint8_t* pixels, std::size_t row_pitch, double qpc_seconds) {
    std::shared_ptr<Session> s;
    {
        std::lock_guard lock(state_mutex);
        s = session;
    }
    if (!s || !pixels) return;
    Frame f;
    f.time = qpc_seconds;
    f.pixels.resize(std::size_t(s->width) * s->height * 4);
    for (unsigned y = 0; y < s->height; ++y) memcpy(f.pixels.data() + std::size_t(y) * s->width * 4, pixels + std::size_t(y) * row_pitch, std::size_t(s->width) * 4);
    std::lock_guard lock(s->mutex);
    if (s->queue.size() >= 8) { ++s->dropped; return; }
    s->queue.push_back(std::move(f));
    s->cv.notify_one();
}
}
