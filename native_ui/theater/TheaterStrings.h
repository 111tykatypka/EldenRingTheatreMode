#pragma once
// English and Russian UI text for the in-game Theater overlay. Every visible
// string goes through Tr() so a language switch never needs a restart.
// Literals are UTF-8: the overlay library compiles with /utf-8.
#include <cstdint>

namespace TheaterUI
{
    enum class Lang : uint8_t { English, Russian };

    enum class Str : uint16_t
    {
        // rail
        Scene, Camera, Look, Replays, Export, Debug, Settings, HideUi,
        // transport and sequencer
        Play, Stop, Restart, StepBack, StepForward, Speed, Record, StopRecording, NoReplay, ReplayLoaded,
        HideUiHint, ShowUiHint, Tracks, TrackReplay, TrackActors, TrackCamera, TrackBookmarks,
        // badges
        BadgeIdle, BadgeReady, BadgeStarting, BadgePlaying, BadgePaused, BadgeStopping, BadgeFinished,
        BadgeError, BadgeRestarting, BadgePreparing, BadgeOffline,
        // panels
        Connection, Game, Player, Connected, Waiting, Found, LivePosition, Actors, Previous, NextPage,
        ActorRow, SelectedActor, EventLog, EventLogEmpty, NotYetAvailable, CameraNotes, LookNotes,
        ReplaysNotes, ExportNotes, Diagnostic, NativeGhost, NativeGhostNone, Language, UiScale,
        Recording, RecordingSaving, RecordingPaused, HotkeysTitle, HotkeysBody, ReplayTime, Duration,
        // event log
        LogHostLinked, LogHostLost, LogGameConnected, LogGameWaiting, LogPlayerFound, LogPlayerLost,
        LogRecStopped, LogRecStarted, LogRecPaused, LogRecSaving,
        // replay library
        Library, Open, NoReplays, LoadedTag, LogReplayOpenSent,
        Count
    };

    struct StrPair { const char* en; const char* ru; };

    // Order must match Str.
    inline constexpr StrPair kStrings[] = {
        { "Scene", "Сцена" }, { "Camera", "Камера" }, { "Look", "Вид" }, { "Replays", "Записи" },
        { "Export", "Экспорт" }, { "Debug", "Отладка" }, { "Settings", "Настройки" }, { "Hide UI", "Скрыть" },

        { "Play", "Пуск" }, { "Stop", "Стоп" }, { "Restart", "С начала" }, { "Previous tick", "Шаг назад" },
        { "Next tick", "Шаг вперёд" }, { "Speed", "Скорость" }, { "Record", "Запись" }, { "Stop recording", "Остановить запись" },
        { "No replay loaded", "Запись не загружена" }, { "Replay loaded", "Запись загружена" },
        { "F4  Hide UI", "F4  Скрыть интерфейс" }, { "F4  Show UI", "F4  Показать интерфейс" },
        { "TRACKS", "ДОРОЖКИ" }, { "Replay Player", "Игрок (запись)" }, { "Recorded actors", "Записанные персонажи" },
        { "Camera", "Камера" }, { "Bookmarks", "Закладки" },

        { "IDLE", "ОЖИДАНИЕ" }, { "READY", "ГОТОВО" }, { "STARTING", "ЗАПУСК" }, { "PLAYING", "ВОСПРОИЗВЕДЕНИЕ" },
        { "PAUSED", "ПАУЗА" }, { "STOPPING", "ОСТАНОВКА" }, { "FINISHED", "ЗАВЕРШЕНО" }, { "ERROR", "ОШИБКА" },
        { "RESTARTING", "ПЕРЕЗАПУСК" }, { "PREPARING", "ПОДГОТОВКА" }, { "HOST OFFLINE", "НЕТ СВЯЗИ" },

        { "Connection", "Подключение" }, { "Game", "Игра" }, { "Player", "Игрок" }, { "Connected", "Подключено" },
        { "Waiting", "Ожидание" }, { "Found", "Найден" }, { "Live position", "Позиция" }, { "Recorded actors", "Записанные персонажи" },
        { "Previous", "Назад" }, { "Next", "Далее" }, { "Actor %llu  entity %u  NPC %d", "Персонаж %llu  сущность %u  NPC %d" },
        { "Selected actor", "Выбранный персонаж" }, { "EVENT LOG", "ЖУРНАЛ" }, { "No events yet", "Событий пока нет" },
        { "Not available yet", "Пока недоступно" },
        { "Free camera, follow, orbit and dolly keys need a camera backend that does not exist yet. Nothing here changes the game.",
          "Свободная камера, слежение, орбита и ключи долли требуют модуль камеры, которого пока нет. Здесь ничего не меняет игру." },
        { "Time of day, weather, fog and exposure controls need engine research first.",
          "Время суток, погода, туман и экспозиция требуют сначала исследования движка." },
        { "Record with the button below or F5, stop with F6. Click a replay to open it.",
          "Запись: кнопка ниже или F5, стоп: F6. Нажмите на запись, чтобы открыть её." },
        { "Video export is planned after the camera tools.", "Экспорт видео запланирован после инструментов камеры." },
        { "Host diagnostic", "Диагностика" }, { "Native ghost", "Нативный призрак" }, { "No native ghost status yet", "Статуса нативного призрака пока нет" },
        { "Language", "Язык" }, { "UI scale", "Масштаб интерфейса" },
        { "REC", "REC" }, { "SAVING", "СОХРАНЕНИЕ" }, { "REC PAUSED", "ЗАПИСЬ НА ПАУЗЕ" },
        { "Keys", "Клавиши" },
        { "F4 show/hide UI   Shift+F4 hide REC pill too\nF5 start recording   F6 stop\nWheel zooms the timeline   Shift+Wheel scrolls tracks",
          "F4 показать/скрыть   Shift+F4 скрыть и метку REC\nF5 начать запись   F6 стоп\nКолесо: масштаб шкалы   Shift+колесо: прокрутка дорожек" },
        { "Replay time", "Время записи" }, { "Duration", "Длительность" },
        { "Host connected", "Связь с программой есть" }, { "Host not connected (open the Theater Mode window)", "Нет связи с программой (откройте окно Theater Mode)" },
        { "Game connected", "Игра подключена" }, { "Game waiting", "Ожидание игры" }, { "Player found", "Игрок найден" }, { "Player not found", "Игрок не найден" },
        { "Recording stopped", "Запись остановлена" }, { "Recording started", "Запись начата" }, { "Recording paused", "Запись на паузе" }, { "Saving recording", "Сохранение записи" },
        { "LIBRARY", "БИБЛИОТЕКА" }, { "Open", "Открыть" }, { "No replays yet. Record one with F5.", "Записей пока нет. Начните запись клавишей F5." },
        { "LOADED", "ЗАГРУЖЕНА" }, { "Opening replay", "Открытие записи" },
    };
    static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == (size_t)Str::Count, "kStrings must match Str");

    inline const char* Tr(Lang lang, Str s)
    {
        const StrPair& p = kStrings[(size_t)s];
        return lang == Lang::Russian ? p.ru : p.en;
    }
}
