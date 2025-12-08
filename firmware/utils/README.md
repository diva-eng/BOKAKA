```markdown
**Provision Script**: Brief usage

- **Purpose**: `provision.py` provisions a device over USB serial by sending `HELLO`, provisioning a generated 32-byte secret via `PROVISION_KEY`, validating via `SIGN_STATE`, and storing keys locally.

- **Run (PowerShell)**:

  ```powershell
  # Watch for new COM ports and provision automatically (interactive confirmation enabled)
  python .\utils\provision.py

  # Watch a specific port and provision whenever it appears
  python .\utils\provision.py --port COM3

  # Provision once and exit
  python .\utils\provision.py --port COM3 --once

  # Skip signature validation step (device will not be asked to sign state)
  python .\utils\provision.py --no-validate

  # Emulate a device locally (no hardware required)
  python .\utils\provision.py --emulate

  # Emulate with a longer artificial response delay so the UI updates are visible
  python .\utils\provision.py --emulate --emulate-delay 0.8

  # Skip interactive confirmation prompts (auto-accept provisioning)
  python .\utils\provision.py --emulate --no-confirm

  # Use a master key (hex) to derive device keys deterministically
  python .\utils\provision.py --master-key-hex ABCDEF0123...    # pass 32-byte master key as hex

  # Or load a PEM private key to sign provisioned secrets
  python .\utils\provision.py --master-key-pem path/to/key.pem
  ```

- **Dependencies**:
  - `pyserial` — install with `pip install pyserial`.
  - `rich` — optional but recommended for the improved in-place UI (Device / Status / Log panels, colored output, spinner and elapsed timer). Install with `pip install rich`.
  - `cryptography` — optional; required when using `--master-key-hex` or `--master-key-pem` to sign provisioned secrets. Install with `pip install cryptography`.

- **New / notable features**:
  - In-place UI (uses `rich` when available): separate Device / Status / Log panels, colored log output, auto-scroll to latest lines, and a small spinner + elapsed timer in the status bar.
  - Emulation improvements: `--emulate-delay` controls artificial response delay so UI updates are visible.
  - Interactive confirmation: the script prompts before provisioning a newly-detected device (shows device info from `HELLO`) and temporarily suspends the live UI to accept keyboard input.
  - Non-interactive mode: pass `--no-confirm` to skip confirmation prompts and accept provisioning automatically (useful for CI or batch runs).

- **Output**: keys are saved to `utils/provision_keys.json` by default; use `--out <path>` to change the file.

If you want me to add support for selecting VID/PID, filtering candidate ports, or changing colors/layout, I can extend the script.
```