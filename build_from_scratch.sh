#!/usr/bin/env bash

set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: ./build_from_scratch.sh [options]

Configure and build MassivePolyPusher and its dependencies from scratch.

Options:
  --with-lfs           Download this repository's Git LFS files.
  --config CONFIG      CMake build configuration (default: Release).
  --build-dir DIR      Build directory, relative to the repository root unless
                       absolute (default: build).
  -h, --help           Show this help.

Environment:
  CC, CXX               Select the C and C++ compilers during configuration.
  CMAKE_GENERATOR       Select a CMake generator.
  CMAKE_BUILD_PARALLEL_LEVEL
                        Limit the number of parallel build jobs.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

WITH_LFS=false
CONFIG=Release
BUILD_DIR=build

while (($#)); do
    case "$1" in
        --with-lfs)
            WITH_LFS=true
            shift
            ;;
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

command -v git >/dev/null 2>&1 || fail "Git is required but was not found on PATH"
command -v cmake >/dev/null 2>&1 || fail "CMake is required but was not found on PATH"

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
cd "$ROOT_DIR"

git rev-parse --is-inside-work-tree >/dev/null 2>&1 || fail "$ROOT_DIR is not a Git checkout"
[[ -f CMakeLists.txt && -f .gitmodules ]] || \
    fail "run this script from the MassivePolyPusher checkout"

if [[ "$WITH_LFS" == true ]] && ! git lfs version >/dev/null 2>&1; then
    cat >&2 <<'EOF'
error: --with-lfs requires Git LFS, but 'git lfs' is not installed.

Install Git LFS, then run this script again:
  Ubuntu/Debian: sudo apt update && sudo apt install git-lfs
  Fedora:        sudo dnf install git-lfs
  macOS:         brew install git-lfs
  Windows:       winget install GitHub.GitLFS

For other systems, see https://git-lfs.com/.
EOF
    exit 1
fi

printf 'Synchronizing and checking out all submodules...\n'
git submodule sync --recursive
git submodule update --init --recursive

SUBMODULE_STATUS=$(git submodule status --recursive)
printf '%s\n' "$SUBMODULE_STATUS"
if grep -Eq '^[+-U]' <<<"$SUBMODULE_STATUS"; then
    fail "one or more submodules are not checked out at the commits recorded by their parent"
fi

if [[ "$WITH_LFS" == true ]]; then
    printf 'Downloading MassivePolyPusher Git LFS files...\n'
    git lfs install --local
    git lfs pull
fi

case "$BUILD_DIR" in
    ''|.|..|*/.|*/..|../*|*/../*) fail "refusing to remove unsafe build directory: $BUILD_DIR" ;;
esac
if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$ROOT_DIR/$BUILD_DIR"
fi

[[ "$BUILD_DIR" != / && "$BUILD_DIR" != "$ROOT_DIR" ]] || \
    fail "refusing to remove unsafe build directory: $BUILD_DIR"
case "$ROOT_DIR/" in
    "$BUILD_DIR/"*) fail "refusing to remove a directory containing this checkout: $BUILD_DIR" ;;
esac

# CMake deliberately places final artifacts under source/build even when its
# binary tree is elsewhere, so both locations are build output.
OUTPUT_DIR="$ROOT_DIR/build"
printf 'Removing previous build output...\n'
rm -rf -- "$BUILD_DIR"
if [[ "$OUTPUT_DIR" != "$BUILD_DIR" ]]; then
    rm -rf -- "$OUTPUT_DIR"
fi

printf 'Configuring %s build in %s...\n' "$CONFIG" "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"

printf 'Building MassivePolyPusher and dependencies...\n'
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel

printf 'Build completed successfully.\n'
printf 'Binaries: %s/bin/%s\n' "$ROOT_DIR/build" "$CONFIG"
