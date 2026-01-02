@echo off
if not exist build mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --config RelWithDebInfo -j 16

@REM echo [Build] Generating PDBs...
@REM cd bin
@REM call ..\..\scripts\gen_pdb.bat
@REM cd ..

@REM cd ..
