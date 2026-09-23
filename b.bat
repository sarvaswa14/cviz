@echo off
g++ -std=c++17 -Wall -Wextra -Iinclude -o cviz.exe src/source.cpp src/token.cpp src/ast.cpp src/lexer.cpp src/parser.cpp src/sema.cpp src/ir.cpp src/irgen.cpp src/cfg.cpp src/opt.cpp src/interp.cpp src/validate.cpp src/gen.cpp src/trace.cpp src/main.cpp
if %errorlevel% neq 0 (echo BUILD FAILED & exit /b 1)
echo build ok
