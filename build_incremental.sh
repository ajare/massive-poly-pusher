#!/usr/bin/env bash

set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: ./build_incremental.sh [--config CONFIG] [--build-dir DIR]

Build the existing MassivePolyPusher build tree incrementally.

Options:
  --config CONFIG    CMake build configuration (default: Release).
  --build-dir DIR    Build directory, relative to the repository root unless
                     absolute (default: build).
  -h, --help         Show this help.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

CONFIG=Release
BUILD_DIR=build

while (($#)); do
    case "$1" in
        --config)
            (($# >= 2)) || fail "--config requires a value"
            CONFIG=$2
            shift 2
            ;;
        --build-dir)
            (($# >= 2)) || fail "--build-dir requires a value"
            BUILD_DIR=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1 (run with --help for usage)"
            ;;
    esac
done

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$ROOT_DIR/$BUILD_DIR"
fi

cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel
