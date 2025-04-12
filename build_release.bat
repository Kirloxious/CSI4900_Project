@echo off
cmake --preset=release-windows
cmake --build --preset=release-windows
start cmd /k ".\out\build\release\main.exe" 
