#!/usr/bin/env bash
set -euo pipefail

# Find C++ sources and run clang-format -i
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
find "$ROOT_DIR/src" "$ROOT_DIR/tests" -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.h" -o -name "*.hpp" \) -print0 | xargs -0 -n 1 clang-format -style=file -i || true

echo "Formatting complete"
