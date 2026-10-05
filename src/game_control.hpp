#pragma once
#include <windows.h>
#include <array>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace game_control {
inline constexpr wchar_t pipe_name[]=L"\\\\.\\pipe\\EldenRingTheaterMode_1_17_Control";
inline constexpr std::uint32_t magic=0x544d4354;
enum Kind : std::uint16_t { hello=1, heartbeat=2, probe_nudge=3, stop=4, status=0x8000 };
enum Phase : std::uint32_t { off=0, armed=1, observing=2, complete=3, error=4 };
struct Packet {
    std::uint32_t magic_value{magic}; std::uint16_t version{1},kind{};
    std::uint64_t sequence{},timestamp_ns{};
    float position[3]{},quaternion[4]{0,0,0,1};
    std::uint32_t state{},detail{},flags{};
};
static_assert(std::endian::native==std::endian::little && sizeof(Packet)==64);
static_assert(offsetof(Packet,position)==24 && offsetof(Packet,quaternion)==36 && offsetof(Packet,state)==52);
struct State {
    bool connected{},ready{},pending{}; std::uint32_t phase{off},detail{};
    std::uint64_t command_sequence{}; std::wstring diagnostic{L"Waiting for game sample connection"};
};
// The UI never performs pipe I/O; this worker owns the control connection.
class Client {
public:
    explicit Client(std::wstring pipe=pipe_name):pipe_(std::move(pipe)){}
    ~Client();
    void start(std::atomic<DWORD>& sample_process_id);
    bool nudge();
    void emergency_stop();
    void close();
    [[nodiscard]] State state() const;
private:
    std::wstring pipe_;
    mutable std::mutex mutex_; State state_;
    std::atomic_bool shutting_down_{false}; std::atomic_int pending_{0};
    std::mutex wake_mutex_; std::condition_variable wake_; std::thread worker_;
    void run(std::atomic<DWORD>& sample_process_id);
    void disconnected(const std::wstring& reason);
};
}
