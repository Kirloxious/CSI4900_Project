@echo off
cmake --preset=debug-windows
cmake --build --preset=debug-windows
start cmd /k ".\out\build\debug\main.exe" 