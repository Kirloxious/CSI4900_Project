#!/bin/sh

cmake --preset=debug-macos
cmake --build --preset=debug-macos
./out/build/debug/main