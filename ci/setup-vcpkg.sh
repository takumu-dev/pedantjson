#!/bin/sh
# Clones vcpkg into VCPKG_ROOT at the builtin-baseline of vcpkg.json, and bootstraps it. vcpkg reads
# the ports at the versions the manifest names from the registry's history, so a shallow clone
# won't do; a blobless one fetches only the files vcpkg reads.
set -eu

baseline=$(jq --raw-output '."builtin-baseline"' vcpkg.json)
git clone --quiet --filter=blob:none --no-checkout https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
git -C "$VCPKG_ROOT" checkout --quiet "$baseline"
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
if [ -n "${VCPKG_DEFAULT_BINARY_CACHE:-}" ]; then
    mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"
fi
