# Bokaka (ボカカ)

## Overview
Bokaka is an open-hardware, open-firmware credit-card–sized PCB distributed as a collectible at VOCALOID events (including Hatsune Miku concerts). Each unit contains a microcontroller, LEDs, a tap-to-connect interface, and local storage to record brief peer-to-peer interactions. The device visualizes interaction progress through onboard LEDs and supports synchronization with a companion website over USB.

## Key Capabilities
- Record unique, short-range interactions with other Bokaka units via a simple 1‑wire handshake.
- Persist interaction records in on‑board flash and present a visual progress/connection meter using LEDs.
- Connect to a host computer over USB to claim the device, attach personal information, and upload interaction data to a companion website.

## Hardware Summary
- Form factor: Credit-card–sized PCB
- Microcontroller: STM32L052K8T6 (low power, USB, unique device ID)
- Power:
    - Primary: CR2032 coin cell for standalone operation
    - Optional: USB‑C for data and power
- Communication:
    - Tap connector (pogo pins / magnetic pads) implementing a 1‑wire Bokaka protocol
    - USB for host connectivity
- Peripherals:
    - Multiple low‑power colored LEDs for progress and animation
    - Single status button
    - Buzzer for simple sound feedback

## Functional Description
When two Bokaka units are tapped together, they perform a one‑wire handshake and exchange unique identifiers. Each successful exchange is recorded to on‑board flash memory. The device uses LED animations to indicate the connection status and to represent a cumulative “connection meter.” When connected to a host computer via USB, the user may:

1. Claim ownership of the card.
2. Attach optional personal metadata to the device.
3. Synchronize recorded interactions with a companion website to visualize social graphs.

## Development and Firmware
- Firmware: PlatformIO with the Arduino framework targeting the STM32L0 series.
- PCB design: KiCad (open hardware files included).
- Goals: Maintain low power consumption for coin‑cell operation, robust flash management for interaction logs, and a simple, extensible protocol for tap interactions.

## Project Structure (typical)
- hardware/ — KiCad project files and manufacturing outputs
- firmware/ — PlatformIO project, source code, and build scripts
- docs/ — Design notes, protocol specification, and user documentation
- tools/ — Utility scripts for flashing, testing, and data conversion

## Usage
- Standalone: Power via CR2032 and perform tap interactions with other units.
- Host mode: Connect via USB‑C to a host to perform claiming, metadata attachment, and data synchronization.
- Interaction protocol: 1‑wire handshake using the tap connector; exchanged identifiers are stored locally.

## Contribution & Licensing
Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for process and coding standards. Utility scripts (including the provisioning helper) live in `utils/` — see [utils/README.md](utils/README.md) for usage. The project is released under an open hardware / open source license (see LICENSE).

## Contacts & Resources
Refer to the repository for detailed build instructions, protocol documentation, and firmware API references.