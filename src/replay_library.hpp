#pragma once
// Replay Library operations used by the in-game overlay: display names, safe unique file names,
// sorting, rename (display name + file + sidecars) and delete to the Recycle Bin.
#include "editor_backend.hpp"
#include <string>
#include <vector>

namespace theater::library {

enum class SortKey : std::uint32_t { date, size, name, duration };
struct SortOrder { SortKey key{SortKey::date}; bool descending{true}; };

// Title stored in the replay, or the file name for replays recorded before names existed.
std::string display_name(const ReplayEntry& entry);
// "Replay 2026-10-07 01-07" in local time.
std::string default_name();
// Windows-safe file stem from a display name: invalid characters removed, trimmed, reserved device
// names avoided, length bounded. Never empty.
std::wstring safe_stem(const std::string& display_name);
// <directory>/<stem>.erplay, or "<stem> (2).erplay" and so on if the file (or its .tmp) exists.
fs::path unique_replay_path(const fs::path& directory, const std::wstring& stem, const fs::path& ignore = {});
void sort(std::vector<ReplayEntry>& entries, SortOrder order);
SortOrder load_sort(const fs::path& root);
void save_sort(const fs::path& root, SortOrder order);

// Results are user-facing sentences (English); empty error means success.
struct Result { bool ok{}; std::string message; fs::path path; };
// Rejects empty and duplicate names (case-insensitive, against other replays' display names).
Result rename(const ReplayEntry& entry, const std::string& new_name, const std::vector<ReplayEntry>& all);
// Moves the replay and every sidecar ("<file>.*", e.g. .bookmarks) to the Recycle Bin.
Result recycle(const ReplayEntry& entry);
}
