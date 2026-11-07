"""Expose the native DPI handle without changing panel initialization/ownership."""
from pathlib import Path
import argparse

GETTER = "    esp_lcd_panel_handle_t voxelDpiPanel() const { return _disp_panel_handle; }\n"
ANCHOR = "    Bus_DSI* getBusDSI(void) const\n"

def patch(library: Path) -> None:
    header = library / "src/lgfx/v1/platforms/esp32p4/Panel_DSI.hpp"
    source = header.with_suffix(".inl")
    text = header.read_text()
    if "dpi_config.num_fbs = 2;" not in source.read_text():
        raise RuntimeError("M5GFX DSI no tiene la configuracion de dos framebuffers esperada")
    if GETTER in text:
        return
    if text.count(ANCHOR) != 1 or "esp_lcd_panel_handle_t _disp_panel_handle" not in text:
        raise RuntimeError("M5GFX incompatible: revisar el adaptador DSI antes de compilar")
    header.write_text(text.replace(ANCHOR, GETTER + "\n" + ANCHOR))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path)
    patch(parser.parse_args().library)
