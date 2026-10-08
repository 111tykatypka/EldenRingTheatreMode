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
        ReplaysNotes, ExportNotes, Diagnostic, Language, UiScale,
        Recording, RecordingSaving, RecordingPaused, HotkeysTitle, HotkeysBody, ReplayTime, Duration,
        // event log
        LogHostLinked, LogHostLost, LogGameConnected, LogGameWaiting, LogPlayerFound, LogPlayerLost,
        LogRecStopped, LogRecStarted, LogRecPaused, LogRecSaving,
        // replay library
        Library, Open, NoReplays, LoadedTag, LogReplayOpenSent,
        // layout menu, recording names, library actions
        MenuLayout, ResetLayout, PanelTools, PanelSide, PanelTimeline,
        NameTitle, NameHint, StartRecordingBtn, Cancel,
        ColName, ColDate, ColDuration, ColSize, ColArea,
        Load, Rename, Delete, LoadTitle, LoadTeleportNote, RenameTitle, Save, DeleteTitle, DeleteBody,
        LoadedBlocked, NameEmpty, AreaUnknown, GameVersion, FileLabel,
        CopyAll, CopyErrors, Copied, ClickToCopy,
        UnloadReplay,
        UiSounds, UiSoundsOn,
        ReplayWorld, ReplayWorldFlags, ReplayWorldFlagsNote, ReplayPuppets, ReplayPuppetsNote, ReplayFullRate, ReplayFullRateNote, ReplayFreezeAi, ReplayFreezeAiNote, ReplayEquipment, ReplayEquipmentNote, ReplaySummonHorse, ReplaySummonHorseNote, NoNearFade, NoNearFadeNote, ReplayEffects, ReplayEffectsNote,
        Weather, WeatherEditor,
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
        { "Host diagnostic", "Диагностика" },
        { "Language", "Язык" }, { "UI scale", "Масштаб интерфейса" },
        { "REC", "REC" }, { "SAVING", "СОХРАНЕНИЕ" }, { "REC PAUSED", "ЗАПИСЬ НА ПАУЗЕ" },
        { "Keys", "Клавиши" },
        { "F4 show/hide UI   Shift+F4 hide REC pill too\nF5 start recording   F6 stop\nSpace play/pause (replay loaded or UI open)\nWheel zooms the timeline   Shift+Wheel scrolls tracks",
          "F4 показать/скрыть   Shift+F4 скрыть и метку REC\nF5 начать запись   F6 стоп\nПробел: пуск/пауза (запись загружена или открыт интерфейс)\nКолесо: масштаб шкалы   Shift+колесо: прокрутка дорожек" },
        { "Replay time", "Время записи" }, { "Duration", "Длительность" },
        { "Host connected", "Связь с программой есть" }, { "Host not connected (open the Theater Mode window)", "Нет связи с программой (откройте окно Theater Mode)" },
        { "Game connected", "Игра подключена" }, { "Game waiting", "Ожидание игры" }, { "Player found", "Игрок найден" }, { "Player not found", "Игрок не найден" },
        { "Recording stopped", "Запись остановлена" }, { "Recording started", "Запись начата" }, { "Recording paused", "Запись на паузе" }, { "Saving recording", "Сохранение записи" },
        { "LIBRARY", "БИБЛИОТЕКА" }, { "Open", "Открыть" }, { "No replays yet. Record one with F5.", "Записей пока нет. Начните запись клавишей F5." },
        { "LOADED", "ЗАГРУЖЕНА" }, { "Opening replay", "Открытие записи" },
        { "Layout", "Раскладка" }, { "Reset Layout", "Сбросить раскладку" }, { "Tools", "Инструменты" },
        { "Tool panel", "Панель инструмента" }, { "Timeline", "Шкала времени" },
        { "Name this replay", "Название записи" }, { "Enter starts recording. Esc cancels.", "Enter начинает запись. Esc отменяет." },
        { "Start recording", "Начать запись" }, { "Cancel", "Отмена" },
        { "Name", "Название" }, { "Date / Time", "Дата / время" }, { "Duration", "Длительность" }, { "Size", "Размер" }, { "Map / Area", "Карта / область" },
        { "Load", "Загрузить" }, { "Rename", "Переименовать" }, { "Delete", "Удалить" },
        { "Load replay?", "Загрузить запись?" },
        { "When the replay plays, you are moved to where it was recorded (by grace travel if it is far away or in another map).",
          "При воспроизведении вы переместитесь к месту записи (через благодать, если оно далеко или на другой карте)." },
        { "Rename replay", "Переименовать запись" }, { "Save", "Сохранить" }, { "Delete replay?", "Удалить запись?" },
        { "Delete '%s'? It will be moved to the Recycle Bin together with its bookmarks.",
          "Удалить «%s»? Запись и её закладки будут перемещены в корзину." },
        { "This replay is loaded. Load another replay first, then try again.", "Эта запись загружена. Сначала загрузите другую запись." },
        { "The name can't be empty.", "Название не может быть пустым." },
        { "Not recorded yet (Milestone 3)", "Пока не записывается (этап 3)" }, { "Game version", "Версия игры" }, { "File", "Файл" },
        { "Copy all", "Копировать всё" }, { "Copy errors", "Копировать ошибки" }, { "Copied", "Скопировано" },
        { "Click to copy this line", "Нажмите, чтобы скопировать строку" },
        { "Unload Replay", "Выгрузить запись" },
        { "UI sounds", "Звуки интерфейса" }, { "Play sounds for overlay actions", "Звуки действий оверлея" },
        { "Replay world", "Мир при воспроизведении" }, { "Restore doors, fog walls and bosses as recorded", "Восстанавливать двери, туманные стены и боссов как в записи" },
        { "Changes event flags (save state) while a replay plays; your own flags are put back when it ends. Time of day is always replayed.",
          "Меняет флаги событий (состояние сохранения) во время воспроизведения; ваши флаги возвращаются после. Время суток воспроизводится всегда." },
        { "Replay puppets (experimental)", "Заменяющие персонажи (эксперимент)" },
        { "When the recording says an enemy is alive but it is dead or gone in your game, ask the game to create a stand-in for the replay (invincible, no rewards, removed afterwards).",
          "Если в записи враг жив, а в вашей игре мертв или исчез, игра создаст замену для повтора (неуязвима, без наград, удаляется после)." },
        { "Full update rate for distant characters", "Полная частота обновления дальних персонажей" },
        { "The game updates far or off-screen characters less often (they lag and stutter). While recording or replaying, force every character to update each frame. Untick to use the game's own savings.",
          "Игра реже обновляет дальних персонажей и тех, кого не видно (лаги и рывки). При записи и повторе обновлять всех каждый кадр. Снимите флажок, чтобы вернуть экономию игры." },
        { "Freeze other characters during replay", "Останавливать остальных персонажей при повторе" },
        { "While a replay plays, every character that is not part of it stops moving and attacking (no one gets in the way or hits your replayed body). Their behaviour comes back when the replay ends.",
          "Пока идёт повтор, все персонажи, не входящие в запись, не двигаются и не атакуют (никто не мешает и не бьёт воспроизводимое тело). Поведение возвращается после повтора." },
        { "Replay equipment appearance (experimental)", "Показывать экипировку из записи (эксперимент)" },
        { "Off by default. Writes the recorded gear into your character while a replay plays. It never edits your inventory on purpose, but it has been reported to coincide with armor going missing, so only use it with a backup save.",
          "Выключено по умолчанию. Записывает снаряжение из записи в персонажа во время повтора. Инвентарь намеренно не меняется, но сообщалось о пропаже брони, поэтому используйте только с резервной копией сохранения." },
        { "Summon Torrent when the recording has him", "Призывать Торрента, если он есть в записи" },
        { "Uses the game's own Spectral Steed Whistle effect once per replay when the recording contains Torrent but he is not out. He stays out until you dismiss him.",
          "Один раз за повтор применяет эффект Свистка призрачного коня игры, если в записи есть Торрент, а его нет рядом. Он остаётся, пока вы его не отпустите." },
        { "No fade-out near the camera", "Не скрывать объекты вблизи камеры" },
        { "While a replay plays or a recording runs, grass, trees, rocks and models do not fade away when the camera comes close. Only the in-memory parameter tables change, and they are restored exactly.",
          "Пока идёт повтор или запись, трава, деревья, скалы и модели не исчезают при приближении камеры. Таблицы параметров изменяются только в памяти и возвращаются." },
        { "Replay particles and hit effects (experimental)", "Воспроизводить частицы и эффекты ударов (экспериментально)" },
        { "Effects the game creates at a position (hit sparks, blood, impacts) are saved into the replay while recording and created again in sync with the timeline when played forward. Sounds and effects that follow a character are not covered yet.",
          "Эффекты, которые игра создаёт в точке (искры, кровь, удары), сохраняются в повтор при записи и снова создаются синхронно с таймлайном при воспроизведении вперёд. Звуки и эффекты, привязанные к персонажу, пока не поддерживаются." },
        { "Weather", "Погода" }, { "Weather editor", "Редактор погоды" },
    };
    static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == (size_t)Str::Count, "kStrings must match Str");

    inline const char* Tr(Lang lang, Str s)
    {
        const StrPair& p = kStrings[(size_t)s];
        return lang == Lang::Russian ? p.ru : p.en;
    }
}
