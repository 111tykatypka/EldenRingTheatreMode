#pragma once
#include <cstdint>
#include <map>
#include <string>
// UI-thread-only annotations. Names are user observations, not native resource names.
namespace particle_catalog {
struct Reference {
    std::uint32_t id;
    const char *bank, *resources, *refs, *origin, *color, *behavior, *info;
};
const Reference* reference(std::uint32_t id);
std::string search_text(std::uint32_t id);
struct Entry { std::string name, category; bool favorite=false; };
const std::map<std::uint32_t,Entry>& entries();
bool save(std::uint32_t id, Entry entry);
const std::string& status();
std::string label(std::uint32_t id);
}
