#!/bin/sh
# Copies the logs a failed job leaves into a directory, for CircleCI to keep: each build's test logs
# and vcpkg install log, and the texts that crashed the fuzzer, under their paths in the checkout,
# and vcpkg's build logs, under vcpkg/.
#
#   ci/gather-logs.sh logs
set -eu

destination=$1
for path in build/*/Testing/Temporary build/*/fuzz/crashes build/*/vcpkg-manifest-install.log; do
    if [ -e "$path" ]; then
        mkdir -p "$destination/$(dirname "$path")"
        cp -R "$path" "$destination/$(dirname "$path")/"
    fi
done
for log in "${VCPKG_ROOT:-/nonexistent}"/buildtrees/*/*.log; do
    if [ -e "$log" ]; then
        port=$(basename "$(dirname "$log")")
        mkdir -p "$destination/vcpkg/$port"
        cp "$log" "$destination/vcpkg/$port/"
    fi
done
