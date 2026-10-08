#include "TheaterSounds.h"
#include <windows.h>
#include <xaudio2.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

namespace TheaterUI::Sound
{
namespace
{
    struct Clip { WAVEFORMATEX format{}; std::vector<std::uint8_t> data; };
    // Relative to sounds\ui next to the DLL; numbered variants are picked at random.
    const char* const kFiles[(int)Cue::Count][3] = {
        { "inventory/open/ui_inventory_open_01.wav", "inventory/open/ui_inventory_open_02.wav", "inventory/open/ui_inventory_open_03.wav" },
        { "inventory/close/ui_inventory_close_01.wav", "inventory/close/ui_inventory_close_02.wav", "inventory/close/ui_inventory_close_03.wav" },
        { "menu/focus/ui_menu_focus_01.wav", "menu/focus/ui_menu_focus_02.wav", "menu/focus/ui_menu_focus_03.wav" },
        { "menu/tabs/ui_menu_tab_01.wav", "menu/tabs/ui_menu_tab_02.wav", "menu/tabs/ui_menu_tab_03.wav" },
        { "menu/ui_menu_ok.wav", nullptr, nullptr },
        { "menu/ui_menu_cancel.wav", nullptr, nullptr },
        { "menu/ui_menu_prevnext.wav", nullptr, nullptr },
        { "menu/ui_menu_bracket.wav", nullptr, nullptr },
        { "ui_message.wav", nullptr, nullptr },
    };
    // Minimal RIFF/WAVE reader: PCM or IEEE float "fmt " and "data" chunks.
    bool ReadWav(const std::filesystem::path& path, Clip& clip)
    {
        std::ifstream in(path, std::ios::binary);
        std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(in)), {});
        if (b.size() < 12 || memcmp(b.data(), "RIFF", 4) || memcmp(b.data() + 8, "WAVE", 4)) return false;
        bool fmt = false, data = false;
        for (size_t o = 12; o + 8 <= b.size();)
        {
            std::uint32_t size; memcpy(&size, b.data() + o + 4, 4);
            if (o + 8 + size > b.size()) break;
            if (!memcmp(b.data() + o, "fmt ", 4) && size >= 16) { memcpy(&clip.format, b.data() + o + 8, 16); clip.format.cbSize = 0; fmt = true; }
            else if (!memcmp(b.data() + o, "data", 4)) { clip.data.assign(b.begin() + o + 8, b.begin() + o + 8 + size); data = true; }
            o += 8 + size + (size & 1);
        }
        return fmt && data && (clip.format.wFormatTag == WAVE_FORMAT_PCM || clip.format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    }
    std::filesystem::path SoundRoot()
    {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&SoundRoot), &module);
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(module, path, MAX_PATH);
        return std::filesystem::path(path).parent_path() / L"sounds" / L"ui";
    }

    class Engine
    {
    public:
        Engine() { worker_ = std::thread([this] { Run(); }); }
        void Request(Cue c) { { std::lock_guard l(m_); if (queue_.size() < 16) queue_.push_back(c); } cv_.notify_one(); }
        std::atomic<bool> enabled{ true },focused{true};
        std::atomic<float> volume{ 0.6f };
    private:
        void Run()
        {
            if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return;
            IXAudio2* audio = nullptr; IXAudio2MasteringVoice* master = nullptr;
            if (FAILED(XAudio2Create(&audio, 0, XAUDIO2_DEFAULT_PROCESSOR)) || FAILED(audio->CreateMasteringVoice(&master))) { if (audio) audio->Release(); return; }
            std::vector<Clip> clips[(int)Cue::Count];
            const auto root = SoundRoot();
            for (int c = 0; c < (int)Cue::Count; ++c)
                for (const char* f : kFiles[c]) { Clip clip; if (f && ReadWav(root / f, clip)) clips[c].push_back(std::move(clip)); }
            std::mt19937 rng{ GetTickCount() };
            std::vector<IXAudio2SourceVoice*> voices;
            for (;;)
            {
                Cue cue;
                {
                    std::unique_lock l(m_);
                    cv_.wait_for(l, std::chrono::milliseconds(25), [&] { return !queue_.empty(); });
                    DWORD currentFocusPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&currentFocusPid);
                    if(!focused||currentFocusPid!=GetCurrentProcessId()){for(auto*voice:voices)voice->DestroyVoice();voices.clear();queue_.clear();}
                    // Finished voices are released here, on this thread only.
                    std::erase_if(voices, [](IXAudio2SourceVoice* v) { XAUDIO2_VOICE_STATE s; v->GetState(&s); if (s.BuffersQueued) return false; v->DestroyVoice(); return true; });
                    if (queue_.empty()) continue;
                    cue = queue_.front(); queue_.pop_front();
                }
                auto& set = clips[(int)cue];
                DWORD focusedPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&focusedPid);
                if (focusedPid!=GetCurrentProcessId() || !enabled || set.empty() || voices.size() >= 12) continue;
                const Clip& clip = set[set.size() == 1 ? 0 : rng() % set.size()];
                IXAudio2SourceVoice* voice = nullptr;
                if (FAILED(audio->CreateSourceVoice(&voice, &clip.format))) continue;
                XAUDIO2_BUFFER buffer{}; buffer.AudioBytes = (UINT32)clip.data.size(); buffer.pAudioData = clip.data.data(); buffer.Flags = XAUDIO2_END_OF_STREAM;
                voice->SetVolume(volume.load());
                if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) { voice->DestroyVoice(); continue; }
                voices.push_back(voice);
            }
        }
        std::thread worker_; std::mutex m_; std::condition_variable cv_; std::deque<Cue> queue_;
    };
    Engine& engine() { static auto* e = new Engine; return *e; } // process lifetime, like the render backend
}

void Play(Cue cue) { DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);if(pid!=GetCurrentProcessId())return;if (engine().enabled) engine().Request(cue); }
void SetFocused(bool focused) { engine().focused=focused; }
void SetEnabled(bool enabled) { engine().enabled = enabled; }
void SetVolume(float volume) { engine().volume = volume < 0 ? 0 : volume > 1 ? 1 : volume; }
bool Enabled() { return engine().enabled; }
float Volume() { return engine().volume; }
}
