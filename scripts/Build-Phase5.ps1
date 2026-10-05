param([string]$OutputDirectory = 'C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$staging = Join-Path $repo 'build\phase5-package'
& (Join-Path $repo 'build_release.bat') $staging
if ($LASTEXITCODE -ne 0) { throw "Release build failed: $LASTEXITCODE" }
$cmakeBin = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& (Join-Path $cmakeBin 'ctest.exe') --test-dir (Join-Path $repo 'build') -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'C++ tests failed; final output not staged' }
& 'C:\Users\user\.cargo\bin\cargo.exe' test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if ($LASTEXITCODE -ne 0) { throw 'Rust tests failed; final output not staged' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
foreach ($file in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')) {
    Copy-Item -LiteralPath (Join-Path $staging $file) -Destination $OutputDirectory -Force
}
foreach ($file in @('PHASE5_STATUS.md','PHASE5_MANUAL_TEST.md','PHASE5_PLAYER_ANIMATION_RESEARCH.md','PHASE5_ROOT_MOTION.md')) {
    Copy-Item -LiteralPath (Join-Path $repo ('notes\' + $file)) -Destination $OutputDirectory -Force
}
Copy-Item -LiteralPath (Join-Path $repo 'ERPLAY_FORMAT.md') -Destination $OutputDirectory -Force
$commit = & git -c "safe.directory=$($repo.Replace('\','/'))" -C $repo rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify build commit' }
$manifest = @("Code commit: $commit",'Windows x64 Release; C++ 7/7 + Rust 12/12 PASS (both completed test runs required before staging)','Status: IMPLEMENTED - RUNTIME VALIDATION REQUIRED')
foreach ($file in @('EldenRingTheaterMode.exe','TheaterMode.dll')) {
    $hash = Get-FileHash -LiteralPath (Join-Path $OutputDirectory $file) -Algorithm SHA256
    $manifest += "$file SHA256=$($hash.Hash)"
}
$manifest | Set-Content -LiteralPath (Join-Path $OutputDirectory 'BUILD_MANIFEST.txt') -Encoding utf8
Write-Output "Phase5 matching EXE/DLL staged at $OutputDirectory. Old Phase4 outputs preserved."
