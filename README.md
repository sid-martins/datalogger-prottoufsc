# Datalogger ESP32-S3 (ESP-IDF + FreeRTOS + TDD com Google Test)

Ambiente de desenvolvimento de firmware para o **Datalogger com ESP32-S3**, utilizando **ESP-IDF nativo (C++17)**, **FreeRTOS (Dual-Core)** e arquitetura orientada a **TDD (Test-Driven Development)** com **Google Test (GTest + GMock)** rodando localmente no computador (Host Windows).

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
│   ├── board_config/               # Mapeamento de hardware (GPIOs x Constantes PIN_* e TAG_*)
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       └── board_pins.h        # Definições de pinos do ESP32-S3, guia de pinos seguros e tabela kBoardPinMap
│   ├── hal_interfaces/             # Contratos/Interfaces C++ puras para desacoplamento de hardware (TDD)
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       ├── i_sensor.hpp        # Interface ISensor e struct SensorReading
│   │       ├── i_storage.hpp       # Interface IStorage (mount, append_line, sync)
│   │       └── i_rtos_queue.hpp    # Interface IRtosQueue<T> para filas do FreeRTOS
│   ├── ring_buffer/                # Buffer circular estático (sem alocação dinâmica / heap)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── ring_buffer.hpp
│   │   └── src/
│   │       └── ring_buffer.cpp
│   └── datalogger_core/            # Regra de negócio central do Datalogger
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── datalogger_service.hpp
│       └── src/
│           └── datalogger_service.cpp
└── tests/
    ├── CMakeLists.txt              # Build isolado de testes no Host + GoogleTest v1.14.0
    ├── esp_idf_stubs/              # Headers stub do ESP-IDF/FreeRTOS para IntelliSense e compilação Host
    │   └── include/
    │       ├── esp_log.h
    │       ├── esp_timer.h
    │       ├── driver/gpio.h
    │       └── freertos/ (FreeRTOS.h, task.h, queue.h)
    ├── mocks/                      # Mocks GMock (MockSensor, MockStorage) e FakeRtosQueue
    ├── test_ring_buffer.cpp        # Testes unitários do SampleRingBuffer
    └── test_datalogger_service.cpp # Testes unitários do DataloggerService e filas do FreeRTOS
```

---

## O Que Já Foi Implementado

1. **Ambiente de Compilação e Testes no Windows (Host TDD)**:
   - Toolchain C/C++ (**LLVM-MinGW Clang/G++ 23.1.4 UCRT**), **CMake 4.4.4** e **Ninja 1.13.2** configurados.
   - **Google Test & Google Mock (v1.14.0)** integrado em `tests/external/googletest` (funcionando 100% offline).
   - 10 testes unitários implementados e passando (`SampleRingBufferTest` e `DataloggerServiceTest`).
2. **Mapeamento de Portas e IOs do ESP32-S3 (`components/board_config/include/board_pins.h`)**:
   - Documentação dos pinos seguros, ADC1, ADC2, Strapping Pins e pinos reservados (Flash/PSRAM/USB) do ESP32-S3.
   - Definições `PIN_*` e `TAG_*` para LEDs, Botão, Power Gate de sensores, I2C, SPI (MicroSD), ADC1 (Bateria e canais analógicos), Entradas Digitais de Pulso, Saídas Digitais/Relés e UART1/RS-485.
   - Tabela `kBoardPinMap` associando `gpio_num_t`, `tag`, direção e descrição.
3. **Arquitetura Desacoplada para FreeRTOS (`components/hal_interfaces` e `components/datalogger_core`)**:
   - Injeção de dependência via `ISensor`, `IStorage` e `IRtosQueue<T>`.
   - Separação em duas Tasks do FreeRTOS no `main/main.cpp`:
     - **Core 1 (`sensor_acquisition_task`)**: Amostragem periódica determinística com `vTaskDelayUntil`.
     - **Core 0 (`storage_writer_task`)**: Consumo da fila do FreeRTOS, formatação CSV e `sync` periódico no armazenamento.
4. **Configuração de IntelliSense no VS Code (`.vscode/` e `tests/esp_idf_stubs/`)**:
   - Autocompletar funcionando sem erros para código ESP-IDF, FreeRTOS, componentes próprios e Google Test mesmo antes de rodar o `idf.py build`.

---

## Como Rodar os Testes Unitários (TDD no PC)

Na raiz do projeto, execute no PowerShell:

```powershell
.\run_tests.ps1
```

Para filtrar apenas um conjunto de testes específico:

```powershell
.\build_tests\datalogger_unit_tests.exe --gtest_filter=*DataloggerService*
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
