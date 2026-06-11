"""
Emit the GitHub Actions strategy.matrix.include list as JSON.

Reads data/matrix.yml and prints a single line:
    matrix={"include":[{"arch":"X64","target":"DEBUG","tools":"VS2022"}, ...]}

Used by the Build workflow's generate-matrix pre-job:

    - id: setmatrix
      run: python .github/workflows/Build/scripts/generate_matrix.py >> "$GITHUB_OUTPUT"
"""

from __future__ import annotations

import json

from ci_common import load_variants


def main() -> None:
    include = [
        {"arch": v["arch"], "target": v["target"], "tools": v["tools"]}
        for v in load_variants()
    ]
    print(f"matrix={json.dumps({'include': include}, separators=(',', ':'))}")


if __name__ == "__main__":
    main()
