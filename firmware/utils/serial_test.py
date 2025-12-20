#!/usr/bin/env python3
r"""Serial test helper for Bokaka-Eval device.

Usage examples (PowerShell):
  python .\utils\serial_test.py --port COM3 --cmds HELLO,GET_STATE
  python .\utils\serial_test.py --interactive

Features:
- Auto-detects a single serial port if none provided (prompts when multiple).
- Sends commands (newline-terminated) and prints any JSON objects received.
- Interactive mode: type commands, or 'exit' to quit.
"""
from __future__ import annotations
import argparse
import json
import sys
import time
import serial
from serial.tools import list_ports
from typing import List, Optional


def balanced_json_objects(s: str) -> List[str]:
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


def list_serial_ports() -> List[str]:
    return [p.device for p in list_ports.comports()]


def choose_port(interactive: bool) -> Optional[str]:
    ports = list_serial_ports()
    if not ports:
        print("No serial ports found.")
        return None
    if len(ports) == 1:
        return ports[0]
    if not interactive:
        # choose the first by default
        return ports[0]
    # interactive: prompt user
    print("Multiple serial ports found:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p}")
    try:
        idx = int(input("Choose port index: "))
        if 0 <= idx < len(ports):
            return ports[idx]
    except Exception:
        pass
    return None


def read_json_line(ser: serial.Serial, timeout: float = 2.0) -> Optional[dict]:
    deadline = time.time() + timeout
    buffer = ""
    while time.time() < deadline:
        chunk = ser.read(256).decode(errors='ignore')
        if chunk:
            buffer += chunk
            for objtxt in balanced_json_objects(buffer):
                try:
                    data = json.loads(objtxt)
                    # consume
                    buffer = buffer.split(objtxt, 1)[1]
                    return data
                except Exception:
                    # ignore parse errors and continue
                    pass
        else:
            time.sleep(0.05)
    return None


def send_cmd(ser: serial.Serial, cmd: str) -> None:
    if not cmd.endswith('\n'):
        cmd = cmd + '\n'
    print(f">>> {cmd.strip()}")
    ser.write(cmd.encode())
    ser.flush()


def pretty_print(data: dict) -> None:
    try:
        print(json.dumps(data, indent=2))
    except Exception:
        print(str(data))


def run_once(ser: serial.Serial, cmds: List[str], timeout: float) -> None:
    for cmd in cmds:
        send_cmd(ser, cmd)
        # try to read multiple JSON responses until timeout
        start = time.time()
        while True:
            data = read_json_line(ser, timeout=timeout)
            if data is None:
                break
            pretty_print(data)
            # short delay to allow more responses
            if (time.time() - start) > timeout:
                break


def interactive_loop(ser: serial.Serial) -> None:
    print("Interactive mode. Type commands to send (e.g. HELLO). Type 'exit' to quit.")
    try:
        while True:
            try:
                cmd = input('> ').strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if not cmd:
                continue
            if cmd.lower() in ('exit', 'quit'):
                break
            send_cmd(ser, cmd)
            # read responses for a short period
            t0 = time.time()
            while True:
                data = read_json_line(ser, timeout=0.5)
                if data is None:
                    break
                pretty_print(data)
                if (time.time() - t0) > 1.0:
                    break
    except KeyboardInterrupt:
        pass


def main(argv: Optional[List[str]] = None) -> int:
    p = argparse.ArgumentParser(description='Serial test helper for Bokaka-Eval')
    p.add_argument('--port', help='Serial port (e.g. COM3)')
    p.add_argument('--baud', type=int, default=115200)
    p.add_argument('--timeout', type=float, default=2.0, help='Read timeout (s)')
    p.add_argument('--cmds', help='Comma-separated commands to send (e.g. HELLO,GET_STATE)')
    p.add_argument('--interactive', action='store_true', help='Interactive mode')
    args = p.parse_args(argv)

    port = args.port
    if not port:
        port = choose_port(interactive=args.interactive)
    if not port:
        print('No port selected. Exiting.')
        return 1

    try:
        ser = serial.Serial(port, baudrate=args.baud, timeout=0.1)
        # Small delay to allow serial port and device to stabilize after connection
        time.sleep(0.1)
        # Flush any stale data from the port
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception as e:
        print(f'Failed to open {port}: {e}')
        return 2

    print(f'Opened {port} @ {args.baud}')

    try:
        if args.cmds:
            cmds = [c.strip() for c in args.cmds.split(',') if c.strip()]
            run_once(ser, cmds, timeout=args.timeout)
        elif args.interactive:
            interactive_loop(ser)
        else:
            # default sequence
            run_once(ser, ['HELLO', 'GET_STATE', 'DUMP 0 10'], timeout=args.timeout)
    finally:
        try:
            ser.close()
        except Exception:
            pass

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
