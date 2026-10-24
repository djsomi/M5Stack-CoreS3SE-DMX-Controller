from pathlib import Path
import sys
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(env.subst("$PROJECT_DIR")) / "scripts"))
from esp_dmx_tx_patch import apply

apply(Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "esp_dmx/src")
