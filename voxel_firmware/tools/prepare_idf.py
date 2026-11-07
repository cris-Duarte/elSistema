"""Prepare independent pinned M5 components, optionally from an offline cache.

Local components are build inputs, ignored by git. Existing firmware is read-only.
An offline copy is recorded as such; version labels do not certify upstream identity.
"""
from pathlib import Path
from io import BytesIO
import argparse
import hashlib
import json
import shutil
import urllib.request
import zipfile
from patch_m5gfx import patch

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = (("M5GFX", "0.2.32", "m5stack__m5gfx"),
             ("M5Unified", "0.2.25", "m5stack__m5unified"))

def digest_tree(directory):
    digest = hashlib.sha256()
    for file in sorted(directory.rglob("*")):
        if file.is_file():
            digest.update(str(file.relative_to(directory)).encode())
            digest.update(file.read_bytes())
    return digest.hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--from-existing", type=Path, help="Directorio managed_components ya disponible")
    args = parser.parse_args()
    components = ROOT / "components"
    components.mkdir(exist_ok=True)
    records = []
    for name, version, cached_name in LIBRARIES:
        destination = components / name
        if not destination.exists():
            if args.from_existing:
                source = args.from_existing.resolve() / cached_name
                metadata = json.loads((source / "library.json").read_text())
                if metadata["version"] != version:
                    raise RuntimeError(f"Version incorrecta de {name}: {metadata['version']}")
                shutil.copytree(source, destination, ignore=shutil.ignore_patterns(".git", "build", ".pio"))
            else:
                url = f"https://github.com/m5stack/{name}/archive/refs/tags/{version}.zip"
                with urllib.request.urlopen(url, timeout=60) as response:
                    archive = zipfile.ZipFile(BytesIO(response.read()))
                prefix = f"{name}-{version}/"
                for entry in archive.infolist():
                    if not entry.filename.startswith(prefix) or entry.is_dir():
                        continue
                    relative = Path(entry.filename[len(prefix):])
                    if relative.is_absolute() or ".." in relative.parts:
                        raise RuntimeError("Ruta de archivo invalida")
                    target = destination / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(archive.read(entry))
        if json.loads((destination / "library.json").read_text())["version"] != version:
            raise RuntimeError(f"Componente existente {name} con version inesperada")
        if name == "M5GFX":
            patch(destination)
        records.append({"name": name, "version": version, "sha256_tree": digest_tree(destination),
                        "source": "offline-copy" if args.from_existing else "github-tag-or-existing"})
    (components / "sources.json").write_text(json.dumps(records, indent=2) + "\n")
    print(json.dumps(records, indent=2))

if __name__ == "__main__":
    main()
