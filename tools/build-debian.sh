#!/usr/bin/env bash
set -euo pipefail
export LANG=C.UTF-8
export LC_ALL=C.UTF-8
repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_dir"
# Dependencies are installed by the caller (CI container or local administrator).
. /etc/os-release
if [[ ${ID:-} != debian || ! ${VERSION_ID:-} =~ ^(12|13)$ ]]; then
    echo 'This script packages Debian 12 or 13.' >&2
    exit 1
fi
build_dir="build/debian${VERSION_ID}"
cmake -S . -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr -DPGADM_FLTK_TESTS=ON -DPGADM_FLTK_UI_TESTS=OFF -DFLTK_BACKEND_WAYLAND=OFF \
    "-DPGADM_DEBIAN_VERSION=$VERSION_ID" "-DPGADM_RELEASE_VERSION=${PGADM_RELEASE_VERSION:-}" "$@"
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
# Fast, noninteractive tests; display-backed checks below only open and close the app.
ctest --test-dir "$build_dir" --output-on-failure
for language in nl en de; do
    printf 'Checking application startup (%s)\n' "$language"
    xvfb-run -a "$build_dir/penthario" --language "$language" \
        --settings-file "$repo_dir/$build_dir/smoke-$language.json" --startup-check \
        --startup-report "$repo_dir/$build_dir/startup-$language.txt"
    test -s "$repo_dir/$build_dir/startup-$language.txt"
done
mkdir -p dist
cpack --config "$build_dir/CPackConfig.cmake" -G DEB -B dist
