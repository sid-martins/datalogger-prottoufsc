# Datalogger ESP32-S3 — Arquitetura Motorsport (ESP-IDF + FreeRTOS + SIL TDD)

Firmware embarcado para **Datalogger Automotivo / Motorsport** baseado no **ESP32-S3 (`ESP32-S3-DEVKITC-1-N8R2`)**, desenvolvido em **C++17** sobre o **ESP-IDF nativo** e **FreeRTOS (Dual-Core)**, seguindo a arquitetura em camadas da indústria de **Motorsport / AUTOSAR** (`BSP`, `OSAL`, `HAL`, `LLD`, `BSW`, `ASW`) e validação **SIL (*Software-in-the-Loop*)** com **Google Test & Google Mock**.

---

## Arquitetura em Camadas (Padrão Motorsport)

| Camada | Diretório / Namespace | Papel na Engenharia |
| :--- | :--- | :--- |
| **BSP** (*Board Support Package*) | `components/bsp/` (`datalogger::bsp`) | *Pinning Table* da placa física ([bsp_pins.h](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/bsp/include/bsp_pins.h)): mapeia os 26 sinais do esquemático (`PIN_*` e `TAG_*`). |
| **OSAL** (*OS Abstraction Layer*) | `components/osal/` (`datalogger::osal`) | Abstrai o sistema operacional de tempo real ([i_osal_queue.hpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/osal/include/i_osal_queue.hpp) e [freertos_queue_adapter.hpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/osal/include/freertos_queue_adapter.hpp)). |
| **HAL** (*Hardware Abstraction Layer*) | `components/hal/` (`datalogger::hal`) | Contratos puramente virtuais e tipos de fronteira ([i_can_hal.hpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/hal/include/i_can_hal.hpp), [i_storage_hal.hpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/hal/include/i_storage_hal.hpp)). |
| **LLD** (*Low-Level Drivers / MCAL*) | `components/lld/` (`datalogger::lld`) | Drivers específicos do silício ESP32-S3 e CIs da placa ([can_twai_lld.cpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/lld/src/can_twai_lld.cpp) para o transceptor `SN65HVD230DR`). |
| **BSW** (*Basic Software Services*) | `components/bsw/` (`datalogger::bsw`) | Serviços de comunicação, diagnóstico, validação e buffers estáticos sem heap ([can_service.cpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/bsw/src/can_service.cpp), [ring_buffer.hpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/components/bsw/include/ring_buffer.hpp)). |
| **ASW** (*Application Software*) | `main/` ([main.cpp](file:///c:/Users/gusta/OneDrive/Documentos/datalogger/main/main.cpp)) | Aplicação final e orquestração das Tasks Dual-Core no ESP32-S3 (Core 1 para aquisição em tempo real; Core 0 para log/armazenamento). |
| **SIL** (*Software-in-the-Loop*) | `tests/` (`datalogger::sil`) | Suíte de testes automatizados e dublês de hardware (`tests/sil_mocks/`) executados no PC via Google Test. |

---

## Estrutura de Pastas do Projeto

```text
datalogger/
├── CMakeLists.txt                  # CMake híbrido: Firmware ESP-IDF (via idf.py) ou Testes SIL no Host
├── GEMINI.md                       # Contexto e regras arquiteturais para o Agente IA (equivalente ao CLAUDE.md)
├── README.md                       # Documentação geral do projeto
├── sdkconfig.defaults              # Configurações padrão do ESP32-S3 (Flash 8MB, FreeRTOS 1000Hz)
├── partitions.csv                  # Tabela de partições customizada (NVS, Factory App, Storage FAT)
├── run_tests.ps1                   # Script PowerShell para compilar e rodar a suíte SIL no Windows
├── .clangd                         # Flags de compilação para o servidor Clangd
├── .vscode/
│   ├── c_cpp_properties.json       # Configuração do IntelliSense (ESP32-S3 + SIL Host TDD)
│   └── settings.json               # Associações de arquivos C/C++ no VS Code
├── main/                           # [ASW] Camada de Aplicação
│   ├── CMakeLists.txt
│   └── main.cpp                    # Entrypoint app_main() + Tasks Dual-Core (can_rx_task e can_logger_task)
├── components/
│   ├── bsp/                        # [BSP] Board Support Package
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       └── bsp_pins.h          # Pinning Table oficial dos 26 sinais do esquemático (CPU.SchDoc)
│   ├── osal/                       # [OSAL] Operating System Abstraction Layer
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       ├── i_osal_queue.hpp    # Interface IOsalQueue<T>
│   │       └── freertos_queue_adapter.hpp # Implementação real com xQueue do FreeRTOS
│   ├── hal/                        # [HAL] Hardware Abstraction Layer
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       ├── i_can_hal.hpp       # Contrato ICanHal, CanFrame (11/29-bit), CanConfig (500 kbps)
│   │       └── i_storage_hal.hpp   # Contrato IStorageHal
│   ├── lld/                        # [LLD] Low-Level Drivers (ESP32-S3 Silício + Periféricos)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── can_twai_lld.hpp    # Driver LLD TWAI para o transceptor SN65HVD230DR (GPIO 15/16)
│   │   └── src/
│   │       └── can_twai_lld.cpp
│   └── bsw/                        # [BSW] Basic Software Services & Static Containers
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── can_service.hpp     # Serviço CAN (validação, fila OSAL RX, auto-recovery Bus-Off)
│       │   └── ring_buffer.hpp     # RingBuffer<T, Capacity> estático (sem alocação dinâmica)
│       └── src/
│           ├── can_service.cpp
│           └── ring_buffer.cpp
└── tests/                          # [SIL] Ambiente de Testes Software-in-the-Loop (Host PC)
    ├── CMakeLists.txt              # Build isolado SIL + GoogleTest v1.14.0
    ├── esp_idf_stubs/              # Headers stub do ESP-IDF/FreeRTOS para compilação Host e IntelliSense
    ├── sil_mocks/                  # Mocks e Fakes para simulação SIL (MockCanHal, MockStorageHal, FakeOsalQueue)
    ├── test_bsp_pins.cpp           # Valida os 26 pinos e tags da Pinning Table do esquemático
    ├── test_ring_buffer.cpp        # Valida o container estático bsw::RingBuffer
    └── test_can_service.cpp        # Valida o bsw::CanService (11/29-bit, 500 kbps, fila OSAL, Bus-Off)
```

---

## Como Executar os Testes SIL (TDD no PC)

Na raiz do projeto, execute no PowerShell:

```powershell
.\run_tests.ps1
```

Para executar apenas uma suíte específica (ex: apenas testes do serviço CAN):

```powershell
.\build_tests\datalogger_unit_tests.exe --gtest_filter=*CanService*
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
