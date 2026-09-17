"""
Sets or unsets git http.<url>.extraheader auth for internal ADO repos.

The URL list is read from data/auth-urls.json so the Set and Unset phases
share the same source and cannot drift.

For 'set' mode, the OAuth bearer token is read from the SYSTEM_ACCESSTOKEN
environment variable so it does not appear in process arguments (where ADO
task logs may echo them verbatim). Inject it via the task's env: block:

    - script: python set_repo_auth.py set
      env:
        SYSTEM_ACCESSTOKEN: $(System.AccessToken)
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

_DEFAULT_URLS_JSON = Path(__file__).parent.parent / "data" / "auth-urls.json"


def _git(*args: str) -> int:
    return subprocess.run(["git", *args]).returncode


def set_auth(urls: list[str], token: str) -> None:
    for url in urls:
        cfg_key = f"http.{url}.extraheader"
        print(f"Set auth: {url}")
        rc = _git("config", "--global", cfg_key, f"AUTHORIZATION: Bearer {token}")
        if rc != 0:
            print(f"ERROR: git config --global {cfg_key} failed (exit {rc})", file=sys.stderr)
            sys.exit(rc)


def unset_auth(urls: list[str]) -> None:
    for url in urls:
        cfg_key = f"http.{url}.extraheader"
        print(f"Unset auth: {url}")
        rc = _git("config", "--global", "--unset", cfg_key)
        # git config --unset exits 5 if the key is already absent; tolerate it
        # so re-runs and partial-state cleanups don't fail the job.
        if rc not in (0, 5):
            print(f"ERROR: git config --global --unset {cfg_key} failed (exit {rc})", file=sys.stderr)
            sys.exit(rc)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", choices=["set", "unset"])
    parser.add_argument(
        "--urls-json",
        type=Path,
        default=_DEFAULT_URLS_JSON,
        help="Path to auth-urls.json (default: ../data/auth-urls.json relative to this script)",
    )
    args = parser.parse_args()

    if not args.urls_json.exists():
        print(f"ERROR: auth-urls.json not found at: {args.urls_json}", file=sys.stderr)
        sys.exit(1)

    data = json.loads(args.urls_json.read_text())
    urls: list[str] = data.get("urls", [])
    if not urls:
        print(f"ERROR: No 'urls' entries in {args.urls_json}", file=sys.stderr)
        sys.exit(1)

    if args.mode == "set":
        token = os.environ.get("SYSTEM_ACCESSTOKEN", "").strip()
        if not token:
            print(
                "ERROR: SYSTEM_ACCESSTOKEN is not set. "
                "Inject it via the task env: block (do not pass it as an argument).",
                file=sys.stderr,
            )
            sys.exit(1)
        set_auth(urls, token)
    else:
        unset_auth(urls)


if __name__ == "__main__":
    main()
