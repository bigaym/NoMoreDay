@echo off
setlocal

:: 设置 cv2pdb.exe 的绝对路径
set "CV2PDB=F:\NoMoreDay\third_party\cv2pdb-0.54\cv2pdb.exe"
set "TARGET_DIR=F:\NoMoreDay\build\bin"

if not exist "%CV2PDB%" (
    echo [Error] cv2pdb.exe not found at: "%CV2PDB%"
    echo Please check if the path is correct.
    exit /b 1
)

if not exist "%TARGET_DIR%" (
    echo [Error] Target directory not found: "%TARGET_DIR%"
    exit /b 1
)

pushd "%TARGET_DIR%"
echo [Info] Converting EXE and DLL files to PDB in: "%TARGET_DIR%"

for %%f in (*.exe *.dll) do (
    echo [cv2pdb] Processing %%f...
    "%CV2PDB%" "%%f"
)

popd
echo [Info] PDB generation finished.