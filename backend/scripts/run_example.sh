#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
OUTPUT_DIR="$ROOT_DIR/examples/generated"

mkdir -p "$OUTPUT_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release

"$BUILD_DIR/topoopt" \
  --config "$ROOT_DIR/examples/input/wuli.json" \
  --output-dir "$OUTPUT_DIR" \
  --pop 20 \
  --gen 5 \
  --mutation 0.01 \
  --seed 42

echo "Generated:"
echo "  $OUTPUT_DIR/luoji.json"
echo "  $OUTPUT_DIR/youhua1.json"
echo "  $OUTPUT_DIR/youhua2.json"
