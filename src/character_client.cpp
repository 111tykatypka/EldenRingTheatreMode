#include "character_client.hpp"
#include "clock_helpers.hpp"
#include <sstream>
#include <algorithm>

namespace erplay {
CharacterRecord read_character_record(std::istream &);
}
namespace character_capture {
namespace {
struct Event {
  HANDLE handle{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
  ~Event() {
    if (handle)
      CloseHandle(handle);
  }
};
bool complete(HANDLE pipe, OVERLAPPED &operation, HANDLE stop,
              DWORD &transferred) {
  const HANDLE events[]{stop, operation.hEvent};
  if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) {
    CancelIoEx(pipe, &operation);
    GetOverlappedResult(pipe, &operation, &transferred, TRUE);
    return false;
  }
  return GetOverlappedResult(pipe, &operation, &transferred, FALSE) != FALSE;
}
} // namespace
void Client::start(std::atomic<DWORD> &pid) {
  if (worker_.joinable())
    return;
  stop_ = false;
  stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!stop_event_)
    throw std::runtime_error("Cannot create character IPC stop event");
  worker_ = std::jthread([this, &pid] { run(pid); });
}
void Client::close() {
  stop_ = true;
  if (stop_event_)
    SetEvent(stop_event_);
  if (worker_.joinable())
    worker_.join();
  if (stop_event_) {
    CloseHandle(stop_event_);
    stop_event_ = nullptr;
  }
}
Stats Client::stats() const {
  std::lock_guard lock(mutex_);
  auto s = stats_;
  s.queued = frames_.size();
  return s;
}
std::vector<Frame> Client::take_until(std::uint64_t timestamp) {
  std::lock_guard lock(mutex_);
  std::vector<Frame> out;
  while (!frames_.empty() && frames_.front().timestamp <= timestamp) {
    out.push_back(std::move(frames_.front()));
    frames_.pop_front();
  }
  return out;
}
void Client::run(std::atomic<DWORD> &pid) {
  struct Header {
    std::uint32_t magic;
    std::uint16_t version, flags;
    std::uint32_t count, reserved;
    std::uint64_t sequence, timestamp, drops;
  };
  static_assert(sizeof(Header) == 40);
  Event event;
  if (!event.handle)
    return;
  while (!stop_) {
    auto pipe = CreateNamedPipeW(
        pipe_.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
      return;
    ResetEvent(event.handle);
    OVERLAPPED connect{};
    connect.hEvent = event.handle;
    DWORD ignored = 0;
    bool connected = ConnectNamedPipe(pipe, &connect) != FALSE;
    if (!connected) {
      const auto error = GetLastError();
      connected = error == ERROR_PIPE_CONNECTED ||
                  (error == ERROR_IO_PENDING &&
                   complete(pipe, connect, stop_event_, ignored));
    }
    if (!connected) {
      CloseHandle(pipe);
      continue;
    }
    DWORD client = 0;
    GetNamedPipeClientProcessId(pipe, &client);
    if (!pid.load() || client != pid.load()) {
      DisconnectNamedPipe(pipe);
      CloseHandle(pipe);
      continue;
    }
    {
      std::lock_guard lock(mutex_);
      stats_.connected = true;
      frames_.clear();
    }
    auto read = [&](void *data, std::size_t bytes) {
      auto *cursor = static_cast<char *>(data);
      while (bytes && !stop_) {
        ResetEvent(event.handle);
        OVERLAPPED operation{};
        operation.hEvent = event.handle;
        DWORD got = 0;
        if (!ReadFile(pipe, cursor, static_cast<DWORD>(bytes), &got,
                      &operation)) {
          if (GetLastError() != ERROR_IO_PENDING ||
              !complete(pipe, operation, stop_event_, got))
            return false;
        }
        if (!got)
          return false;
        cursor += got;
        bytes -= got;
      }
      return bytes == 0;
    };
    std::uint64_t last = 0, last_time = 0, window = GetTickCount64(),
                  window_frames = 0;
    bool malformed = false;
    while (!stop_) {
      Header header{};
      if (!read(&header, sizeof(header)))
        break;
      const auto now_ns = theater_clock::monotonic_ns();
      if (header.magic != 0x54524843 || (header.version != 1&&header.version!=2) ||
          (header.flags & ~6) || (header.version==1&&header.reserved) || header.reserved>16385 || header.count > 16384 ||
          header.sequence <= last || header.timestamp < last_time ||
          header.timestamp > now_ns + 1'000'000'000ULL) {
        malformed = true;
        break;
      }
      std::string payload(static_cast<std::size_t>(header.count) *
                              erplay::character_record_bytes+std::size_t(header.reserved)*erplay::visual_record_bytes,
                          '\0');
      if (!read(payload.data(), payload.size()))
        break;
      Frame frame{
          header.sequence, header.timestamp, header.drops, header.flags, {}};
      frame.rows.reserve(header.count);
      try {
        std::istringstream in(payload, std::ios::binary);
        erplay::CharacterValidator validator;
        for (std::uint32_t i = 0; i < header.count; ++i) {
          auto row = erplay::read_character_record(in);
          if (row.kind != erplay::CharacterKind::transform ||
              row.timestamp_ns != header.timestamp)
            throw std::runtime_error("character IPC kind/time");
          auto registry = row;
          registry.kind = erplay::CharacterKind::registry;
          validator.accept(registry);
          validator.accept(row);
          frame.rows.push_back(row);
        }
        for(std::uint32_t j=0;j<header.reserved;++j){auto visual=erplay::read_visual(in);if(visual.timestamp_ns!=header.timestamp)throw std::runtime_error("visual IPC time");if(visual.id&&std::none_of(frame.rows.begin(),frame.rows.end(),[&](const auto&r){return r.id==visual.id;}))throw std::runtime_error("visual IPC unregistered actor");if(std::any_of(frame.visuals.begin(),frame.visuals.end(),[&](const auto&v){return v.id==visual.id;}))throw std::runtime_error("duplicate visual IPC identity");frame.visuals.push_back(visual);}
      } catch (...) {
        malformed = true;
        break;
      }
      {
        std::lock_guard lock(mutex_);
        if (last && header.sequence > last + 1)
          stats_.dropped += header.sequence - last - 1;
        if (frames_.size() >= 16) {
          frames_.pop_front();
          ++stats_.dropped;
        }
        ++stats_.frames;
        ++window_frames;
        const auto now = GetTickCount64();
        if (now - window >= 1000) {
          stats_.rate = double(window_frames) * 1000 / double(now - window);
          window_frames = 0;
          window = now;
        }
        stats_.tracked = header.count;
        stats_.source_drops = header.drops;
        frames_.push_back(std::move(frame));
      }
      last = header.sequence;
      last_time = header.timestamp;
    }
    {
      std::lock_guard lock(mutex_);
      stats_.connected = false;
      stats_.tracked = 0;
      stats_.rate = 0;
      frames_.clear();
      if (malformed)
        ++stats_.rejected;
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }
}
} // namespace character_capture
