# Phase 4B — первые 5 секунд transform replay в Elden Ring

Статус нового playback: **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**.
Запуск кнопкой, YAFSML, загрузка DLL, IPC и Player FOUND уже **VERIFIED пользователем** на `3761a7e`. Новая запись position/orientation во время replay ещё не подтверждена визуально.

## Какие файлы использовать

- EXE: `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4B\EldenRingTheaterMode.exe`.
- DLL: `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4B\TheaterMode.dll`.
- Заголовок нового окна: **Elden Ring Theater Mode — Phase 4B transform replay**.
- Сборка при необходимости: из `C:\Users\user\Documents\ChatGPT\elden ring theater mode\EldenRingTheatreMode-phase4` выполни `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build-Phase4B.ps1`.

Для обычного запуска менять старую YAFSML.ini вручную не нужно: **START ELDEN RING** создаёт отдельный config с DLL рядом с этим EXE. Если запускаешь загрузчик вручную другим способом, его `theater_mode=` должен указывать на **Phase4B\TheaterMode.dll**, не на Phase4A или стабильную DLL. Не загружай две версии одновременно. Новый host требует control protocol v2 — старую DLL он не примет для replay.

## Точный порядок

1. Закрой Elden Ring, старый YAFSML и все старые hosts. Новую DLL нельзя подменить внутри работающей игры. Записывать/выгружать исходные game files не требуется.
2. Открой указанный **Phase4B EXE**. Нажми **START ELDEN RING**. Это тот же установленный offline/modded workflow через YAFSML.
3. Загрузи обычного персонажа в известном ровном открытом месте без врагов рядом. Не переходи между картами, не варпайся и не начинай бой во время первого теста. Дождись **CONNECTED**, **Player FOUND**, **Control IPC CONNECTED / Runtime READY**. С probe/replay INACTIVE походи и повернись: управление должно оставаться обычным.
4. Старый `replay_2026-10-05_022417.erplay` удалён пользователем; для теста делаем **новую настоящую запись**. Запомни точку старта, отпусти движение и нажми **START RECORDING** или F5. За первые 5 секунд пройди несколько шагов и поверни персонажа; продолжай спокойную ходьбу/повороты в пределах 3–5 метров в общей сложности 10–15 секунд. Пока не нужны атаки, прыжки или перекаты: их анимации не записываются.
5. Нажми **STOP** в recording controls либо F6 и дождись завершения сохранения/валидации. Новый `.erplay` появится в списке Recent Recordings и в `%LOCALAPPDATA%\EldenRingTheaterMode\replays` (кнопка **OPEN REPLAY FOLDER**). Вернись к точке начала **пешком**, выбери новый файл и нажми **OPEN**. Ожидай ненулевые samples, 10–15 секунд и **Transform: YES | Animation: NO**. Оставайся в том же месте и coordinate origin: ERPLAY v2 хранит Havok position, но не map/origin, поэтому близость координат сама по себе не доказывает совпадение карты.
6. Оставь **TEST: first 5 seconds** — это режим короткого теста, не ограничение recording/file duration. Выбери **1.0x**. Отпусти WASD/стик и нажми **PLAY IN GAME**. Host проверит текущее положение и sample 0; при расстоянии >15 единиц откажет. Никакого большого teleport/override нет: вернись к стартовой точке. DLL повторно проверит расстояние на игровом callback.
7. Ожидай **STARTING → PLAYING**. После подтверждения первой записи DLL clock начнётся с sample 0. Персонаж должен физически идти по recorded trajectory и менять направление. Анимации записи отсутствуют: модель может скользить/использовать текущую live-анимацию. Это не animation replay. Камера остаётся обычной игровой.
8. Во время движения нажми replay **PAUSE**. Timeline должен остановиться; DLL должна удерживать текущие координаты/поворот. Затем нажми **PLAY IN GAME** — продолжение с той же точки. Проверяй отсутствие дрожания, самопроизвольного разгона, отскока модели или teleport loop. Если что-то из этого появилось — сразу F6.
9. Примерно на 2–3 секунде replay нажми **F6** или replay **STOP**. Дальнейшие transform writes должны прекратиться; походи/повернись вручную и проверь нормальное управление. Не ожидай teleport назад к исходной live-позиции: STOP не восстанавливает её. Команда действует на следующем callback после обработки IPC; уже выполняющуюся единичную запись отменить нельзя. При потере свежих запросов записи автоматически прекращаются через 250 ms, даже если heartbeat продолжает приходить.
10. Повтори PLAY на **5-second** режиме и дай ему закончиться. При **FINISHED** финальный transform применяется один раз, затем writes OFF; управление снова должно работать. Проверь **0.5x** (5 replay seconds ≈10 секунд без пауз) и **2.0x** (≈2.5 секунды). При **RESTART** приложение сначала ждёт STOP ACK, затем снова применяет sample 0 с проверкой расстояния.

Для завершения Stage 1 сообщи: двигалась ли модель по маршруту, поворачивалась ли, удерживалась ли при Pause, продолжался ли replay с той же точки, вернулось ли управление после Stop/Finish, были ли crash/рывки/отскок/разгон. Только после успешных 5 секунд переходи к **TEST: first 10 seconds**, а после них к **FULL replay**. Программа не включает длинный тест автоматически.

## Если ошибка или модель не движется

Пришли последние запуски:

- `%TEMP%\TheaterModeGame.log` — строки `REPLAY_START`, `REPLAY_APPLY`, `REPLAY_STATE`, `REPLAY_STOP`, `REPLAY_FINISHED`, `REPLAY_ERROR`, `REPLAY_PLAYER_LOST`, а также PID/profile/task initialization.
- `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log` — `LAUNCHER`, `REPLAY_START_GUARD`, `REPLAY_START`, `HOST_REPLAY_STATE`, `REPLAY_STOP`, `ANIMATION_TRACK_MISSING`.
- `%LOCALAPPDATA%\EldenRingTheaterMode\launch\log\YAFSML.log` — кнопка **OPEN LAUNCH LOGS**.
- Имя выбранного `.erplay`, место записи/воспроизведения и желательно короткое видео визуального результата.

DLL replay detail: 2=link/heartbeat, 3=player lost, 4=invalid transform, 5=runtime/probe busy, 6=panic/mailbox, 8=malformed protocol, 9=session/order, 10=start displacement, 11=timestamp regression, 12=target stale, 13=target jump >5 units between updates. При ошибке replay OFF; автоматического возобновления после загрузки/warp нет.

Если память показывает requested transform, но модель/Havok proxy его не повторяет, следующий эксперимент будет отдельно включать документированные proxy sync flags. В этой сборке они **не изменяются**. Не продолжать к animation capture до анализа live-результата.

Scrub/step/bookmark seek во время in-game replay сначала отключают writes и переходят к offline inspection. Следующий PLAY начинает с sample 0. Безопасное in-game scrubbing через карты пока не реализовано.
