#include "replay_library.hpp"
#include "erplay.hpp"
#include "game_launcher.hpp"
#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace theater::library {
namespace {
std::wstring lower(std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c)); return s; }
bool legacy_title(const std::string& t) { return t.empty() || t == "Elden Ring recording"; }
// Sidecar files belong to a replay when their name is "<replay file name>.<anything>".
std::vector<fs::path> sidecars(const fs::path& replay) {
  std::vector<fs::path> out;
  std::error_code ec;
  const auto prefix = lower(replay.filename().wstring() + L".");
  for (const auto& it : fs::directory_iterator(replay.parent_path(), ec)) {
    if (ec) break;
    const auto name = lower(it.path().filename().wstring());
    if (it.is_regular_file() && name.size() > prefix.size() && name.starts_with(prefix)) out.push_back(it.path());
  }
  return out;
}
}

std::string display_name(const ReplayEntry& e) {
  return legacy_title(e.summary.metadata.title) ? game_launcher::utf8(e.path.stem().wstring()) : e.summary.metadata.title;
}

std::string default_name() {
  const std::time_t now = std::time(nullptr);
  std::tm t{};
  localtime_s(&t, &now);
  std::ostringstream s;
  s << "Replay " << std::put_time(&t, "%Y-%m-%d %H-%M");
  return s.str();
}

std::wstring safe_stem(const std::string& display) {
  std::wstring in;
  try { in = game_launcher::wide(display); } catch (...) { in.clear(); }
  std::wstring out;
  for (wchar_t c : in) {
    if (c < 32 || std::wcschr(L"<>:\"/\\|?*", c)) continue;
    out.push_back(c);
  }
  while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
  while (!out.empty() && out.front() == L' ') out.erase(out.begin());
  if (out.size() > 80) out.resize(80);
  while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
  if (out.empty()) out = L"Replay";
  static const wchar_t* reserved[] = {L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4", L"com5", L"com6",
                                      L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4", L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9"};
  for (auto* r : reserved) if (lower(out) == r) { out += L"_"; break; }
  return out;
}

fs::path unique_replay_path(const fs::path& dir, const std::wstring& stem, const fs::path& ignore) {
  auto taken = [&](const fs::path& p) {
    if (!ignore.empty() && lower(p.wstring()) == lower(ignore.wstring())) return false;
    return fs::exists(p) || fs::exists(fs::path(p.wstring() + L".tmp"));
  };
  auto candidate = dir / (stem + L".erplay");
  for (unsigned i = 2; taken(candidate); ++i) candidate = dir / (stem + L" (" + std::to_wstring(i) + L").erplay");
  return candidate;
}

void sort(std::vector<ReplayEntry>& entries, SortOrder order) {
  auto less = [&](const ReplayEntry& a, const ReplayEntry& b) {
    switch (order.key) {
    case SortKey::size: return a.bytes < b.bytes;
    case SortKey::duration: return a.summary.duration_ns < b.summary.duration_ns;
    case SortKey::name: return lower(game_launcher::wide(display_name(a))) < lower(game_launcher::wide(display_name(b)));
    case SortKey::date:
    default: return a.modified < b.modified;
    }
  };
  std::stable_sort(entries.begin(), entries.end(), [&](const auto& a, const auto& b) { return order.descending ? less(b, a) : less(a, b); });
}

SortOrder load_sort(const fs::path& root) {
  SortOrder order;
  std::ifstream in(root / L"Library.settings");
  std::string key;
  unsigned value = 0;
  while (in >> key >> value) {
    if (key == "sort_key" && value <= 3) order.key = static_cast<SortKey>(value);
    else if (key == "sort_descending") order.descending = value != 0;
  }
  return order;
}

void save_sort(const fs::path& root, SortOrder order) {
  std::ofstream out(root / L"Library.settings", std::ios::trunc);
  out << "sort_key " << static_cast<unsigned>(order.key) << "\nsort_descending " << (order.descending ? 1 : 0) << "\n";
}

Result rename(const ReplayEntry& entry, const std::string& requested, const std::vector<ReplayEntry>& all) {
  std::string name = requested;
  while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
  while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.erase(name.begin());
  if (name.empty()) return {false, "The name can't be empty."};
  if (name.size() > 120) return {false, "The name is too long (120 characters at most)."};
  std::wstring wanted;
  try { wanted = lower(game_launcher::wide(name)); } catch (...) { return {false, "The name contains invalid text."}; }
  for (const auto& other : all)
    if (other.path != entry.path && lower(game_launcher::wide(display_name(other))) == wanted)
      return {false, "Another replay is already named '" + name + "'."};
  try {
    erplay::set_title(entry.path, name);
    const auto target = unique_replay_path(entry.path.parent_path(), safe_stem(name), entry.path);
    if (lower(target.wstring()) != lower(entry.path.wstring())) {
      const auto old_sidecars = sidecars(entry.path);
      fs::rename(entry.path, target);
      for (const auto& s : old_sidecars) {
        const auto suffix = s.filename().wstring().substr(entry.path.filename().wstring().size());
        std::error_code ec;
        fs::rename(s, target.parent_path() / (target.filename().wstring() + suffix), ec);
        if (ec) log_line("LIBRARY_RENAME sidecar kept old name: " + game_launcher::utf8(s.wstring()) + " error=" + ec.message());
      }
      log_line("LIBRARY_RENAME " + game_launcher::utf8(entry.path.wstring()) + " -> " + game_launcher::utf8(target.wstring()));
      return {true, "Renamed to '" + name + "'.", target};
    }
    log_line("LIBRARY_RENAME title only: " + game_launcher::utf8(entry.path.wstring()));
    return {true, "Renamed to '" + name + "'.", entry.path};
  } catch (const std::exception& e) {
    log_line(std::string("LIBRARY_RENAME failed: ") + e.what());
    return {false, std::string("Rename failed: ") + e.what()};
  }
}

Result recycle(const ReplayEntry& entry) {
  auto files = sidecars(entry.path);
  files.insert(files.begin(), entry.path);
  std::wstring list;
  for (const auto& f : files) { list += fs::absolute(f).wstring(); list.push_back(L'\0'); }
  list.push_back(L'\0');
  SHFILEOPSTRUCTW op{};
  op.wFunc = FO_DELETE;
  op.pFrom = list.c_str();
  op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
  const int result = SHFileOperationW(&op);
  if (result != 0 || op.fAnyOperationsAborted) {
    log_line("LIBRARY_DELETE failed code=" + std::to_string(result) + " file=" + game_launcher::utf8(entry.path.wstring()));
    return {false, "Couldn't move the replay to the Recycle Bin (error " + std::to_string(result) + ")."};
  }
  log_line("LIBRARY_DELETE recycled " + std::to_string(files.size()) + " file(s): " + game_launcher::utf8(entry.path.wstring()));
  return {true, "Moved '" + display_name(entry) + "' to the Recycle Bin."};
}
}
