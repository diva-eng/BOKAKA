**Provision Script**: Brief usage

- **Purpose**: `provision.py` provisions a device over USB serial by sending `HELLO`, provisioning a generated 32-byte secret via `PROVISION_KEY`, validating via `SIGN_STATE`, and storing keys locally.

- **Run (PowerShell)**:

  ```powershell
  python .\utils\provision.py           # wait for new COM ports and provision them automatically
  python .\utils\provision.py --port COM3   # watch and provision COM3 whenever it appears
  python .\utils\provision.py --port COM3 --once   # provision COM3 once and exit
  python .\utils\provision.py --no-validate # skip signature validation
  # Use a master key (hex) to derive device keys deterministically
  python .\utils\provision.py --master-key-hex ABCDEF0123...    # pass 32-byte master key as hex
  # or set environment variable BOKAKA_MASTER_KEY to the hex value and run without --master-key-hex
  # Emulate a device locally (no hardware required)
  python .\utils\provision.py --emulate
  ```

- **Dependencies**: `pyserial` — install with `pip install pyserial`.
 - **Dependencies**: `pyserial` — install with `pip install pyserial`.
 - **Optional (recommended)**: `cryptography` — needed when using `--master-key-hex` or `--master-key-pem` to sign provisioned secrets; install with `pip install cryptography`.
- **Output**: keys saved to `utils/provision_keys.json` by default.

If you want me to add support for selecting VID/PID or filtering candidate ports, I can extend the script.