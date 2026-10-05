#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include "player_action.hpp"
#include "character_track.hpp"

namespace erplay {

struct Quaternion { float x{}, y{}, z{}, w{1.0f}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Sample {
    std::uint64_t index{};
    std::uint64_t replay_time_ns{};
    std::uint64_t source_time_ns{};
    Vec3 position{};
    Quaternion orientation{};
    std::optional<ActionState> action;
};
struct Metadata {
    std::uint32_t format_version{2};
    std::string title;
    std::string description;
    std::string tags;
    std::string game_version{"2.7.0.0"};
    std::string mod_version{"0.2.0"};
    std::uint64_t recording_start_unix_ns{};
    double requested_rate_hz{60.0};
};
struct Summary {
    Metadata metadata;
    std::uint64_t sample_count{};
    std::uint64_t duration_ns{};
    std::uint64_t paused_duration_ns{};
    double actual_rate_hz{};
    std::uint64_t chunk_count{};
    std::uint64_t action_event_count{};
    bool animation_sync_observations{};
    std::uint64_t character_count{},character_sample_count{};
};
enum class RecordingState { idle, recording, paused, saving, ready, error };

// Incremental .tmp writer. finalize() validates the completed stream and atomically
// renames it to the requested .erplay path. Destruction leaves an incomplete .tmp.
class Writer {
public:
    Writer(std::filesystem::path final_path, Metadata metadata,
           std::uint32_t samples_per_chunk = 600);
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    void append(Sample sample);
    void append_character(CharacterRecord record);
    [[nodiscard]] Summary finalize(std::uint64_t paused_duration_ns = 0);
    [[nodiscard]] const std::filesystem::path& temporary_path() const noexcept;
private:
    struct Impl;
    Impl* impl_;
};

// Converts the adapter's monotonic source clock into an active replay clock.
// Paused intervals are omitted from replay time and their wall duration is retained.
class RecordingSession {
public:
    explicit RecordingSession(Writer& writer) noexcept;
    void start();
    void ingest(Sample source_sample);
    void ingest_character(CharacterRecord source_record);
    void pause(std::uint64_t source_time_ns);
    void resume(std::uint64_t source_time_ns);
    [[nodiscard]] Summary stop();
    [[nodiscard]] RecordingState state() const noexcept { return state_; }
    [[nodiscard]] std::uint64_t accepted_samples() const noexcept { return sample_index_; }
    [[nodiscard]] std::uint64_t paused_duration_ns() const noexcept { return paused_total_ns_; }
private:
    Writer& writer_;
    RecordingState state_{RecordingState::idle};
    std::uint64_t source_origin_ns_{}, pause_started_ns_{}, paused_total_ns_{}, sample_index_{};
    bool has_origin_{};
};

// Validated random-access reader. It keeps a compact timestamp/file-offset index
// and only caches one serialized chunk of samples at a time.
class Reader {
public:
    explicit Reader(const std::filesystem::path& path);
    ~Reader();
    Reader(Reader&&) noexcept;
    Reader& operator=(Reader&&) noexcept;
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    [[nodiscard]] const Summary& summary() const noexcept;
    [[nodiscard]] Sample sample(std::uint64_t index) const;
    [[nodiscard]] std::pair<Sample, Sample> bracket(std::uint64_t replay_time_ns) const;
    [[nodiscard]] std::uint64_t lower_sample(std::uint64_t replay_time_ns) const;
    [[nodiscard]] const std::vector<ActionEvent>& action_events()const noexcept;
    [[nodiscard]] std::vector<CharacterInfo> characters()const;
    [[nodiscard]] std::vector<CharacterRecord> character_preview(std::uint64_t id,std::size_t maximum=3000)const;
    [[nodiscard]] std::optional<CharacterRecord> character_at(std::uint64_t id,std::uint64_t time)const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Full integrity scan. Throws with a diagnostic for malformed/truncated data.
[[nodiscard]] Summary validate(const std::filesystem::path& path);
// Salvages complete checksum-valid chunks from an interrupted .tmp file.
// The original remains untouched; recovered output is validated before rename.
[[nodiscard]] Summary recover_incomplete(const std::filesystem::path& temporary_path,
                                         const std::filesystem::path& final_path);

} // namespace erplay
