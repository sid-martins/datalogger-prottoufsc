# Contexto e Diretrizes do Projeto: Datalogger ESP32-S3 (Padrão Motorsport)

Este arquivo (`GEMINI.md`) funciona como a memória persistente e guia arquitetural do projeto para o assistente de IA (equivalente ao `CLAUDE.md`). Ele é carregado automaticamente em todas as sessões dentro deste repositório.

---

## 1. Visão Geral do Projeto
- **Produto:** Firmware de um Datalogger Automotivo / Motorsport baseado no microcontrolador **ESP32-S3** (`ESP32-S3-DEVKITC-1-N8R2`).
- **Repositório Remoto (GitHub):** `git@github.com:sid-martins/datalogger-prottoufsc.git`
- **Framework Principal:** **ESP-IDF nativo** com **FreeRTOS (Dual-Core)** e **C++17**.
- **Arquitetura de Software:** Camadas no padrão **Motorsport / AUTOSAR / Aeroespacial** (`BSP`, `OSAL`, `HAL`, `LLD`, `BSW`, `ASW` e `SIL`).
- **Metodologia de Desenvolvimento:** **TDD (Test-Driven Development)** com testes **SIL (Software-in-the-Loop)** utilizando **Google Test (`gtest` + `gmock` v1.14.0)** compilados e executados localmente no Host via CMake + Ninja + GCC/Clang.

---

## 2. Arquitetura em Camadas (Padrão Motorsport)

```text
+-------------------------------------------------------------------+
| ASW (Application Software) - main/main.cpp                        |
| Orquestração das Tasks Dual-Core e Máquina de Estados de Pista    |
+---------------------------------+---------------------------------+
                                  |
+---------------------------------v---------------------------------+
| BSW (Basic Software Services) - components/bsw/                   |
| Serviços independentes de silício (CanService, RingBuffer, etc.)  |
+----------------+---------------------------------+----------------+
                 |                                 |
+----------------v----------------+ +--------------v----------------+
| HAL (Hardware Abstraction Layer)| | OSAL (OS Abstraction Layer)   |
| components/hal/ (ICanHal, etc.) | | components/osal/ (IOsalQueue) |
+--------+---------------+--------+ +-------+---------------+-------+
         |               |                  |               |
   [No ESP32-S3]    [No PC / SIL]     [No ESP32-S3]    [No PC / SIL]
         |               |                  |               |
+--------v-------+ +-----v--------+ +-------v-------+ +-----v-------+
| LLD (Low-Level | | SIL Mocks    | | FreeRTOS      | | FakeOsal    |
| Drivers/MCAL)  | | tests/       | | Adapter       | | Queue       |
| components/lld/| | sil_mocks/   | | components/   | | tests/      |
+--------+-------+ +--------------+ | osal/         | | sil_mocks/  |
         |                          +---------------+ +-------------+
+--------v-------+
| BSP (Board     |
| Support Pkg)   |
| components/bsp/|
+----------------+
```

### Responsabilidade de Cada Camada:
1. **`components/bsp/` (Board Support Package — namespace `datalogger::bsp`):**
   - Contém a *Pinning Table* oficial da placa (`bsp_pins.h`), mapeando os 26 sinais elétricos do esquemático (`Main.SchDoc` / `CPU.SchDoc`) com pares `PIN_*` e `TAG_*`.
2. **`components/osal/` (Operating System Abstraction Layer — namespace `datalogger::osal`):**
   - Abstrai primitivas do RTOS (`i_osal_queue.hpp` / `IOsalQueue<T>`) e fornece o adaptador real para o FreeRTOS (`freertos_queue_adapter.hpp` / `FreeRtosQueueAdapter<T>`).
3. **`components/hal/` (Hardware Abstraction Layer — namespace `datalogger::hal`):**
   - Define os tipos de dados de fronteira (`CanFrame`, `CanConfig`) e contratos puramente virtuais (`ICanHal`, `IStorageHal`). Não possui dependência de registradores do ESP32-S3.
4. **`components/lld/` (Low-Level Drivers / MCAL — namespace `datalogger::lld`):**
   - Implementa os drivers de silício do ESP32-S3 e transceptores da PCB (ex: `CanTwaiLld` em `can_twai_lld.cpp`, conversando com `driver/twai.h` e o `SN65HVD230DR`).
5. **`components/bsw/` (Basic Software Services — namespace `datalogger::bsw`):**
   - Serviços de comunicação, diagnóstico, validação, formatação e estruturas estáticas sem uso de heap (`CanService`, `RingBuffer<T, Capacity>`). Depende apenas de `hal`, `osal` e `bsp`.
6. **`main/` (ASW — Application Software):**
   - Instancia os objetos estáticos, conecta `LLD -> HAL -> BSW` e cria as tasks do FreeRTOS fixadas por núcleo:
     - **Core 1 (APP_CPU):** Aquisição determinística em tempo real (CAN RX, leitura de ADC).
     - **Core 0 (PRO_CPU):** Gravação em MicroSD/Flash, telemetria e diagnóstico.
7. **`tests/` e `tests/sil_mocks/` (SIL — Software-in-the-Loop — namespace `datalogger::sil`):**
   - Testes unitários e de integração no PC usando Google Test / Google Mock (`MockCanHal`, `MockStorageHal`, `FakeOsalQueue`).

---

## 3. Fluxo Obrigatório de TDD e Git

1. **Ciclo Red -> Green -> Refactor:**
   - Defina ou atualize o contrato em `components/hal/` ou `components/osal/` (se necessário) e o mock correspondente em `tests/sil_mocks/`.
   - Escreva primeiro o teste SIL/unitário em `tests/`.
   - Execute `.\run_tests.ps1` para validar o teste no Host.
   - Implemente o serviço em `components/bsw/` e o driver de silício em `components/lld/` mantendo **0 warnings** (`-Wall -Wextra -Wpedantic`).
2. **Registro de Novos Componentes:**
   - Caso adicione um novo componente em `components/`, registre-o em `main/CMakeLists.txt`, `tests/CMakeLists.txt`, `.vscode/c_cpp_properties.json` e `.clangd`.
3. **Portabilidade e Repositório Remoto:**
   - **Nunca** insira caminhos absolutos locais da máquina (ex: `C:\Users\...` ou `file:///c:/Users/...`) em arquivos versionados no Git (`README.md`, `GEMINI.md`, `CMakeLists.txt`, scripts `.ps1`). Use sempre caminhos relativos ao repositório ou variáveis de ambiente (`$env:USERPROFILE`, `${CMAKE_CURRENT_SOURCE_DIR}`).

---

## 4. Pinning Table Oficial no BSP (`components/bsp/include/bsp_pins.h`)
- Todos os 26 pinos do **ESP32-S3-DEVKITC-1-N8R2 (`U3`)** mapeados a partir do esquemático:
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

## 5. Comandos Úteis do Ambiente

- **Executar todos os testes SIL / Unitários no Host:**
  ```powershell
  .\run_tests.ps1
  ```
- **Verificar sintaxe do `main/main.cpp` (ASW) com os stubs do ESP-IDF:**
  ```powershell
  g++ -fsyntax-only -std=c++17 -Wall -Wextra -Imain -Icomponents/bsp/include -Icomponents/osal/include -Icomponents/hal/include -Icomponents/lld/include -Icomponents/bsw/include -Itests/esp_idf_stubs/include main/main.cpp
  ```
- **Compilar e gravar no ESP32-S3 (via terminal ESP-IDF):**
  ```powershell
  idf.py set-target esp32s3
  idf.py build
  idf.py -p COMx flash monitor
  ```

---

## 6. Estado Atual do Projeto (Histórico de Progresso)
- [x] Estrutura base do projeto ESP-IDF (`main/`, `components/`, `sdkconfig.defaults`, `partitions.csv`) e versionamento conectado ao repositório remoto `git@github.com:sid-martins/datalogger-prottoufsc.git`.
- [x] Ambiente de testes SIL no Host configurado com CMake, Ninja, LLVM-MinGW e Google Test v1.14.0 (`tests/` e `run_tests.ps1`).
- [x] Reestruturação completa para nomenclatura e arquitetura em camadas **Motorsport** (`bsp`, `osal`, `hal`, `lld`, `bsw`, `asw` e `sil_mocks`).
- [x] **BSP (`components/bsp/include/bsp_pins.h`):** Pinning Table com os 26 sinais reais do esquemático elétrico validada via TDD (`test_bsp_pins.cpp`).
- [x] **OSAL (`components/osal/`):** Interface `IOsalQueue<T>`, adaptador real `FreeRtosQueueAdapter<T>` e dublê SIL `FakeOsalQueue<T>`.
- [x] **CAN Bus (`ICanHal` + `CanTwaiLld` + `CanService`):** Comunicação com o transceptor `SN65HVD230DR` (`GPIO 15` TX / `GPIO 16` RX @ 500 kbps), suporte a Standard 11-bit e Extended 29-bit, enfileiramento via OSAL e recuperação automática de *Bus-Off*, validado com 18 testes SIL (`test_can_service.cpp`).
- [ ] Implementação dos próximos módulos (LLD + HAL + BSW) via TDD: Aquisição Analógica ADC1 (10 canais), RTC `DS3231SN#` (I2C), GPS `Neo-8M` (UART), Cartão MicroSD (`J4`) e Interface de Botões/LEDs.

> **Regra de Manutenção:** Sempre que uma etapa importante for concluída ou um novo módulo for adicionado ao projeto, atualize a seção **6. Estado Atual do Projeto** deste arquivo `GEMINI.md` e o `README.md`.
