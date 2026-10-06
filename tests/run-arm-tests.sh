#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-.}/cpufetch-arm-tests.XXXXXX")
trap 'rm -f "$test_dir/check"; rmdir "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -DARCH_ARM -std=c99 -Wall -Wextra -Wno-unused-parameter \
  -fsanitize=undefined -ffunction-sections -fdata-sections -I. \
  -Wl,--gc-sections -Wl,--wrap=getauxval -Wl,--wrap=prctl \
  tests/arm-capabilities.c src/arm/midr.c src/arm/uarch.c \
  src/common/cpu.c src/common/global.c src/common/args.c -o "$test_dir/check"
"$test_dir/check"
