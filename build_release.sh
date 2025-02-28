#!/bin/sh

cmake --preset release
cmake --build --preset release
./out/build/release/main