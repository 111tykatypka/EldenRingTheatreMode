#include "playback_worker.hpp"
#include <atomic>
#include <thread>
#include <iostream>
int main(){std::atomic<unsigned> ticks{};const auto start=std::chrono::steady_clock::now();std::jthread producer([&](std::stop_token stop){playback_worker::run(stop,[&]{++ticks;});});Sleep(1000); // Main/UI-like thread blocked; producer must continue.
producer.request_stop();producer.join();const double dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::cout<<"OFFLINE_WORKER requested=120 Hz measured="<<double(ticks)/dt<<" ticks="<<ticks<<" main_thread_blocked=YES; not an IPC/game rate\n";return ticks>=20?0:1;}
