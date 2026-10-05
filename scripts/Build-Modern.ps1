param([string]$OutputDirectory='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Modern')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$build=Join-Path $repo 'build-modern'
$cmakeBin='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$cargo=Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
$outputFull=[IO.Path]::GetFullPath($OutputDirectory)
if ($outputFull -match '\\Phase5(C)?(\\|$)') { throw 'Refusing to overwrite a preserved Phase5 checkpoint' }
& (Join-Path $cmakeBin 'cmake.exe') -S $repo -B $build -G 'Visual Studio 18 2026' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& (Join-Path $cmakeBin 'cmake.exe') --build $build --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Host build failed' }
& (Join-Path $cmakeBin 'ctest.exe') --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'C++ tests failed' }
& $cargo test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if ($LASTEXITCODE -ne 0) { throw 'Rust tests failed' }
& $cargo build --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if ($LASTEXITCODE -ne 0) { throw 'DLL build failed' }
& (Join-Path $cmakeBin 'cmake.exe') -S (Join-Path $repo 'probe') -B (Join-Path $repo 'probe\build') -G 'Visual Studio 18 2026' -A x64
if ($LASTEXITCODE -ne 0) { throw 'Probe configuration failed' }
& (Join-Path $cmakeBin 'cmake.exe') --build (Join-Path $repo 'probe\build') --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Probe build failed' }
New-Item -ItemType Directory -Path $outputFull -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'Release\EldenRingTheaterMode.exe') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'probe\build\Release\EldenRingCompatibilityProbe.exe') -Destination $outputFull -Force
foreach($name in @('MODERN_REBUILD_STATUS','IMGUI_UI_ARCHITECTURE','CHARACTER_REPLAY_DESIGN','CHARACTER_RUNTIME_RESEARCH','CURRENT_RUNTIME_LIMITATIONS','RUNTIME_TEST_PLAN')) {
 Copy-Item -LiteralPath (Join-Path $repo "notes\$name.md") -Destination $outputFull -Force
}
Copy-Item -LiteralPath (Join-Path $repo 'third_party\imgui\LICENSE.txt') -Destination (Join-Path $outputFull 'DearImGui-LICENSE.txt') -Force
$safe="safe.directory=$($repo.Replace('\','/'))"
$commit=& git -c $safe -C $repo rev-parse HEAD
$branch=& git -c $safe -C $repo branch --show-current
$dirty=& git -c $safe -C $repo status --porcelain
$manifest=@("Source commit: $commit","Branch: $branch","Working tree dirty: $([bool]$dirty)","Built UTC: $([DateTime]::UtcNow.ToString('o'))",'Windows AMD64 Release','Target: EldenRing_1_17 / file+product 2.7.0.0','Target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134','Dear ImGui: 3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c','C++ CTest: PASS (all configured tests)','Rust tests: PASS','Modern runtime status: IMPLEMENTED - RUNTIME VALIDATION REQUIRED')
foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')){$hash=Get-FileHash -LiteralPath (Join-Path $outputFull $name) -Algorithm SHA256;$manifest+="$name SHA256=$($hash.Hash)"}
$manifest | Set-Content -LiteralPath (Join-Path $outputFull 'BUILD_MANIFEST.txt') -Encoding utf8
Write-Output "Modern matching EXE/DLL/probe staged at $outputFull"
