$ErrorActionPreference = "Stop"

Write-Host "Running Cppcheck Static Analysis..." -ForegroundColor Cyan

if (Get-Command cppcheck -ErrorAction SilentlyContinue) {
    # --enable=all might be too noisy, start with warning,performance,portability
    # --suppress=missingIncludeSystem is often needed if includes aren't perfect
    # -i build/ -i third_party/ to ignore these
    cppcheck --enable=warning,performance,portability --suppress=missingIncludeSystem --inline-suppr -I src/ -i build/ -i third_party/ -i tests/ --error-exitcode=1 src/
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Cppcheck passed!" -ForegroundColor Green
    } else {
        Write-Host "Cppcheck found issues." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Cppcheck not found in PATH. Please install it to use this feature." -ForegroundColor Yellow
    Write-Host "Download: https://cppcheck.sourceforge.io/"
}
