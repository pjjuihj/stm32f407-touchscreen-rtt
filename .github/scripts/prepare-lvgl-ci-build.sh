#!/usr/bin/env bash
set -euo pipefail

template=".github/ci/lvgl-os-desktop.cmake"
target="LVGL/env_support/cmake/os_desktop.cmake"

test -s "$template"

if [[ -e "$target" ]]; then
  echo "LVGL desktop CMake integration exists; leaving it unchanged."
  exit 0
fi

mkdir -p "$(dirname "$target")"
cp "$template" "$target"
