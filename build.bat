@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake --preset x64-debug -S MapGenerator
cmake --build MapGenerator/out/build/x64-debug
