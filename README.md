# Datalogger ESP32-S3 (ESP-IDF + FreeRTOS + TDD com Google Test)

Ambiente de desenvolvimento de firmware para o **Datalogger Automotivo com ESP32-S3**, utilizando **ESP-IDF nativo (C++17)**, **FreeRTOS (Dual-Core)** e arquitetura orientada a **TDD (Test-Driven Development)** com **Google Test (GTest + GMock)** rodando localmente no computador (Host Windows).

---

## Estrutura de Pastas do Projeto

```text
datalogger/
├── CMakeLists.txt                  # CMake híbrido: ESP-IDF (via idf.py) ou Testes no Host
├── GEMINI.md                       # Arquivo de contexto e regras do projeto para o Agente IA (equivalente ao CLAUDE.md)
├── README.md                       # Documentação geral do projeto
├── sdkconfig.defaults              # Configurações padrão do ESP32-S3 (Flash 8MB, FreeRTOS 1000Hz)
├── partitions.csv                  # Tabela de partições customizada (NVS, Factory App, Storage FAT)
├── run_tests.ps1                   # Script PowerShell para compilar e executar os testes TDD no Host
├── .clangd                         # Configuração de flags para servidor de linguagem Clangd
├── .vscode/
│   ├── c_cpp_properties.json       # Configuração do IntelliSense (ESP32-S3 + Host TDD)
│   └── settings.json               # Associações de arquivos C/C++ no VS Code
├── main/
│   ├── CMakeLists.txt              # Registro do componente principal no ESP-IDF
│   └── main.cpp                    # Entrypoint app_main() + Tasks do FreeRTOS (Core 0 e Core 1)
├── components/
│   ├── board_config/               # Mapeamento oficial do esquemático (GPIOs x Constantes PIN_* e TAG_*)
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       └── board_pins.h        # 26 sinais do esquemático (ADC1, CAN, RTC I2C, GPS UART, MicroSD, Botões/LEDs)
│   ├── hal_interfaces/             # Contratos/Interfaces C++ puras para desacoplamento de hardware (TDD)
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       ├── i_can_driver.hpp    # Interface ICanDriver, CanFrame, CanConfig (SN65HVD230DR / TWAI)
│   │       ├── i_storage.hpp       # Interface IStorage (mount, append_line, sync)
│   │       └── i_rtos_queue.hpp    # Interface IRtosQueue<T> para filas do FreeRTOS
│   ├── ring_buffer/                # Buffer circular estático genérico RingBuffer<T, Capacity> (sem heap)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── ring_buffer.hpp
│   │   └── src/
│   │       └── ring_buffer.cpp
│   └── can_bus/                    # Módulo de comunicação CAN (Transceptor SN65HVD230DR - Communication.SchDoc)
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── can_controller.hpp  # Lógica de controle CAN, validação 11/29-bit, fila RX e auto-recovery Bus-Off
│       │   └── esp32_twai_driver.hpp # Driver concreto do periférico TWAI do ESP32-S3
│       └── src/
│           ├── can_controller.cpp
│           └── esp32_twai_driver.cpp
└── tests/
    ├── CMakeLists.txt              # Build isolado de testes no Host + GoogleTest v1.14.0
    ├── esp_idf_stubs/              # Headers stub do ESP-IDF/FreeRTOS para IntelliSense e compilação Host
    │   └── include/
    │       ├── esp_err.h
    │       ├── esp_log.h
    │       ├── esp_timer.h
    │       ├── driver/ (gpio.h, twai.h)
    │       └── freertos/ (FreeRTOS.h, task.h, queue.h)
    ├── mocks/                      # Mocks GMock (MockCanDriver, MockStorage) e FakeRtosQueue
    ├── test_board_pins.cpp         # Testes unitários validando os 26 pinos e tags do esquemático
    ├── test_ring_buffer.cpp        # Testes unitários do RingBuffer<T, Capacity>
    └── test_can_controller.cpp     # Testes unitários do CanController (Standard 11-bit, Extended 29-bit, 500kbps, Bus-Off)
```

---

## O Que Já Foi Implementado

1. **Ambiente de Compilação e Testes no Windows (Host TDD)**:
   - Toolchain C/C++ (**LLVM-MinGW Clang/G++ 23.1.4 UCRT**), **CMake 4.4.4** e **Ninja 1.13.2** configurados.
   - **Google Test & Google Mock (v1.14.0)** integrado em `tests/external/googletest` (funcionando 100% offline).
   - 18 testes unitários implementados e passando (`BoardPinsTest`, `RingBufferTest` e `CanControllerTest`).
2. **Mapeamento Oficial de Portas e IOs do Esquemático (`components/board_config/include/board_pins.h`)**:
   - Mapeamento completo dos **26 sinais do esquemático** (`Main.SchDoc` / `CPU.SchDoc` - `U3: ESP32-S3-DEVKITC-1-N8R2`).
   - **10 Canais Analógicos no ADC1 (`GPIO 1` a `GPIO 10`)**: `OilPressure`, `OilTemp`, `FuelPressure`, `Lambda`, `ADC5to3v3_1..4` e `ADC12to3v3_1..2`.
   - **Periféricos e UI**: RTC `DS3231SN#` via I2C (`GPIO 11/12`), GPS `Neo-8M` via UART (`GPIO 13/47`), CAN/TWAI `SN65HVD230DR` (`GPIO 15/16`), Cartão MicroSD SPI/SDMMC 4-bit (`GPIO 14, 17, 18, 21, 40, 41`), Botões `Free_SW1/SW2` (`GPIO 35/36`) e LEDs `LED_Acquiring/Working` (`GPIO 37/39`).
3. **Módulo de Comunicação CAN (`components/can_bus` - `Communication.SchDoc`)**:
   - Configurado para o transceptor **`SN65HVD230DR` (`U2`)** nos pinos `PIN_CAN_TX` (`GPIO 15`) e `PIN_CAN_RX` (`GPIO 16`) com baudrate padrão de **500 kbps**.
   - Suporte completo a frames **Standard (11-bit)** e **Extended (29-bit)**, validação de DLC (`0..8`), enfileiramento direto na fila do FreeRTOS (`receive_to_queue`) e recuperação automática de **Bus-Off** (`check_and_recover_bus`).

---

## Como Rodar os Testes Unitários (TDD no PC)

Na raiz do projeto, execute no PowerShell:

```powershell
.\run_tests.ps1
```

Para filtrar apenas os testes do módulo CAN:

```powershell
.\build_tests\datalogger_unit_tests.exe --gtest_filter=*CanController*
```

---

## Como Compilar e Gravar no ESP32-S3 (ESP-IDF)

Com o terminal do **ESP-IDF 5.x** aberto na pasta do projeto:

```powershell
# 1. Definir o target para ESP32-S3 (apenas na primeira vez)
idf.py set-target esp32s3

# 2. Compilar o binário (.bin)
idf.py build

# 3. Gravar na placa (substitua COM4 pela porta do seu kit) e abrir o monitor serial
idf.py -p COM4 flash monitor
```
*(Pressione `Ctrl + ]` para sair do monitor serial).*
