@echo off
setlocal

:: 设置 cv2pdb.exe 的绝对路径
set "CV2PDB=F:\NoMoreDay\third_party\cv2pdb-0.54\cv2pdb.exe"

if not exist "%CV2PDB%" (
    echo [Error] cv2pdb.exe not found at: "%CV2PDB%"
    echo Please check if the path is correct.
    exit /b 1
)

echo [Info] Converting EXE and DLL files to PDB in: "%CD%"

for %%f in (*.exe *.dll) do (
    echo [cv2pdb] Processing %%f...
    "%CV2PDB%" "%%f"
)

echo [Info] PDB generation finished.