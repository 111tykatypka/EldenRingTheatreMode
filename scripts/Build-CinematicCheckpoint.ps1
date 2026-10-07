param([string]$Checkpoint='Cinematic-C3-native-camera-prototype')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$native=Join-Path $repo 'build-cinematic'
$out=Join-Path (Join-Path $repo 'outputs') $Checkpoint
if(Test-Path -LiteralPath $out){throw "Preserved package already exists: $out"}
$cmake='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
$cargo=Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
function Checked([scriptblock]$command){& $command;if($LASTEXITCODE -ne 0){throw "Command failed: $LASTEXITCODE"}}
$priorNative=$env:THEATER_NATIVE_LIBRARY_ROOT
Push-Location $repo
try {
 Checked {& $cmake -S $repo -B $native -G 'Visual Studio 18 2026' -A x64}
 Checked {& $cmake --build $native --config Release --parallel 4}
 $env:THEATER_NATIVE_LIBRARY_ROOT=$native
 Checked {& $cargo test --manifest-path adapter/Cargo.toml --release --locked --offline --target x86_64-pc-windows-msvc}
 Checked {& $cargo build --manifest-path adapter/Cargo.toml --release --locked --offline --target x86_64-pc-windows-msvc}
 Checked {& $ctest --test-dir $native -C Release --output-on-failure}
 New-Item -ItemType Directory -Path $out | Out-Null
 Copy-Item -LiteralPath (Join-Path $native 'Release\EldenRingTheaterMode.exe') -Destination $out
 Copy-Item -LiteralPath (Join-Path $repo 'adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll') -Destination $out
 $baseline=Join-Path $repo 'outputs\P2d-fidelity-core'
 Copy-Item -LiteralPath (Join-Path $baseline 'EldenRingCompatibilityProbe.exe') -Destination $out
 Copy-Item -LiteralPath (Join-Path $baseline 'sounds') -Destination $out -Recurse
 Copy-Item -LiteralPath (Join-Path $repo 'notes\CINEMATIC_EDITOR_CHECKPOINT_1.md') -Destination $out
 Copy-Item -LiteralPath (Join-Path $repo 'notes\CINEMATIC_C3_NATIVE_CAMERA.md') -Destination $out
 Copy-Item -LiteralPath (Join-Path $repo 'notes\CINEMATIC_C4_CAMERA_EDITOR.md') -Destination $out
 Copy-Item -LiteralPath (Join-Path $repo 'notes\CINEMATIC_RENDERING_RESEARCH_C4.md') -Destination $out
 $safe="safe.directory=$($repo.Replace('\','/'))"
 $commit=& git -c $safe rev-parse HEAD
 $dirty=& git -c $safe status --porcelain
 $manifest=@("source_commit=$commit","source_dirty=$([bool]$dirty)",'branch=codex/cinematic-editor-pass','configuration=Release AMD64',"checkpoint=$Checkpoint",'runtime=UNVERIFIED','camera_writes_default=OFF','tests=14 CTest, 49 Rust passed; 1 optional Rust test ignored')
 foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')){$hash=Get-FileHash -LiteralPath (Join-Path $out $name) -Algorithm SHA256;$manifest+="$name SHA256=$($hash.Hash)"}
 $manifest | Set-Content -LiteralPath (Join-Path $out 'BUILD_MANIFEST.txt') -Encoding utf8
 Write-Output "Experimental camera checkpoint staged: $out"
} finally {$env:THEATER_NATIVE_LIBRARY_ROOT=$priorNative;Pop-Location}
