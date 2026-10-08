// The adapter DLL (Rust) provides these; the stand-alone test programs have no adapter, so they get inert stand-ins.
extern "C" void tm_lighting_time_connected(int) {}
extern "C" void tm_lighting_time_request(int) {}
