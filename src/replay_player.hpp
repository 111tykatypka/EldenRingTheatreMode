#pragma once
#include "erplay.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>

namespace replay {
enum class Status { stopped, loading, ready, playing, paused, seeking, error };
struct State {
    std::uint64_t timestamp_ns{};
    std::uint64_t sample_index{};
    erplay::Vec3 position{};
    erplay::Quaternion orientation{};
    double speed{1.0};
    erplay::ActionState current_action;bool has_action{},dense_action{};
    Status status{Status::stopped};
};
class Player {
public:
    using Clock=std::chrono::steady_clock;
    explicit Player(const std::filesystem::path& file);
    [[nodiscard]] const erplay::Summary& summary() const noexcept;
    [[nodiscard]] const State& state() const noexcept { return state_; }
    [[nodiscard]] const erplay::Reader& reader() const noexcept { return *reader_; }
    void play(Clock::time_point now=Clock::now());
    void pause(Clock::time_point now=Clock::now());
    void stop();
    void restart(Clock::time_point now=Clock::now());
    void set_speed(double speed,Clock::time_point now=Clock::now());
    void seek(std::uint64_t timestamp_ns);
    void step(int direction);
    void advance(Clock::time_point now=Clock::now());
private:
    std::unique_ptr<erplay::Reader> reader_;
    State state_{};
    std::uint64_t clock_ns_{};
    Clock::time_point anchor_{};std::size_t action_cursor_{};bool action_cursor_valid_{};std::uint64_t previous_action_clock_{};
    void update_state();
};
class BookmarkStore {
public:
    explicit BookmarkStore(std::filesystem::path sidecar):path_(std::move(sidecar)){}
    void load();
    void save() const;
    void add(std::uint64_t timestamp_ns);
    void erase(std::size_t index);
    [[nodiscard]] const std::vector<std::uint64_t>& timestamps() const noexcept{return timestamps_;}
private:
    std::filesystem::path path_;
    std::vector<std::uint64_t> timestamps_;
};
[[nodiscard]] erplay::Quaternion slerp(erplay::Quaternion a,erplay::Quaternion b,double t);
}
