param()
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$native=Join-Path $repo 'build-p2e'
$out=Join-Path $repo 'outputs\P2e1-actor-observations'
if(Test-Path -LiteralPath $out){throw "Package already exists: $out. Preserve it; choose a new checkpoint name in this script."}
$cmake='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
$cargo=Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
$python=Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
function Checked([scriptblock]$command){& $command;if($LASTEXITCODE -ne 0){throw "Command failed: $LASTEXITCODE"}}
Push-Location $repo
$oldNative=$env:THEATER_NATIVE_LIBRARY_ROOT
try{
 Checked {& $cmake -S $repo -B $native -G 'Visual Studio 18 2026' -A x64}
 Checked {& $cmake --build $native --config Release --parallel 4}
 $env:THEATER_NATIVE_LIBRARY_ROOT=$native
 Checked {& $cargo test --manifest-path adapter/Cargo.toml --release --locked --offline --target x86_64-pc-windows-msvc}
 Checked {& $cargo build --manifest-path adapter/Cargo.toml --release --locked --offline --target x86_64-pc-windows-msvc}
 Checked {& $ctest --test-dir $native -C Release --output-on-failure}
 Checked {& $python -m unittest discover -s tests -p test_actor_lifetime_inspector.py -v}
 New-Item -ItemType Directory -Path $out | Out-Null
 Copy-Item -LiteralPath (Join-Path $native 'Release\EldenRingTheaterMode.exe') -Destination $out
 Copy-Item -LiteralPath (Join-Path $repo 'adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll') -Destination $out
 # Unchanged compatibility probe and sounds are preserved from our own P2d package.
 $baseline=Join-Path $repo 'outputs\P2d-fidelity-core'
 Copy-Item -LiteralPath (Join-Path $baseline 'EldenRingCompatibilityProbe.exe') -Destination $out
 Copy-Item -LiteralPath (Join-Path $baseline 'sounds') -Destination $out -Recurse
 Copy-Item -LiteralPath (Join-Path $repo 'notes\P2E_RESEARCH_AND_STATUS.md') -Destination $out
 Copy-Item -LiteralPath (Join-Path $repo 'notes\P2E_RUNTIME_TEST.md') -Destination $out
 Copy-Item -LiteralPath (Join-Path $repo 'docs\ERWORLD_ACTOR_OBSERVATIONS.md') -Destination $out
 $safe="safe.directory=$($repo.Replace('\','/'))"
 $commit=& git -c $safe rev-parse HEAD
 $branch=& git -c $safe branch --show-current
 $dirty=& git -c $safe status --porcelain
 $manifest=@("source_commit=$commit","branch=$branch","source_dirty=$([bool]$dirty)","build_utc=$([DateTime]::UtcNow.ToString('o'))",'configuration=Release AMD64','checkpoint=P2e1 observations; NOT full enemy resurrection or VFX replay','runtime=UNVERIFIED','game=EldenRing_1_17 / 2.7.0.0','target_sha256=D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134','probe_and_sounds=unchanged P2d package','tests=Rust, CTest and inspector passed during this build')
 foreach($name in @('EldenRingTheaterMode.exe','TheaterMode.dll','EldenRingCompatibilityProbe.exe')){$hash=Get-FileHash -LiteralPath (Join-Path $out $name) -Algorithm SHA256;$manifest+="$name SHA256=$($hash.Hash)"}
 $manifest | Set-Content -LiteralPath (Join-Path $out 'BUILD_MANIFEST.txt') -Encoding utf8
 'P2e1 — actor lifecycle observation capture; native recreation NOT IMPLEMENTED' | Set-Content -LiteralPath (Join-Path $out 'BUILD_NAME.txt') -Encoding utf8
 Write-Output "Matching experimental host/DLL staged: $out"
}finally{
 $env:THEATER_NATIVE_LIBRARY_ROOT=$oldNative
 Pop-Location
}
