#pragma once
// UI sounds for the overlay. Sounds are the user's own WAV files, shipped next to TheaterMode.dll in
// sounds\ui\ (same layout as the source folder). Playback runs on its own XAudio2 thread, so
// Play() never blocks the render or game thread; missing files or audio devices only disable sound.
namespace TheaterUI::Sound
{
    enum class Cue
    {
        Open,      // overlay shown            inventory/open/ui_inventory_open_0N.wav
        Close,     // overlay hidden           inventory/close/ui_inventory_close_0N.wav
        Focus,     // pointer onto a control   menu/focus/ui_menu_focus_0N.wav
        Tab,       // rail tool / panel switch menu/tabs/ui_menu_tab_0N.wav
        Ok,        // confirm / play / load    menu/ui_menu_ok.wav
        Cancel,    // cancel / pause / stop    menu/ui_menu_cancel.wav
        PrevNext,  // step, page, previous/next menu/ui_menu_prevnext.wav
        Bracket,   // toggles, sort, select    menu/ui_menu_bracket.wav
        Message,   // event log notice/error   ui_message.wav
        Count
    };
    void Play(Cue cue);
    void SetEnabled(bool enabled);
    void SetVolume(float volume); // 0..1
    bool Enabled();
    float Volume();
}
