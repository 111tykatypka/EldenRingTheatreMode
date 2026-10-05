param([switch]$Disable)
$ErrorActionPreference = 'Stop'
$researchDirectory = Join-Path $env:LOCALAPPDATA 'EldenRingTheaterMode'
New-Item -ItemType Directory -Force -Path $researchDirectory | Out-Null
$researchConfig = Join-Path $researchDirectory 'Research.readonly.ini'
$researchSetting = if ($Disable) { 'enabled=0' } else { 'enabled=1' }
[System.IO.File]::WriteAllText($researchConfig, $researchSetting, [System.Text.UTF8Encoding]::new($false))
Write-Output "Saved $researchConfig. Restart the game to apply. No game files changed."
