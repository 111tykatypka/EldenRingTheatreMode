#include "game_launcher.hpp"
#include "GameProfile.h"
#include <tlhelp32.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace game_launcher {
namespace fs = std::filesystem;
namespace {
struct Handle {
    HANDLE value{};
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
void require_amd64(const fs::path& path, bool dll) {
    std::ifstream input(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    input.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!input || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
        throw std::runtime_error("Invalid PE file: " + utf8(path.wstring()));
    input.seekg(dos.e_lfanew);
    DWORD signature{};
    IMAGE_FILE_HEADER header{};
    input.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input || signature != IMAGE_NT_SIGNATURE || header.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        static_cast<bool>(header.Characteristics & IMAGE_FILE_DLL) != dll)
        throw std::runtime_error("Expected AMD64 " + std::string(dll ? "DLL: " : "EXE: ") + utf8(path.wstring()));
}
std::string read_config(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read YAFSML configuration: " + utf8(path.wstring()));
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.starts_with("\xFF\xFE")) {
        if (bytes.size() % 2) throw std::runtime_error("Invalid UTF-16 YAFSML configuration");
        std::wstring text;
        for (std::size_t i = 2; i < bytes.size(); i += 2)
            text.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) | (static_cast<unsigned char>(bytes[i + 1]) << 8)));
        bytes = utf8(text);
    }
    if (bytes.starts_with("\xEF\xBB\xBF")) bytes.erase(0, 3);
    (void)wide(bytes); // Reject an unknown encoding rather than corrupt Unicode paths.
    return bytes;
}
void write_config(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    auto temporary = path;
    temporary += L".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.flush();
    if (!output) throw std::runtime_error("Cannot write launch configuration: " + utf8(temporary.wstring()));
    output.close();
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot finalize launch configuration; Windows error=" + std::to_string(GetLastError()));
}
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("Invalid UTF-16 path/text");
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}
std::wstring wide(const std::string& value) {
    if (value.empty()) return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!count) throw std::runtime_error("Invalid UTF-8 configuration/text");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}
std::wstring quote_argument(const std::wstring& value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const auto c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        result += c;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}
std::string set_console(const std::string& config, bool show) {
    std::istringstream input(config);
    std::string output, line;
    bool in_log = false, seen_log = false, written = false;
    const bool crlf = config.find("\r\n") != std::string::npos;
    const std::string eol = crlf ? "\r\n" : "\n", value = std::string("console=") + (show ? "1" : "0");
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto normalized = trim(line);
        if (normalized.starts_with("[") && normalized.ends_with("]")) {
            if (in_log && !written) { output += value + eol; written = true; }
            in_log = lower(normalized.substr(1, normalized.size() - 2)) == "log";
            seen_log = seen_log || in_log;
        } else if (in_log && !normalized.empty() && normalized.front() != ';' && normalized.front() != '#') {
            const auto equal = line.find('=');
            if (equal != std::string::npos && lower(trim(line.substr(0, equal))) == "console") {
                if (!written) { output += value + eol; written = true; }
                continue;
            }
        }
        output += line + eol;
    }
    if (in_log && !written) output += value + eol;
    if (!seen_log) output += "[log]" + eol + value + eol;
    return output;
}
std::string prepare_config(const std::string& source, const fs::path& directory, const fs::path& dll) {
    if (!directory.is_absolute() || !dll.is_absolute()) throw std::runtime_error("Launch config requires absolute paths");
    (void)wide(source);
    std::istringstream input(source);
    std::string output, line, section;
    unsigned theater_entries = 0, game_entries = 0;
    while (std::getline(input, line)) {
        const bool cr = !line.empty() && line.back() == '\r';
        if (cr) line.pop_back();
        const auto normalized = trim(line);
        if (normalized.starts_with("[") && normalized.ends_with("]")) section = lower(normalized.substr(1, normalized.size() - 2));
        else if (!normalized.empty() && normalized.front() != ';' && normalized.front() != '#') {
            const auto equal = line.find('=');
            if (equal != std::string::npos) {
                const auto key = lower(trim(line.substr(0, equal)));
                const auto value = trim(line.substr(equal + 1));
                if (section.empty() && key == "game") {
                    ++game_entries;
                    if (lower(value) != "eldenring") throw std::runtime_error("YAFSML configuration is not for Elden Ring");
                }
                if (section == "dll" || section == "mod") {
                    const auto split = value.find('|');
                    const auto path_text = trim(value.substr(0, split));
                    const auto suffix = split == std::string::npos ? "" : value.substr(split);
                    if (section == "dll" && key == "theater_mode") {
                        ++theater_entries;
                        line = line.substr(0, equal + 1) + utf8(dll.wstring()) + suffix;
                    } else if (!path_text.empty()) {
                        auto path = fs::path(wide(path_text));
                        if (section == "dll" && lower(utf8(path.filename().wstring())).starts_with("theatermode"))
                            throw std::runtime_error("Another TheaterMode DLL entry exists; refuse duplicate loading");
                        if (path.is_relative()) {
                            if (path.has_root_name() || path.has_root_directory()) throw std::runtime_error("Ambiguous drive-relative YAFSML path");
                            line = line.substr(0, equal + 1) + utf8((directory / path).lexically_normal().wstring()) + suffix;
                        }
                    }
                }
            }
        }
        output += line;
        if (!input.eof()) output += cr ? "\r\n" : "\n";
    }
    if (game_entries != 1 || theater_entries != 1)
        throw std::runtime_error("Expected one game=eldenring and one [dll] theater_mode entry in YAFSML.ini");
    return output;
}
std::wstring launch_arguments(const Paths& paths) {
    return L" -t eldenring -p " + quote_argument(paths.game.wstring()) + L" -c " + quote_argument(paths.config.wstring()) +
           L" -d " + quote_argument((paths.loader.parent_path() / L"YAFSML.dll").wstring());
}
void validate_dependencies(const Paths& paths) {
    for (const auto& file : {paths.loader, paths.loader.parent_path() / L"YAFSML.dll", paths.loader.parent_path() / L"YAFSML.ini", paths.game, paths.dll})
        if (!fs::is_regular_file(file)) throw std::runtime_error("Required file missing: " + utf8(file.wstring()));
    require_amd64(paths.loader, false);
    require_amd64(paths.loader.parent_path() / L"YAFSML.dll", true);
    require_amd64(paths.dll, true);
}
DWORD find_game_process() {
    Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (snapshot.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot check existing game processes");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.value, &entry)) throw std::runtime_error("Cannot enumerate game processes");
    do { if (CompareStringOrdinal(entry.szExeFile, -1, L"eldenring.exe", -1, TRUE) == CSTR_EQUAL) return entry.th32ProcessID; }
    while (Process32NextW(snapshot.value, &entry));
    if (GetLastError() != ERROR_NO_MORE_FILES) throw std::runtime_error("Process enumeration failed");
    return 0;
}
State Launcher::state() const { std::lock_guard lock(mutex_); return state_; }
void Launcher::set_state(Phase phase, std::wstring text, const Logger& logger) {
    { std::lock_guard lock(mutex_); state_ = {phase, text}; }
    logger("LAUNCHER " + utf8(text));
}
bool Launcher::start(Paths paths, RuntimeReader runtime, Logger logger) {
    if (state().busy()) return false; // UI thread owns start/close; worker owns transitions.
    if (worker_.joinable()) worker_.join();
    set_state(Phase::checking, L"CHECKING files and EldenRing_1_17 profile...", logger);
    worker_ = std::jthread([this, paths = std::move(paths), runtime = std::move(runtime), logger = std::move(logger)](std::stop_token stop) {
        run(stop, paths, runtime, logger);
    });
    return true;
}
void Launcher::close() { if (worker_.joinable()) { worker_.request_stop(); worker_.join(); } }
void Launcher::run(std::stop_token stop, const Paths& paths, const RuntimeReader& runtime, const Logger& logger) {
    try {
        if (!runtime().host_ready) throw std::runtime_error("Host IPC server is unavailable. Close other hosts and reopen this application.");
        if (find_game_process()) throw std::runtime_error("Elden Ring is already running. Close it before launching a new DLL session.");
        validate_dependencies(paths);
        // The user's YAFSML.ini may enable YAFSML's debug console (console=1). Theater Mode launches
        // without any console window unless "Show debug console" is ticked in the launcher.
        const auto config = set_console(prepare_config(read_config(paths.loader.parent_path() / L"YAFSML.ini"), paths.loader.parent_path(), paths.dll), paths.show_console);
        logger(std::string("LAUNCHER YAFSML console=") + (paths.show_console ? "1 (debug console requested)" : "0 (no console window)"));
        TmValidationReport report{};
        const auto result = tm_validate_profile(paths.game.c_str(), 0, &report);
        std::ostringstream validation;
        validation << "LAUNCHER PROFILE=" << TM_PROFILE_NAME << " status=" << result << " passed=" << report.passed
                   << " game=" << utf8(paths.game.wstring()) << " file_version=" << report.file_version[0] << '.' << report.file_version[1]
                   << '.' << report.file_version[2] << '.' << report.file_version[3] << " disk_sha256=" << report.sha256;
        logger(validation.str());
        if (result) throw std::runtime_error("EldenRing_1_17 compatibility check failed; code=" + std::to_string(result) + ". See recorder log.");
        if (stop.stop_requested()) return;
        // Check again after hashing, before creating a second game process.
        if (!runtime().host_ready || find_game_process()) throw std::runtime_error("Host readiness or game process changed during preflight; launch cancelled.");
        write_config(paths.config, config);
        auto command = quote_argument(paths.loader.wstring()) + launch_arguments(paths);
        logger("LAUNCHER command=" + utf8(command) + " working_directory=" + utf8(paths.loader.parent_path().wstring()));
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (stop.stop_requested()) return;
        if (!CreateProcessW(paths.loader.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                            paths.loader.parent_path().c_str(), &startup, &process))
            throw std::runtime_error("YAFSML launch failed; Windows error=" + std::to_string(GetLastError()));
        Handle launcher{process.hProcess}, thread{process.hThread};
        set_state(Phase::waiting, L"YAFSML STARTED | Waiting for game module...", logger);
        DWORD game_pid = 0;
        const auto deadline = GetTickCount64() + 120'000;
        bool connected = false;
        while (!stop.stop_requested()) {
            const auto now = runtime();
            const auto running_pid = find_game_process();
            if (running_pid) game_pid = running_pid;
            if (game_pid && !running_pid) {
                set_state(Phase::idle, L"Game closed | Ready to launch again", logger);
                return;
            }
            if (now.sample_pid && now.sample_pid == running_pid && now.control_connected) {
                const auto text = now.player_ready ? L"CONNECTED | Runtime READY | Player FOUND" : L"CONNECTED | Load a playable world";
                if (state().diagnostic != text) set_state(Phase::connected, text, logger);
                connected = true;
            } else if (connected) {
                set_state(Phase::error, L"Game module disconnected. Restart the game for a fresh DLL session.", logger);
                return;
            }
            DWORD exit_code{};
            if (!game_pid && GetExitCodeProcess(launcher.value, &exit_code) && exit_code != STILL_ACTIVE && exit_code != 0)
                throw std::runtime_error("YAFSML exited with code=" + std::to_string(exit_code) + ". See launch/log/YAFSML.log.");
            if (!connected && GetTickCount64() >= deadline)
                throw std::runtime_error("No game-module connection within 120 seconds. See recorder, game and YAFSML logs; do not launch a second game.");
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        // Closing the host never terminates Elden Ring or its loader.
    } catch (const std::exception& error) { set_state(Phase::error, wide(error.what()), logger); }
}
}
