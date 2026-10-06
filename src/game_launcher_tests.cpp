#include "game_launcher.hpp"
#include "GameProfile.h"
#include <shellapi.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace game_launcher;
template<class Function> bool rejects(Function function) {
    try { function(); return false; } catch (const std::exception&) { return true; }
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 4) {
        Paths installed{argv[1], argv[2], argv[3], fs::temp_directory_path() / L"Theater Mode test" / L"YAFSML.ini"};
        validate_dependencies(installed);
        std::ifstream input(installed.loader.parent_path() / L"YAFSML.ini", std::ios::binary);
        std::string original{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto prepared = prepare_config(original, installed.loader.parent_path(), installed.dll);
        assert(prepared.find("theater_mode=" + utf8(installed.dll.wstring())) != std::string::npos);
        TmValidationReport report{};
        assert(tm_validate_profile(installed.game.c_str(), 0, &report) == 0);
        std::cout << "Installed prerequisites PASS: AMD64 loader/DLL, original YAFSML config, shared EldenRing_1_17 profile, file/product version 2.7.0.0, disk SHA256=" << report.sha256 << ". No game launched or original config changed.\n";
        return 0;
    }
    const fs::path directory = L"C:\\Tools\\YAFSML с пробелами";
    const fs::path dll = L"D:\\Theater Mode\\Тест\\TheaterMode.dll";
    const std::string source = "; original settings\r\ngame=eldenring\r\n[patch]\r\ndisable_arxan=0\r\n[log]\r\nconsole=1\r\n[dll]\r\ntheater_mode=C:\\old\\TheaterMode.dll\r\n[mod]\r\n;mod1=mod\r\n";
    auto expected = source;
    expected.replace(expected.find("C:\\old\\TheaterMode.dll"), std::string("C:\\old\\TheaterMode.dll").size(), utf8(dll.wstring()));
    assert(prepare_config(source, directory, dll) == expected);
    const auto relative = prepare_config("game=eldenring\n[dll]\ntheater_mode=old.dll|delay=100\nhelper=helper.dll|early\n[mod]\nmod1=mods\\тест\n", directory, dll);
    assert(relative.find("theater_mode=" + utf8(dll.wstring()) + "|delay=100") != std::string::npos);
    assert(relative.find("helper=" + utf8((directory / L"helper.dll").wstring()) + "|early") != std::string::npos);
    assert(relative.find("mod1=" + utf8((directory / L"mods\\тест").wstring())) != std::string::npos);
    assert(rejects([&] { prepare_config(source + "[dll]\ntheater_mode=second.dll\n", directory, dll); }));
    assert(rejects([&] { prepare_config("game=nightreign\n[dll]\ntheater_mode=x.dll", directory, dll); }));
    assert(rejects([&] { prepare_config("game=eldenring\n[dll]\nother=x.dll", directory, dll); }));
    assert(rejects([&] { prepare_config(source + "[dll]\nother=TheaterMode.dll", directory, dll); }));
    assert(rejects([&] { prepare_config(source + "[mod]\nmod1=C:relative", directory, dll); }));
    assert(rejects([&] { wide(std::string(1, '\xff')); }));
    {
        const auto off = set_console(source, false);
        assert(off.find("console=1") == std::string::npos && off.find("[log]\r\nconsole=0\r\n") != std::string::npos);
        assert(set_console(source, true).find("[log]\r\nconsole=1\r\n") != std::string::npos);
        const auto missing = set_console("game=eldenring\n[dll]\ntheater_mode=x.dll\n", false);
        assert(missing.find("[log]\nconsole=0\n") != std::string::npos);
        const auto empty_log = set_console("game=eldenring\n[log]\nlog_file=1\n[dll]\n", false);
        assert(empty_log.find("[log]\nlog_file=1\nconsole=0\n[dll]") != std::string::npos);
    }
    assert(wide(utf8(L"Русский filename 😀")) == L"Русский filename 😀");
    Paths paths{directory / L"YAFSML.exe", L"C:\\Game with spaces\\eldenring.exe", dll, L"C:\\Запуск\\YAFSML.ini"};
    auto command = quote_argument(paths.loader.wstring()) + launch_arguments(paths);
    int count = 0;
    auto arguments = CommandLineToArgvW(command.c_str(), &count);
    assert(arguments && count == 9);
    assert(arguments[0] == paths.loader.wstring());
    assert(std::wstring(arguments[1]) == L"-t" && std::wstring(arguments[2]) == L"eldenring");
    assert(arguments[4] == paths.game.wstring() && arguments[6] == paths.config.wstring());
    assert(arguments[8] == (directory / L"YAFSML.dll").wstring());
    LocalFree(arguments);
    const std::wstring tricky = L"D:\\path with \"quotes\"\\";
    command = L"test " + quote_argument(tricky);
    arguments = CommandLineToArgvW(command.c_str(), &count);
    assert(arguments && count == 2 && arguments[1] == tricky);
    LocalFree(arguments);
    assert(rejects([&] { validate_dependencies(paths); })); // No real launch required.
    Launcher launcher;
    assert(launcher.start(paths, [] { return Runtime{}; }, [](const auto&) {}));
    for (unsigned i = 0; i < 100 && launcher.state().phase != Phase::error; ++i) Sleep(10);
    assert(launcher.state().phase == Phase::error);
    assert(launcher.state().diagnostic.find(L"Host pipe or F6") != std::wstring::npos);
    launcher.close();
    std::cout << "Launcher PASS: config preservation, relative paths/conditions, Unicode, argument quoting, duplicate/wrong config rejection, missing dependencies, host-not-ready refusal. No loader or game was launched.\n";
}
