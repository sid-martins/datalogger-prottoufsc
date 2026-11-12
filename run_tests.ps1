$ErrorActionPreference = "Stop"

# Garante que CMake, Ninja e MinGW/LLVM instalados via Winget estejam no PATH da sessão atual
$extraPaths = @(
    "C:\Program Files\CMake\bin"
)

$wingetRoots = @(
    "$env:LOCALAPPDATA\Microsoft\WinGet\Packages",
    "$env:USERPROFILE\AppData\Local\Microsoft\WinGet\Packages"
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

foreach ($root in $wingetRoots) {
    Get-ChildItem -Path $root -Recurse -Depth 3 -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -in @("g++.exe", "ninja.exe") } |
        ForEach-Object { $extraPaths += $_.DirectoryName }
}

foreach ($p in ($extraPaths | Select-Object -Unique)) {
    if ((Test-Path $p) -and ($env:PATH -notlike "*$p*")) {
        $env:PATH = "$p;$env:PATH"
    }
}

if (-not (Test-Path "build_tests/build.ninja")) {
    Write-Host "==> Configurando build de testes TDD (Google Test) no Host..." -ForegroundColor Cyan
    cmake -S tests -B build_tests -G Ninja
    if ($LASTEXITCODE -ne 0) { throw "Falha na configuracao do CMake." }
}

Write-Host "==> Compilando testes unitarios..." -ForegroundColor Cyan
cmake --build build_tests
if ($LASTEXITCODE -ne 0) { throw "Falha na compilacao dos testes." }

Write-Host "==> Executando suite de testes (CTest + GoogleTest)..." -ForegroundColor Green
ctest --test-dir build_tests --output-on-failure --verbose
if ($LASTEXITCODE -ne 0) { throw "Um ou mais testes falharam!" }
