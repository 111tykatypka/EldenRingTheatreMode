param(
    [string]$OutputDirectory = 'C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4A',
    [string]$LoaderDirectory = 'C:\Users\user\Desktop\YAFSML-v0.10.4'
)
$ErrorActionPreference = 'Stop'
$repositoryDirectory = Split-Path -Parent $PSScriptRoot
& (Join-Path $repositoryDirectory 'build_release.bat') $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw "Release build failed: $LASTEXITCODE" }
$cmakeBin = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& (Join-Path $cmakeBin 'ctest.exe') --test-dir (Join-Path $repositoryDirectory 'build') -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'C++ tests failed' }
& 'C:\Users\user\.cargo\bin\cargo.exe' test --manifest-path (Join-Path $repositoryDirectory 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if ($LASTEXITCODE -ne 0) { throw 'Rust tests failed' }

# Prepare a selectable config, leaving the user's existing YAFSML config intact.
# No game or reference binaries are copied, loaded, or launched by this script.
$loaderConfig = Join-Path $LoaderDirectory 'YAFSML.ini'
$text = Get-Content -LiteralPath $loaderConfig -Raw
if ([regex]::Matches($text, '(?m)^theater_mode=.*$').Count -ne 1) { throw 'Expected one theater_mode DLL entry in current YAFSML.ini' }
$dllPath = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'TheaterMode.dll'
$text = [regex]::Replace($text, '(?m)^theater_mode=.*$', [System.Text.RegularExpressions.MatchEvaluator]{param($match) 'theater_mode=' + $dllPath})
Set-Content -LiteralPath (Join-Path $OutputDirectory 'YAFSML_Phase4A.ini') -Value $text -Encoding utf8
$launchText = @"
@echo off
setlocal
cd /d "$LoaderDirectory"
"$LoaderDirectory\YAFSML.exe" -p "C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe" -c "%~dp0YAFSML_Phase4A.ini"
endlocal
"@
Set-Content -LiteralPath (Join-Path $OutputDirectory 'Start-EldenRing-Phase4A.cmd') -Value $launchText -Encoding ascii
Copy-Item -LiteralPath (Join-Path $repositoryDirectory 'notes\PHASE4A_MANUAL_TEST.md') -Destination (Join-Path $OutputDirectory 'PHASE4A_MANUAL_TEST.md') -Force
Write-Output "Phase 4A checkpoint staged at $OutputDirectory. No runtime test was performed."
