"""Execute provider template bodies with explicit, isolated test environments."""

import os
import shutil
import subprocess
from pathlib import Path

from ci.tests.test_ado_poc import load_yaml

POWERSHELL = (shutil.which("pwsh") or shutil.which("powershell")) if os.name == "nt" else None
ROOT = Path(__file__).resolve().parents[2]


def run_template(
    path: str, environment: dict[str, str], *, cwd: Path = ROOT,
) -> subprocess.CompletedProcess[str]:
    assert POWERSHELL is not None, "Template execution requires Windows PowerShell or pwsh"
    script = load_yaml(path)["steps"][0]["pwsh"]
    return subprocess.run(
        [POWERSHELL, "-NoProfile", "-NonInteractive", "-Command", script],
        cwd=cwd, env={**os.environ, **environment}, text=True, capture_output=True,
    )
