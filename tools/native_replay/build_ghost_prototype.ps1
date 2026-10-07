param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $repoRoot 'outputs/P2d-fidelity-core'}
$buildRoot=Join-Path $repoRoot 'build-native-ghost'
$cargoRoot=Join-Path $repoRoot 'adapter/target/native-ghost-prototype'
$cmake='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$cargo=Join-Path $env:USERPROFILE '.cargo/bin/cargo.exe'
function Invoke-Checked([string]$Program,[string[]]$Arguments){& $Program @Arguments;if($LASTEXITCODE){throw "$Program exited $LASTEXITCODE"}}
Invoke-Checked $cmake @('-S',$repoRoot,'-B',$buildRoot,'-G','Visual Studio 18 2026','-A','x64','-DBUILD_TESTING=OFF')
Invoke-Checked $cmake @('--build',$buildRoot,'--config','Release','--target','EldenRingTheaterMode','TheaterRenderBackend','--parallel')
Invoke-Checked $cmake @('-S',(Join-Path $repoRoot 'probe'),'-B',(Join-Path $buildRoot 'probe'),'-G','Visual Studio 18 2026','-A','x64')
Invoke-Checked $cmake @('--build',(Join-Path $buildRoot 'probe'),'--config','Release','--parallel')
$priorLib=$env:THEATER_NATIVE_LIBRARY_ROOT;$priorTarget=$env:CARGO_TARGET_DIR
try {
 $env:THEATER_NATIVE_LIBRARY_ROOT=$buildRoot;$env:CARGO_TARGET_DIR=$cargoRoot
 Invoke-Checked $cargo @('build','--manifest-path',(Join-Path $repoRoot 'adapter/Cargo.toml'),'--release','--locked','--offline','--target','x86_64-pc-windows-msvc')
}finally{$env:THEATER_NATIVE_LIBRARY_ROOT=$priorLib;$env:CARGO_TARGET_DIR=$priorTarget}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $buildRoot 'Release/EldenRingTheaterMode.exe') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $cargoRoot 'x86_64-pc-windows-msvc/release/TheaterMode.dll') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $buildRoot 'probe/Release/EldenRingCompatibilityProbe.exe') -Destination $OutputDirectory
# UI sounds: the user's own WAV files (not in git). THEATER_UI_SOUNDS overrides the folder.
$sounds=if($env:THEATER_UI_SOUNDS){$env:THEATER_UI_SOUNDS}else{Join-Path $env:USERPROFILE 'Documents/claude ui/sound/fx/ui'}
if(Test-Path -LiteralPath $sounds){$target=Join-Path $OutputDirectory 'sounds/ui';New-Item -ItemType Directory -Path $target -Force|Out-Null;Copy-Item -Path (Join-Path $sounds '*') -Destination $target -Recurse -Force}else{Write-Warning "UI sounds folder not found: $sounds (the overlay runs silent)"}
$gitArgs=@('-c',('safe.directory='+$repoRoot.Replace('\','/')),'-C',$repoRoot)
$commit=(& git @gitArgs rev-parse HEAD);$branch=(& git @gitArgs branch --show-current)
$status=(& git @gitArgs status --porcelain)
$sources=@('adapter/Cargo.toml','adapter/Cargo.lock','adapter/build.rs','CMakeLists.txt')+
 @((Get-ChildItem -LiteralPath (Join-Path $repoRoot 'adapter/src'),(Join-Path $repoRoot 'src'),(Join-Path $repoRoot 'shared'),(Join-Path $repoRoot 'native_ui'),(Join-Path $repoRoot 'third_party') -File -Recurse).FullName | ForEach-Object{$_.Substring($repoRoot.Length).TrimStart([char[]]"\/").Replace('\','/')})
$hashRows=@($sources|Sort-Object -Unique|ForEach-Object{$h=Get-FileHash -LiteralPath (Join-Path $repoRoot $_) -Algorithm SHA256;"$($h.Hash)  $_"})
$hashRows|Set-Content -LiteralPath (Join-Path $OutputDirectory 'SOURCE_SHA256.txt') -Encoding utf8
$binaryRows=@(Get-ChildItem -LiteralPath $OutputDirectory -File|Where-Object Extension -in '.exe','.dll'|ForEach-Object{"$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)  $($_.Name)"})
@("branch=$branch","source_commit=$commit","build_timestamp_utc=$([DateTime]::UtcNow.ToString('o'))",'feature=bone-replay','configuration=Release x64','runtime=UNVERIFIED','game_sha256=D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134',"source_manifest_sha256=$((Get-FileHash -LiteralPath (Join-Path $OutputDirectory 'SOURCE_SHA256.txt')).Hash)",'worktree_status_at_build:')+$status+$binaryRows|Set-Content -LiteralPath (Join-Path $OutputDirectory 'BUILD_MANIFEST.txt') -Encoding utf8
$implementationNotes = Join-Path $repoRoot 'notes/P2D_ROADMAP_STATUS.md'
$runtimeNotes = Join-Path $repoRoot 'notes/P2D_RUNTIME_TEST.md'
if (!(Test-Path -LiteralPath $implementationNotes)) { $implementationNotes = Join-Path $repoRoot 'notes/NATIVE_GHOST_PROTOTYPE_IMPLEMENTATION_STATUS.md' }
if (!(Test-Path -LiteralPath $runtimeNotes)) { $runtimeNotes = Join-Path $repoRoot 'notes/NATIVE_GHOST_PROTOTYPE_RUNTIME_TEST_PLAN.md' }
Copy-Item -LiteralPath $implementationNotes -Destination (Join-Path $OutputDirectory 'IMPLEMENTATION_STATUS.md')
Copy-Item -LiteralPath $runtimeNotes -Destination (Join-Path $OutputDirectory 'RUNTIME_TEST_PLAN.md')
'P2d — lossless skeletal fidelity/core checkpoint; FULL ROADMAP NOT COMPLETE; RUNTIME VALIDATION REQUIRED' | Set-Content -LiteralPath (Join-Path $OutputDirectory 'BUILD_NAME.txt') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/PHASES_SUMMARY.md') -Destination (Join-Path $OutputDirectory 'PHASES_SUMMARY.md')
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/WORLD_COMPANIONS_FORMAT.md') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/ERWORLD_V2_FORMAT.md') -Destination $OutputDirectory
if(Test-Path -LiteralPath (Join-Path $repoRoot 'notes/P2D_TEST_RESULTS.md')){Copy-Item -LiteralPath (Join-Path $repoRoot 'notes/P2D_TEST_RESULTS.md') -Destination (Join-Path $OutputDirectory 'TEST_RESULTS.md')}
Write-Output "Release staged: $OutputDirectory (runtime UNVERIFIED)"
