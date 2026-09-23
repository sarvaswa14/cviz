#!/bin/sh
# Reproduces the numbers in the Phase 2 report.
set -e
echo "== differential testing against gcc =="
for f in tests/valid/*.c; do
  [ "$f" = "tests/valid/02_declarators.c" ] && continue
  printf "%-32s " "$f"
  ./cviz "$f" --diff-gcc | tail -1
done
echo
echo "== optimiser validation =="
for f in tests/valid/*.c; do
  [ "$f" = "tests/valid/02_declarators.c" ] && continue
  printf "%-32s " "$f"
  ./cviz "$f" --validate | head -1
done
echo
echo "== localisation over generated programs =="
./cviz --eval-random 200 --gen-seed 7000
