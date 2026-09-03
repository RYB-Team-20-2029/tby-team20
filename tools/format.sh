#!/usr/bin/env bash
# Purpose: apply .clang-format in place to every tracked C source and header.
# Owning module: management (mgmt/).
#
# Usage:  ./tools/format.sh          format in place
#         ./tools/format.sh --check  fail if anything is unformatted (no writes)
set -euo pipefail

cd "$(dirname "$0")/.."

if [ "${1:-}" = "--check" ]; then
    git ls-files '*.c' '*.h' | xargs clang-format --dry-run --Werror
else
    git ls-files '*.c' '*.h' | xargs clang-format -i
fi
