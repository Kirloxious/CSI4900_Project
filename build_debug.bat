@echo off
cmake --preset debug
cmake --build --preset debug
start "" out/build/debug/main.exe