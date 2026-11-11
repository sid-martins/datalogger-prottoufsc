# Datalogger ESP32-S3 (ESP-IDF + FreeRTOS + TDD com Google Test)

Ambiente de desenvolvimento de firmware para o **ESP32-S3** utilizando **ESP-IDF nativo**, **FreeRTOS** e arquitetura orientada a **TDD (Test-Driven Development)** com **Google Test (GTest + GMock)** rodando localmente no computador (Host).

## Estrutura de Pastas

```text
datalogger/
├── CMakeLists.txt                  # CMake híbrido: ESP-IDF (via idf.py) ou Testes no Host
├── sdkconfig.defaults              # Configurações do ESP32-S3 (Flash 8MB, FreeRTOS 1000Hz)
├── partitions.csv                  # Tabela de partições (NVS, Factory App, Storage FAT)
├── run_tests.ps1                   # Script rápido para compilar e rodar os testes TDD no Windows
├── main/
│   ├── CMakeLists.txt
│   └── main.cpp                    # Entrypoint app_main() + Tasks do FreeRTOS (Core 0 e Core 1)
├── components/
│   ├── hal_interfaces/             # Contratos/Interfaces C++ (ISensor, IStorage, IRtosQueue)
│   ├── ring_buffer/                # Buffer circular estático (sem heap) para amostras
│   └── datalogger_core/            # Lógica de negócio do Datalogger (aquisição, validação, CSV)
└── tests/
    ├── CMakeLists.txt              # Build isolado de testes + FetchContent do GoogleTest v1.14.0
    ├── mocks/                      # Mocks GMock (MockSensor, MockStorage) e FakeRtosQueue
    ├── test_ring_buffer.cpp        # Testes unitários do buffer circular
    └── test_datalogger_service.cpp # Testes unitários do serviço do Datalogger e filas FreeRTOS
```

## Como Rodar os Testes Unitários (TDD no Host)

Basta executar o script PowerShell na raiz do projeto:

```powershell
.\run_tests.ps1
```

Ou manualmente via CMake + CTest:

```powershell
cmake -S tests -B build_tests -G Ninja
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

## Como Compilar e Gravar no ESP32-S3 (ESP-IDF)

Com o terminal do **ESP-IDF** aberto:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

