param(
 [string]$OutputDirectory='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase8_PlayerCapture_Fidelity1',
 [string]$CMakeBin='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$outputFull=[IO.Path]::GetFullPath($OutputDirectory)
if($outputFull -match '\\(Phase5[^\\]*|TesterBuild|Nightly_ResearchIntegration|Phase7_Runtime_UI|Phase7_Runtime_UI_Hotfix1|Phase7_Runtime_UI_Hotfix2|Phase7_Runtime_UI_Hotfix3|Phase7_Runtime_UI_Hotfix4|Phase7_Runtime_UI_Hotfix5|Phase7_Runtime_UI_Hotfix6)(\\|$)'){throw 'Preserved build directory cannot be an output'}
$preserved=@{}
$outputs='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode'
foreach($folder in @('Phase5','TesterBuild','Nightly_ResearchIntegration','Phase7_Runtime_UI','Phase7_Runtime_UI_Hotfix1','Phase7_Runtime_UI_Hotfix2','Phase7_Runtime_UI_Hotfix3','Phase7_Runtime_UI_Hotfix4','Phase7_Runtime_UI_Hotfix5','Phase7_Runtime_UI_Hotfix6')){
 foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','BUILD_MANIFEST.txt')){
  $path=Join-Path (Join-Path $outputs $folder) $name
  if(Test-Path -LiteralPath $path){$preserved[$path]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
 }
}
$cmake=Join-Path $CMakeBin 'cmake.exe';$ctest=Join-Path $CMakeBin 'ctest.exe'
$cargo=Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
$python=Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$build=Join-Path $repo 'build'
& $cmake -S $repo -B $build -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON
if($LASTEXITCODE){throw 'CMake configuration failed'}
& $cmake --build $build --config Release --parallel 2
if($LASTEXITCODE){throw 'Host Release build failed'}
& $ctest --test-dir $build -C Release --output-on-failure --output-log (Join-Path $build 'phase7-cpp-tests.log')
if($LASTEXITCODE){throw 'C++ tests failed'}
# Explicit real-device test, separate from CPU CTest. Never interpreted as an Elden Ring test.
$smoke=Start-Process -FilePath (Join-Path $build 'Release\render-dx12-smoke.exe') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $build 'phase7-dx12-smoke.log') -RedirectStandardError (Join-Path $build 'phase7-dx12-smoke-errors.log')
if(-not $smoke.WaitForExit(20000)){$smoke.Kill();throw 'DX12 smoke test timed out'}
if($smoke.ExitCode){throw "DX12 smoke test failed: $($smoke.ExitCode)"}
Get-Content -LiteralPath (Join-Path $build 'phase7-dx12-smoke.log')
# Windows PowerShell treats redirected compiler stderr as error records.
$ErrorActionPreference='Continue'
& $cargo test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath (Join-Path $build 'phase7-rust-tests.log')
$cargoTestExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
if($cargoTestExit){throw 'Rust tests failed'}
& $cargo build --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if($LASTEXITCODE){throw 'DLL Release build failed'}
Push-Location $repo
try{& $python -m unittest discover -s tools/research_integration -p test_tools.py;if($LASTEXITCODE){throw 'Research tool tests failed'}}finally{Pop-Location}
Push-Location $repo
try{& $python -m unittest discover -s tools/fidelity_capture -p test_capture.py;if($LASTEXITCODE){throw 'Capture parser tests failed'}}finally{Pop-Location}
$ErrorActionPreference='Continue'
& $cargo test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc sdk_layout_inventory -- --nocapture 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath (Join-Path $build 'capture-sdk-layout.log')
$layoutExit=$LASTEXITCODE;$ErrorActionPreference='Stop';if($layoutExit){throw 'SDK layout audit failed'}
& $cmake --build (Join-Path $repo 'probe\build') --config Release --parallel 2
if($LASTEXITCODE){throw 'Compatibility probe build failed'}
New-Item -ItemType Directory -Path $outputFull -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'Release\EldenRingTheaterMode.exe') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'probe\build\Release\EldenRingCompatibilityProbe.exe') -Destination $outputFull -Force
foreach($name in @('PHASE7_IMPLEMENTATION_STATUS','PHASE7_RUNTIME_TEST_PLAN','PHASE7_KNOWN_ISSUES')){
 $dest=@{PHASE7_IMPLEMENTATION_STATUS='IMPLEMENTATION_STATUS.md';PHASE7_RUNTIME_TEST_PLAN='RUNTIME_TEST_PLAN.md';PHASE7_KNOWN_ISSUES='KNOWN_ISSUES.md'}[$name]
 Copy-Item -LiteralPath (Join-Path $repo "notes\$name.md") -Destination (Join-Path $outputFull $dest) -Force
}
Copy-Item -LiteralPath (Join-Path $repo 'notes\PHASE7_CRASH_DIAGNOSTIC.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'notes\PHASE7_RECORDING_HOTKEY_FIX.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'notes\PHASE7_REPLAY_LEASE_FIX.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'notes\PHASE7_SIMPLIFIED_CONTROLS.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'third_party\minhook_vendor\LICENSE.txt') -Destination (Join-Path $outputFull 'MinHook-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $repo 'ERPLAY_FORMAT.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'research\symbols_2_7_0_0.json') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'third_party\imgui\LICENSE.txt') -Destination (Join-Path $outputFull 'DearImGui-LICENSE.txt') -Force
# The dependency pin/lock is unchanged. Reuse license texts only; no reference binaries.
$tester=Join-Path $outputs 'TesterBuild'
foreach($name in @('licenses','THIRD_PARTY_NOTICES.txt')){
 $p=Join-Path $tester $name;if(Test-Path -LiteralPath $p){Copy-Item -LiteralPath $p -Destination $outputFull -Recurse -Force}
}
Copy-Item -LiteralPath (Join-Path $build 'phase7-cpp-tests.log'),(Join-Path $build 'phase7-rust-tests.log'),(Join-Path $build 'phase7-dx12-smoke.log') -Destination $outputFull -Force
$git=Join-Path $env:ProgramFiles 'Git\cmd\git.exe';$safe="safe.directory=$($repo.Replace('\','/'))"
$commit=& $git -c $safe -C $repo rev-parse HEAD
$branch=& $git -c $safe -C $repo branch --show-current
$dirty=& $git -c $safe -C $repo status --porcelain
Copy-Item -LiteralPath (Join-Path $repo 'notes\PHASE8_CAPTURE_FIDELITY.md'),(Join-Path $repo 'notes\PHASE8_CAPTURE_FIELDS.md'),(Join-Path $repo 'shared\capture_schema.json'),(Join-Path $repo 'tools\fidelity_capture\inspect_capture.py'),(Join-Path $build 'capture-sdk-layout.log') -Destination $outputFull -Force
$manifest=@('CAPTURE FIDELITY 1: EXPERIMENTAL READ-ONLY FIELD CAPTURE; runtime validation REQUIRED',
'DEVELOPER EXPERIMENTAL BUILD',"Branch: $branch","Source commit: $commit","Working tree dirty: $([bool]$dirty)","Built UTC: $([DateTime]::UtcNow.ToString('o'))",'AMD64 Release; EldenRing_1_17 / file+product 2.7.0.0','Target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134','Pinned SDK: 3c8c1d7633a99309fb004c9f894ea10b7967d0e0','ERPLAY02 reader retained; ERPLAY03 optional track IDs5..13 added, capture schema1; no ERPLAY04 introduced','Control v3/128 bytes; new capability 128; XZ flag 4; editor pipe v1/32-byte request with bounded actor pages','C++ CTest 14/14 PASS; Rust 28/28 PASS; Python research tests 3/3 + capture parser 4/4 PASS; see captured logs.','Previously user-verified: player/NPC capture, host trajectories; player replay PARTIAL.','Current Phase7 checkpoint: NO NEW LIVE GAME VERIFICATION. Grounding/NPC playback/animation remain FAILED or UNVERIFIED.','New diagnostic binding: typed CSLuaEventManImp READ-ONLY; runtime UNVERIFIED','Exact native +538 debug tail static verified; +530 callback. noMove/noAttack probes gated by live callback+owner check; noUpdate blocked','animationSpeed=0 selected-NPC 2s experiment only; identity/module reacquisition; UNVERIFIED','DX12 backend + host transport implemented; GPU/game runtime UNVERIFIED. Camera/warp/WALK/Dolly NOT IMPLEMENTED','One host ReplayPlayer clock; no second ERPLAY parser; no custom injector; YAFSML retained')
$manifest+='Hotfix1: original Phase7 startup crash REPORTED; root cause UNRESOLVED; Clean startup/manual Insert and WndProc publication race fix; in-game validation REQUIRED.'
$manifest+='User verified Hotfix1 loads world and displays Overlay; game terminates on RMB. Hotfix2 input fix: RUNTIME VALIDATION REQUIRED.'
$manifest+='User subsequently verified Hotfix2 UI/mouse. Hotfix3 host-only global recording hotkey dispatch fix: live F5 validation REQUIRED.'
$manifest+='Hotfix3 live F5/start/stop/file finalization verified in logs. Hotfix4 fixes callback-vs-IPC clock ordering; new DLL runtime validation REQUIRED; timeout limits unchanged.'
$manifest+='Hotfix4 moving replay user-confirmed on flat indoor floor: path repeated without falls/jerks; sliding model, animations not implemented. Hotfix5 removes Pause/Resume UI + F7/F8 and owns timeline wheel; new UI runtime validation REQUIRED.'
$manifest+='Hotfix6: Shift+wheel vertically scrolls timeline tracks; wheel without modifiers retains cursor-anchored zoom; automated bidirectional/bounds tests PASS, manual validation REQUIRED.'
$manifest+='Real-device DX12 smoke: PASS (120 Presents, Clean/Overlay/Editor, mouse buttons, ResizeBuffers, shutdown); separate from Elden Ring verification; see phase7-dx12-smoke.log.'
foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')){
 $p=Join-Path $outputFull $name;$manifest+="$name bytes=$((Get-Item -LiteralPath $p).Length) SHA256=$((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash)"
}
foreach($path in $preserved.Keys){
 if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $preserved[$path]){throw "Preserved build changed: $path"}
 $manifest+="Preserved: $path SHA256=$($preserved[$path])"
}
$manifest|Set-Content -LiteralPath (Join-Path $outputFull 'BUILD_MANIFEST.txt') -Encoding utf8
Write-Output "Capture-fidelity package staged at $outputFull"
