#pragma once
#include <windows.h>
#include <array>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace game_control {
inline constexpr wchar_t pipe_name[]=L"\\\\.\\pipe\\EldenRingTheaterMode_1_17_Control";
inline constexpr std::uint32_t magic=0x544d4354;
enum Kind : std::uint16_t { hello=1, heartbeat=2, probe_nudge=3, stop=4, replay_begin=5, replay_apply=6, replay_finish=7, status=0x8000 };
enum Phase : std::uint32_t { off=0, armed=1, observing=2, complete=3, error=4 };
enum ReplayPhase : std::uint32_t { inactive=0, playing=1, paused=2, finished=3, replay_error=4 };
struct Transform {std::array<float,3> position{};std::array<float,4> quaternion{0,0,0,1};};
struct Packet {
    std::uint32_t magic_value{magic}; std::uint16_t version{2},kind{};
    std::uint64_t sequence{},timestamp_ns{};
    float position[3]{},quaternion[4]{0,0,0,1};
    std::uint32_t state{},detail{},flags{};
    std::uint64_t replay_timestamp_ns{},session{};
    std::uint32_t replay_state{},replay_detail{};
    std::uint64_t applied_sequence{};
};
static_assert(std::endian::native==std::endian::little && sizeof(Packet)==96);
static_assert(offsetof(Packet,position)==24 && offsetof(Packet,quaternion)==36 && offsetof(Packet,state)==52);
static_assert(offsetof(Packet,replay_timestamp_ns)==64 && offsetof(Packet,session)==72 && offsetof(Packet,applied_sequence)==88);
struct State {
    bool connected{},ready{},pending{}; std::uint32_t phase{off},detail{};
    std::uint64_t command_sequence{}; std::wstring diagnostic{L"Waiting for game sample connection"};
    bool replay_supported{};std::uint32_t replay_phase{inactive},replay_detail{};
    std::uint64_t session{},applied_sequence{},replay_timestamp_ns{},sample_timestamp_ns{},last_replay_session{};
    Transform live;
};
// The UI never performs pipe I/O; this worker owns the control connection.
class Client {
public:
    explicit Client(std::wstring pipe=pipe_name):pipe_(std::move(pipe)){}
    ~Client();
    void start(std::atomic<DWORD>& sample_process_id);
    bool nudge();
    bool begin_replay(std::uint64_t session,Transform transform);
    bool apply_replay(std::uint64_t session,std::uint64_t replay_ns,Transform transform,bool pause);
    bool finish_replay(std::uint64_t session,std::uint64_t replay_ns,Transform transform);
    void emergency_stop();
    void close();
    [[nodiscard]] State state() const;
private:
    std::wstring pipe_;
    mutable std::mutex mutex_; State state_;
    std::atomic_bool shutting_down_{false}; std::atomic_int pending_{0};
    std::mutex wake_mutex_; std::condition_variable wake_; std::thread worker_;
    std::optional<Packet> latest_request_;std::uint64_t active_session_{};
    std::atomic_bool replay_pending_{};
    bool queue_replay(std::uint16_t kind,std::uint64_t session,std::uint64_t replay_ns,Transform transform,bool pause);
    void run(std::atomic<DWORD>& sample_process_id);
    void disconnected(const std::wstring& reason);
};
}
