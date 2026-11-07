"""Capture an explicitly chosen serial port; does not flash or issue commands."""
import argparse
import json
import re
from pathlib import Path
import time

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seconds", type=float, default=280)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds debe ser positivo")
    import serial  # Included in the ESP-IDF Python environment.
    port = serial.Serial(port=None, baudrate=115200, timeout=1)
    port.port, port.dtr, port.rts = args.port, False, False
    try:
        port.open()
        with port, args.output.open("x", encoding="utf-8") as output:
            deadline = time.monotonic() + args.seconds
            saw_start = False
            print("Capturando. Reinicia manualmente el Tab5 para registrar el benchmark completo.")
            while time.monotonic() < deadline:
                line = port.readline().decode("utf-8", errors="replace")
                if not line:
                    continue
                output.write(line)
                output.flush()
                if "VOXEL_BENCH " in line:
                    print(line.strip())
                    try:
                        payload = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", line.split("VOXEL_BENCH ", 1)[1])
                        event = json.loads(payload).get("event")
                        if event == "start":
                            saw_start = True
                        if event == "finished" and saw_start:
                            return 0
                    except json.JSONDecodeError:
                        pass
        print("Captura terminada sin recibir el evento final; revisar el informe parcial.")
        return 1
    finally:
        if port.is_open:
            port.close()

if __name__ == "__main__":
    raise SystemExit(main())
