param([string]$OutputDirectory = 'C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4B')
$ErrorActionPreference = 'Stop'
$repositoryDirectory = Split-Path -Parent $PSScriptRoot
& (Join-Path $repositoryDirectory 'build_release.bat') $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw "Release build failed: $LASTEXITCODE" }
$cmakeBin = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& (Join-Path $cmakeBin 'ctest.exe') --test-dir (Join-Path $repositoryDirectory 'build') -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'C++ tests failed' }
& 'C:\Users\user\.cargo\bin\cargo.exe' test --manifest-path (Join-Path $repositoryDirectory 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if ($LASTEXITCODE -ne 0) { throw 'Rust tests failed' }
Copy-Item -LiteralPath (Join-Path $repositoryDirectory 'notes\PHASE4B_MANUAL_TEST.md') -Destination $OutputDirectory -Force
Copy-Item -LiteralPath (Join-Path $repositoryDirectory 'notes\PHASE4B_STATUS.md') -Destination $OutputDirectory -Force
Write-Output "Phase 4B staged at $OutputDirectory. Runtime transform playback still requires the user's in-game test."
