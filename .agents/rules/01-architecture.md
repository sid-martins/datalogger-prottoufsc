---
trigger: always_on
---

# Regras de Arquitetura (Padrão Motorsport / AUTOSAR)

## 1. Camadas Fixas e Direção de Dependência

O diretório `components/` possui **exatamente 5 componentes fixos**, um para cada camada Motorsport. **Nenhuma camada inferior pode depender de uma camada superior, e novos módulos devem ser adicionados dentro dessas 5 camadas (não crie pastas paralelas fora delas).**

```text
ASW (main/main.cpp)
 └──► BSW (components/bsw/)
       ├──► HAL (components/datalogger_hal/) ◄── implementado por LLD (components/lld/)
       ├──► OSAL (components/osal/)          ◄── implementado por FreeRtosQueueAdapter
       └──► BSP (components/bsp/)
```

### Responsabilidades Obrigatórias por Camada:
- **`components/bsp/` (`datalogger::bsp`)**: Apenas definições de pinos (`PIN_*`), tags (`TAG_*`), endereços I2C/SPI fixos da placa e tabelas `constexpr` (`PinDescriptor` em `bsp_pins.h`). Zero lógica de negócio.
- **`components/datalogger_hal/` (`datalogger::hal`)**: Apenas structs de fronteira e interfaces puramente virtuais (`ICanHal`, `IStorageHal`, `IAdcHal`, `IRtcHal`, `IGpsHal`, `IGpioHal`). O prefixo `datalogger_` evita sobrescrever o componente nativo `hal` do ESP-IDF. **Proibido** incluir headers de silício (`driver/*.h`, `esp_*.h`, exceto `driver/gpio.h` via `bsp_pins.h`) ou lógica implementada.
- **`components/osal/` (`datalogger::osal`)**: Abstrações de primitivas do RTOS (filas, semáforos, timers) e seus adaptadores concretos para FreeRTOS.
- **`components/lld/` (`datalogger::lld`)**: Drivers de baixo nível específicos do **ESP32-S3** e dos CIs da PCB (`CanTwaiLld`, `AdcOneshotLld`, `Ds3231I2cLld`, etc.). Único lugar permitido para chamadas `esp_err_t`, `driver/*.h` e registradores.
- **`components/bsw/` (`datalogger::bsw`)**: Serviços independentes de silício (`CanService`, `AdcService`, `RtcService`, `GpsService`, `LoggerService`, `UiService`, `RingBuffer`). Depende **somente** de `datalogger_hal`, `osal`, `bsp` e C++ STL.
- **`main/` (`ASW`)**: Instanciação estática dos objetos (`LLD -> BSW -> OSAL`), criação de Tasks do FreeRTOS e orquestração Dual-Core.

## 2. Alocação Dual-Core (ESP32-S3 FreeRTOS)
- **Core 1 (`APP_CPU`) — Tempo Real / Aquisição Determinística:**
  - Tasks de leitura de CAN RX, amostragem periódica de ADC (`vTaskDelayUntil`), leitura de GPS/RTC e atualização de estado crítico.
  - Nunca executar operações bloqueantes de disco/flash ou rede no Core 1.
- **Core 0 (`PRO_CPU`) — Persistência / I/O Lento / Telemetria:**
  - Escrita em MicroSD (`IStorageHal`), flush/sync de arquivos, telemetria serial/Wi-Fi e diagnósticos.

## 3. Checklist ao Adicionar um Novo Módulo nas Camadas
Sempre que criar um novo módulo (ex: `AdcService` + `AdcOneshotLld`):
1. **`components/datalogger_hal/`**: Criar `include/i_<modulo>_hal.hpp`.
2. **`components/bsw/`**: Criar `include/<modulo>_service.hpp` e `src/<modulo>_service.cpp`, e adicionar `src/<modulo>_service.cpp` em `components/bsw/CMakeLists.txt` (`SRCS`).
3. **`components/lld/`**: Criar `include/<modulo>_lld.hpp` e `src/<modulo>_lld.cpp`, e adicionar `src/<modulo>_lld.cpp` em `components/lld/CMakeLists.txt` (`SRCS` e `REQUIRES` do ESP-IDF se necessário).
4. **`tests/`**: Criar `tests/sil_mocks/mock_<modulo>_hal.hpp`, `tests/test_<modulo>_service.cpp`, stubs necessários em `tests/esp_idf_stubs/include/` e registrar os `.cpp` em `tests/CMakeLists.txt`.
