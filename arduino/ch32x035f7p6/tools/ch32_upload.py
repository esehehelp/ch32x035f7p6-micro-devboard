#!/usr/bin/env python3
"""
ch32_upload.py — 1200bps touch + wchisp upload for Arduino IDE.

Usage (called from platform.txt):
    python3 ch32_upload.py <serial_port> <firmware.bin>

Sends 1200bps CDC touch to trigger BootROM, waits for BootROM, then
flashes with wchisp.

SPDX-License-Identifier: MIT
"""

import os
import sys
import shutil
import subprocess
import time

BOOTROM_PID = 0x55E0
BOOTROM_VIDS = (0x4348, 0x1A86)


def find_wchisp():
    """Locate wchisp binary."""
    # setup.ps1 installs a private copy, so Arduino does not depend on PATH.
    local_name = "wchisp.exe" if os.name == "nt" else "wchisp"
    local_wchisp = os.path.join(os.path.dirname(os.path.abspath(__file__)), local_name)
    if os.path.isfile(local_wchisp):
        return local_wchisp
    # Check PATH first
    w = shutil.which("wchisp")
    if w:
        return w
    # Check common PlatformIO install
    home = os.path.expanduser("~")
    pio_wchisp = os.path.join(home, ".platformio", "packages", "tool-wchisp", "wchisp")
    if os.path.isfile(pio_wchisp):
        return pio_wchisp
    # Check cargo install location
    cargo_wchisp = os.path.join(home, ".cargo", "bin", "wchisp")
    if os.path.isfile(cargo_wchisp):
        return cargo_wchisp
    return None


def lsusb_has(vids, pid):
    if not sys.platform.startswith("linux"):
        return False
    try:
        return any(
            subprocess.run(
                ["lsusb", "-d", "%04x:%04x" % (vid, pid)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            ).returncode == 0
            for vid in vids
        )
    except FileNotFoundError:
        return False


def wchisp_probe_has_device(wchisp):
    try:
        r = subprocess.run(
            [wchisp, "probe"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=2.0,
        )
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False

    out = r.stdout or ""
    if "Found 0 device" in out:
        return False
    return ("Found " in out and "device" in out) or "CH32" in out


def wait_for_bootrom(wchisp, timeout=8.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if lsusb_has(BOOTROM_VIDS, BOOTROM_PID) or wchisp_probe_has_device(wchisp):
            return True
        time.sleep(0.1)
    return False


def main():
    if len(sys.argv) < 3:
        print("Usage: %s <serial_port> <firmware.bin>" % sys.argv[0], file=sys.stderr)
        sys.exit(2)

    port = sys.argv[1]
    firmware = sys.argv[2]

    wchisp = find_wchisp()
    if not wchisp:
        print("ERROR: wchisp not found. Re-run setup.bat (Windows) or install wchisp.", file=sys.stderr)
        sys.exit(1)

    if not os.path.isfile(firmware):
        print("ERROR: firmware file not found: %s" % firmware, file=sys.stderr)
        sys.exit(1)

    # 1200bps touch to trigger BootROM
    touched = False
    if port and port != "none":
        try:
            import serial
            print("Triggering BootROM via 1200bps touch on %s" % port)
            serial.Serial(port, 1200).close()
            touched = True
        except ImportError:
            print("WARNING: pyserial not installed, trying raw open")
            try:
                import termios
                fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
                attrs = termios.tcgetattr(fd)
                termios.cfsetispeed(attrs, termios.B1200)
                termios.cfsetospeed(attrs, termios.B1200)
                termios.tcsetattr(fd, termios.TCSANOW, attrs)
                os.close(fd)
                touched = True
            except Exception as e:
                print("WARNING: 1200bps touch failed: %s" % e)
        except Exception as e:
            print("WARNING: 1200bps touch failed: %s" % e)
    else:
        print("No serial port specified, trying wchisp directly")

    if touched:
        if wait_for_bootrom(wchisp):
            print("BootROM detected")
        else:
            print("BootROM not detected before timeout -- trying wchisp anyway")

    # Flash with wchisp
    print("Flashing %s" % firmware)
    r = subprocess.run([wchisp, "flash", firmware])
    if r.returncode != 0:
        print("Retrying...")
        if touched:
            wait_for_bootrom(wchisp, timeout=2.0)
        else:
            time.sleep(1.0)
        r = subprocess.run([wchisp, "flash", firmware])

    sys.exit(r.returncode)


if __name__ == "__main__":
    main()
