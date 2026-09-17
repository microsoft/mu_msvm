"""
Computes the OneBranch vpack package name for a Hyper-V UEFI build variant.

Naming rule:
    <repo_name>.mscoreuefi.<pr_dot><arch>.<target><dot_salt><dot_test>

Components:
    pr_dot   = 'PR.' for PR pipeline, '' for Official
    dot_salt = '.<salt>' if salt non-empty, '' otherwise
    dot_test = '.Test' if salt non-empty OR pipeline == PR, '' otherwise

Production (shippable) variants have no salt and run in the Official pipeline —
they carry neither dot_salt nor dot_test. All other variants carry .Test.

NOTE: This script does not influence the rightmost auto-increment digit added
by OneBranch; it only produces the package ID. The YAML pipeline composes the
same name inline (ob_createvpack_packagename); the pytest suite locks both
implementations so they cannot silently diverge.
"""

from __future__ import annotations

import argparse

from ci_common import VALID_ARCHS, VALID_PIPELINES, VALID_TARGETS, Arch, Pipeline, Target


def get_vpack_name(
    repo_name: str,
    pipeline: Pipeline,
    arch: Arch,
    target: Target,
    salt: str = "",
) -> str:
    """Return the vpack package name for the given variant."""
    pr_dot = "PR." if pipeline == "PR" else ""
    dot_salt = f".{salt}" if salt else ""
    dot_test = ".Test" if (salt or pipeline == "PR") else ""
    return f"{repo_name}.mscoreuefi.{pr_dot}{arch}.{target}{dot_salt}{dot_test}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo-name", required=True, help="Build.Repository.Name in ADO (case-sensitive)")
    parser.add_argument("--pipeline", required=True, choices=VALID_PIPELINES)
    parser.add_argument("--arch", required=True, choices=VALID_ARCHS)
    parser.add_argument("--target", required=True, choices=VALID_TARGETS)
    parser.add_argument("--salt", default="", help="Matrix row salt (e.g. 'XDCV'). Empty for production rows.")
    args = parser.parse_args()

    print(get_vpack_name(
        repo_name=args.repo_name,
        pipeline=args.pipeline,
        arch=args.arch,
        target=args.target,
        salt=args.salt,
    ))


if __name__ == "__main__":
    main()
