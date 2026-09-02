#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <xcrun-sdk> <target-triple>" >&2
    exit 2
fi

sdk="$1"
target="$2"
repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
compiler="$(xcrun --sdk "$sdk" --find clang++)"

sources=("$repository_root"/tests/*_apple_mobile.cpp)
if [[ ! -e "${sources[0]}" ]]; then
    echo "no Apple mobile test sources found" >&2
    exit 1
fi

for source in "${sources[@]}"; do
    echo "Compiling ${source#"$repository_root"/} for $target"
    "$compiler" \
        -target "$target" \
        -isysroot "$sdk_path" \
        -std=c++17 \
        -fsyntax-only \
        -I"$repository_root/include" \
        -Wall \
        -Wextra \
        -Wpedantic \
        -pedantic-errors \
        -Wconversion \
        -Wsign-conversion \
        -Werror \
        "$source"
done
