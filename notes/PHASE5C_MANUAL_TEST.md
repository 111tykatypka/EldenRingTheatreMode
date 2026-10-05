# Phase5C — один диагностический тест в Elden Ring

**Это diagnostic checkpoint. WALK/RUN/SPRINT replay driver пока не подтверждён и не реализован.** Новые поля trace ещё требуют вашей проверки в игре. Подтверждённые ранее transform replay/rotation/smoothness сохранены.

## Запуск

1. Заверши запись, если она активна. Закрой Elden Ring, старый EldenRingTheaterMode.exe и старый YAFSML. Новая DLL требует перезапуска игры; не заменяй её в уже запущенном процессе.
2. Собирать самостоятельно не требуется. Готовая пара Release x64:
   - `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5C\EldenRingTheaterMode.exe`
   - `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5C\TheaterMode.dll`
   Для пересборки из корня repo: `powershell -ExecutionPolicy Bypass -File scripts\Build-Phase5C.ps1`.
3. Запусти **Phase5C EXE первым**, затем `START ELDEN RING`. Существующий launcher запускает YAFSML и создаёт appdata config с DLL **рядом с этим EXE**. Если нужно, выбери YAFSML кнопкой `YAFSML...`. При ручном запуске loader укажи путь именно Phase5C DLL в своём профиле. Копировать DLL в Game не нужно. Оригинальные game files/YAFSML config/reference material не меняются.
4. Используй установленный offline single-player workflow. Загрузи обычный offline save, выйди в знакомое ровное безопасное место без врагов/обрывов. Подойдёт площадка возле знакомой благодати. Для trace координаты старого replay не нужны. Оставь одну экипировку на всю последовательность, не садись на Torrent, не включай lock-on.
5. Дождись CONNECTED / PLAYER FOUND / control ready. Проверь, что обычное управление работает. **Не запускай replay или transform probe одновременно с trace.**

## Одна последовательность — около 25 секунд

6. В основном окне нажми `LOCOMOTION TRACE / F9 markers`. В панели нажми `START STATE TRACE`. Дождись `TRACE ACTIVE`, marker `IDLE`. Метка — ваше обозначение фазы, не автоопределение действия.
7. Верни фокус игре. Стой **3 секунды**.
8. Нажми **F9** один раз (короткий звуковой сигнал, marker WALK). Иди обычной медленной ходьбой **3 секунды**. На controller используй частичный наклон стика; на keyboard используй свой назначенный игровой Walk control. Не используй внешнюю эмуляцию клавиш.
9. **F9 → RUN**. Беги обычным бегом без sprint **3 секунды**.
10. **F9 → SPRINT**. Удерживай свой игровой sprint control и двигайся **3 секунды**.
11. **F9 → ROLL**. Выполни один безопасный перекат, подожди **3 секунды**.
12. **F9 → JUMP**. Один безопасный прыжок на ровной площадке, подожди **3 секунды**. Можно пропустить само действие, если место неудобно; сообщи, что фаза пропущена.
13. **F9 → IDLE**. Остановись, стой **3 секунды**.
14. Вернись к панели, нажми `STOP TRACE`, дождись `TRACE OFF` (обычно <0.3 sec), затем `OPEN TRACE LOG`. F9 работает глобально, пока панель открыта. Если регистрация F9 занята, панель сообщит это: выбирай фазу в dropdown и нажимай `MARK PHASE`, возвращаясь в игру; сообщи о fallback.
15. Проверь, что персонаж всё это время управляется нормально. Trace **не записывает transform или animation обратно в игру**. Нажатие F6 экстренно останавливает replay/probe и trace. Закрытие панели также выключает trace.

## Что должно быть видно

Обычная игровая ходьба/бег/sprint с естественными анимациями. От TheaterMode пока не ожидается самостоятельный WALK: тест собирает различия movement flags, action queues, root motion, HKS multipliers, ground/fall state, event request/observed TAE и native recorder counters. HKS graph inputs вроде MoveSpeedLevel пока явно UNAVAILABLE. Derived speed — измеренная разность позиции, **не** реальная graph variable.

Лог содержит TRACE_BEGIN, timestamped STATE с marker/previous, BASELINE при смене фазы, затем только изменившиеся значения. Полный baseline повторяется для сравнения фаз. Callback copies approximately game rate; worker compares latest copied snapshot at maximum 10 Hz. Короткие события <100 ms могут быть coalesced: trace не является точным event recorder. Порог/шума фильтрация описаны в PHASE5C_STATUS.md.

## Что прислать

- `%TEMP%\TheaterModeLocomotionTrace.log` (обычно `C:\Users\user\AppData\Local\Temp\TheaterModeLocomotionTrace.log`). Сессии добавляются с epoch и UTC time, старый файл не затирается.
- `%TEMP%\TheaterModeGame.log`.
- `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`.
- При проблеме запуска: `%LOCALAPPDATA%\EldenRingTheaterMode\launch\log\YAFSML.log` и сгенерированный launcher config.
- Укажи используемый control method, оружие/stance, прошли ли все фазы, было ли зависание/crash. Достаточно одного такого теста.

Новая .erplay для этого этапа не нужна. После trace следующим изменением будет bounded native WALK experiment через подтверждённый producer. Не проверяй RUN/SPRINT prototype: таких кнопок в этой сборке нет. Интеграция normalized gait в recording/playback ждёт видимого WALK.
