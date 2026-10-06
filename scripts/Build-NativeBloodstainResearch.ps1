param(
 [string]$OutputDirectory='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\NativeBloodstainReplayResearch',
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
& $ctest --test-dir $build -C Release --output-on-failure --output-log (Join-Path $build 'native-cpp-tests.log')
if($LASTEXITCODE){throw 'C++ tests failed'}
# Explicit real-device test, separate from CPU CTest. Never interpreted as an Elden Ring test.
$smoke=Start-Process -FilePath (Join-Path $build 'Release\render-dx12-smoke.exe') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $build 'native-dx12-smoke.log') -RedirectStandardError (Join-Path $build 'native-dx12-smoke-errors.log')
if(-not $smoke.WaitForExit(20000)){$smoke.Kill();throw 'DX12 smoke test timed out'}
if($smoke.ExitCode){throw "DX12 smoke test failed: $($smoke.ExitCode)"}
Get-Content -LiteralPath (Join-Path $build 'native-dx12-smoke.log')
# Windows PowerShell treats redirected compiler stderr as error records.
$ErrorActionPreference='Continue'
& $cargo test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --features native-bloodstain-readonly --target x86_64-pc-windows-msvc 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath (Join-Path $build 'native-rust-tests.log')
$cargoTestExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
if($cargoTestExit){throw 'Rust tests failed'}
& $cargo build --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --features native-bloodstain-readonly --target x86_64-pc-windows-msvc
if($LASTEXITCODE){throw 'DLL Release build failed'}
Push-Location $repo
try{& $python -m unittest discover -s tools/research_integration -p test_tools.py;if($LASTEXITCODE){throw 'Research tool tests failed'}}finally{Pop-Location}
Push-Location $repo
try{& $python -m unittest discover -s tools/fidelity_capture -p test_capture.py;if($LASTEXITCODE){throw 'Capture parser tests failed'}}finally{Pop-Location}
$ErrorActionPreference='Continue'
& $cargo test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --features native-bloodstain-readonly --target x86_64-pc-windows-msvc sdk_layout_inventory -- --nocapture 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath (Join-Path $build 'capture-sdk-layout.log')
$layoutExit=$LASTEXITCODE;$ErrorActionPreference='Stop';if($layoutExit){throw 'SDK layout audit failed'}
& $cmake --build (Join-Path $repo 'probe\build') --config Release --parallel 2
if($LASTEXITCODE){throw 'Compatibility probe build failed'}
New-Item -ItemType Directory -Path $outputFull -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'Release\EldenRingTheaterMode.exe') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'probe\build\Release\EldenRingCompatibilityProbe.exe') -Destination $outputFull -Force

Copy-Item -LiteralPath (Join-Path $repo 'research\REPLAY_RECORDER_LAYOUT.md'),(Join-Path $repo 'research\REPLAY_MANIPULATOR.md'),(Join-Path $repo 'research\BLOODSTAIN_GHOST_PIPELINE.md'),(Join-Path $repo 'research\NATIVE_REPLAY_FRAME_FORMAT.md'),(Join-Path $repo 'research\REPLAY_GHOST_ACTOR.md'),(Join-Path $repo 'research\NATIVE_REPLAY_SOURCE_INDEX.md'),(Join-Path $repo 'research\NATIVE_REPLAY_RUNTIME_TEST.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'tools\native_replay\analyze_probe.py') -Destination $outputFull -Force
Push-Location $repo
try{& $python -m unittest discover -s tools/native_replay -p test_probe.py;if($LASTEXITCODE){throw 'Native journal analyzer tests failed'}}finally{Pop-Location}
Copy-Item -LiteralPath (Join-Path $build 'native-cpp-tests.log'),(Join-Path $build 'native-rust-tests.log'),(Join-Path $build 'native-dx12-smoke.log') -Destination $outputFull -Force
foreach($pair in @(@('third_party\minhook_vendor\LICENSE.txt','MinHook-LICENSE.txt'),@('third_party\imgui\LICENSE.txt','DearImGui-LICENSE.txt'))){Copy-Item -LiteralPath (Join-Path $repo $pair[0]) -Destination (Join-Path $outputFull $pair[1]) -Force}
$git=Join-Path $env:ProgramFiles 'Git\cmd\git.exe';$safe="safe.directory=$($repo.Replace('\','/'))"
$commit=& $git -c $safe -C $repo rev-parse HEAD
$branch=& $git -c $safe -C $repo branch --show-current
$dirty=& $git -c $safe -C $repo status --porcelain
foreach($name in @('licenses','THIRD_PARTY_NOTICES.txt')){$p=Join-Path (Join-Path $outputs 'TesterBuild') $name;if(Test-Path -LiteralPath $p){Copy-Item -LiteralPath $p -Destination $outputFull -Recurse -Force}}
Copy-Item -LiteralPath (Join-Path $repo 'research\NATIVE_REPLAY_ACCEPTANCE.md'),(Join-Path $repo 'research\NATIVE_REPLAY_RUNTIME_RESULT.md'),(Join-Path $repo 'research\NATIVE_REPLAY_RUNTIME_RESULT.json') -Destination $outputFull -Force
$manifest=@('Native bloodstain replay READ-ONLY research checkpoint; NOT native replay implementation',"Branch: $branch","Source parent/commit: $commit","Working tree dirty: $([bool]$dirty)",'Feature: native-bloodstain-readonly; write commands/callback mutations disabled','Target: AMD64 EldenRing_1_17 2.7.0.0 exact disk SHA D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134','Pinned SDK unchanged: 3c8c1d7633a99309fb004c9f894ea10b7967d0e0','Runtime: previous read-only build recorder population VERIFIED; ghost absent. Corrected owner+A8 binary reload unverified; see NATIVE_REPLAY_RUNTIME_RESULT.md')
foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')){$p=Join-Path $outputFull $name;$manifest+="$name bytes=$((Get-Item -LiteralPath $p).Length) SHA256=$((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash)"}
foreach($path in $preserved.Keys){if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $preserved[$path]){throw "Preserved build changed: $path"};$manifest+="Preserved: $path SHA256=$($preserved[$path])"}
$manifest|Set-Content -LiteralPath (Join-Path $outputFull 'BUILD_MANIFEST.txt') -Encoding utf8
Write-Output "Read-only native replay package staged at $outputFull"
