#!/usr/bin/env bash
# Compila el núcleo (binjgb + wrapper) a WebAssembly.
# Requiere tener activado el entorno de emsdk (source ./emsdk_env.sh)
set -euo pipefail

cd "$(dirname "$0")"

emcc src/wasm_gbc.c \
	src/binjgb/common.c \
	src/binjgb/memory.c \
	src/binjgb/joypad.c \
	src/binjgb/emulator.c \
	-Isrc \
	-O3 \
	-s WASM=1 \
	-s MODULARIZE=1 \
	-s EXPORT_NAME="createGBModule" \
	-s ENVIRONMENT=web \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s NO_FILESYSTEM=1 \
	-s EXPORTED_RUNTIME_METHODS='["HEAPU8"]' \
	--post-js src/post.js \
	-o web/gb.js

echo "OK -> web/gb.js + web/gb.wasm"
