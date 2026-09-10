@echo off
g++ -std=c++17 -Wall -Wextra -Iinclude -o cviz.exe src/source.cpp src/token.cpp src/ast.cpp src/lexer.cpp src/parser.cpp src/sema.cpp src/trace.cpp src/main.cpp
if %errorlevel% neq 0 (echo BUILD FAILED & exit /b 1)
echo build ok