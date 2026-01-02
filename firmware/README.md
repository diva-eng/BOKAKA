# Bokaka Firmware

**Bokaka** is an interactive smart PCB card designed to connect VOCALOID fans through simple, meaningful physical interactions.  
Powered by a low‑energy microcontroller and magnetic tap‑to‑link interface, Bokaka creates a shared experience that blends hardware, community, and creativity.

---

## 🌱 What is Bokaka?

Bokaka is a pocket‑sized device that allows fans to exchange identity tokens simply by tapping their cards together.  
Each successful interaction lights up part of an LED pattern on the card—forming leeks, Miku‑themed shapes, or rare animations as you meet more fans.

- Meet someone → Tap cards  
- Cards exchange secure IDs  
- Your LED pattern grows  
- Reach milestones → unlock animations  
- Sync to the website → view your connection graph

Bokaka turns fan encounters into a visual memory you can carry.

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| **🔵 Tap‑to‑Connect** | Magnetic pogo‑pin contact pads with stable one‑wire handshake protocol |
| **🟢 LED Status Display** | Energy‑efficient LEDs showing connection status and role |
| **🟣 WebUSB Support** | No drivers required - configure and sync directly in your browser |
| **🛡 Per‑Device Security** | Unique device ID with locally stored HMAC secret key |
| **🌐 Connection Graph** | Sync to companion site for worldwide fan visualization |

---

## 🧩 Technical Overview

| Component | Detail |
|----------|--------|
| MCU | STM32L053R8 with unique ID + low‑power modes |
| Interface | Magnetic pogo‑pin contact pads (1-wire open-drain) |
| Security | Per‑device HMAC-SHA256 with rotating key versions |
| Connectivity | USB CDC (WebUSB compatible) |
| Storage | Internal flash with CRC32 integrity checking |
| Framework | Arduino (via PlatformIO) with platform abstraction layer |

---

## 📁 Project Structure

```
firmware/
├── include/                    # Header files
│   ├── platform_*.h            # Platform abstraction interfaces
│   ├── storage.h               # Persistent storage interface
│   ├── tap_link.h              # Tap link protocol
│   ├── usb_serial.h            # USB command handler
│   └── status_display.h        # LED status display
├── src/                        # Source files
│   ├── main.cpp                # Main application
│   ├── platform_*_arduino.cpp  # Arduino platform implementations
│   ├── storage.cpp             # Storage implementation
│   ├── tap_link.cpp            # Tap link protocol
│   └── status_display.cpp      # LED status display
├── lib/                        # Libraries
│   └── crypto_config/          # mbedTLS configuration
├── docs/                       # Documentation
│   ├── FLASH_LIFETIME_ANALYSIS.md
│   ├── TAP_LINK_TIMING.md
│   ├── TAP_INTERFACE_WIRING.md
│   └── STATUS_DISPLAY.md
├── utils/                      # Utility scripts
│   ├── provision.py            # Key provisioning tool
│   └── serial_test.py          # Serial testing utility
├── platformio.ini              # Build configuration
└── PLATFORM_ABSTRACTION.md     # Platform abstraction documentation
```

---

## 🔧 Building

### Prerequisites

- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)
- ST-Link programmer/debugger

### Build Commands

```bash
# Build for production
pio run -e nucleo_l053r8

# Build with test commands enabled
pio run -e dev

# Upload to device
pio run -e nucleo_l053r8 -t upload

# Monitor serial output
pio device monitor
```

### Build Environments

| Environment | Description |
|-------------|-------------|
| `nucleo_l053r8` | Production build |
| `dev` | Development build with `ENABLE_TEST_COMMANDS` |

---

## ⚠️ Flash Memory Wear Management

The firmware uses EEPROM emulation on STM32 flash memory. **Flash wear is a critical consideration:**

- **Flash Endurance**: STM32L0 flash has ~10,000 erase/write cycles per page
- **Write Strategy**: 
  - Regular operations use a 30-second delayed write to batch changes
  - Critical operations (like key provisioning) write immediately but occur rarely
  - Optimized partial writes for tap count and links (8-18 bytes vs 896 bytes)

**Lifetime Estimates** (for <100 links/event, multiple events/year):
- **Realistic case**: ~40 years at 10 events/year
- **Worst case**: ~10 years if writes aren't batched
- **With external flash**: 400+ years (recommended for production)

📊 **See [docs/FLASH_LIFETIME_ANALYSIS.md](docs/FLASH_LIFETIME_ANALYSIS.md) for detailed calculations.**

---

## 📖 Documentation

| Document | Description |
|----------|-------------|
| [PLATFORM_ABSTRACTION.md](PLATFORM_ABSTRACTION.md) | Platform abstraction layer design and migration guide |
| [MIGRATION_PLAN.md](MIGRATION_PLAN.md) | Step-by-step STM32CubeIDE migration plan |
| [docs/REFACTORING_GUIDE.md](docs/REFACTORING_GUIDE.md) | Code refactoring opportunities for testability |
| [docs/TAP_LINK_TIMING.md](docs/TAP_LINK_TIMING.md) | Tap link protocol timing diagrams |
| [docs/TAP_INTERFACE_WIRING.md](docs/TAP_INTERFACE_WIRING.md) | Hardware wiring guide for two-board testing |
| [docs/STATUS_DISPLAY.md](docs/STATUS_DISPLAY.md) | LED status patterns and configuration |
| [docs/FLASH_LIFETIME_ANALYSIS.md](docs/FLASH_LIFETIME_ANALYSIS.md) | Flash wear analysis and recommendations |

---

## ❤️ Community

Bokaka is made for fans who want to share a moment of connection—whether at concerts, events, or anywhere creativity brings people together.

If you'd like to contribute, discuss features, or share ideas, feel free to open issues or PRs!

---

## 📄 License

MIT License

Copyright (c) 2025 Diva Engineering
