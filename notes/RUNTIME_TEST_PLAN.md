# Modern — единый сеанс проверки

Статус: IMPLEMENTED — RUNTIME VALIDATION REQUIRED. NPC playback пока выключен.

## Что запускать

Готовая папка:
`C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Modern`

- `EldenRingTheaterMode.exe`
- `TheaterMode.dll`
- `EldenRingCompatibilityProbe.exe`
- `BUILD_MANIFEST.txt`

Сборка из исходников: `powershell -ExecutionPolicy Bypass -File scripts\Build-Modern.ps1`.
Не используйте старые Build-Phase5/Phase5C из новой ветки для сохранённого эталона.

1. Остановите запись/F6. Закройте старый Theater Mode и Elden Ring. Перезапуск
   игры нужен для замены уже загруженной DLL. Steam/YAFSML переустанавливать не нужно.
2. Откройте **Modern\EldenRingTheaterMode.exe**. В Launcher при необходимости
   выберите существующий YAFSML.exe. Нажмите **Start Elden Ring**.
3. Launcher использует `TheaterMode.dll` рядом с этим EXE и генерирует свой конфиг
   в `%LOCALAPPDATA%\EldenRingTheaterMode\launch\YAFSML.ini`.
   При таком запуске вручную переносить DLL не нужно. Если запускаете собственным
   YAFSML-конфигом, замените только путь мода на **Modern\TheaterMode.dll**.
   Не копируйте DLL в папку игры/Phase5 и не загружайте две версии одновременно.
4. Используйте свой обычный offline/modded workflow. Загрузите знакомое безопасное
   место с несколькими обычными NPC/врагами, без босса. Дождитесь PLAYER FOUND.

## Одна запись и проверка редактора

5. До replay проверьте нормальное управление. Recorder должен показывать nearby
   Characters и фактическую частоту. Если count=0 при видимых близких NPC — сохраните
   лог; не делайте вывод, что NPC отсутствуют в мире.
6. Нажмите F5 / Start. Запишите 20–30 секунд: постойте, пройдите короткий маршрут,
   повернитесь, вернитесь к началу. Пусть рядом двигаются несколько обычных врагов.
   Не меняйте карту. F7/F8 позволяют проверить паузу записи. F6 завершает файл.
7. Откройте запись в Replay Library. Проверьте Player + Characters, раскрытие actor
   groups, raw IDs, траектории, hide/show, bookmarks, seek и скорости.
   Проверьте начало/50%/90%/99%/конец. NPC пока отображаются только как данные.

## Контролируемый player replay

8. Вернитесь в ту же область к началу записи. В Settings выберите First 5 seconds,
   experimental animation requests оставьте OFF. Нажмите Play / Resume in game.
   При предупреждении о расстоянии подтвердите только если уверены в области.
9. Проверьте position/rotation, Pause/Resume. Попробуйте движение и лёгкую атаку:
   новый input lock экспериментален. F6 должен вернуть обычное управление.
10. Отметьте отдельно: висит ли модель над землёй; идут ли ноги; пропадают ли атаки;
    работают ли управление/пауза/Stop. Затем, если устойчиво, выберите Full replay.
11. Проверьте resize/maximize, перенос на другой DPI, повторное открытие и Unload.
    Это проверка нового UI; успешный headless test не заменяет её.

## Что прислать

- `%TEMP%\TheaterModeGame.log` — INPUT_LOCK, REPLAY_APPLY, GROUNDING, initialization.
- `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`.
- Новый `.erplay` из `%LOCALAPPDATA%\EldenRingTheaterMode\replays`.
- `Modern\BUILD_MANIFEST.txt`.
- При сбое запуска: `%LOCALAPPDATA%\EldenRingTheaterMode\launch\log`.

Короткий ответ: UI / PLAYER FOUND / NPC count / сохранение / NPC tracks / движение
replay / rotation / Pause+Resume / F6+normal control / input blocked / floating /
crash. Лучше один полный результат, чем отдельная проверка после каждой кнопки.

Следующее решение зависит от этого сеанса: подтверждение NPC read path, уточнение
порядка input callback и решение о proxy synchronization. Не переходить к записи
transform в NPC до этой проверки.
