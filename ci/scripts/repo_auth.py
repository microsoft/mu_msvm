"""Set or clear URL-scoped Git authentication in a dedicated CI container.

ADO supplies SYSTEM_ACCESSTOKEN; shared Stuart runners remain provider-neutral.
Call unset in an always-running cleanup step, including after setup failures.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from urllib.parse import urlsplit


def main(argv: list[str] | None = None) -> int:
    """Configure Git, reporting failures without echoing credential arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("set", "unset"))
    parser.add_argument("--url", required=True, help="HTTPS repository URL to authenticate")
    args = parser.parse_args(argv)
    url = urlsplit(args.url)
    if url.scheme != "https" or not url.hostname or url.username or url.password or url.query or url.fragment:
        parser.error("--url must be an HTTPS repository URL without credentials, query, or fragment")
    key = f"http.{args.url}.extraheader"
    command = ["git", "config", "--global"]
    if args.mode == "set":
        token = os.environ.get("SYSTEM_ACCESSTOKEN", "").strip()
        if not token or token == "$(System.AccessToken)" or "\n" in token or "\r" in token:
            parser.error("SYSTEM_ACCESSTOKEN must contain the pipeline job token")
        command.extend(["--replace-all", key, f"AUTHORIZATION: bearer {token}"])
    else:
        command.extend(["--unset-all", key])

    environment = {
        key: value for key, value in os.environ.items()
        if not key.startswith("GIT_TRACE") and key != "GIT_CURL_VERBOSE"
    }
    result = subprocess.run(command, capture_output=True, env=environment)
    if result.returncode != 0 and not (args.mode == "unset" and result.returncode == 5):
        print(f"Git authentication {args.mode} failed (exit {result.returncode})", file=sys.stderr)
        return result.returncode
    print(f"Git authentication {args.mode} completed for {args.url}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
