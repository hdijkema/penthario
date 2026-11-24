#!/usr/bin/env bash
# Build a native Apple Silicon bundle and package it using the shared deployment script.
set -euo pipefail
repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_dir"
if [[ $(uname -s) != Darwin || $(uname -m) != arm64 ]]; then
    echo 'This initial macOS build targets a native Apple Silicon machine.' >&2
    exit 1
fi
version=${PGADM_RELEASE_VERSION:-}
if [[ -n $version && ! $version =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
    echo 'Use a numeric release version, e.g. 0.2.0.' >&2
    exit 1
fi
pg_root=$(brew --prefix libpq@18)
build_dir="$repo_dir/build/macos"
cmake -S . -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    "-DCMAKE_PREFIX_PATH=$pg_root" "-DPostgreSQL_ROOT=$pg_root" \
    -DPGADM_FLTK_TESTS=ON -DPGADM_FLTK_UI_TESTS=OFF "-DPGADM_RELEASE_VERSION=$version"
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$build_dir" --output-on-failure
python3 tools/package-macos.py --build-dir "$build_dir" --pg-root "$pg_root" --version "$version"
