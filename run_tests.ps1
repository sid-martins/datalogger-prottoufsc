$ErrorActionPreference = "Stop"

# 1. Garante que CMake, Ninja e MinGW/LLVM instalados via Winget estejam no PATH da sessão atual
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

# 2. Configura e compila a suíte SIL (Google Test + Google Mock)
if (-not (Test-Path "build_tests/build.ninja")) {
    Write-Host "==> [1/4] Configurando build de testes SIL (Google Test) no Host..." -ForegroundColor Cyan
    cmake -S tests -B build_tests -G Ninja
    if ($LASTEXITCODE -ne 0) { throw "Falha na configuracao do CMake." }
}

Write-Host "==> [2/4] Compilando camadas Motorsport (BSW + LLD + SIL Tests)..." -ForegroundColor Cyan
cmake --build build_tests
if ($LASTEXITCODE -ne 0) { throw "Falha na compilacao dos testes." }

# 3. Executa os testes unitários SIL
Write-Host "==> [3/4] Executando suite de testes SIL (CTest + GoogleTest)..." -ForegroundColor Green
ctest --test-dir build_tests --output-on-failure --verbose
if ($LASTEXITCODE -ne 0) { throw "Um ou mais testes SIL falharam!" }

# 4. Quality Gate: Verifica sintaxe da camada ASW (main/main.cpp) e limite de linhas do GEMINI.md
Write-Host "==> [4/4] Validando Quality Gate (ASW main.cpp syntax + GEMINI.md <= 200 linhas)..." -ForegroundColor Cyan
g++ -fsyntax-only -std=c++17 -Wall -Wextra -Wpedantic -Werror `
    -Imain `
    -Icomponents/bsp/include `
    -Icomponents/osal/include `
    -Icomponents/datalogger_hal/include `
    -Icomponents/lld/include `
    -Icomponents/bsw/include `
    -Itests/esp_idf_stubs/include `
    main/main.cpp
if ($LASTEXITCODE -ne 0) { throw "Falha na verificacao de sintaxe de main/main.cpp!" }

if (Test-Path "GEMINI.md") {
    $geminiLines = (Get-Content "GEMINI.md").Count
    if ($geminiLines -gt 200) {
        throw "Quality Gate falhou: GEMINI.md possui $geminiLines linhas (limite maximo: 200)."
    }
    Write-Host "    [OK] main/main.cpp (0 warnings) | GEMINI.md ($geminiLines/200 linhas)" -ForegroundColor Green
}
