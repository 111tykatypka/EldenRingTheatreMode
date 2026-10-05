param(
 [string]$OutputDirectory='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Nightly_ResearchIntegration',
 [string]$CMakeBin='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$outputFull=[IO.Path]::GetFullPath($OutputDirectory)
if($outputFull -match '\\(Phase5[^\\]*|TesterBuild)(\\|$)'){throw 'Preserved build directory cannot be an output'}
$preserved=@{}
$outputs='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode'
foreach($folder in @('Phase5','TesterBuild')){
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
& $ctest --test-dir $build -C Release --output-on-failure --output-log (Join-Path $build 'nightly-cpp-tests.log')
if($LASTEXITCODE){throw 'C++ tests failed'}
& $cargo test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc 2>&1 | Tee-Object -FilePath (Join-Path $build 'nightly-rust-tests.log')
if($LASTEXITCODE){throw 'Rust tests failed'}
& $cargo build --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if($LASTEXITCODE){throw 'DLL Release build failed'}
Push-Location $repo
try{& $python -m unittest discover -s tools/research_integration -p test_tools.py;if($LASTEXITCODE){throw 'Research tool tests failed'}}finally{Pop-Location}
& $cmake --build (Join-Path $repo 'probe\build') --config Release --parallel 2
if($LASTEXITCODE){throw 'Compatibility probe build failed'}
New-Item -ItemType Directory -Path $outputFull -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'Release\EldenRingTheaterMode.exe') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'probe\build\Release\EldenRingCompatibilityProbe.exe') -Destination $outputFull -Force
foreach($name in @('NIGHTLY_STATUS','RUNTIME_TEST_PLAN','NIGHTLY_RESEARCH_FINDINGS','FAILED_EXPERIMENTS')){
 Copy-Item -LiteralPath (Join-Path $repo "notes\$name.md") -Destination $outputFull -Force
}
Copy-Item -LiteralPath (Join-Path $repo 'notes\NIGHTLY_KNOWN_ISSUES.md') -Destination (Join-Path $outputFull 'KNOWN_ISSUES.md') -Force
Copy-Item -LiteralPath (Join-Path $repo 'ERPLAY_FORMAT.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'research\symbols_2_7_0_0.json') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'third_party\imgui\LICENSE.txt') -Destination (Join-Path $outputFull 'DearImGui-LICENSE.txt') -Force
# The dependency pin/lock is unchanged. Reuse license texts only; no reference binaries.
$tester=Join-Path $outputs 'TesterBuild'
foreach($name in @('licenses','THIRD_PARTY_NOTICES.txt')){
 $p=Join-Path $tester $name;if(Test-Path -LiteralPath $p){Copy-Item -LiteralPath $p -Destination $outputFull -Recurse -Force}
}
Copy-Item -LiteralPath (Join-Path $build 'nightly-cpp-tests.log'),(Join-Path $build 'nightly-rust-tests.log') -Destination $outputFull -Force
$git=Join-Path $env:ProgramFiles 'Git\cmd\git.exe';$safe="safe.directory=$($repo.Replace('\','/'))"
$commit=& $git -c $safe -C $repo rev-parse HEAD
$branch=& $git -c $safe -C $repo branch --show-current
$dirty=& $git -c $safe -C $repo status --porcelain
$manifest=@('DEVELOPER EXPERIMENTAL BUILD',"Branch: $branch","Source commit: $commit","Working tree dirty: $([bool]$dirty)","Built UTC: $([DateTime]::UtcNow.ToString('o'))",'AMD64 Release; EldenRing_1_17 / file+product 2.7.0.0','Target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134','Pinned SDK: 3c8c1d7633a99309fb004c9f894ea10b7967d0e0','ERPLAY02 reader retained; ERPLAY03 unchanged; no ERPLAY04 introduced','Control v3/128 bytes, nightly capability 64; trace kinds 12/13; ownership probe kind 14; selected-only replay flag 2','C++ CTest PASS; Rust tests PASS; see exact captured logs. Python research tests 3/3 PASS.','Previously user-verified: player/NPC capture, host trajectories; player replay PARTIAL.','Current nightly: NO NEW LIVE GAME VERIFICATION. Grounding/NPC playback/animation remain FAILED or UNVERIFIED.','New diagnostic binding: typed CSLuaEventManImp READ-ONLY; runtime UNVERIFIED','Debug flag layouts CONFLICT: pinned SDK 0x530 vs Freecam 0x538; debug flag writes BLOCKED','animationSpeed=0 selected-NPC 2s experiment only; identity/module reacquisition; UNVERIFIED','Camera/warp/native animation event calls NOT IMPLEMENTED; candidate ABI UNKNOWN','One host ReplayPlayer clock; no second ERPLAY parser; no custom injector; YAFSML retained')
foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')){
 $p=Join-Path $outputFull $name;$manifest+="$name bytes=$((Get-Item -LiteralPath $p).Length) SHA256=$((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash)"
}
foreach($path in $preserved.Keys){
 if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $preserved[$path]){throw "Preserved build changed: $path"}
 $manifest+="Preserved: $path SHA256=$($preserved[$path])"
}
$manifest|Set-Content -LiteralPath (Join-Path $outputFull 'BUILD_MANIFEST.txt') -Encoding utf8
Write-Output "Nightly package staged at $outputFull"
