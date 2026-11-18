---
trigger: always_on
---

# Restrições de Hardware e Pinning Table (ESP32-S3-DEVKITC-1-N8R2)

## 1. Fonte Única da Verdade
- Todos os pinos e tags do hardware estão definidos exclusivamente em `components/bsp/include/bsp_pins.h`.
- Sempre utilize as macros `PIN_*` e `TAG_*` no código. Nunca escreva números mágicos de GPIO (ex: `15`, `GPIO_NUM_15`) fora do arquivo `bsp_pins.h`.

## 2. Restrições Críticas do Silício ESP32-S3
- **Proibidos (Flash SPI interna):** `GPIO 26` a `GPIO 32`.
- **USB Nativo (D- / D+ / JTAG):** `GPIO 19` e `GPIO 20`.
- **UART0 Console / Gravação:** `GPIO 43` (TX0) e `GPIO 44` (RX0).
- **Strapping Pins (cuidado no boot):** `GPIO 0`, `GPIO 3`, `GPIO 45`, `GPIO 46`.
- **Nota sobre `GPIO 35`, `36` e `37` (Módulo N8R2):**
  - No esquemático (`CPU.SchDoc`), esses pinos são usados para `Free_SW1`, `Free_SW2` e `LED_Acquiring`.
  - No ESP32-S3-N8R2 (Quad SPI Flash / Quad PSRAM), **não** habilitar Octal PSRAM (`CONFIG_SPIRAM_MODE_OCT`). Caso a PSRAM não seja necessária, manter `CONFIG_SPIRAM=n` no `sdkconfig.defaults`.
- **ADC1 vs ADC2:**
  - Todos os 10 canais analógicos do esquemático (`GPIO 1` a `GPIO 10`) utilizam **ADC1**, evitando conflito com o rádio Wi-Fi (que bloqueia o ADC2).
