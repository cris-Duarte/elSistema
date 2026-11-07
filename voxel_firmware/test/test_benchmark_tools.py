"""Synthetic parser fixtures: these are NOT measured device results."""
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("benchmark_report", Path(__file__).resolve().parents[1] / "tools/benchmark_report.py")
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)

def log(event):
    return "I (100) voxel: VOXEL_BENCH " + json.dumps(event)

def sample(scene, fps=25):
    return {"event": "result", "scene": scene, "elapsed_us": 60000000,
            "frames": fps * 60, "samples": fps * 60, "fps": fps,
            "min_window_fps": fps, "render_ms": 20, "present_ms": 10,
            "min_psram": 20000000, "min_internal": 100000}

def full_run():
    return [log({"event": "start", "version": "fixture"})] + [log(sample(s)) for s in report.SCENES] + [log({"event": "finished"})]

class BenchmarkParserTests(unittest.TestCase):
    def test_complete(self):
        result = report.summarize(full_run())
        self.assertTrue(result["complete"] and result["fps_goal_met"])
    def test_serial_colors(self):
        result = report.summarize(["\x1b[0;32m" + line + "\x1b[0m" for line in full_run()])
        self.assertTrue(result["fps_goal_met"])
    def test_partial(self):
        result = report.summarize(full_run()[:2])
        self.assertFalse(result["complete"] or result["fps_goal_met"])
        self.assertEqual(len(result["missing"]), 3)
    def test_slow_window(self):
        lines = full_run()
        slow = sample("bosque"); slow["min_window_fps"] = 19.9
        lines[3] = log(slow)
        self.assertFalse(report.summarize(lines)["fps_goal_met"])
    def test_reboot(self):
        lines = full_run() + [log({"event": "start"})]
        self.assertFalse(report.summarize(lines)["complete"])
    def test_corruption(self):
        for bad in (float("nan"), -1, True, None):
            row = sample("vacio"); row["fps"] = bad
            with self.assertRaises(ValueError):
                report.summarize([log(row)])
    def test_duplicate(self):
        with self.assertRaises(ValueError):
            report.summarize([log(sample("vacio")), log(sample("vacio"))])
    def test_device_error(self):
        lines = full_run(); lines.insert(2, "E (100) lcd: underrun happens")
        self.assertFalse(report.summarize(lines)["fps_goal_met"])
    def test_no_frames(self):
        row = sample("vacio", 0); row["samples"] = 100
        result = report.summarize([log(row)])
        self.assertFalse(result["fps_goal_met"])

if __name__ == "__main__":
    unittest.main()
