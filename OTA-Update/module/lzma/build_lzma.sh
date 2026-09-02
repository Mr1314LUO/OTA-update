#!/usr/bin/env bash

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR="${SCRIPT_DIR}/../build"
SDK_DIR="${SCRIPT_DIR}/sdk"
UNZIP_DIR="${SCRIPT_DIR}/unzip"
ZIP_DIR="${SCRIPT_DIR}/zip"
FLOW_UNZIP_DIR="${SCRIPT_DIR}/flow-unzip"

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -Wextra -std=c11 -g -fPIC}
CPPFLAGS="-I${SCRIPT_DIR}/../include -I${SDK_DIR}"

mkdir -p "${BUILD_DIR}"

compile() {
    local src=$1
    local obj="${BUILD_DIR}/$(basename "${src%.c}.o")"
    echo "Compiling: $src -> $obj"
    ${CC} ${CPPFLAGS} ${CFLAGS} -c "$src" -o "$obj"
}

echo "=== Building LZMA modules ==="

for src in \
    "${SDK_DIR}/Alloc.c" \
    "${SDK_DIR}/LzmaDec.c" \
    "${SDK_DIR}/7zCrc.c" \
    "${SDK_DIR}/7zCrcOpt.c" \
    "${UNZIP_DIR}/unzip.c" \
    "${ZIP_DIR}/zip.c" \
    "${FLOW_UNZIP_DIR}/unzip_stream.c"; do
    compile "$src"
done

echo "=== LZMA build completed ==="
echo "Object files in: ${BUILD_DIR}"