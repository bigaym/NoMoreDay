$ErrorActionPreference = "Stop"

$VsPath = "C:\Program Files\Microsoft Visual Studio\18\Community"
$ClangFormatPath = "$VsPath\VC\Tools\Llvm\x64\bin\clang-format.exe"

if (-not (Test-Path $ClangFormatPath)) {
    Write-Host "Error: clang-format.exe not found at $ClangFormatPath" -ForegroundColor Red
    exit 1
}

Write-Host "Using clang-format from Visual Studio..." -ForegroundColor Cyan

# Find all C++ source files
$files = Get-ChildItem -Path src, tests -Include *.cpp, *.hpp, *.h, *.c -Recurse

foreach ($file in $files) {
    Write-Host "Formatting $($file.FullName)..."
    & $ClangFormatPath -i -style=file $file.FullName
}

Write-Host "Formatting complete." -ForegroundColor Green
