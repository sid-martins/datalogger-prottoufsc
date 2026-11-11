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

## 3. Mapeamento Oficial de Pinos e IOs (`board_pins.h` - Esquemático `CPU.SchDoc`)
- Todos os 26 pinos do **ESP32-S3-DEVKITC-1-N8R2 (`U3`)** e suas respectivas tags de identificação ficam centralizados em `components/board_config/include/board_pins.h`:
  - **ADC1 - Motor (`Measurement_Engine.SchDoc` - `U8`/`U9` MCP6022):**
    - `GPIO 1` (`ADC1_CH0`): `PIN_ADC_OIL_PRESSURE` (`"OilPressure"`)
    - `GPIO 2` (`ADC1_CH1`): `PIN_ADC_OIL_TEMP` (`"OilTemp"`)
    - `GPIO 3` (`ADC1_CH2`): `PIN_ADC_FUEL_PRESSURE` (`"FuelPressure"`)
    - `GPIO 4` (`ADC1_CH3`): `PIN_ADC_LAMBDA` (`"Lambda"`)
  - **ADC1 - Sensores 5V -> 3.3V (`Measurement_5to3v3.SchDoc` - `U5`/`U6` MCP6022):**
    - `GPIO 5` a `GPIO 8` (`ADC1_CH4` a `ADC1_CH7`): `PIN_ADC_5TO3V3_1` a `PIN_ADC_5TO3V3_4` (`"ADC5to3v3_1"` a `"ADC5to3v3_4"`)
  - **ADC1 - Sensores 12V -> 3.3V (`Measurement_12to3v3.SchDoc` - `U7` MCP6022):**
    - `GPIO 9` e `GPIO 10` (`ADC1_CH8` e `ADC1_CH9`): `PIN_ADC_12TO3V3_1` e `PIN_ADC_12TO3V3_2` (`"ADC12to3v3_1"` e `"ADC12to3v3_2"`)
  - **I2C - RTC Externo `DS3231SN#` (`RTC.SchDoc` - `B8`):**
    - `GPIO 11`: `PIN_I2C_SDA` (`"SDA_I2C_1"`)
    - `GPIO 12`: `PIN_I2C_SCL` (`"SCL_I2C_1"`)
  - **UART - GPS `Neo-8M` (`CPU.SchDoc` - `J5`):**
    - `GPIO 13`: `PIN_GPS_RX` (`"GPS_RX"` - conectado ao TX do GPS)
    - `GPIO 47`: `PIN_GPS_TX` (`"GPS_TX"` - conectado ao RX do GPS)
  - **CAN / TWAI - Transceptor `SN65HVD230DR` (`Communication.SchDoc` - `U2`):**
    - `GPIO 15`: `PIN_CAN_TX` (`"CAN_TX"`)
    - `GPIO 16`: `PIN_CAN_RX` (`"CAN_RX"`)
  - **Cartão MicroSD (`CPU.SchDoc` - `J4` - SPI / SDMMC 4-bits):**
    - `GPIO 14`: `PIN_SD_MISO` / `DAT0` (`"SD_MISO"`)
    - `GPIO 17`: `PIN_SD_CS` / `DAT3` (`"SD_CS"`)
    - `GPIO 18`: `PIN_SD_MOSI` / `CMD` (`"SD_MOSI"`)
    - `GPIO 21`: `PIN_SD_SCK` / `CLK` (`"SD_SCK"`)
    - `GPIO 40`: `PIN_SD_DAT1` (`"SD_DAT1"`)
    - `GPIO 41`: `PIN_SD_DAT2` (`"SD_DAT2"`)
  - **Interface (Botões e LEDs Verdes - `CPU.SchDoc`):**
    - `GPIO 35`: `PIN_BTN_FREE_SW1` (`"Free_SW1"` - Botão `SW2`, ativo baixo)
    - `GPIO 36`: `PIN_BTN_FREE_SW2` (`"Free_SW2"` - Botão `SW1`, ativo baixo)
    - `GPIO 37`: `PIN_LED_ACQUIRING` (`"LED_Acquiring"` - LED `D2`, ativo alto)
    - `GPIO 39`: `PIN_LED_WORKING` (`"LED_Working"` - LED `D3`, ativo alto)

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
- [x] Arquivo central de mapeamento de portas e tags do ESP32-S3 (`components/board_config/include/board_pins.h`) atualizado com os 26 sinais reais do esquemático elétrico (`Main.SchDoc` / `CPU.SchDoc`) e validado via TDD (`test_board_pins.cpp`).
- [ ] Implementação dos módulos de periféricos reais do Datalogger via TDD (ADC 10 canais, RTC DS3231 I2C, CAN/TWAI SN65HVD230, GPS Neo-8M UART, MicroSD e UI de Botões/LEDs).

> **Regra de Manutenção:** Sempre que uma etapa importante for concluída ou um novo componente arquitetural for adicionado ao projeto, atualize a seção **5. Estado Atual do Projeto** deste arquivo `GEMINI.md` e o `README.md`.

