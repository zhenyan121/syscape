#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <test-executable> [arguments...]" >&2
    exit 2
fi

host_executable=$1
shift

remote_directory=/data/local/tmp/syscape-tests
remote_executable="$remote_directory/$(basename "$host_executable")"

adb shell mkdir -p "$remote_directory"
adb push "$host_executable" "$remote_executable" >/dev/null
adb shell chmod 700 "$remote_executable"

set +e
adb shell "$remote_executable" "$@"
test_status=$?
set -e

adb shell rm -f "$remote_executable"
exit "$test_status"
