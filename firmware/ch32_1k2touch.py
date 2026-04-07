"""
CH32 1200bps-touch + wchisp upload for PlatformIO.

Sends 1200bps CDC touch to trigger BootROM, then flashes with wchisp.

platformio.ini:
    upload_protocol = ch32-1k2touch
    extra_scripts = pre:ch32_1k2touch.py
"""

import os
import sys
import subprocess
import time


def _find_cdc_port(vid=0x1A86, pid=0xFE0C):
    from serial.tools.list_ports import comports
    for p in comports():
        if p.vid == vid and p.pid == pid:
            return p.device
    return None


def _flash(wchisp, firmware):
    if firmware.endswith(".elf"):
        bin_path = firmware[:-4] + ".bin"
        if os.path.isfile(bin_path):
            firmware = bin_path

    port = _find_cdc_port()
    if port:
        import serial
        print("Triggering BootROM via 1200bps touch on %s" % port)
        serial.Serial(port, 1200).close()
        time.sleep(2.0)
    else:
        print("CDC port not found -- trying wchisp directly")

    print("Flashing %s" % firmware)
    r = subprocess.run([wchisp, "flash", firmware])
    if r.returncode != 0 and port:
        print("Retrying...")
        time.sleep(1.0)
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
        _wchisp = os.path.join(
            env.PioPlatform().get_package_dir("tool-wchisp") or "", "wchisp"  # type: ignore
        )
        _script = os.path.join(env.subst("$PROJECT_DIR"), "ch32_1k2touch.py")  # type: ignore
        env.Replace(  # type: ignore
            UPLOADCMD='"%s" "%s" "%s" "$SOURCE"'
            % (sys.executable, _script, _wchisp)
        )
