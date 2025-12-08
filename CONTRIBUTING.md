**Contributing to BOKAKA**

Thank you for your interest in contributing to BOKAKA. This document explains the preferred workflow, how to set up the development environment, and the process for reporting issues or submitting changes.

**Get the code**

- Fork the repository and create a feature branch from `master`.
- Use a short, descriptive branch name: `feature/<area>-<short-desc>` or `fix/<issue-number>-<short-desc>`.

**Development environment**

- This project uses PlatformIO for building the embedded firmware. Install PlatformIO (VS Code extension) or the CLI.
- To build firmware from the project root:

  ```powershell
  pio run
  ```

- The `utils/` folder contains Python helpers. Install Python 3.8+ and the dependencies where needed:

  ```powershell
  pip install pyserial
  # Optional: cryptography for master-key signing
  pip install cryptography
  ```

**Running the provisioner locally (developer helper)**

- You can test the provisioning workflow without hardware using the emulation mode:

  ```powershell
  python .\utils\provision.py --emulate
  ```

- To provision a real device, either let the script auto-detect new COM ports or pass `--port COM3`.

**Coding guidelines**

- Keep changes small and self-contained. Avoid formatting unrelated files.
- For C/C++ firmware files, follow the existing style and keep changes consistent with the codebase.
- Do not commit generated binaries or build artifacts.

**Commit messages**

- Use clear, imperative commit messages. Include a short summary line (<=72 chars) and an optional body with details.

**Pull request process**

- Open a pull request against `master`. Describe the purpose of the change, any testing performed, and any risks.
- Link related issues in the PR description (for example: `Fixes #123`).

**Reporting bugs / Feature requests**

- Use the repository Issues page and include:
  - A concise title
  - Steps to reproduce
  - Expected vs actual behavior
  - Relevant logs or error messages

**Security**

- Do not include private keys, passwords, or sensitive information in commits or issues. If you need to report a security vulnerability, contact repository maintainers privately.

**License**

- Contributions will be accepted under the project's existing license. By submitting a PR you agree to license your contribution under the project's license.

If you want more detail (git hooks, CI, contribution checklist), tell me what you'd like added and I can update this file.
