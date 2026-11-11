# Contexto e Diretrizes do Projeto: Datalogger ESP32-S3

Este arquivo (`GEMINI.md`) funciona como a memória persistente e guia arquitetural do projeto para o assistente de IA (equivalente ao `CLAUDE.md`). Ele é carregado automaticamente em todas as sessões dentro deste repositório.

---

## 1. Visão Geral do Projeto
- **Produto:** Firmware de um Datalogger embarcado baseado no microcontrolador **ESP32-S3** (Kit de desenvolvimento padrão DevKitC-1).
- **Framework Principal:** **ESP-IDF nativo** com **FreeRTOS** e **C++17**.
- **Metodologia de Desenvolvimento:** **TDD (Test-Driven Development)** utilizando **Google Test (`gtest` + `gmock` v1.14.0)** compilado e executado localmente no Host (Windows x64) via CMake + Ninja + LLVM-MinGW (`Clang/G++ 23.1.4`).

---

## 2. Arquitetura e Regras de Projeto (TDD & FreeRTOS)

1. **Desacoplamento de Hardware (HAL / Injeção de Dependência):**
   - Toda regra de negócio, filtragem, formatação de dados, máquina de estados e lógica das tasks do FreeRTOS deve ficar em `components/` e depender exclusivamente de interfaces puras definidas em `components/hal_interfaces/include/` (ex: `ISensor`, `IStorage`, `IRtosQueue<T>`).
   - **Nunca** acople chamadas diretas de periféricos do ESP-IDF (`driver/i2c.h`, `driver/adc.h`, `esp_vfs_fat.h`) dentro da lógica de negócio (`datalogger_core`, `ring_buffer`, etc.). Crie interfaces em `hal_interfaces`, mocks em `tests/mocks/` e implementações reais do ESP32-S3 em componentes de driver específicos ou em `main/`.

2. **Fluxo Obrigatório de TDD (Red -> Green -> Refactor):**
   - Ao criar ou alterar qualquer funcionalidade do Datalogger:
     1. Escreva ou atualize primeiro o teste unitário correspondente em `tests/`.
     2. Execute `.\run_tests.ps1` para validar o teste no Host.
     3. Implemente o código de produção em `components/` mantendo `0 warnings` (`-Wall -Wextra -Wpedantic`).
     4. Caso adicione um novo componente em `components/`, registre-o também em:
        - `main/CMakeLists.txt` (`REQUIRES`)
        - `tests/CMakeLists.txt` (`add_library` / `target_include_directories`)
        - `.vscode/c_cpp_properties.json` e `.clangd` (para manter o IntelliSense sem erros)

3. **Boas Práticas de Sistemas Embarcados (ESP32-S3 & FreeRTOS):**
   - Evite alocação dinâmica de memória (heap / `new` / `malloc`) dentro de loops de amostragem em tempo real; prefira estruturas estáticas como `SampleRingBuffer<N>`.
   - Mantenha a separação Dual-Core do ESP32-S3:
     - **Core 1 (APP_CPU):** Tasks de aquisição em tempo real e leitura determinística de sensores (`vTaskDelayUntil`).
     - **Core 0 (PRO_CPU):** Tasks de escrita em armazenamento (SD Card / Flash FATFS), comunicação serial/Wi-Fi e serviços de sistema.

---

## 3. Mapeamento de Pinos e IOs (`board_pins.h`)
- Todos os pinos e suas respectivas tags de identificação ficam centralizados em `components/board_config/include/board_pins.h`.
- Sempre utilize as constantes `PIN_*` (para configurar o `gpio_num_t`) e `TAG_*` (para logs e cabeçalhos de arquivos CSV/JSON).
- **Restrições de Pinos do ESP32-S3 que devem ser respeitadas:**
  - **Proibidos (Flash/PSRAM SPI):** `GPIO 26` a `GPIO 32` (e `GPIO 33` a `GPIO 37` em módulos Octal PSRAM).
  - **USB Nativo (D-/D+ JTAG):** `GPIO 19` e `GPIO 20`.
  - **UART0 Console:** `GPIO 43` (TX) e `GPIO 44` (RX).
  - **Strapping Pins (cuidado no boot):** `GPIO 0`, `GPIO 3`, `GPIO 45`, `GPIO 46`.
  - **ADC:** Priorizar **ADC1** (`GPIO 1` a `GPIO 10`), pois o ADC2 conflita com o rádio Wi-Fi.

---

## 4. Comandos Úteis do Ambiente

- **Executar todos os testes unitários no Windows (Host):**
  ```powershell
  .\run_tests.ps1
  ```
- **Verificar sintaxe do `main/main.cpp` com os stubs do ESP-IDF:**
  ```powershell
  & "C:\Users\gusta\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\g++.exe" -fsyntax-only -std=c++17 -Wall -Wextra -Imain -Icomponents/board_config/include -Icomponents/hal_interfaces/include -Icomponents/ring_buffer/include -Icomponents/datalogger_core/include -Itests/esp_idf_stubs/include main/main.cpp
  ```
- **Compilar e gravar no ESP32-S3 (via terminal ESP-IDF):**
  ```powershell
  idf.py set-target esp32s3
  idf.py build
  idf.py -p COMx flash monitor
  ```

---

## 5. Estado Atual do Projeto (Histórico de Progresso)
- [x] Estrutura base do projeto ESP-IDF (`main/`, `components/`, `sdkconfig.defaults`, `partitions.csv`).
- [x] Ambiente de testes Host no Windows configurado com CMake, Ninja, LLVM-MinGW e Google Test v1.14.0 (`tests/` e `run_tests.ps1`).
- [x] Interfaces HAL (`ISensor`, `IStorage`, `IRtosQueue`) e Mocks (`MockSensor`, `MockStorage`, `FakeRtosQueue`).
- [x] Componentes iniciais testados via TDD: `SampleRingBuffer` e `DataloggerService`.
- [x] Configuração de IntelliSense do VS Code (`.vscode/c_cpp_properties.json`, `.clangd`) e headers stub do ESP-IDF (`tests/esp_idf_stubs/`).
- [x] Arquivo central de mapeamento de portas e tags do ESP32-S3 (`components/board_config/include/board_pins.h`).
- [ ] Definição final dos sensores e periféricos reais do Datalogger e implementação dos seus respectivos módulos via TDD.

> **Regra de Manutenção:** Sempre que uma etapa importante for concluída ou um novo componente arquitetural for adicionado ao projeto, atualize a seção **5. Estado Atual do Projeto** deste arquivo `GEMINI.md` e o `README.md`.

