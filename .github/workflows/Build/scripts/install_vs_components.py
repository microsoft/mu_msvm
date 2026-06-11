"""
Install / verify the Visual Studio components required for a Build variant.

Designed to run on GitHub-hosted windows-latest runners (where VS 2022 is
pre-installed but missing some components we need).

Dispatch:
    arch=X64                       -> requires VC.Tools.x86.x64 + Windows SDK
    arch=AARCH64                   -> requires VC.Tools.ARM64
    tools=CLANGPDB (any arch)      -> requires VC.Llvm.Clang + the Clang component group

We install the *union* of required components for the requested variant in
one `vs_installer modify` call, then verify everything is on disk.

Local dev machines normally have all of these already; the script is a no-op
in that case (vs_installer exits 0 when components are already installed).
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

from ci_common import VALID_ARCHS, VALID_TOOLCHAINS, Arch, ToolChainTag, die

# Component IDs are stable across VS 2022 minor versions.
_COMPONENTS_COMMON: tuple[str, ...] = (
    "Microsoft.VisualStudio.Component.VC.CoreBuildTools",
    "Microsoft.VisualStudio.Component.Windows11SDK.22621",
)
_COMPONENTS_X64: tuple[str, ...] = (
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
)
_COMPONENTS_AARCH64: tuple[str, ...] = (
    "Microsoft.VisualStudio.Component.VC.Tools.ARM64",
)
_COMPONENTS_CLANG: tuple[str, ...] = (
    "Microsoft.VisualStudio.Component.VC.Llvm.Clang",
    "Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Llvm.Clang",
)


def _required_components(arch: Arch, tools: ToolChainTag) -> list[str]:
    comps: list[str] = list(_COMPONENTS_COMMON)
    if arch == "X64":
        comps.extend(_COMPONENTS_X64)
    elif arch == "AARCH64":
        comps.extend(_COMPONENTS_AARCH64)
    if tools == "CLANGPDB":
        comps.extend(_COMPONENTS_CLANG)
    return comps


def _vs_install_path() -> Path:
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if not vswhere.exists():
        die(f"vswhere.exe not found: {vswhere}")
    result = subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
        check=True, capture_output=True, text=True,
    )
    path = Path(result.stdout.strip())
    if not path.exists():
        die(f"Visual Studio installation path not found: {path}")
    return path


def _install(arch: Arch, tools: ToolChainTag) -> None:
    if sys.platform != "win32":
        die("install_vs_components.py only runs on Windows.")

    vs_path = _vs_install_path()
    print(f"VS installation: {vs_path}", flush=True)

    vs_installer = Path(
        r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe"
    )
    if not vs_installer.exists():
        die(f"vs_installer.exe not found: {vs_installer}")

    components = _required_components(arch, tools)
    print("Required components:", flush=True)
    for c in components:
        print(f"  - {c}")

    cmd: list[str] = [
        str(vs_installer), "modify",
        "--installPath", str(vs_path),
    ]
    for c in components:
        cmd.extend(["--add", c])
    cmd.extend(["--quiet", "--wait"])

    print(f"\n== {' '.join(cmd)} ==", flush=True)
    result = subprocess.run(cmd)
    if result.returncode != 0:
        die(f"vs_installer modify failed with exit code {result.returncode}")

    # vs_installer occasionally returns before paths are visible to child procs.
    print("Waiting 10s for VS installer to finalize...", flush=True)
    time.sleep(10)


def _verify(arch: Arch, tools: ToolChainTag) -> None:
    vs_path = _vs_install_path()

    vc_tools = vs_path / "VC" / "Tools" / "MSVC"
    if not vc_tools.exists():
        die(f"VC++ tools directory missing: {vc_tools}")
    print(f"OK: VC++ tools at {vc_tools}")

    win_sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
    if not win_sdk.exists():
        die(f"Windows SDK missing: {win_sdk}")
    print(f"OK: Windows SDK at {win_sdk}")

    if arch == "AARCH64":
        arm_glob = list(vc_tools.glob("*/bin/Hostx64/arm64"))
        if not arm_glob:
            die(f"AARCH64 cross-compiler missing under {vc_tools}/*/bin/Hostx64/arm64")
        print(f"OK: AARCH64 tools at {arm_glob[0]}")

    if tools == "CLANGPDB":
        clang_exe = vs_path / "VC" / "Tools" / "Llvm" / "x64" / "bin" / "clang.exe"
        if not clang_exe.exists():
            die(f"clang.exe missing: {clang_exe}")
        print(f"OK: clang.exe at {clang_exe}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--arch", required=True, choices=VALID_ARCHS)
    parser.add_argument("--tools", required=True, choices=VALID_TOOLCHAINS)
    parser.add_argument("--verify-only", action="store_true",
                        help="Only check that components are present; do not invoke vs_installer.")
    args = parser.parse_args()

    if not args.verify_only:
        _install(args.arch, args.tools)
    _verify(args.arch, args.tools)


if __name__ == "__main__":
    main()
