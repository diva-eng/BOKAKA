#!/usr/bin/env python3
r"""
Provisioning helper for Bokaka-Eval devices over USB serial.

Behavior:
- Waits for a new serial port to appear (or uses `--port` if provided).
- Sends `HELLO` to read device info (device_id, fw, ...).
- Generates a 32-byte random secret key, sends `PROVISION_KEY <ver> <hex>`.
- Calls `GET_STATE` and `DUMP` to collect state, then `SIGN_STATE <nonce>`.
- Verifies returned HMAC locally using the generated key.
- Stores mapping in a JSON file (`utils/provision_keys.json` by default).

Usage (PowerShell):
  python .\utils\provision.py
  python .\utils\provision.py --port COM3

Dependencies: `pyserial` (install with `pip install pyserial`)
"""

from __future__ import annotations
import argparse
import json
import os
import sys
import time
import hmac
import hashlib
import secrets
from pathlib import Path
from typing import Optional, List, Dict, Any
import threading

import serial
from serial.tools import list_ports
import builtins

# Optional: use Rich for prettier terminal UI. Fall back to simple print if not installed.
try:
    from rich.console import Console
    from rich.traceback import install as rich_traceback_install
    rich_traceback_install()
    console = Console()
    _USE_RICH = True
except Exception:
    class _FallbackConsole:
        def print(self, *args, **kwargs):
            builtins.print(*args, **kwargs)
        def status(self, *args, **kwargs):
            # simple no-op context manager for .status()
            class _CM:
                def __enter__(self):
                    return self
                def __exit__(self, exc_type, exc, tb):
                    return False
                def update(self, **kwargs):
                    return None
            return _CM()
    console = _FallbackConsole()
    _USE_RICH = False


# UI manager: if Rich is available use Live to render a persistent panel (no scrolling);
# otherwise fall back to simple prints. The `cprint()` wrapper routes messages into
# the UI when active.
class UIManager:
    def __init__(self, max_lines: int = 200):
        self.max_lines = max_lines
        self.lines: List[str] = []
        self.status = ""
        self.device_info: Dict[str, str] = {}
        self._status_time: Optional[float] = None
        self._active = False
        if _USE_RICH:
            from rich.live import Live
            from rich.panel import Panel
            from rich.text import Text
            from rich.table import Table
            from rich.layout import Layout
            from rich.align import Align
            self._Live = Live
            self._Panel = Panel
            self._Text = Text
            self._Table = Table
            self._Layout = Layout
            self._Align = Align
            self.live = None
        # background refresher to ensure Live re-renders during blocking operations
        self._refresh_thread: Optional[threading.Thread] = None
        self._stop_event: Optional[threading.Event] = None

    def _render(self):
        if not _USE_RICH:
            return "\n".join(self.lines[-self.max_lines:])
        layout = self._Layout()
        # split into three rows: device info, status, log
        layout.split_column(self._Layout(name="device", size=6), self._Layout(name="status", size=3), self._Layout(name="log"))

        # device info panel: build a Table for clearer formatting
        if self.device_info:
            table = self._Table.grid(expand=True)
            table.add_column(justify="right", ratio=1)
            table.add_column(justify="left", ratio=3)
            for k, v in self.device_info.items():
                table.add_row(f"[bold cyan]{k}[/]", str(v))
            device_panel = self._Panel(self._Align.left(table), title="Device", title_align="left")
        else:
            device_panel = self._Panel(self._Align.left(self._Text("No device")), title="Device", title_align="left")

        # status panel: include spinner and elapsed timer
        status_text = self._Text()
        # spinner frames
        spinner_frames = ["⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"]
        elapsed = ""
        if getattr(self, "_status_time", None) is not None:
            try:
                elapsed_s = int(time.time() - self._status_time)
                elapsed = f"{elapsed_s}s"
            except Exception:
                elapsed = ""
        # choose spinner based on current time
        if _USE_RICH:
            frame = spinner_frames[int(time.time() * 4) % len(spinner_frames)]
        else:
            frame = ""
        header = f"{frame} {self.status or ''}"
        if elapsed:
            header = f"{header} ({elapsed})"
        status_text.append(header)
        status_panel = self._Panel(self._Align.left(status_text), title="Status", title_align="left")

        # log panel: show last lines and highlight latest. To simulate auto-scroll
        # (keep newest lines visible) compute an available height from the
        # console and pad the Text with leading newlines so the last lines sit
        # at the bottom of the Panel.
        last_lines = self.lines[-self.max_lines:]
        # estimate available height for log panel using console size and the
        # fixed sizes we used for device/status panes (6 and 3 respectively)
        try:
            console_height = console.size.height if _USE_RICH else 24
        except Exception:
            console_height = 24
        # reserve some rows for borders/titles; make sure height >= 3
        log_panel_height = max(3, console_height - 6 - 3 - 4)

        # number of content lines we can show inside the panel (rough)
        content_lines = max(1, log_panel_height - 2)
        # take only the most recent content_lines
        tail = last_lines[-content_lines:]
        pad = max(0, content_lines - len(tail))

        log_text = self._Text()
        # prepend blank lines to push content to bottom
        if pad:
            log_text.append("\n" * pad)

        for i, ln in enumerate(tail):
            style = None
            if ln.startswith("[✓]"):
                style = "green"
            elif ln.startswith("[i]"):
                style = "cyan"
            elif ln.startswith(">>>"):
                style = "yellow"
            elif ln.startswith("<<<"):
                style = "magenta"
            # highlight last line
            if i == len(tail) - 1:
                if style:
                    log_text.append(ln + "\n", style=f"bold {style}")
                else:
                    log_text.append(ln + "\n", style="bold")
            else:
                if style:
                    log_text.append(ln + "\n", style=style)
                else:
                    log_text.append(ln + "\n")

        # render Panel with explicit height so padding takes effect
        log_panel = self._Panel(self._Align.left(log_text), title=f"Log (last {min(len(self.lines), self.max_lines)})", title_align="left", height=log_panel_height)

        layout["device"].update(device_panel)
        layout["status"].update(status_panel)
        layout["log"].update(log_panel)
        return layout

    def set_device_info(self, info: Dict[str, str]):
        """Set key/value pairs to display in the Device panel."""
        # store as strings
        self.device_info = {k: str(v) for k, v in info.items()}
        if _USE_RICH and self._active and self.live is not None:
            self.live.update(self._render())

    def set_status(self, s: str):
        self.status = s
        try:
            self._status_time = time.time()
        except Exception:
            self._status_time = None
        if _USE_RICH and self._active and self.live is not None:
            self.live.update(self._render())

    def __enter__(self):
        self._active = True
        if _USE_RICH:
            self.live = self._Live(self._render(), console=console, refresh_per_second=5)
            self.live.__enter__()
            # start a lightweight thread to periodically call live.update() so
            # the UI animates (spinner) even if the main thread blocks briefly
            try:
                self._stop_event = threading.Event()
                def _refresher():
                    while not self._stop_event.is_set():
                        try:
                            if self.live is not None:
                                self.live.update(self._render())
                        except Exception:
                            pass
                        time.sleep(0.12)
                self._refresh_thread = threading.Thread(target=_refresher, daemon=True)
                self._refresh_thread.start()
            except Exception:
                self._refresh_thread = None
                self._stop_event = None
        return self

    def __exit__(self, exc_type, exc, tb):
        self._active = False
        # stop refresher thread first
        try:
            if getattr(self, '_stop_event', None) is not None:
                self._stop_event.set()
            if getattr(self, '_refresh_thread', None) is not None:
                self._refresh_thread.join(timeout=0.5)
        except Exception:
            pass
        if _USE_RICH and self.live is not None:
            self.live.__exit__(exc_type, exc, tb)

    def log(self, msg: str):
        for line in str(msg).splitlines():
            self.lines.append(line)
        if len(self.lines) > self.max_lines:
            self.lines = self.lines[-self.max_lines:]
        if _USE_RICH and self._active and self.live is not None:
            self.live.update(self._render())
        else:
            # fallback: print normally
            builtins.print(msg)

    def set_status(self, s: str):
        self.status = s
        if _USE_RICH and self._active and self.live is not None:
            self.live.update(self._render())


# global UI instance (created in main)
ui: Optional[UIManager] = None
# global flag to auto-accept provisioning prompts
AUTO_ACCEPT = False


def cprint(*args, **kwargs):
    """Console-print wrapper that routes to UIManager when active, else to console."""
    text = " ".join(str(a) for a in args)
    if ui is not None and getattr(ui, "_active", False):
        ui.log(text)
    else:
        console.print(*args, **kwargs)


def set_status(s: str):
    """Set UI status line when UI is active. No-op otherwise."""
    if ui is not None and getattr(ui, "_active", False):
        try:
            ui.set_status(s)
        except Exception:
            pass


def confirm_provision(port: str, device_id: Optional[str] = None) -> bool:
    """Ask the user to confirm provisioning. Temporarily stops the live UI to accept input."""
    global ui
    # auto-accept if requested
    if globals().get('AUTO_ACCEPT'):
        return True
    # If no UI, just ask via input()
    prompt = f"Provision device {device_id or ''} on port {port}? [y/N]: "
    if ui is None or not getattr(ui, "_active", False) or not _USE_RICH or getattr(ui, 'live', None) is None:
        # no Rich live UI active; fallback to standard input
        try:
            resp = input(prompt)
            return resp.strip().lower() in ("y", "yes")
        except Exception:
            return False

    # If Rich Live is active, prefer pausing the Live renderer so the prompt
    # doesn't cause the UI to jump. Use Live.pause() when available, else
    # fall back to the previous enter/exit approach.
    try:
        try:
            with ui.live.pause():
                # use Rich Console.input to get consistent behavior
                resp = console.input(prompt)
        except Exception:
            # fallback: stop live(), prompt with builtin input, then restart
            try:
                ui.__exit__(None, None, None)
            except Exception:
                pass
            try:
                resp = input(prompt)
            except Exception:
                resp = ''
            try:
                ui.__enter__()
            except Exception:
                pass
        ok = resp.strip().lower() in ("y", "yes")
    except Exception:
        ok = False
    return ok


def set_device_info(info: Dict[str, str]):
    """Convenience wrapper to update the device panel."""
    if ui is not None and getattr(ui, "_active", False):
        try:
            ui.set_device_info(info)
        except Exception:
            pass

# Optional cryptography import for Ed25519 signing
try:
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
    from cryptography.hazmat.primitives import serialization
    _HAS_CRYPTO = True
except Exception:
    Ed25519PrivateKey = None
    Ed25519PublicKey = None
    serialization = None
    _HAS_CRYPTO = False


KEYSTORE_PATH = Path(__file__).resolve().parent / "provision_keys.json"


def list_serial_ports() -> List[str]:
    return [p.device for p in list_ports.comports()]


def wait_for_port(existing: List[str], timeout: Optional[float]) -> Optional[str]:
    """Wait for a new serial port not present in `existing`.

    Uses a Rich status spinner when available, otherwise falls back to simple prints.
    """
    start = time.time()
    set_status("Waiting for new serial port...")
    if _USE_RICH:
        with console.status("", spinner="dots"):
            while True:
                cur = list_serial_ports()
                new = [p for p in cur if p not in existing]
                if new:
                    set_status(f"Detected new port: {new[0]}")
                    cprint(f"Detected new port: {new[0]}")
                    return new[0]
                if timeout is not None and (time.time() - start) > timeout:
                    set_status("Timeout waiting for new serial port")
                    return None
                time.sleep(0.5)
    else:
        spinner = 0
        print("Waiting for new serial port...")
        while True:
            cur = list_serial_ports()
            new = [p for p in cur if p not in existing]
            if new:
                print(f"Detected new port: {new[0]}")
                return new[0]
            if timeout is not None and (time.time() - start) > timeout:
                return None
            # simple progress indicator
            if spinner % 8 == 0:
                print('.', end='', flush=True)
            spinner += 1
            time.sleep(0.5)


def open_serial(port: str, baud: int = 115200, timeout: float = 1.0) -> serial.Serial:
    ser = serial.Serial(port, baudrate=baud, timeout=timeout)
    # flush any existing input
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    return ser


class EmulatedSerial:
    """Simple in-process emulated serial device for testing provision flow without hardware.

    Implements a minimal subset of `serial.Serial` used by this script: `read`, `write`,
    `reset_input_buffer`, `reset_output_buffer`, `close`.
    """
    def __init__(self, device_id: Optional[bytes] = None, delay: float = 0.15):
        # device id 12 bytes
        import os as _os
        if device_id is None:
            self.selfid = _os.urandom(12)
        else:
            self.selfid = device_id
        self.selfid_hex = hex_bytes(self.selfid)
        self._out = bytearray()
        self._inbuf = ""
        self.secret = None
        self.key_version = 0
        self.totalTapCount = 0
        self.linkCount = 0
        self.peers: List[bytes] = []
        # artificial delay between handling lines (seconds) for emulation UI visibility
        self._delay = delay

    def reset_input_buffer(self):
        self._out = bytearray()

    def reset_output_buffer(self):
        self._inbuf = ""

    def close(self):
        pass

    def flush(self):
        # no-op for emulated serial to match `serial.Serial` API
        return

    def write(self, data: bytes):
        # called when script sends a command
        try:
            txt = data.decode()
        except Exception:
            txt = data.decode(errors='ignore')
        self._inbuf += txt
        # process complete lines
        while '\n' in self._inbuf:
            line, self._inbuf = self._inbuf.split('\n', 1)
            line = line.strip()
            if not line:
                continue
            self._handle_line(line)

    def read(self, size: int) -> bytes:
        # return up to size bytes from output buffer
        if not self._out:
            return b""
        n = min(len(self._out), size)
        chunk = bytes(self._out[:n])
        self._out = self._out[n:]
        return chunk

    def _enqueue(self, s: str):
        if not s.endswith('\n'):
            s = s + '\n'
        self._out += s.encode()

    def _handle_line(self, line: str):
        # simulate device processing time so UI updates are visible in emulate mode
        try:
            if getattr(self, '_delay', 0):
                time.sleep(self._delay)
        except Exception:
            pass
        parts = line.split()
        cmd = parts[0].upper()
        if cmd == 'HELLO':
            resp = {
                "event": "hello",
                "device_id": self.selfid_hex,
                "fw": "0.0-test",
                "build": "test",
                "hash": "TEST"
            }
            self._enqueue(json.dumps(resp))
        elif cmd == 'GET_STATE':
            resp = {"event": "state", "totalTapCount": self.totalTapCount, "linkCount": self.linkCount}
            self._enqueue(json.dumps(resp))
        elif cmd == 'DUMP':
            # DUMP <offset> <count>
            offset = 0
            count = 10
            if len(parts) >= 2:
                try:
                    offset = int(parts[1])
                except Exception:
                    offset = 0
            if len(parts) >= 3:
                try:
                    count = int(parts[2])
                except Exception:
                    count = 10
            items = []
            end = min(len(self.peers), offset + count)
            for i in range(offset, end):
                items.append({"peer": hex_bytes(self.peers[i])})
            resp = {"event": "links", "offset": offset, "count": len(items), "items": items}
            self._enqueue(json.dumps(resp))
        elif cmd == 'PROVISION_KEY':
            # PROVISION_KEY <ver> <hex>
            if len(parts) < 3:
                self._enqueue(json.dumps({"event": "error", "msg": "PROVISION_KEY args"}))
                return
            try:
                ver = int(parts[1])
                keyhex = parts[2]
                key = bytes.fromhex(keyhex)
                if len(key) != 32:
                    raise ValueError()
                self.secret = key
                self.key_version = ver
                self._enqueue(json.dumps({"event": "ack", "cmd": "PROVISION_KEY", "keyVersion": ver}))
            except Exception:
                self._enqueue(json.dumps({"event": "error", "msg": "invalid key hex"}))
        elif cmd == 'SIGN_STATE':
            if len(parts) < 2:
                self._enqueue(json.dumps({"event": "error", "msg": "SIGN_STATE args"}))
                return
            if self.secret is None:
                self._enqueue(json.dumps({"event": "error", "msg": "no_key"}))
                return
            noncehex = parts[1]
            try:
                nonce = bytes.fromhex(noncehex)
            except Exception:
                self._enqueue(json.dumps({"event": "error", "msg": "invalid nonce hex"}))
                return
            # Build msg: selfId + nonce + totalTapCount(4 LE) + linkCount(2 LE) + peers
            msg = bytearray()
            msg += self.selfid
            msg += nonce
            msg += int(self.totalTapCount).to_bytes(4, 'little')
            msg += int(self.linkCount).to_bytes(2, 'little')
            for p in self.peers:
                msg += p
            hm = hmac.new(self.secret, msg, hashlib.sha256).digest()
            resp = {
                "event": "SIGNED_STATE",
                "device_id": self.selfid_hex,
                "nonce": noncehex,
                "totalTapCount": self.totalTapCount,
                "linkCount": self.linkCount,
                "keyVersion": self.key_version,
                "hmac": hex_bytes(hm)
            }
            self._enqueue(json.dumps(resp))
        else:
            self._enqueue(json.dumps({"event": "error", "msg": f"unknown command: {cmd}"}))


def balanced_json_objects(s: str) -> List[str]:
    # Extract balanced JSON objects from a string (handles nested braces)
    objs = []
    depth = 0
    start = None
    for i, ch in enumerate(s):
        if ch == '{':
            if depth == 0:
                start = i
            depth += 1
        elif ch == '}':
            if depth > 0:
                depth -= 1
                if depth == 0 and start is not None:
                    objs.append(s[start:i+1])
                    start = None
    return objs


def read_json_line(ser: serial.Serial, timeout: float = 5.0) -> Optional[Dict[str, Any]]:
    deadline = time.time() + timeout
    buffer = ""
    set_status("Waiting for device response...")
    while time.time() < deadline:
        chunk = ser.read(128).decode(errors='ignore')
        if chunk:
            buffer += chunk
            # look for JSON object(s)
            for objtxt in balanced_json_objects(buffer):
                try:
                    data = json.loads(objtxt)
                    # remove consumed portion from buffer
                    buffer = buffer.split(objtxt, 1)[1]
                    try:
                        pretty = json.dumps(data, indent=2)
                        cprint("<<<")
                        cprint(pretty)
                    except Exception:
                        cprint("<<< " + objtxt)
                    # update status with event type if available
                    try:
                        ev = data.get("event") if isinstance(data, dict) else None
                        if ev:
                            set_status(f"Received: {ev}")
                        else:
                            set_status("Received data")
                    except Exception:
                        pass
                    return data
                except Exception:
                    # skip parse error and continue reading
                    pass
        else:
            time.sleep(0.05)
    return None


def send_cmd(ser: serial.Serial, cmd: str) -> None:
    # show command for visibility
    set_status(f"Sending: {cmd}")
    cprint(f">>> {cmd}")
    # force an immediate refresh so the command is visible in the Live UI
    try:
        if ui is not None and getattr(ui, '_active', False) and _USE_RICH and getattr(ui, 'live', None) is not None:
            try:
                ui.live.update(ui._render())
            except Exception:
                pass
    except Exception:
        pass
    if not cmd.endswith('\n'):
        cmd = cmd + '\n'
    ser.write(cmd.encode())
    ser.flush()


def hex_bytes(b: bytes) -> str:
    return b.hex().upper()


def hex_to_bytes(h: str) -> bytes:
    return bytes.fromhex(h)


def provision_device(ser: serial.Serial, key_version: int = 1, validate: bool = True, store_path: Path = KEYSTORE_PATH, master_key: Optional[bytes] = None, master_signing_key: Optional[Any] = None) -> Dict[str, Any]:
    # 1) HELLO
    set_status("Step: HELLO")
    send_cmd(ser, "HELLO")
    hello = read_json_line(ser, timeout=2.0)
    if not hello or hello.get("event") != "hello":
        raise RuntimeError(f"No HELLO reply, got: {hello}")

    device_id_hex = hello.get("device_id")
    if not device_id_hex:
        raise RuntimeError("HELLO reply missing device_id")

    cprint(f"[ ] Device: {device_id_hex} - discovered")
    # update device panel with info from HELLO if available
    try:
        devinfo = {
            "device_id": device_id_hex,
            "fw": hello.get("fw") or "",
            "build": hello.get("build") or "",
            "hash": hello.get("hash") or "",
        }
        set_device_info(devinfo)
    except Exception:
        pass
    set_status(f"Device: {device_id_hex}")

    # 2) generate secret key (always random). If a master signing key is provided, sign the secret so
    # the mapping can be later validated with the corresponding public key.
    secret = secrets.token_bytes(32)
    secret_hex = hex_bytes(secret)
    cprint(f"[i] Secret generated randomly")
    set_status("Generated secret")

    # If master_signing_key provided, prepare to sign the (device_id || secret || key_version)
    signature_hex = None
    master_pub_hex = None
    if master_signing_key is not None:
        if not _HAS_CRYPTO:
            raise RuntimeError("cryptography package required for master signing key")
        try:
            device_bytes = hex_to_bytes(device_id_hex)
        except Exception as e:
            raise RuntimeError(f"invalid device id hex for signing: {e}")
        msg_to_sign = device_bytes + secret + bytes([key_version & 0xFF])
        sig = master_signing_key.sign(msg_to_sign)
        signature_hex = hex_bytes(sig)
        # export public key raw bytes (32 bytes)
        pub = master_signing_key.public_key().public_bytes(encoding=serialization.Encoding.Raw, format=serialization.PublicFormat.Raw)
        master_pub_hex = hex_bytes(pub)
        cprint("[i] Secret signed by master key")

    # 3) send PROVISION_KEY <ver> <hex>
    set_status("Sending PROVISION_KEY")
    send_cmd(ser, f"PROVISION_KEY {key_version} {secret_hex}")
    ack = read_json_line(ser, timeout=2.0)
    if not ack or ack.get("event") != "ack" or ack.get("cmd") != "PROVISION_KEY":
        raise RuntimeError(f"Provision ack missing or bad: {ack}")
    cprint(f"[✓] Provisioned: {device_id_hex}")
    set_status("Provisioned")

    # 4) Optional validation: call GET_STATE, DUMP, then SIGN_STATE with nonce
    validation = None
    if validate:
        # request state
        set_status("Validating: GET_STATE")
        send_cmd(ser, "GET_STATE")
        st = read_json_line(ser, timeout=2.0)
        if not st or st.get("event") != "state":
            raise RuntimeError(f"GET_STATE failed: {st}")
        totalTapCount = int(st.get("totalTapCount", 0))
        linkCount = int(st.get("linkCount", 0))

        # request links
        set_status("Validating: DUMP")
        send_cmd(ser, f"DUMP 0 {linkCount}")
        linksmsg = read_json_line(ser, timeout=2.0)
        if not linksmsg or linksmsg.get("event") != "links":
            raise RuntimeError(f"DUMP failed: {linksmsg}")
        items = linksmsg.get("items", [])
        peers = [hex_to_bytes(it.get("peer")) for it in items]

        # create nonce
        nonce = secrets.token_bytes(16)
        nonce_hex = hex_bytes(nonce)

        set_status("Validating: SIGN_STATE")
        send_cmd(ser, f"SIGN_STATE {nonce_hex}")
        signed = read_json_line(ser, timeout=2.0)
        if not signed or signed.get("event") != "SIGNED_STATE":
            raise RuntimeError(f"SIGN_STATE failed: {signed}")

        # Build message as device does: selfId(12) + nonce + totalTapCount(4 LE) + linkCount(2 LE) + each peer(12)
        selfid = hex_to_bytes(device_id_hex)
        if len(selfid) != 12:
            raise RuntimeError(f"unexpected device id length: {len(selfid)}")

        msg = bytearray()
        msg += selfid
        msg += nonce
        # totalTapCount 4 bytes little endian
        msg += int(totalTapCount).to_bytes(4, 'little')
        # linkCount 2 bytes little endian
        msg += int(len(peers)).to_bytes(2, 'little')
        for p in peers:
            if len(p) != 12:
                raise RuntimeError("peer id length unexpected")
            msg += p

        # compute HMAC-SHA256
        hm = hmac.new(secret, msg, hashlib.sha256).digest()
        hm_hex = hex_bytes(hm)

        if hm_hex.upper() != signed.get("hmac", "").upper():
            raise RuntimeError("HMAC validation failed: computed != device")

        validation = {
            "nonce": nonce_hex,
            "hmac": signed.get("hmac"),
            "totalTapCount": totalTapCount,
            "linkCount": len(peers),
        }
        cprint(f"[✓] Verified: {device_id_hex}")
        set_status("Verified")

    set_status("Saving mapping")
    # Persist mapping
    entry = {
        "device_id": device_id_hex,
        "key_version": key_version,
        "secret_key": secret_hex,
        "provisioned_at": int(time.time()),
        "validation": validation,
    }
    if signature_hex is not None:
        entry["master_signature"] = signature_hex
        entry["master_public_key"] = master_pub_hex

    # load existing file
    data = {}
    if store_path.exists():
        try:
            data = json.loads(store_path.read_text())
        except Exception:
            data = {}

    data[device_id_hex] = entry
    store_path.write_text(json.dumps(data, indent=2))

    return entry


def wait_for_removal(port_name: str, poll_interval: float = 0.5):
    """Block until `port_name` no longer appears in the system's port list."""
    try:
        while True:
            cur = list_serial_ports()
            if port_name not in cur:
                return
            time.sleep(poll_interval)
    except KeyboardInterrupt:
        return


def main(argv: Optional[List[str]] = None) -> int:
    p = argparse.ArgumentParser(description="Provision Bokaka-Eval device over USB serial")
    p.add_argument("--port", help="Serial port (e.g. COM3). If omitted, script waits for new ports and provisions them automatically.")
    p.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    p.add_argument("--timeout", type=float, default=30.0, help="Max time to wait for a device (seconds) when waiting for a new port)")
    p.add_argument("--no-validate", action="store_true", help="Skip signing validation step")
    p.add_argument("--key-version", type=int, default=1, help="Key version number to provision")
    p.add_argument("--out", help="Output JSON file path (default: utils/provision_keys.json)")
    p.add_argument("--master-key-hex", help="Master key in HEX (32 bytes seed) used to sign provisioned secrets (Ed25519). Can also be provided via BOKAKA_MASTER_KEY env var.")
    p.add_argument("--master-key-pem", help="Path to PEM file containing an Ed25519 private key to sign provisioned secrets.")
    p.add_argument("--emulate", action="store_true", help="Run with an in-process emulated device for testing (single run)")
    p.add_argument("--emulate-delay", type=float, default=0.3, help="Delay in seconds between emulated device responses (for UI visibility)")
    p.add_argument("--no-confirm", dest="no_confirm", action="store_true", help="Skip interactive confirmation prompts and accept provisioning automatically")
    p.add_argument("--once", action="store_true", help="Provision a single device then exit")
    args = p.parse_args(argv)

    # Initialize UI manager early so subsequent output is routed into the
    # in-place UI (no scrolling). We register an atexit handler to ensure
    # the live UI is cleaned up on exit.
    global ui
    ui = UIManager()
    try:
        ui.__enter__()
    except Exception:
        # If Rich isn't available or Live can't start, proceed with fallback prints
        pass
    # honor auto-accept / skip-confirm flag
    global AUTO_ACCEPT
    AUTO_ACCEPT = bool(getattr(args, "no_confirm", False))
    try:
        import atexit
        atexit.register(lambda: ui.__exit__(None, None, None))
    except Exception:
        pass

    store_path = Path(args.out) if args.out else KEYSTORE_PATH

    # master key: CLI argument takes precedence, else env var BOKAKA_MASTER_KEY
    master_key_hex = args.master_key_hex or os.environ.get('BOKAKA_MASTER_KEY')
    master_key_bytes: Optional[bytes] = None
    master_signing_key: Optional[Any] = None
    if args.master_key_pem:
        if not _HAS_CRYPTO:
            cprint("cryptography package is required to load PEM keys. Install with: pip install cryptography")
            return 4
        try:
            pem_data = Path(args.master_key_pem).read_bytes()
            master_signing_key = serialization.load_pem_private_key(pem_data, password=None)
            if not isinstance(master_signing_key, Ed25519PrivateKey):
                cprint("PEM key is not an Ed25519 private key")
                return 5
        except Exception as e:
            cprint(f"Failed to load PEM key: {e}")
            return 5
    elif master_key_hex:
        try:
            master_key_bytes = bytes.fromhex(master_key_hex)
        except Exception:
            cprint("Invalid master key hex provided; must be hex string")
            return 3
        # If cryptography available, treat 32-byte hex as Ed25519 seed for private key
        if _HAS_CRYPTO:
            if len(master_key_bytes) != 32:
                cprint("Master key hex must be 32 bytes (64 hex chars) for Ed25519 seed")
                return 6
            try:
                master_signing_key = Ed25519PrivateKey.from_private_bytes(master_key_bytes)
            except Exception as e:
                cprint(f"Failed to construct Ed25519 key from seed: {e}")
                return 7
        else:
            cprint("cryptography package is required to use master key signing. Install with: pip install cryptography")
            return 4

    # If emulate flag provided, run a single emulated device and exit
    if args.emulate:
        cprint("Running in emulation mode (single-run)")
        ser = EmulatedSerial(delay=args.emulate_delay)
        try:
            # do a quick HELLO to populate device info and ask for confirmation
            try:
                send_cmd(ser, "HELLO")
                hello = read_json_line(ser, timeout=1.0)
                dev_id = (hello or {}).get("device_id") if isinstance(hello, dict) else None
            except Exception:
                hello = None
                dev_id = None

            # If not in no-confirm mode, ask user whether to proceed
            if not getattr(args, "no_confirm", False):
                ok = confirm_provision("EMULATED", dev_id)
                if not ok:
                    cprint("Skipping emulated provisioning.")
                    try:
                        ser.close()
                    except Exception:
                        pass
                    return 0

            entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
            cprint("Provisioning succeeded (emulated):")
            cprint(json.dumps(entry, indent=2))
            return 0
        except Exception as e:
            cprint(f"Provisioning error (emulated): {e}")
            return 1

    try:
        if args.port:
            # If user passed a specific port, provision once (or repeatedly if not --once)
            if args.once:
                cprint(f"Provisioning {args.port} once...")
                ser = open_serial(args.port, args.baud, timeout=0.2)
                try:
                    entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
                    cprint("Provisioning succeeded:")
                    cprint(json.dumps(entry, indent=2))
                finally:
                    ser.close()
                return 0

            # Loop: wait for the port to be present, provision, then wait until it's removed before next iteration
            cprint(f"Watching for port {args.port} and provisioning whenever it appears. Press Ctrl+C to stop.")
            while True:
                cur = list_serial_ports()
                if args.port in cur:
                    cprint(f"Detected {args.port}, opening...")
                    try:
                        ser = open_serial(args.port, args.baud, timeout=0.2)
                        # Try a quick HELLO to show device info before confirming
                        try:
                            send_cmd(ser, "HELLO")
                            hello = read_json_line(ser, timeout=1.0)
                            dev_id = (hello or {}).get("device_id") if isinstance(hello, dict) else None
                        except Exception:
                            hello = None
                            dev_id = None

                        if not args.no_validate:
                            # prompt user to confirm provisioning
                            ok = confirm_provision(args.port, dev_id)
                            if not ok:
                                cprint(f"Skipping provisioning of {args.port}")
                                try:
                                    ser.close()
                                except Exception:
                                    pass
                                time.sleep(0.2)
                                continue

                        entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
                        cprint("Provisioning succeeded:")
                        cprint(json.dumps(entry, indent=2))
                    except Exception as e:
                        cprint(f"Provisioning error for {args.port}: {e}")
                    finally:
                        try:
                            ser.close()
                        except Exception:
                            pass
                    cprint(f"Waiting for {args.port} to be removed before next cycle...")
                    wait_for_removal(args.port)
                    cprint(f"{args.port} removed; resuming watch for re-attachment.")
                else:
                    time.sleep(0.5)

        else:
            # No explicit port: automatically wait for any new port and provision it, then continue until cancelled
            cprint("Waiting for new serial ports. Provisioning devices automatically when they appear. Press Ctrl+C to stop.")
            seen = set(list_serial_ports())
            while True:
                port = wait_for_port(list(seen), timeout=args.timeout)
                if not port:
                    cprint("No new serial port detected within timeout; continuing to wait.")
                    seen = set(list_serial_ports())
                    continue
                cprint(f"Detected new port {port}, attempting to provision...")
                try:
                    ser = open_serial(port, args.baud, timeout=0.2)
                    # quick HELLO to show device info before asking
                    try:
                        send_cmd(ser, "HELLO")
                        hello = read_json_line(ser, timeout=1.0)
                        dev_id = (hello or {}).get("device_id") if isinstance(hello, dict) else None
                    except Exception:
                        hello = None
                        dev_id = None

                    # ask for confirmation unless --no-validate used; allow skipping
                    ok = True
                    if not args.no_validate:
                        ok = confirm_provision(port, dev_id)
                    if not ok:
                        cprint(f"Skipping provisioning of {port}")
                        try:
                            ser.close()
                        except Exception:
                            pass
                        seen = set(list_serial_ports())
                        continue

                    entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
                    cprint("Provisioning succeeded:")
                    cprint(json.dumps(entry, indent=2))
                except Exception as e:
                    cprint(f"Provisioning error for {port}: {e}")
                finally:
                    try:
                        ser.close()
                    except Exception:
                        pass

                # After provisioning, wait until device is removed before listening for the next new port
                cprint(f"Waiting for {port} to be removed before next cycle...")
                wait_for_removal(port)
                cprint(f"{port} removed; resuming watch for new ports.")
                # update seen list to current
                seen = set(list_serial_ports())

    except KeyboardInterrupt:
        cprint("Interrupted by user, exiting.")
        return 0

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
