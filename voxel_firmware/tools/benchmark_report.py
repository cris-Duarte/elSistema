"""Summarize a real device log; never fill missing scenes with estimated FPS."""
import argparse
import json
import math
import re
from pathlib import Path

SCENES = ("vacio", "plano", "bosque", "construccion")
MARKER = "VOXEL_BENCH "
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")

def summarize(lines):
    metadata = None
    results = {}
    finished = False
    errors = []
    for line in lines:
        line = ANSI.sub("", line)
        if any(token in line for token in ("Guru Meditation", "panic'ed", "underrun happens", "Timeout de refresh DSI")):
            errors.append(line.strip())
        if MARKER not in line:
            continue
        event = json.loads(line.split(MARKER, 1)[1].strip())
        kind = event.get("event")
        if kind == "start":
            # A reboot invalidates an incomplete earlier run.
            metadata, results, finished, errors = event, {}, False, []
        elif kind == "result":
            scene = event.get("scene")
            if scene not in SCENES or scene in results:
                raise ValueError("Escena desconocida o duplicada")
            for key in ("fps", "min_window_fps", "render_ms", "present_ms", "elapsed_us", "frames", "samples", "min_psram", "min_internal"):
                value = event.get(key)
                if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
                    raise ValueError(f"Dato invalido: {key}")
            if event["elapsed_us"] < 60000000 or not event["samples"]:
                raise ValueError("Escena sin una medicion completa")
            measured = event["frames"] * 1000000 / event["elapsed_us"]
            if abs(measured - event["fps"]) > .01:
                raise ValueError("FPS incompatibles con frames/duracion")
            results[scene] = event
        elif kind == "finished":
            finished = True
    complete = metadata is not None and finished and len(results) == len(SCENES)
    passed = complete and not errors and all(r["fps"] >= 20 and r["min_window_fps"] >= 20 for r in results.values())
    return {"complete": complete, "fps_goal_met": passed, "metadata": metadata,
            "scenes": [results[name] for name in SCENES if name in results],
            "missing": [name for name in SCENES if name not in results], "device_errors": errors,
            "scope": "FPS observados; no certifica tacto, tearing ni estabilidad de 30 minutos"}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = summarize(args.log.read_text(errors="replace").splitlines())
    text = json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text, end="")
    return 0 if result["fps_goal_met"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
