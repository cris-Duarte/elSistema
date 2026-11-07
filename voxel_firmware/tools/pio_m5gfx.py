"""PlatformIO post script: dependencies exist, compilation has not started."""
from pathlib import Path
import importlib.util

Import("env")  # noqa: F821 - SCons injects Import
project = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
spec = importlib.util.spec_from_file_location("voxel_patch", project / "tools/patch_m5gfx.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
library = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "M5GFX"  # noqa: F821
module.patch(library)
