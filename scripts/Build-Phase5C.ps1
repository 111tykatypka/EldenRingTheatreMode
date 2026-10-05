param([string]$OutputDirectory = 'C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5C')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$staging = Join-Path $repo 'build\phase5c-package'
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
foreach ($file in @('PHASE5C_STATUS.md','PHASE5C_MANUAL_TEST.md','PHASE5C_CHARACTER_DRIVING_SYSTEMS.md')) {
    Copy-Item -LiteralPath (Join-Path $repo ('notes\' + $file)) -Destination $OutputDirectory -Force
}
$commit = & git -c "safe.directory=$($repo.Replace('\','/'))" -C $repo rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify build commit' }
$dirty = & git -c "safe.directory=$($repo.Replace('\','/'))" -C $repo status --porcelain
$manifest = @("Code commit: $commit", "Working tree changes: $([bool]$dirty)", 'Windows x64 Release; all configured CTest + Rust tests completed before staging', 'Status: DIAGNOSTIC IMPLEMENTED - RUNTIME VALIDATION REQUIRED; locomotion driver not verified')
foreach ($file in @('EldenRingTheaterMode.exe','TheaterMode.dll')) {
    $hash = Get-FileHash -LiteralPath (Join-Path $OutputDirectory $file) -Algorithm SHA256
    $manifest += "$file SHA256=$($hash.Hash)"
}
$manifest | Set-Content -LiteralPath (Join-Path $OutputDirectory 'BUILD_MANIFEST.txt') -Encoding utf8
Write-Output "Phase5C matching EXE/DLL staged at $OutputDirectory. Existing packages preserved."
