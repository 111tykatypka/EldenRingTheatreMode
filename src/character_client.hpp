#pragma once
#include "erplay.hpp"
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <windows.h>
namespace character_capture {
struct Frame {
  std::uint64_t sequence{}, timestamp{}, source_drops{};
  std::uint16_t flags{};
  std::vector<erplay::CharacterRecord> rows;
};
struct Stats {
  double rate{};
  bool connected{};
  std::uint64_t frames{}, dropped{}, source_drops{}, tracked{}, queued{},
      rejected{};
};
class Client {
public:
  explicit Client(
      std::wstring pipe = L"\\\\.\\pipe\\EldenRingTheaterMode_1_17_Characters")
      : pipe_(std::move(pipe)) {}
  ~Client() { close(); }
  void start(std::atomic<DWORD> &);
  void close();
  std::vector<Frame> take_until(std::uint64_t timestamp);
  Stats stats() const;

private:
  mutable std::mutex mutex_;
  std::deque<Frame> frames_;
  Stats stats_;
  std::jthread worker_;
  std::atomic_bool stop_{};
  std::wstring pipe_;
  HANDLE stop_event_{};
  void run(std::atomic<DWORD> &);
};
} // namespace character_capture
