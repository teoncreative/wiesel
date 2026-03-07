#!/usr/bin/env bash
# Check Google C++ naming conventions in Wiesel engine code using clang-tidy.
#
# Usage:
#   ./scripts/check_naming.sh              # Check all engine headers
#   ./scripts/check_naming.sh <file.hpp>   # Check a single file
#
# Requires: clang-tidy, compile_commands.json in cmake-build-debug/
#
# Note: Checks header files (.hpp/.h) since MSVC PCH in compile_commands.json
# is incompatible with clang-tidy on .cpp files.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/cmake-build-debug"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"

# Try common Windows LLVM path if clang-tidy not in PATH
if ! command -v "$CLANG_TIDY" &>/dev/null; then
  if [ -f "/c/Program Files/LLVM/bin/clang-tidy.exe" ]; then
    CLANG_TIDY="/c/Program Files/LLVM/bin/clang-tidy.exe"
  else
    echo "Error: clang-tidy not found. Install LLVM or set CLANG_TIDY env var." >&2
    exit 1
  fi
fi

if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
  echo "Error: compile_commands.json not found at $BUILD_DIR/" >&2
  echo "Run CMake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON first." >&2
  exit 1
fi

FILES=("$@")

# If no files specified, collect all engine header files
if [ ${#FILES[@]} -eq 0 ]; then
  mapfile -t FILES < <(find "$PROJECT_ROOT/wiesel/include" "$PROJECT_ROOT/wiesel/src" \
    \( -name '*.hpp' -o -name '*.h' \) | sort)
fi

echo "=== Wiesel Engine Naming Convention Check (Google C++ Style) ==="
echo "Checking ${#FILES[@]} file(s)..."
echo ""

TOTAL=0
FILE_COUNT=0

for f in "${FILES[@]}"; do
  OUTPUT=$("$CLANG_TIDY" -p "$BUILD_DIR" \
    --checks='-*,readability-identifier-naming' \
    "$f" 2>/dev/null || true)

  WARNINGS=$(echo "$OUTPUT" | grep 'warning:.*readability-identifier-naming' || true)
  COUNT=$(echo "$WARNINGS" | grep -c 'warning:' || true)

  if [ "$COUNT" -gt 0 ]; then
    FILE_COUNT=$((FILE_COUNT + 1))
    TOTAL=$((TOTAL + COUNT))
    echo "$WARNINGS"
    echo ""
  fi
done

echo "=== Summary: $TOTAL violation(s) across $FILE_COUNT file(s) ==="
