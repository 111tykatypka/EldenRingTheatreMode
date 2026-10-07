#!/bin/sh
# Unit tests for the pure Rust modules (no game, no native libraries needed).
set -e
cd "$(dirname "$0")"
out=${TMPDIR:-/tmp}/tm_rust_unit
mkdir -p "$out"
rustc --edition 2024 --test world_file_harness.rs -o "$out/world_file.exe" && "$out/world_file.exe"
rustc --edition 2024 --test ../../adapter/src/replay_interpolation.rs -o "$out/interp.exe" && "$out/interp.exe"
