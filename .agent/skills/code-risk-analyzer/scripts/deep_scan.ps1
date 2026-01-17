$ErrorActionPreference = "Stop"

Write-Host "Running Deep Code Analysis (Cppcheck)..." -ForegroundColor Cyan

if (Get-Command cppcheck -ErrorAction SilentlyContinue) {
    # --enable=all for deep scan.
    # --inconclusive to show potential but unsure issues.
    cppcheck --enable=style,performance,portability,warning --inconclusive --inline-suppr -I src/ -i build/ -i third_party/ -i tests/ --force --error-exitcode=0 src/
    
    Write-Host "Scan complete." -ForegroundColor Green
} else {
    Write-Host "Cppcheck not found in PATH." -ForegroundColor Red
}
