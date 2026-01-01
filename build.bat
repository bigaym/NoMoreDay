@echo off
if not exist build mkdir build
cd build
cmake ..
cmake --build . --config Release -j 16
cd ..
