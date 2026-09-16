"""
CH32 1200bps-touch + wchisp upload for PlatformIO.

Sends 1200bps CDC touch to trigger BootROM, waits for BootROM, then
flashes with wchisp.

platformio.ini:
    upload_protocol = ch32-1k2touch
    extra_scripts = pre:ch32_1k2touch.py
"""

import os
import sys
import subprocess
import time

CDC_VID = 0x1A86
CDC_PID = 0xFE0C
BOOTROM_PID = 0x55E0
BOOTROM_VIDS = (0x4348, 0x1A86)


def _find_cdc_port(vid=CDC_VID, pid=CDC_PID):
    from serial.tools.list_ports import comports
    for p in comports():
        if p.vid == vid and p.pid == pid:
            return p.device
    return None


def _lsusb_has(vids, pid):
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


def _wchisp_probe_has_device(wchisp):
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


def _wait_for_bootrom(wchisp, timeout=8.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if _lsusb_has(BOOTROM_VIDS, BOOTROM_PID) or _wchisp_probe_has_device(wchisp):
            return True
        time.sleep(0.1)
    return False


def _flash(wchisp, firmware):
    if firmware.endswith(".elf"):
        bin_path = firmware[:-4] + ".bin"
        if os.path.isfile(bin_path):
            firmware = bin_path

    port = _find_cdc_port()
    touched = False
    if port:
        import serial
        print("Triggering BootROM via 1200bps touch on %s" % port)
        serial.Serial(port, 1200).close()
        touched = True
        if _wait_for_bootrom(wchisp):
            print("BootROM detected")
        else:
            print("BootROM not detected before timeout -- trying wchisp anyway")
    else:
        print("CDC port not found -- trying wchisp directly")

    print("Flashing %s" % firmware)
    r = subprocess.run([wchisp, "flash", firmware])
    if r.returncode != 0 and touched:
        print("Retrying...")
        _wait_for_bootrom(wchisp, timeout=2.0)
        r = subprocess.run([wchisp, "flash", firmware])

    return r.returncode


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: %s <wchisp> <firmware>" % sys.argv[0], file=sys.stderr)
        sys.exit(2)
    sys.exit(_flash(sys.argv[1], sys.argv[2]))
else:
    Import("env")  # type: ignore  # noqa: F821
    if env.GetProjectOption("upload_protocol", "") == "ch32-1k2touch":  # type: ignore
        env.Replace(UPLOAD_PROTOCOL="custom")  # type: ignore
        _package_wchisp = os.path.join(
            env.PioPlatform().get_package_dir("tool-wchisp") or "", "wchisp"  # type: ignore
        )
        _local_wchisp = os.path.join(
            env.subst("$PROJECT_DIR"), "tools", "wchisp.exe"  # type: ignore
        )
        # Both curl-based and local setup install a current build that uses
        # WCH's native CH375 backend instead of requiring Zadig/WinUSB.
        _wchisp = _package_wchisp
        if os.name == "nt":
            shared_root = os.environ.get("LOCALAPPDATA")
            if shared_root:
                _shared_wchisp = os.path.join(
                    shared_root, "CH32X035", "tools", "wchisp.exe"
                )
                if os.path.isfile(_shared_wchisp):
                    _wchisp = _shared_wchisp
            if os.path.isfile(_local_wchisp):
                _wchisp = _local_wchisp
        _script = os.path.join(env.subst("$PROJECT_DIR"), "ch32_1k2touch.py")  # type: ignore
        env.Replace(  # type: ignore
            UPLOADCMD='"%s" "%s" "%s" "$SOURCE"'
            % (sys.executable, _script, _wchisp)
        )
