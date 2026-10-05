param(
 [string]$OutputDirectory='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\TesterBuild',
 [string]$CMakeBin='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin',
 [string]$Generator='Visual Studio 18 2026'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$build=Join-Path $repo 'build' # adapter/build.rs links this exact GameProfile.lib
$outputFull=[IO.Path]::GetFullPath($OutputDirectory)
if($outputFull -match '\\Phase5[^\\]*(\\|$)'){throw 'Refusing to overwrite a preserved Phase5 build'}
$golden='C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5'
$goldenHashes=@{}
foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll')){
 $path=Join-Path $golden $name
 if(Test-Path -LiteralPath $path){$goldenHashes[$path]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
}
$cmake=Join-Path $CMakeBin 'cmake.exe'
$ctest=Join-Path $CMakeBin 'ctest.exe'
$cargo=Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
& $cmake -S $repo -B $build -G $Generator -A x64 -DBUILD_TESTING=ON
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed'}
& $cmake --build $build --config Release --parallel 2
if($LASTEXITCODE -ne 0){throw 'Release host build failed'}
& $ctest --test-dir $build -C Release --output-on-failure
if($LASTEXITCODE -ne 0){throw 'C++ tests failed; run named-pipe tests under a normal Windows token'}
& $cargo test --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if($LASTEXITCODE -ne 0){throw 'Rust tests failed'}
& $cargo build --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --release --locked --offline --target x86_64-pc-windows-msvc
if($LASTEXITCODE -ne 0){throw 'Release DLL build failed'}
& $cmake -S (Join-Path $repo 'probe') -B (Join-Path $repo 'probe\build') -G $Generator -A x64
if($LASTEXITCODE -ne 0){throw 'Probe configuration failed'}
& $cmake --build (Join-Path $repo 'probe\build') --config Release --parallel 2
if($LASTEXITCODE -ne 0){throw 'Release probe build failed'}

New-Item -ItemType Directory -Path $outputFull -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'Release\EldenRingTheaterMode.exe') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'probe\build\Release\EldenRingCompatibilityProbe.exe') -Destination $outputFull -Force
foreach($name in @('TESTER_README','KNOWN_ISSUES','TEST_RESULTS_TEMPLATE','PHASE6_TESTER_STATUS')){
 Copy-Item -LiteralPath (Join-Path $repo "notes\$name.md") -Destination $outputFull -Force
}
Copy-Item -LiteralPath (Join-Path $repo 'ERPLAY_FORMAT.md') -Destination $outputFull -Force
Copy-Item -LiteralPath (Join-Path $repo 'third_party\imgui\LICENSE.txt') -Destination (Join-Path $outputFull 'DearImGui-LICENSE.txt') -Force

# Include attribution/license texts, never third-party binaries or game exports.
$raw=& $cargo metadata --manifest-path (Join-Path $repo 'adapter\Cargo.toml') --locked --offline --filter-platform x86_64-pc-windows-msvc --format-version 1
if($LASTEXITCODE -ne 0){throw 'Cannot obtain pinned dependency/license metadata'}
$metadata=$raw|ConvertFrom-Json
$notices=@('Third-party Rust package metadata from the locked dependency graph.','Dear ImGui license is included separately.','No Elden Ring executable, camera-tool binary, game asset, Ghidra database or YAFSML binary is bundled.','')
foreach($package in $metadata.packages){
 if($package.name -eq 'theater-mode-adapter'){continue}
 $notices+="$($package.name) $($package.version) | $($package.license) | $($package.repository) | $($package.source)"
 $directory=Split-Path -Parent $package.manifest_path
 for($level=0;$level -lt 5;$level++){
  $files=Get-ChildItem -LiteralPath $directory -File | Where-Object {$_.Name -match '^(LICENSE|COPYING|NOTICE)([.\-_]|$)'}
  if($files){
   $destination=Join-Path $outputFull "licenses\$($package.name)-$($package.version)"
   New-Item -ItemType Directory -Path $destination -Force | Out-Null
   foreach($file in $files){Copy-Item -LiteralPath $file.FullName -Destination $destination -Force}
   break
  }
  $parent=Split-Path -Parent $directory
  if(!$parent -or $parent -eq $directory){break}
  $directory=$parent
 }
}
$notices|Set-Content -LiteralPath (Join-Path $outputFull 'THIRD_PARTY_NOTICES.txt') -Encoding utf8
$safe="safe.directory=$($repo.Replace('\','/'))"
$git=Join-Path $env:ProgramFiles 'Git\cmd\git.exe'
$commit=& $git -c $safe -C $repo rev-parse HEAD
$branch=& $git -c $safe -C $repo branch --show-current
$dirty=& $git -c $safe -C $repo status --porcelain
$manifest=@("Source commit: $commit","Branch: $branch","Working tree dirty: $([bool]$dirty)","Built UTC: $([DateTime]::UtcNow.ToString('o'))",'Windows AMD64 Release','Target: EldenRing_1_17 / file+product 2.7.0.0','Target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134','Path policy: eldenring.exe name + mandatory exact version/AMD64/full disk hash; configurable installation location','SDK pin: 3c8c1d7633a99309fb004c9f894ea10b7967d0e0','ERPLAY03; optional character track 3 and visual-state track 4/schema 1; ERPLAY02 reader retained','Control v3/128 bytes, actor capability bit 32/kind 11; character observation pipe v2','C++ CTest: 11/11 PASS','Rust tests: 17/17 PASS','Player replay: retained baseline; current-build regression still required','Actor transforms: EXPERIMENTAL opt-in OFF by default','Player raw animation requests: EXPERIMENTAL opt-in OFF by default','NPC animation / equipment / appearance / VFX / death restoration: NOT IMPLEMENTED','Native actor AI ownership / grounding: UNVERIFIED','Capture defaults: radius 200, exit x1.2, requested 60Hz, configurable budget 1024 (1..16384)','Stop: F6; stale target lease 250ms; no cached-reference writes','Runtime status: IMPLEMENTED - RUNTIME VALIDATION REQUIRED','No live game FPS, new capture data rate, animation/control or NPC runtime verification claimed')
foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')){
 $path=Join-Path $outputFull $name
 $manifest+="$name bytes=$((Get-Item -LiteralPath $path).Length) SHA256=$((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)"
}
foreach($path in $goldenHashes.Keys){
 if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $goldenHashes[$path]){throw 'Golden Phase5 changed unexpectedly'}
 $manifest+="Preserved golden: $path SHA256=$($goldenHashes[$path])"
}
$manifest|Set-Content -LiteralPath (Join-Path $outputFull 'BUILD_MANIFEST.txt') -Encoding utf8
Write-Output "Matching tester EXE/DLL/probe and instructions staged at $outputFull"
