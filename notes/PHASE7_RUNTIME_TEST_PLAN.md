# Phase7 — точный порядок проверки

**Текущий статус:** пользователь подтвердил работу Hotfix2 интерфейса/мыши.
Далее тест B/C: короткая новая запись, guard и player-only XZ replay на ровной
площадке. Известные FULL XYZ grounding и NPC проблемы ещё не подтверждены исправленными.

**Hotfix2:** сначала проверяйте правую кнопку в Overlay/Editor по
PHASE7_CRASH_DIAGNOSTIC.md; запуск через Phase7_Runtime_UI_Hotfix2. Replay пока не нужен.

**После вылета 2026-10-06 используйте сначала PHASE7_CRASH_DIAGNOSTIC.md.**
Hotfix1 стартует в Clean: Insert включает Overlay, следующий — Editor, следующий — Clean.
До завершения проверки без replay остальные тесты ниже отложены.

Это экспериментальный checkpoint. Grounding/NPC/WALK/UI в Elden Ring пока не подтверждены.
Не начинайте с полного боя или полной длинной записи.

## Сборка и запуск

1. Готовые Release x64 файлы находятся в
   `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase7_Runtime_UI`.
   Повторная сборка: из актуального репозитория запустить
   `powershell -ExecutionPolicy Bypass -File scripts\Build-Phase7.ps1`.
2. Закройте Elden Ring, старый Theater host и завершившийся YAFSML launcher.
   Нельзя заменить уже загруженную DLL. Старые Phase5/Tester/Nightly не перезаписывайте.
3. Запустите **Phase7_Runtime_UI\EldenRingTheaterMode.exe первым**.
   Проверьте Launcher: путь к вашему существующему YAFSML и целевому eldenring.exe.
4. Нажмите существующую кнопку запуска offline игры в Launcher. Host создаёт
   конфигурацию для **TheaterMode.dll рядом с этим EXE**. Если запускаете YAFSML вручную,
   замените только путь DLL в используемой конфигурации на Phase7_Runtime_UI\TheaterMode.dll.
   Не копируйте DLL в игру и не редактируйте оригинальные game files.
5. Загрузите обычный save в знакомом безопасном месте с ровным полом.
   Дождитесь CONNECTED / PLAYER FOUND. Сначала проверьте обычную ходьбу без replay.

## Тест A — только интерфейс, никаких writes

1. Insert переключает Overlay → Editor → Clean → Overlay.
2. Editor должен показывать transport и Scene. До открытия записи время/акторы пустые.
3. Проверьте курсор, переключение режимов, Alt+Tab и один обычный resize/borderless transition.
4. Сверните desktop host: in-game команды всё равно должны достигать host.
5. Если интерфейса нет: отправьте `%TEMP%\TheaterModeRender.log` и Game.log.
   Ожидаемые стадии: DX12_HOOKS_INSTALLED → DX12_QUEUE_BOUND → DX12_IMGUI_INITIALIZED.
   Не пытайтесь hot-reload DLL. При поздней загрузке нужен полный restart игры.

## Тест B — новая короткая запись и guard

1. На ровной площадке запомните исходную точку. F5: запись; idle → 5 секунд ходьбы → idle;
   F6: stop/finalize. Это существующий реальный recorder, не synthetic fixture.
2. В desktop Library откройте только что созданную .erplay.
3. Оставаясь вдали от исходной точки, нажмите Play. Должен появиться START BLOCKED,
   и персонаж **не должен телепортироваться**. Это ожидаемый успешный guard-тест.
4. Вернитесь к исходной точке: <=1 по X/Z и <=0,25 по Y. Map UNKNOWN не означает
   совместимость сцены; выбирайте ту же площадку и те же загруженные объекты.

## Тест C — сначала XZ diagnostic

1. Settings: выключить actor replay, selected NPC-only и raw animation.
   Включить `Developer XZ-only diagnostic (native Y; not final replay)`.
   Выбрать ограниченный тест First 5 seconds вместо Full.
2. Diagnostics: `Runtime differential trace (10s)`; затем Play.
   Не двигайте персонажа вручную во время воспроизведения.
3. Наблюдать: следует ли X/Z траектории, сохраняется ли контакт с землёй,
   вращается ли персонаж. При падении/рывке немедленно F6.
4. Pause → ждать → Resume: время не должно возвращаться на ноль.
   Stop/F6: нормальная ходьба должна вернуться.
5. Сохраните JSONL и Game.log отдельно как XZ-run. В JSONL transform_mode=4;
   recorded target Y может отличаться, фактический Y не присваивается из replay.
6. FULL XYZ сравнение — только короткие 2 секунды в той же точке, если XZ не падает,
   и вы готовы проверить известный grounding дефект. Выключить XZ, новая trace,
   новый Play, F6 при первом признаке падения. Не считать исправленным заранее.

## Тест D — один NPC, отдельные двухсекундные probes

1. Остановить replay. Выбрать один обычный живой NPC в desktop Characters
   из записи, сделанной в текущей загруженной сцене. Не выбирать 60 акторов.
2. Diagnostics → Selected NPC experiment → noMove only → Run (2s).
   Если DLL пишет LIVE_LAYOUT_CHECK reject, **это безопасный отказ**; ничего не обходите.
3. Наблюдать движение AI, анимацию, grounding и восстановление через 2 сек.
4. Отдельно повторить noAttack, затем noMove+noAttack. Между тестами убедиться,
   что обычный AI вернулся. F6 — emergency stop.
5. noUpdate заблокирован. animationSpeed=0 — отдельный старый diagnostic, не AI ownership.
6. Только после успешных probes можно проверить выбранный NPC-only replay на 2 сек;
   текущая replay ветка не включает эти ownership flags постоянно. Коррекции AI всё ещё возможны.
7. Отправьте видимые результаты и trace: applied callback сам по себе не доказывает ownership.

## Transport / timeline

На короткой записи проверить шесть скоростей, Pause/Resume/Stop/Restart,
переключение actor pages/selection. Seek и previous/next tick намеренно останавливают
native writes и двигают offline playhead; они ещё не перемещают world к этому времени.
Free Camera/Dolly отсутствуют: не искать функциональную кнопку этих режимов.

## Какие файлы отправить

- `%TEMP%\TheaterModeGame.log`
- `%TEMP%\TheaterModeRender.log`
- `%TEMP%\TheaterModeRuntimeTrace.jsonl`
- `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`
- BUILD_MANIFEST.txt из Phase7_Runtime_UI
- использованную короткую .erplay и описание: что двигалось, была ли анимация,
  контакт с землёй, падение, возврат управления, crash/resize результат.

Diagnostics → Collect/Show logs копирует доступные trace/render/runtime logs.
Укажите отдельно XZ/FULL XYZ и выбранный NPC mode. Новая сборка считается runtime
проверенной только после ваших реальных наблюдений, не по unit tests.
