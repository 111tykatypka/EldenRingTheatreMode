#pragma once
#include "game_control.hpp"
#include "replay_player.hpp"
#include <functional>

namespace in_game_replay {
std::optional<erplay::CharacterRecord> find_start_anchor(const erplay::Reader& reader);
enum class Phase { inactive, starting, playing, paused, finishing, finished, error, restarting, preparing };
// Serialized playback coordinator. The existing Player remains the ONLY replay clock.
class Controller {
public:
    using Logger=std::function<void(const std::string&)>;
    explicit Controller(game_control::Client& control,Logger logger):control_(control),logger_(std::move(logger)){}
    bool play(replay::Player& player,std::uint64_t limit_ns,replay::Player::Clock::time_point now=replay::Player::Clock::now());
    bool prepare_start(replay::Player& player,bool autoplay=false,std::uint64_t limit=0,replay::Player::Clock::time_point now=replay::Player::Clock::now());
    void pause(replay::Player::Clock::time_point now=replay::Player::Clock::now());
    void restart(replay::Player& player,std::uint64_t limit_ns,replay::Player::Clock::time_point now=replay::Player::Clock::now());
    void tick(replay::Player::Clock::time_point now=replay::Player::Clock::now());
    void stop();
    bool active() const;
    void enable_animation(bool enabled){if(!active())animation_=enabled;}
    void enable_characters(bool enabled){if(!active())characters_=enabled;}
    void xz_diagnostic(bool enabled){if(!active())xz_only_=enabled;}
    void select_actor_only(std::uint64_t id){if(!active())selected_actor_=id;}
    Phase phase() const {return phase_;}
    const std::wstring& diagnostic() const {return diagnostic_;}
    std::uint64_t limit_ns() const {return limit_ns_;}
private:
    game_control::Client& control_;Logger logger_;replay::Player* player_{};
    Phase phase_{Phase::inactive};std::wstring diagnostic_{L"INACTIVE — no transform writes"};
    std::uint64_t session_{},limit_ns_{};replay::Player::Clock::time_point deadline_{};bool return_autoplay_{}; std::uint64_t return_limit_{}; bool pause_on_start_{};bool animation_{};
    std::uint64_t selected_actor_{},actor_log_ns_{};bool characters_{};bool xz_only_{};std::vector<erplay::CharacterInfo> actors_;
    bool send_characters();
    void transition(Phase phase,const std::wstring& diagnostic);
    void fail(const std::wstring& diagnostic);
    void finish(replay::Player::Clock::time_point now);
    game_control::Transform transform() const;
};
}
