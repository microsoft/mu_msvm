"""Test repository coverage, serialization, and invalid input boundaries."""

import io
import json
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from typing import cast
from unittest.mock import patch

from ci.scripts import build, matrix


class MatrixTests(unittest.TestCase):
    """Preserve execution and package contracts for both CI providers."""

    def test_each_repository_row_is_accepted_by_build_runner(self) -> None:
        for repository in matrix.REPOSITORIES:
            for row in matrix.select_builds(repository):
                with self.subTest(repository=repository, row=row), redirect_stdout(io.StringIO()):
                    args = [
                        "--dry-run", "--arch", row["arch"], "--target", row["target"],
                        "--tool-chain", row["tool_chain"], "--core", row["core"],
                        "--host", row["host"], "--compiler-source", row["compiler_source"],
                        "--legacy-debugger", row["legacy_debugger"],
                    ]
                    if row["compiler_source"] == "windows_org":
                        args.extend(["--clang-bin", "internal-llvm/bin"])
                    with patch("ci.scripts.build.subprocess.run") as run_mock:
                        self.assertEqual(build.main(args), 0)
                        run_mock.assert_not_called()

    def test_profiles_are_filtered_explicitly(self) -> None:
        row = matrix.parse_definition(json.loads(matrix.MATRIX_PATH.read_text())[0])
        open_row: matrix.BuildDefinition = {**row, "target": "DEBUG", "repositories": ["open"]}
        closed_row: matrix.BuildDefinition = {**row, "target": "RELEASE", "repositories": ["closed"], "shipping": False, "package_suffix": "XRCV"}
        with patch.object(Path, "read_text", return_value=json.dumps([open_row, closed_row])):
            self.assertEqual([item["id"] for item in matrix.select_builds("open")], [matrix.build_id(open_row)])
            self.assertEqual([item["id"] for item in matrix.select_builds("closed")], [matrix.build_id(closed_row)])

    def test_coverage_and_derived_identifiers(self) -> None:
        rows = matrix.select_builds("open")
        expected = {
            (arch, target, tool_chain, core)
            for target in ("DEBUG", "RELEASE")
            for arch, tool_chain, core in (
                ("X64", "VS2022", "legacy"),
                ("X64", "CLANGPDB", "legacy"),
                ("AARCH64", "CLANGPDB", "legacy"),
                ("X64", "CLANGPDB", "patina"),
                ("AARCH64", "CLANGPDB", "patina"),
            )
        }
        self.assertEqual({(row["arch"], row["target"], row["tool_chain"], row["core"]) for row in rows}, expected)
        self.assertEqual(len(rows), 10)
        for row in rows:
            self.assertEqual(row["id"], matrix.build_id(row))
        closed = matrix.select_builds("closed")
        self.assertEqual(len(closed), 15)
        self.assertEqual(len({row["id"] for row in closed}), 15)
        self.assertEqual(len(matrix.select_builds("closed", host="windows")), 10)
        self.assertEqual(len(matrix.select_builds("closed", host="linux")), 5)
        shipping = {(row["arch"], row["target"], row["tool_chain"], row["compiler_source"], row["core"])
                    for row in closed if row["shipping"] == "true"}
        self.assertEqual(shipping, {
            ("X64", "DEBUG", "VS2022", "visual_studio", "legacy"),
            ("X64", "RELEASE", "VS2022", "visual_studio", "legacy"),
            ("AARCH64", "RELEASE", "CLANGPDB", "windows_org", "legacy"),
        })
        for row in closed:
            self.assertEqual(row["legacy_debugger"], "1" if row["arch"] == "X64" and row["target"] == "DEBUG" else "0")
            self.assertEqual(row["shipping"] == "true", row["package_suffix"] == "")
        self.assertEqual({row["package_suffix"] for row in closed if row["shipping"] == "false"},
                         {"XDCV", "XRCV", "ADCV", "ARCV", "XDCW", "XRCW", "ADCW", "XDCL", "XRCL", "ADCL", "ARCL", "XRG"})

    def test_provider_formats_preserve_same_flavors(self) -> None:
        for repository in matrix.REPOSITORIES:
            rows = matrix.select_builds(repository)
            for format_name in ("json", "github", "ado"):
                with self.subTest(repository=repository, format=format_name):
                    output = io.StringIO()
                    with redirect_stdout(output):
                        matrix.main(["--repo", repository, "--format", format_name])
                    result: object = json.loads(output.getvalue())
                    expected: object = rows
                    if format_name == "github":
                        expected = {"include": rows}
                    elif format_name == "ado":
                        expected = {row["id"]: {key: value for key, value in row.items() if key != "id"} for row in rows}
                    self.assertEqual(result, expected)

    def test_invalid_matrix_is_rejected(self) -> None:
        row = matrix.parse_definition(json.loads(matrix.MATRIX_PATH.read_text())[0])
        invalid_documents: list[object] = [
            {}, [], [row, row], [{**row, "id": "invalid-id"}],
            [{**row, "repositories": ["auto"]}], [{**row, "arch": "unknown"}],
            [{**row, "core": "unknown"}], [{**row, "unexpected": True}],
            [{**row, "repositories": ["closed"]}],
            [{**row, "arch": "AARCH64", "tool_chain": "VS2022"}],
            [{**row, "host": "linux"}],
            [{**row, "shipping": True, "package_suffix": ""}],
        ]
        for document in invalid_documents:
            with self.subTest(document=document), patch.object(Path, "read_text", return_value=json.dumps(document)):
                with self.assertRaises(ValueError):
                    matrix.select_builds("open")

    def test_wrong_json_types_are_rejected(self) -> None:
        """Untrusted values must fail validation, not leak into typed records."""
        row = matrix.parse_definition(json.loads(matrix.MATRIX_PATH.read_text())[0])
        invalid_rows: list[object] = [None, [], "flavor", 1]
        invalid_scalars: tuple[object, ...] = (None, True, 1, [], {})
        invalid_repositories: tuple[object, ...] = (None, "open", [], [True], [[]])
        for field in (*matrix.FLAVOR_CHOICES, *matrix.EXECUTION_CHOICES):
            invalid_rows.extend({**row, field: value} for value in invalid_scalars)
        invalid_rows.extend({**row, "repositories": value} for value in invalid_repositories)
        for invalid in invalid_rows:
            with self.subTest(row=invalid), self.assertRaises(ValueError):
                matrix.parse_definition(invalid)
        closed = {**row, "repositories": ["closed"], "shipping": False, "package_suffix": "XDCV"}
        for field, value in (("shipping", "false"), ("shipping", 0), ("package_suffix", None)):
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                matrix.parse_definition({**closed, field: value})

    def test_package_collisions_are_rejected(self) -> None:
        """Different executions cannot silently publish the same closed package."""
        raw: object = json.loads(matrix.MATRIX_PATH.read_text())
        definitions = [matrix.parse_definition(value) for value in cast(list[object], raw)]
        closed = next(row for row in definitions if "closed" in row["repositories"])
        other = {**closed, "legacy_debugger": "0"}
        with patch.object(Path, "read_text", return_value=json.dumps([closed, other])):
            with self.assertRaisesRegex(ValueError, "Duplicate closed package"):
                matrix.select_builds("closed")

    def test_repository_mode_is_required(self) -> None:
        for args in ([], ["--repo", "auto"]):
            with self.subTest(args=args), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    matrix.main(args)
                self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
