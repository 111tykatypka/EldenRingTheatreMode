#include "character_client.hpp"
#include "clock_helpers.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
namespace {
void check(bool value) {
  if (!value)
    throw std::runtime_error("character IPC check failed");
}
template <class F> bool wait(F predicate) {
  const auto until = GetTickCount64() + 3000;
  while (GetTickCount64() < until) {
    if (predicate())
      return true;
    Sleep(2);
  }
  return false;
}
struct Pipe {
  HANDLE handle{INVALID_HANDLE_VALUE};
  ~Pipe() {
    if (handle != INVALID_HANDLE_VALUE)
      CloseHandle(handle);
  }
};
struct Header {
  std::uint32_t magic{0x54524843};
  std::uint16_t version{1}, flags{};
  std::uint32_t count{1}, reserved{};
  std::uint64_t sequence{}, timestamp{}, drops{};
};
void send(HANDLE pipe, std::uint64_t sequence, bool invalid = false) {
  Header header;
  header.sequence = sequence;
  header.version=sequence==1?1:2;header.reserved=header.version==2?2:0;
  header.timestamp = theater_clock::monotonic_ns();
  erplay::CharacterRecord row;
  row.id = 42;
  row.native_handle = 123;
  row.timestamp_ns = header.timestamp;
  if (invalid)
    row.orientation = {0, 0, 0, 0};
  std::ostringstream payload(std::ios::binary);
  erplay::write_character_record(payload, row);
  if(header.version==2){
    erplay::VisualState player;player.timestamp_ns=header.timestamp;player.flags=8;player.equipment[0]=123;erplay::write_visual(payload,player);
    erplay::VisualState npc; npc.id=42;npc.timestamp_ns=header.timestamp;npc.flags=3;npc.hp=50;npc.max_hp=100;erplay::write_visual(payload,npc);
  }
  auto bytes = payload.str();
  check(bytes.size() == 112+header.reserved*erplay::visual_record_bytes);
  DWORD written{};
  check(WriteFile(pipe, &header, sizeof(header), &written, nullptr) &&
        written == sizeof(header));
  check(WriteFile(pipe, bytes.data(), static_cast<DWORD>(bytes.size()),
                  &written, nullptr) &&
        written == bytes.size());
}
} // namespace
int main() {
  try {
    const auto name = L"\\\\.\\pipe\\TheaterModernCharacterTest_" +
                      std::to_wstring(GetCurrentProcessId());
    std::atomic<DWORD> pid{GetCurrentProcessId()};
    // Closing without a peer must cancel ConnectNamedPipe.
    {
      character_capture::Client client(name);
      client.start(pid);
      Sleep(10);
      const auto before = GetTickCount64();
      client.close();
      check(GetTickCount64() - before < 1000);
    }
    character_capture::Client client(name);
    client.start(pid);
    Pipe peer;
    check(wait([&] {
      peer.handle = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, 0, nullptr);
      return peer.handle != INVALID_HANDLE_VALUE;
    }));
    check(wait([&] { return client.stats().connected; }));
    for (std::uint64_t i = 1; i <= 24; ++i)
      send(peer.handle, i);
    check(wait([&] { return client.stats().frames == 24; }));
    auto stats = client.stats();
    check(stats.queued == 16 && stats.dropped == 8 && stats.tracked == 1);
    auto frames = client.take_until(UINT64_MAX);
    check(frames.size() == 16 && frames.front().sequence == 9 &&
          frames.back().sequence == 24);
    check(frames.back().rows[0].id == 42 &&
          frames.back().rows[0].native_handle == 123);
    check(frames.back().visuals.size()==2&&frames.back().visuals[0].equipment[0]==123&&frames.back().visuals[1].hp==50);
    send(peer.handle, 25, true);
    check(wait([&] {
      return client.stats().rejected == 1 && !client.stats().connected;
    }));
    // The worker can also close while waiting for a new connection after
    // malformed data.
    client.close();
    CloseHandle(peer.handle);
    peer.handle = INVALID_HANDLE_VALUE;
    character_capture::Client blocked(name);
    blocked.start(pid);
    check(wait([&] {
      peer.handle = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, 0, nullptr);
      return peer.handle != INVALID_HANDLE_VALUE;
    }));
    check(wait([&] { return blocked.stats().connected; }));
    const auto before = GetTickCount64();
    blocked.close();
    check(GetTickCount64() - before < 1000);
    std::cout << "character IPC: bounded queue, identity, invalid quaternion "
                 "rejection, pending connect/read shutdown PASS; no game\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
