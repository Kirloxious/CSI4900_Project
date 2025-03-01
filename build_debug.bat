@echo off
cmake --preset=debug-windows
cmake --build --preset=debug-windows
start "" out/build/debug/main.exe
