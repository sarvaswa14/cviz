@echo off
REM Reproduces the numbers in the Phase 2 report.
for %%f in (tests\valid\*.c) do (
  echo --- %%f
  cviz.exe tests\valid\%~nxf --diff-gcc
)
echo.
cviz.exe --eval-random 200 --gen-seed 7000
