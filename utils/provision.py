#!/usr/bin/env python3
"""
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

import serial
from serial.tools import list_ports

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
    start = time.time()
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
    def __init__(self, device_id: Optional[bytes] = None):
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

    def reset_input_buffer(self):
        self._out = bytearray()

    def reset_output_buffer(self):
        self._inbuf = ""

    def close(self):
        pass

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
                    # Print the raw response for visibility
                    try:
                        pretty = json.dumps(data, indent=2)
                        print(f"<<< {pretty}")
                    except Exception:
                        print(f"<<< {objtxt}")
                    return data
                except Exception:
                    # skip parse error and continue reading
                    pass
        else:
            time.sleep(0.05)
    return None


def send_cmd(ser: serial.Serial, cmd: str) -> None:
    # show command for visibility
    print(f">>> {cmd}")
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
    send_cmd(ser, "HELLO")
    hello = read_json_line(ser, timeout=2.0)
    if not hello or hello.get("event") != "hello":
        raise RuntimeError(f"No HELLO reply, got: {hello}")

    device_id_hex = hello.get("device_id")
    if not device_id_hex:
        raise RuntimeError("HELLO reply missing device_id")

    print(f"[ ] Device: {device_id_hex} - discovered")

    # 2) generate secret key (always random). If a master signing key is provided, sign the secret so
    # the mapping can be later validated with the corresponding public key.
    secret = secrets.token_bytes(32)
    secret_hex = hex_bytes(secret)
    print(f"[i] Secret generated randomly")

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
        print("[i] Secret signed by master key")

    # 3) send PROVISION_KEY <ver> <hex>
    send_cmd(ser, f"PROVISION_KEY {key_version} {secret_hex}")
    ack = read_json_line(ser, timeout=2.0)
    if not ack or ack.get("event") != "ack" or ack.get("cmd") != "PROVISION_KEY":
        raise RuntimeError(f"Provision ack missing or bad: {ack}")
    print(f"[✓] Provisioned: {device_id_hex}")

    # 4) Optional validation: call GET_STATE, DUMP, then SIGN_STATE with nonce
    validation = None
    if validate:
        # request state
        send_cmd(ser, "GET_STATE")
        st = read_json_line(ser, timeout=2.0)
        if not st or st.get("event") != "state":
            raise RuntimeError(f"GET_STATE failed: {st}")
        totalTapCount = int(st.get("totalTapCount", 0))
        linkCount = int(st.get("linkCount", 0))

        # request links
        send_cmd(ser, f"DUMP 0 {linkCount}")
        linksmsg = read_json_line(ser, timeout=2.0)
        if not linksmsg or linksmsg.get("event") != "links":
            raise RuntimeError(f"DUMP failed: {linksmsg}")
        items = linksmsg.get("items", [])
        peers = [hex_to_bytes(it.get("peer")) for it in items]

        # create nonce
        nonce = secrets.token_bytes(16)
        nonce_hex = hex_bytes(nonce)

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
        print(f"[✓] Verified: {device_id_hex}")

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
    p.add_argument("--once", action="store_true", help="Provision a single device then exit")
    args = p.parse_args(argv)

    store_path = Path(args.out) if args.out else KEYSTORE_PATH

    # master key: CLI argument takes precedence, else env var BOKAKA_MASTER_KEY
    master_key_hex = args.master_key_hex or os.environ.get('BOKAKA_MASTER_KEY')
    master_key_bytes: Optional[bytes] = None
    master_signing_key: Optional[Any] = None
    if args.master_key_pem:
        if not _HAS_CRYPTO:
            print("cryptography package is required to load PEM keys. Install with: pip install cryptography")
            return 4
        try:
            pem_data = Path(args.master_key_pem).read_bytes()
            master_signing_key = serialization.load_pem_private_key(pem_data, password=None)
            if not isinstance(master_signing_key, Ed25519PrivateKey):
                print("PEM key is not an Ed25519 private key")
                return 5
        except Exception as e:
            print(f"Failed to load PEM key: {e}")
            return 5
    elif master_key_hex:
        try:
            master_key_bytes = bytes.fromhex(master_key_hex)
        except Exception:
            print("Invalid master key hex provided; must be hex string")
            return 3
        # If cryptography available, treat 32-byte hex as Ed25519 seed for private key
        if _HAS_CRYPTO:
            if len(master_key_bytes) != 32:
                print("Master key hex must be 32 bytes (64 hex chars) for Ed25519 seed")
                return 6
            try:
                master_signing_key = Ed25519PrivateKey.from_private_bytes(master_key_bytes)
            except Exception as e:
                print(f"Failed to construct Ed25519 key from seed: {e}")
                return 7
        else:
            print("cryptography package is required to use master key signing. Install with: pip install cryptography")
            return 4

    # If emulate flag provided, run a single emulated device and exit
    if args.emulate:
        print("Running in emulation mode (single-run)")
        ser = EmulatedSerial()
        try:
            entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
            print("Provisioning succeeded (emulated):")
            print(json.dumps(entry, indent=2))
            return 0
        except Exception as e:
            print(f"Provisioning error (emulated): {e}")
            return 1

    try:
        if args.port:
            # If user passed a specific port, provision once (or repeatedly if not --once)
            if args.once:
                print(f"Provisioning {args.port} once...")
                ser = open_serial(args.port, args.baud, timeout=0.2)
                try:
                    entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
                    print("Provisioning succeeded:")
                    print(json.dumps(entry, indent=2))
                finally:
                    ser.close()
                return 0

            # Loop: wait for the port to be present, provision, then wait until it's removed before next iteration
            print(f"Watching for port {args.port} and provisioning whenever it appears. Press Ctrl+C to stop.")
            while True:
                cur = list_serial_ports()
                if args.port in cur:
                    print(f"Detected {args.port}, opening...")
                    try:
                        ser = open_serial(args.port, args.baud, timeout=0.2)
                        entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
                        print("Provisioning succeeded:")
                        print(json.dumps(entry, indent=2))
                    except Exception as e:
                        print(f"Provisioning error for {args.port}: {e}")
                    finally:
                        try:
                            ser.close()
                        except Exception:
                            pass
                    print(f"Waiting for {args.port} to be removed before next cycle...")
                    wait_for_removal(args.port)
                    print(f"{args.port} removed; resuming watch for re-attachment.")
                else:
                    time.sleep(0.5)

        else:
            # No explicit port: automatically wait for any new port and provision it, then continue until cancelled
            print("Waiting for new serial ports. Provisioning devices automatically when they appear. Press Ctrl+C to stop.")
            seen = set(list_serial_ports())
            while True:
                port = wait_for_port(list(seen), timeout=args.timeout)
                if not port:
                    print("No new serial port detected within timeout; continuing to wait.")
                    seen = set(list_serial_ports())
                    continue

                print(f"Detected new port {port}, attempting to provision...")
                try:
                    ser = open_serial(port, args.baud, timeout=0.2)
                    entry = provision_device(ser, key_version=args.key_version, validate=not args.no_validate, store_path=store_path, master_key=master_key_bytes, master_signing_key=master_signing_key)
                    print("Provisioning succeeded:")
                    print(json.dumps(entry, indent=2))
                except Exception as e:
                    print(f"Provisioning error for {port}: {e}")
                finally:
                    try:
                        ser.close()
                    except Exception:
                        pass

                # After provisioning, wait until device is removed before listening for the next new port
                print(f"Waiting for {port} to be removed before next cycle...")
                wait_for_removal(port)
                print(f"{port} removed; resuming watch for new ports.")
                # update seen list to current
                seen = set(list_serial_ports())

    except KeyboardInterrupt:
        print("Interrupted by user, exiting.")
        return 0

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
