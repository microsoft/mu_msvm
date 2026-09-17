"""
Installs the build toolchain for a given Hyper-V UEFI CI variant.

Dispatch table:
    Linux  + CLANGPDB              -> apt-get install clang lld llvm
    Linux  + GCC                   -> apt-get install binutils-<gnu_arch> gcc-<gnu_arch>
    Windows + VS2022               -> no-op (toolchain ships in the 1ES image)
    Windows + CLANGPDB, clang_windows=false -> no-op (Clang from Visual Studio 2022)
    Windows + CLANGPDB, clang_windows=true  -> copy-install Windows-org Clang from
                                               pre-downloaded NuGet payloads

OneBranch Linux containers run as root; no sudo is needed.
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from ci_common import VALID_POOLS, VALID_TOOLCHAINS, Pool, ToolChainTag, die, run


def _install_linux(tool_chain_tag: ToolChainTag, gnu_arch: str) -> None:
    run("apt-get update", ["apt-get", "update", "-y"])
    if tool_chain_tag == "CLANGPDB":
        run("apt-get install clang lld llvm", ["apt-get", "install", "-y", "clang", "lld", "llvm"])
    elif tool_chain_tag == "GCC":
        if not gnu_arch.strip():
            die("--gnu-arch is required for Linux + GCC.")
        pkg_binutils = f"binutils-{gnu_arch}-linux-gnu"
        pkg_gcc = f"gcc-{gnu_arch}-linux-gnu"
        run(f"apt-get install {pkg_binutils} {pkg_gcc}", ["apt-get", "install", "-y", pkg_binutils, pkg_gcc])
    else:
        die(f"Unsupported Linux toolchain: {tool_chain_tag}")


def _install_clang_windows(
    clang_win_root: Path,
    clang_bin: Path,
    clang_inc: Path,
    llvm_tools_source: Path,
    clang_headers_source: Path,
) -> None:
    required = {
        "--clang-win-root": clang_win_root,
        "--clang-bin": clang_bin,
        "--clang-inc": clang_inc,
        "--llvm-tools-source": llvm_tools_source,
        "--clang-headers-source": clang_headers_source,
    }
    for flag, val in required.items():
        if not str(val).strip():
            die(f"{flag} is required when --clang-windows true.")

    if clang_win_root.exists():
        print(f"Removing existing CLANGWIN root: {clang_win_root}")
        shutil.rmtree(clang_win_root)

    print(f"Installing LLVM tools:    {llvm_tools_source} -> {clang_bin}")
    clang_bin.mkdir(parents=True, exist_ok=True)
    shutil.copytree(llvm_tools_source, clang_bin, dirs_exist_ok=True)

    print(f"Installing clang headers: {clang_headers_source} -> {clang_inc}")
    clang_inc.mkdir(parents=True, exist_ok=True)
    shutil.copytree(clang_headers_source, clang_inc, dirs_exist_ok=True)

    print("Windows-org Clang install complete.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pool", required=True, choices=VALID_POOLS)
    parser.add_argument("--tool-chain-tag", required=True, choices=VALID_TOOLCHAINS)
    parser.add_argument(
        "--clang-windows",
        type=str.lower,
        default="false",
        choices=["true", "false"],
        help="'true' to install Windows-org Clang from NuGet payloads (string to match ADO template bool serialization)",
    )
    parser.add_argument("--gnu-arch", default="", help="GNU triple arch for Linux GCC (e.g. 'x86-64', 'aarch64')")
    parser.add_argument("--clang-win-root", type=Path, default=Path(""), help="Windows-org Clang root; removed before reinstall")
    parser.add_argument("--clang-bin", type=Path, default=Path(""), help="Destination for LLVM tools")
    parser.add_argument("--clang-inc", type=Path, default=Path(""), help="Destination for clang headers")
    parser.add_argument("--llvm-tools-source", type=Path, default=Path(""), help="llvm.tools NuGet payload directory")
    parser.add_argument("--clang-headers-source", type=Path, default=Path(""), help="clang.headers NuGet payload directory")
    args = parser.parse_args()

    if args.pool == "Linux":
        _install_linux(args.tool_chain_tag, args.gnu_arch)
    else:
        if args.clang_windows != "true":
            print(f"Windows pool, {args.tool_chain_tag}: no toolchain install required (using image-provided tools).")
        else:
            _install_clang_windows(
                clang_win_root=args.clang_win_root,
                clang_bin=args.clang_bin,
                clang_inc=args.clang_inc,
                llvm_tools_source=args.llvm_tools_source,
                clang_headers_source=args.clang_headers_source,
            )


if __name__ == "__main__":
    main()
