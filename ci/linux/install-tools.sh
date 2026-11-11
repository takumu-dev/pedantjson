#!/bin/sh
# Installs what a Linux job needs, as root, in the Debian or Ubuntu image it runs in: the tools that
# the presets and vcpkg call, the packages named on the command line, and with LLVM_VERSION set,
# that version's Clang from apt.llvm.org. Clang's directory then goes first on the job's PATH, so
# that the presets' plain `clang++` is that version.
#
#   LLVM_VERSION=20 ci/linux/install-tools.sh g++ libc++-20-dev libc++abi-20-dev
#
# apt.llvm.org is signed with the key beside this script (fingerprint
# 6084 F3CF 814B 57C1 CF12 EFD5 15CF 4D18 AF4F 7421), and keeps no snapshots, so a job installs
# the newest release of the version it names.
set -eu

export DEBIAN_FRONTEND=noninteractive
here=$(dirname "$0")
. /etc/os-release

install() {
    apt-get install --yes --no-install-recommends "$@"
}

# apt checks what it fetches against each archive's signed index, and the certificates are for
# apt.llvm.org, which is served over HTTPS only.
apt-get update
install ca-certificates curl git jq ninja-build pkgconf tar unzip zip

# Debian 12 carries CMake 3.25, older than the 3.28 the project needs, and its backports a newer
# one. Its dependencies have to come from the backports too, so it's installed on its own.
if [ "$VERSION_CODENAME" = bookworm ]; then
    echo "deb http://deb.debian.org/debian bookworm-backports main" \
        > /etc/apt/sources.list.d/bookworm-backports.list
    apt-get update
    install --target-release bookworm-backports cmake
else
    install cmake
fi

if [ -n "${LLVM_VERSION:-}" ]; then
    mkdir -p /etc/apt/keyrings
    cp "$here/apt.llvm.org.asc" /etc/apt/keyrings/apt.llvm.org.asc
    cat > /etc/apt/sources.list.d/apt.llvm.org.sources <<EOF
Types: deb
URIs: https://apt.llvm.org/$VERSION_CODENAME/
Suites: llvm-toolchain-$VERSION_CODENAME-$LLVM_VERSION
Components: main
Signed-By: /etc/apt/keyrings/apt.llvm.org.asc
EOF
    # A distro that carries the same version may number its build higher, as Debian 12 does.
    cat > /etc/apt/preferences.d/apt.llvm.org.pref <<EOF
Package: *
Pin: origin apt.llvm.org
Pin-Priority: 1001
EOF
    apt-get update
    set -- "clang-$LLVM_VERSION" "$@"
    # CircleCI sources BASH_ENV at the start of each later step.
    if [ -n "${BASH_ENV:-}" ]; then
        echo "export PATH=\"/usr/lib/llvm-$LLVM_VERSION/bin:\$PATH\"" >> "$BASH_ENV"
    fi
fi

if [ $# -gt 0 ]; then
    install "$@"
fi
