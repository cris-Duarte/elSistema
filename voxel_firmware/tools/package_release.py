"""Package only real build outputs, using the generated flash offsets."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, default=ROOT / "build-idf")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    description = json.loads((build / "project_description.json").read_text())
    version = description["project_version"]
    commands = json.loads((build / "compile_commands.json").read_text())
    matches = (re.search(r"-DIDF_VER=([^ ]+)", entry["command"]) for entry in commands)
    match = next((item for item in matches if item), None)
    idf_version = match.group(1).replace("\\", "").replace('"', "") if match else "no registrado"
    settings = json.loads((build / "flasher_args.json").read_text())
    files = {}
    for offset, relative in settings["flash_files"].items():
        int(offset, 0)
        source = (build / relative).resolve()
        if not source.is_relative_to(build):
            raise RuntimeError("El manifiesto apunta fuera del directorio de build")
        files[relative] = source.read_bytes()
    for relative in ("flash_args", "flasher_args.json", "sdkconfig"):
        files[relative] = (build / relative).read_bytes()
    files["dependencies.json"] = (ROOT / "components/sources.json").read_bytes()
    cache = (build / "CMakeCache.txt").read_text()
    files["build-info.json"] = (json.dumps({"version": version, "idf_version": idf_version,
        "target": description["target"],
        "benchmark": bool(re.search(r"^VOXEL_BENCHMARK:[^=]+=ON$", cache, re.M)),
        "diagnostic": bool(re.search(r"^VOXEL_DIAGNOSTIC:[^=]+=ON$", cache, re.M)),
        "quality": bool(re.search(r"^VOXEL_QUALITY:[^=]+=ON$", cache, re.M))}, indent=2) + "\n").encode()
    files["LEEME.txt"] = (
        f"Tab5 Voxel {version} - build ESP-IDF {idf_version}\n"
        "Compilado; pruebas fisicas pendientes. No garantiza 20-30 FPS.\n"
        "Descomprimir y ejecutar desde esta carpeta, solo al decidir instalar:\n"
        "python -m esptool --chip esp32p4 --port PUERTO --baud 460800 write_flash @flash_args\n"
        "Usar esptool 4.x compatible con ESP32-P4. Se verifico el build con 4.12.0.\n"
        "No escribir tab5_voxel.bin desde direccion cero: es solo la aplicacion.\n"
        "Este paquete reemplaza la aplicacion/tabla de particiones del equipo.\n"
        "No ejecuta erase-flash ni programa el ESP32-C6.\n"
        "El mundo editado no se guarda al reiniciar.\n"
    ).encode()
    manifest = {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                for name, data in files.items()}
    files["checksums.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr(name, data)
    with zipfile.ZipFile(args.output) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("Paquete ZIP corrupto")
    print(json.dumps({"package": str(args.output), "files": manifest}, indent=2))

if __name__ == "__main__":
    main()
