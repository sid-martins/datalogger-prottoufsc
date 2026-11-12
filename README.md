# Datalogger ESP32-S3 — Protto UFSC (Arquitetura Motorsport)

Firmware embarcado para **Datalogger Automotivo / Motorsport** baseado no **ESP32-S3 (`ESP32-S3-DEVKITC-1-N8R2`)**, desenvolvido em **C++17** sobre o **ESP-IDF nativo** e **FreeRTOS (Dual-Core)**, seguindo a arquitetura em camadas da indústria de **Motorsport / AUTOSAR** (`BSP`, `OSAL`, `HAL`, `LLD`, `BSW`, `ASW`) e validação **SIL (*Software-in-the-Loop*)** com **Google Test & Google Mock**.

- **Repositório Oficial:** `git@github.com:sid-martins/datalogger-prottoufsc.git`

---

## Arquitetura em Camadas (Padrão Motorsport)

| Camada | Diretório / Namespace | Papel na Engenharia |
| :--- | :--- | :--- |
| **BSP** (*Board Support Package*) | [`components/bsp/`](components/bsp) (`datalogger::bsp`) | *Pinning Table* da placa física ([`bsp_pins.h`](components/bsp/include/bsp_pins.h)): mapeia os 26 sinais do esquemático (`PIN_*` e `TAG_*`). |
| **OSAL** (*OS Abstraction Layer*) | [`components/osal/`](components/osal) (`datalogger::osal`) | Abstrai o sistema operacional de tempo real ([`i_osal_queue.hpp`](components/osal/include/i_osal_queue.hpp) e [`freertos_queue_adapter.hpp`](components/osal/include/freertos_queue_adapter.hpp)). |
| **HAL** (*Hardware Abstraction Layer*) | [`components/hal/`](components/hal) (`datalogger::hal`) | Contratos puramente virtuais e tipos de fronteira ([`i_can_hal.hpp`](components/hal/include/i_can_hal.hpp), [`i_storage_hal.hpp`](components/hal/include/i_storage_hal.hpp)). |
| **LLD** (*Low-Level Drivers / MCAL*) | [`components/lld/`](components/lld) (`datalogger::lld`) | Drivers específicos do silício ESP32-S3 e CIs da placa ([`can_twai_lld.cpp`](components/lld/src/can_twai_lld.cpp) para o transceptor `SN65HVD230DR`). |
| **BSW** (*Basic Software Services*) | [`components/bsw/`](components/bsw) (`datalogger::bsw`) | Serviços de comunicação, diagnóstico, validação e buffers estáticos sem heap ([`can_service.cpp`](components/bsw/src/can_service.cpp), [`ring_buffer.hpp`](components/bsw/include/ring_buffer.hpp)). |
| **ASW** (*Application Software*) | [`main/`](main) ([`main.cpp`](main/main.cpp)) | Aplicação final e orquestração das Tasks Dual-Core no ESP32-S3 (Core 1 para aquisição em tempo real; Core 0 para log/armazenamento). |
| **SIL** (*Software-in-the-Loop*) | [`tests/`](tests) (`datalogger::sil`) | Suíte de testes automatizados e dublês de hardware ([`tests/sil_mocks/`](tests/sil_mocks)) executados no PC via Google Test. |

---

## Estrutura de Pastas do Projeto

```text
datalogger-prottoufsc/
├── CMakeLists.txt                  # CMake híbrido: Firmware ESP-IDF (via idf.py) ou Testes SIL no Host
├── GEMINI.md                       # Contexto e regras arquiteturais para o Agente IA (equivalente ao CLAUDE.md)
├── README.md                       # Documentação geral do projeto
├── sdkconfig.defaults              # Configurações padrão do ESP32-S3 (Flash 8MB, FreeRTOS 1000Hz)
├── partitions.csv                  # Tabela de partições customizada (NVS, Factory App, Storage FAT)
├── run_tests.ps1                   # Script PowerShell para compilar e rodar a suíte SIL no Windows
├── .clangd                         # Flags de compilação para o servidor Clangd
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
    ├── CMakeLists.txt              # Build isolado SIL + GoogleTest v1.14.0 (FetchContent automático)
    ├── esp_idf_stubs/              # Headers stub do ESP-IDF/FreeRTOS para compilação Host e IntelliSense
    ├── sil_mocks/                  # Mocks e Fakes para simulação SIL (MockCanHal, MockStorageHal, FakeOsalQueue)
    ├── test_bsp_pins.cpp           # Valida os 26 pinos e tags da Pinning Table do esquemático
    ├── test_ring_buffer.cpp        # Valida o container estático bsw::RingBuffer
    └── test_can_service.cpp        # Valida o bsw::CanService (11/29-bit, 500 kbps, fila OSAL, Bus-Off)
```

---

## Como Clonar e Executar os Testes SIL (TDD no PC)

```powershell
# 1. Clonar o repositório
git clone git@github.com:sid-martins/datalogger-prottoufsc.git
cd datalogger-prottoufsc

# 2. Compilar e rodar todos os testes SIL (Google Test)
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
