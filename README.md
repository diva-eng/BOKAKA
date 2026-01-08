# Bokaka (ボカカ)

<div align="center">
  <img src="assets/logo.png" alt="Bokaka Logo" width="400">
</div>

## Overview

Bokaka is an open-hardware, open-firmware credit-card–sized PCB distributed as a collectible at VOCALOID events (including Hatsune Miku concerts). Each unit contains a microcontroller, LEDs, a tap-to-connect interface, and local storage to record brief peer-to-peer interactions. The device visualizes interaction progress through onboard LEDs and supports synchronization with a companion website over USB.

## Key Capabilities

- Record unique, short-range interactions with other Bokaka units via a simple 1‑wire handshake over magnetic connectors.
- Persist interaction records in on‑board flash and present a visual progress/connection meter using 34 animation LEDs.
- Connect to a host computer over USB (WebUSB compatible) to claim the device, attach personal information, and upload interaction data to a companion website.
- Per-device security with unique device ID and locally stored HMAC-SHA256 secret keys.

## Hardware Summary

- Form factor: Credit-card–sized PCB
- Microcontroller: STM32L053R8 (low power, USB, unique device ID)
- Power:
  - Primary: CR2450 coin cell for sustained standalone operation
  - Optional: USB‑C for data and power
- Communication:
  - 2× 3-wire magnetic connectors with 1-wire open-drain Bokaka protocol
  - USB CDC (WebUSB compatible) for host connectivity
- Peripherals:
  - LED driver: IS31FL3236A (36-channel LED driver)
  - LEDs: 34 animation LEDs, 1 user status LED, 1 debug LED (36 total)
  - Single status button
  - Buzzer for simple sound feedback

## Functional Description

When two Bokaka units are tapped together via their magnetic connectors, they perform a one‑wire open-drain handshake and exchange unique identifiers. Each successful exchange is recorded to on‑board flash memory with CRC32 integrity checking. The device uses the IS31FL3236A LED driver to control 34 animation LEDs that display connection status, progress animations, and cumulative "connection meter" patterns. Two additional LEDs (user status and debug) provide system feedback. When connected to a host computer via USB, the user may:

1. Claim ownership of the card.
2. Attach optional personal metadata to the device.
3. Synchronize recorded interactions with a companion website to visualize social graphs (WebUSB compatible, no drivers required).

## Development and Firmware

- Firmware: PlatformIO with the Arduino framework targeting the STM32L053R8, with platform abstraction layer for future STM32CubeIDE migration.
- PCB design: KiCad (open hardware files included).
- Security: Per-device HMAC-SHA256 with rotating key versions, unique device IDs.
- Storage: Internal flash with EEPROM emulation, CRC32 integrity checking, optimized delayed writes for flash wear management.
- Goals: Maintain low power consumption for CR2450 coin‑cell operation, robust flash management for interaction logs, and a simple, extensible 1-wire protocol for tap interactions.

## Project Structure (typical)

- hardware/ — KiCad project files and manufacturing outputs
- firmware/ — PlatformIO project, source code, and build scripts
- docs/ — Design notes, protocol specification, and user documentation
- tools/ — Utility scripts for flashing, testing, and data conversion

## Usage

- Standalone: Power via CR2450 coin cell and perform tap interactions with other units via magnetic connectors.
- Host mode: Connect via USB‑C to a host (WebUSB compatible) to perform claiming, metadata attachment, and data synchronization.
- Interaction protocol: 1‑wire open-drain handshake using the magnetic connector interface; exchanged identifiers are stored locally with CRC32 verification.

## Contribution & Licensing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for process and coding standards. Utility scripts (including the provisioning helper) live in `utils/` — see [utils/README.md](utils/README.md) for usage. The project is released under an open hardware / open source license (see LICENSE).

## Contacts & Resources

Refer to the repository for detailed build instructions, protocol documentation, and firmware API references.
