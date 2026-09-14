"""Run the real cross-toolchain smoke gate in a fresh evidence directory."""
from pathlib import Path
import subprocess
import sys
import tempfile


if __name__ == "__main__":
    build = Path(sys.argv[1]).resolve()
    parent = build / "backend/experiments/rtems_gr740/smoke-artifacts"
    parent.mkdir(exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=parent)) / "evaluation"
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name("run.py")),
                                     str(build), str(output), "--smoke", "--repeats", "1"]))
