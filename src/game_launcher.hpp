#pragma once
#include <windows.h>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace game_launcher {
struct Paths {
    std::filesystem::path loader, game, dll, config;
    bool show_console{}; // YAFSML debug console; off unless the user ticks "Show debug console"
};
struct Runtime {
    // IPC server readiness; focus-scoped hotkey registration is not a launch prerequisite.
    bool host_ready{};
    DWORD sample_pid{};
    bool control_connected{}, player_ready{};
};
enum class Phase { idle, checking, waiting, connected, error };
struct State {
    Phase phase{Phase::idle};
    std::wstring diagnostic{L"Launch through your installed YAFSML"};
    bool busy() const { return phase == Phase::checking || phase == Phase::waiting || phase == Phase::connected; }
};

std::string utf8(const std::wstring& value);
std::wstring wide(const std::string& value);
std::wstring quote_argument(const std::wstring& value);
// Preserve loader settings; rebase DLL/mod paths when moving the config.
std::string prepare_config(const std::string& source, const std::filesystem::path& source_directory,
                           const std::filesystem::path& theater_dll);
// Forces YAFSML's [log] console= setting in a prepared config (adds it if missing).
// YAFSML's console output still goes to its own log file (log_file=1 is kept).
std::string set_console(const std::string& config, bool show);
std::wstring launch_arguments(const Paths& paths);
void validate_dependencies(const Paths& paths);
DWORD find_game_process();

class Launcher {
public:
    using RuntimeReader = std::function<Runtime()>;
    using Logger = std::function<void(const std::string&)>;
    ~Launcher() { close(); }
    bool start(Paths paths, RuntimeReader runtime, Logger logger);
    State state() const;
    void close();
private:
    void set_state(Phase phase, std::wstring text, const Logger& logger);
    void run(std::stop_token stop, const Paths& paths, const RuntimeReader& runtime, const Logger& logger);
    mutable std::mutex mutex_;
    State state_;
    std::jthread worker_;
};
}
