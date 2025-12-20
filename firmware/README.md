# Bokaka

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

### **🔵 Tap‑to‑Connect Interface**
- Magnetic pogo‑pin contact pads  
- Stable one‑wire handshake protocol  
- Offline identity exchange  

### **🟢 LED Progress & Animations**
- Energy‑efficient LED matrix  
- Connection milestones unlock new patterns  
- Optional hidden Easter eggs  

### **🟣 WebUSB Support**
- No drivers required  
- Configure and sync directly in your browser  
- Built‑in unique device identity  

### **🛡 Per‑Device Security**
- Each Bokaka includes:
  - Unique device ID
  - Locally stored secret key (HMAC)
- Server verifies authenticity without exposing secrets  

### **🌐 Global Fan Connection Graph**
When synced to the companion site, your interactions contribute to a worldwide visualization of Miku fans and their meeting paths.

---

## 🧩 Technical Overview

| Component | Detail |
|----------|--------|
| MCU | STM32 family with unique ID + low‑power modes |
| LEDs | Low‑power addressable matrix |
| Interface | Magnetic pogo‑pin contact pads |
| Security | Per‑device HMAC with rotating key versions |
| Connectivity | WebUSB + browser‑native syncing |
| Storage | Internal flash with schema versioning |

---

## 🔧 Development Notes

This repository may contain:
- Firmware for the Bokaka card  
- Python provisioning tools  
- WebUSB interface and JS client  
- Documentation for the communication protocol

### ⚠️ Flash Memory Wear Management

The firmware uses EEPROM emulation on STM32 flash memory. **Flash wear is a critical consideration:**

- **Flash Endurance**: STM32L0 flash has ~10,000 erase/write cycles per page
- **Write Strategy**: 
  - Regular operations (like `addLink()`) use a 30-second delayed write to batch changes
  - Critical operations (like `setSecretKey()`) write immediately but occur rarely
  - Each write consumes one flash cycle regardless of how many bytes change

**Lifetime Estimates** (for <100 links/event, multiple events/year):
- **Realistic case**: ~40 years at 10 events/year
- **Worst case**: ~10 years if writes aren't batched
- **With external flash**: 400+ years (recommended for production)

📊 **See [Flash Lifetime Analysis](docs/FLASH_LIFETIME_ANALYSIS.md) for detailed calculations and recommendations.**

For production use with high reliability requirements, consider external SPI flash with encryption (see analysis document).

See `include/storage.h` for configuration and `src/storage.cpp` for implementation details.  

---

## ❤️ Community

Bokaka is made for fans who want to share a moment of connection—whether at concerts, events, or anywhere creativity brings people together.

If you’d like to contribute, discuss features, or share ideas, feel free to open issues or PRs!

---

## 📄 License

MIT License

Copyright (c) 2025 Diva Engineering

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.