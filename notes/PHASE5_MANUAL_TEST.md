# Phase 5 — единый ручной тест

**IMPLEMENTED — RUNTIME VALIDATION REQUIRED.** Новый capture/action API, input lock и визуальная плавность этой сборки ещё не проверены в Elden Ring. Старый transform replay пользователь уже видел, но это не подтверждает новые функции.

## Подготовка

1. Закрой Elden Ring (`eldenring.exe`), все старые `EldenRingTheaterMode.exe` и старый процесс YAFSML. Не заменяй DLL внутри работающей игры: потребуется полный перезапуск игры для новой DLL. Сохрани незавершённую запись перед закрытием.
2. Готовые Release x64 файлы:
   - EXE: `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5\EldenRingTheaterMode.exe`
   - DLL: `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5\TheaterMode.dll`
   - Пересборка, если понадобится: из корня repository выполнить `powershell -ExecutionPolicy Bypass -File scripts\Build-Phase5.ps1`. Готовую пару заново собирать не нужно.
3. Запусти **Phase5 EXE первым**. Старые Phase4 EXE/DLL не используй в этой сессии. Новый host ожидает control v3; sample v2 содержит action tail.
4. Нажми `START ELDEN RING`. Используется тот же YAFSML, новый injector не добавлен. Если путь не сохранён, кнопкой `YAFSML...` выбери `C:\Users\user\Desktop\YAFSML-v0.10.4\YAFSML.exe`. Launcher создаёт отдельный конфиг в `%LOCALAPPDATA%\EldenRingTheaterMode\launch`, указывающий **DLL рядом с Phase5 EXE**. Оригинальный YAFSML.ini и game directory не менять. В ручном loader workflow замени только путь своего профиля на указанную Phase5 DLL; копировать её в Game не требуется.
5. Используй существующий offline/modded single-player workflow. Загрузи свой обычный офлайн save и выйди в знакомую ровную безопасную область без врагов, обрывов и переходов карты. Конкретная карта не зашита: новая запись и тест должны происходить **в этом же месте**, с тем же персонажем и экипировкой.
6. Дождись `Connection: CONNECTED`, `Player: FOUND`, готового control/probe status. Если нет — не начинай replay; сохрани логи из последнего раздела.
7. До replay проверь нормальную ходьбу, поворот и действие. Не нажимай `PROBE +0.5 X`: этот тест здесь не нужен.

## Новая настоящая запись

8. Запомни исходную точку и направление, лучше возле узнаваемого ориентира. Нажми `START RECORDING` или F5. Должны расти Duration/Samples, быть `RECORDING`.
9. Запиши примерно 18–25 секунд (больше 800 samples при обычных ~60 Hz): 2 с idle → 2 с walk → 2 с run → 2 с sprint → остановка → один roll → walk с поворотом → один безопасный jump, дождаться land → idle 2 с. Используй обычные controls своего персонажа; не менять экипировку/карту. Запись raw IDs происходит автоматически, отдельной кнопки для action track нет.
10. Нажми F6 или recorder `STOP`. Дождись сохранения и появления `.erplay` в списке. Ожидаются v3, transform samples и action events. Если animation ID/time недоступны, это ограничение, а не повод считать запись анимаций успешной.
11. Вернись обычным управлением к исходной точке и направлению. **Не менять карту.** Start displacement guard = 15 units. Если guard отказал, вернись ближе; автоматического большого teleport override нет.

## Transform / input / полная длительность

12. Выбери новую запись и `OPEN`. Должны быть `Transform: YES`, `Actions: YES` и raw ID. `Time observations: YES` возможен при валидных полях, но это **не** подтверждает exact animation sync. Названия Walk/Run/Sprint могут оставаться `Unknown`: универсальная таблица ID не придумана. Старый v2 показывает Actions/Time observations NO.
13. Выбери `FULL replay`, speed `1.0x`. Сначала оставь `Experimental recorded animation requests` **выключенным**. Нажми `PLAY IN GAME`. Наблюдай начало, прохождение 5 с, 10 с и **полный конец** новой записи, включая sample >800. После FINISHED обычное управление должно вернуться.
14. Проверь плавность пути и quaternion-поворотов. Сравни с прошлой сборкой. Резкие скачки/скольжение/дрожание — сообщить, не считать smoothness verified. На transform-only проходе отсутствие воспроизведения анимаций ожидаемо.
15. Снова вернись к старту и PLAY. Пока PLAYING, попробуй обычные movement/action controls: персонаж не должен реагировать на live input. Физические устройства не отключены. Если input всё ещё влияет — F6, считать input suppression не подтверждённым.
16. Нажми replay `PAUSE`. Position/rotation должны держаться. Попробуй live movement: он должен оставаться заблокированным. В экспериментальном animation mode анимация пока может продолжаться: local animation freeze не реализован.
17. Нажми `PLAY IN GAME` для Resume. Timeline должен продолжиться с paused time без учёта времени паузы. Проверь затем `STOP`/F6 — replay writes выключены и нормальные controls вернулись. Повторить на 0.5x и 2x, возвращаясь к старту; анимационные playback-rate writes ещё не реализованы.

## Экспериментальная записанная анимация

18. Вернись к старту. Включи `Experimental recorded animation requests (no time sync)` и PLAY на 1x. Это запрос **наблюдённого TAE ID** через public `event.request_animation_id`, только при переходе ID. Работу этого engine request нужно проверить визуально; успешная отправка IPC не доказывает, что движок принял ID.
19. Сравни idle/walk/run/sprint и turns с записью. Проверь один roll и jump/fall/land. Помечай каждое действие как работает/не работает/не проверено. Для roll/jump namespace, фазовая синхронизация и blends ещё неизвестны. Если остаётся idle-sliding или прерывается run — F6 и прислать trace; это не считается успешным animation replay.
20. Проверь Pause/Resume/Stop и FINISHED с включённым режимом. Stop должен восстановить gameplay input; не обещается мгновенный возврат уже принятой движком анимации или exact seek в её фазу. Режим выключается перед следующей обычной игрой через checkbox, когда replay inactive.

## UI / lifecycle

21. На остановленном replay scrub на 0%, 50%, 90%, 99%, 100%. Несколько раз быстро подвигай slider возле 90–100%. Preview должен быть компактным и стабильным, без усиления redraw на конце. Проверь speed dropdown: 0.1/0.25/0.5/1/2/4. Отдельно resize/DPI, если удобно. `UI_PREVIEW_PERF` позволяет сверить реальную частоту repaint; визуальный результат подтверждает пользователь.
22. Seek/step/bookmark в этой сборке — **offline state inspection**: scrubbing во время native replay сперва выполняет STOP. Нажми ADD BOOKMARK в двух разных местах, выбери строку, дождись нескольких UI обновлений — selection не должна сбрасываться. Двойной click переносит timestamp; DELETE удаляет только выбранный bookmark. Закрой/открой запись — bookmarks сохраняются в sidecar.
23. Нажми `UNLOAD REPLAY`: timeline/state/bookmarks/preview очищаются, replay writes OFF, сам файл остаётся. Обычные controls работают.
24. Открой новую запись, затем старую `replay_2026-10-05_073848.erplay` (если сохранилась) и обратно. Switching должен остановить старый native replay. Старую запись в игре не запускать в чужой области: для совместимости достаточно открытия/просмотра. Native PLAY после seek/bookmark начинает с sample 0; arbitrary in-game seek пока не заявлен.
25. Итог сообщи одной строкой по каждому: FULL, smoothness, rotation, input PLAY/PAUSE, input restore STOP/FINISH, idle/walk/run/sprint, roll, jump/land, bookmarks/unload/switch, crash YES/NO. При отказе F6; если host недоступен, DLL freshness lease должен отключить writes на следующем callback после 250 ms без валидной цели (control lease также защищает от disconnect).

## Что прислать при проблеме

- `%TEMP%\TheaterModeGame.log` — профиль, PLAYER_ANIMATION_OBSERVED, REPLAY_ANIMATION_REQUEST, REPLAY_PERF, transform error/input transitions.
- `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log` — CAPTURE_PERF, HOST_REPLAY_STATE, REPLAY_PERF, UI_PREVIEW_PERF, файл/validation/errors.
- `%LOCALAPPDATA%\EldenRingTheaterMode\launch\log\YAFSML.log` и сгенерированный launcher config, если connection не появился.
- Новый `.erplay` из `%LOCALAPPDATA%\EldenRingTheaterMode\replays`, имя файла, выбранная speed, состояние checkbox и примерное время дефекта. По возможности короткий визуальный clip только как доказательство поведения, не замена replay data.

**Нельзя считать Phase5 VERIFIED до выполнения этого теста в Elden Ring.** Следующий шаг после него — сверить настоящие ID/семантику и выяснить, принимает ли движок animation override и требуется ли дальнейшая physics/root-motion synchronization.
