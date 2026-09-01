#!/usr/bin/env bash

set -euo pipefail

: "${ANDROID_SDK_ROOT:?ANDROID_SDK_ROOT must point to the Android SDK}"
: "${GITHUB_WORKSPACE:?GITHUB_WORKSPACE must point to the repository}"

android_ndk="$ANDROID_SDK_ROOT/ndk/27.3.13750724"
build_directory="$GITHUB_WORKSPACE/build-android"

cmake -S "$GITHUB_WORKSPACE" -B "$build_directory" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="$android_ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=x86_64 \
    -DANDROID_PLATFORM=android-21 \
    -DCMAKE_CROSSCOMPILING_EMULATOR="bash;$GITHUB_WORKSPACE/.github/scripts/android-ctest-runner.sh" \
    -DSYSCAPE_BUILD_TESTS=ON \
    -DSYSCAPE_BUILD_EXAMPLES=OFF

cmake --build "$build_directory" --parallel
ctest --test-dir "$build_directory" --output-on-failure
