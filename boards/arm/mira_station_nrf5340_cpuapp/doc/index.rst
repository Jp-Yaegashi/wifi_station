# Mira Station nRF5340 Application MCU Board

## Overview

The Mira Station is a custom board featuring the nRF5340 multiprotocol SoC with WM02C (nRF7002) WiFi companion IC. This board is designed for IoT applications requiring WiFi connectivity, BLE, and various peripherals.

## Hardware Features

### Supported Features

- **ARM Cortex-M33** @ 128 MHz (Application core)
- **ARM Cortex-M33** @ 64 MHz (Network core)
- **1 MB Flash** + **512 KB RAM** (Application core)
- **256 KB Flash** + **64 KB RAM** (Network core)
- **WiFi 802.11n** via WM02C (nRF7002)
- **Bluetooth 5.3** with Bluetooth mesh support
- **2x GPIO-controlled LEDs**
- **UART** console interface
- **QSPI** interface for WiFi module
- **IEEE 802.15.4** support
- **micro SD slot**

### Hardware Configuration

| Interface | Pin Configuration |
|-----------|-------------------|
| Status LED | P0.28 (Active High) |
| WiFi LED | P1.09 (Active High) |
| UART TX | P0.20 |
| UART RX | P0.22 |
| QSPI SCK | P0.17 |
| QSPI IO0 | P0.13 |
| QSPI IO1 | P0.14 |
| QSPI IO2 | P0.15 |
| QSPI IO3 | P0.16 |
| QSPI CSN | P0.18 |

### WM02C WiFi Module Connections

| Signal | nRF5340 Pin | Description |
|--------|-------------|-------------|
| BUCKEN | P0.12 | Buck converter enable |
| IOVDD_CTRL | P0.20 | I/O voltage control |
| HOST_IRQ | P0.19 | Host interrupt |
| COEX_REQ | P0.11 | Coexistence request |
| COEX_GRANT | P0.10 | Coexistence grant |
| SW_CTRL0 | P0.09 | Switch control 0 |
| SW_CTRL1 | P0.08 | Switch control 1 |

## Programming and Debugging

### Supported debuggers

- J-Link (default)
- PyOCD

### Flashing

```bash
west flash
```

### Debugging

```bash
west debug
```

## Building Applications

### WiFi Station Application

Build the complete WiFi station application:

```bash
west build -b mira_station_nrf5340_cpuapp samples/wifi/sta
```

### LED Test Application

Build the minimal LED test application:

```bash
west build -b mira_station_nrf5340_cpuapp -- -DCONF_FILE=prj_test.conf
```

## References

- [nRF5340 Product Specification](https://www.nordicsemi.com/Products/nRF5340)
- [nRF7002 Product Specification](https://www.nordicsemi.com/Products/nRF7002)
- [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
