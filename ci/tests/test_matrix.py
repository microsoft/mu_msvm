import importlib.util
import io
import json
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

SCRIPTS = Path(__file__).parents[1] / "scripts"


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / f"{name}.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


matrix = load_script("matrix")
build = load_script("build")


class MatrixTests(unittest.TestCase):
    def test_each_repository_row_is_accepted_by_build_runner(self):
        for repository in matrix.REPOSITORIES:
            for row in matrix.select_builds(repository):
                with self.subTest(repository=repository, row=row), redirect_stdout(io.StringIO()):
                    args = ["--dry-run"]
                    for field in (*matrix.FLAVOR_CHOICES, *matrix.EXECUTION_CHOICES):
                        args.extend(["--" + field.replace("_", "-"), row[field]])
                    if row["compiler_source"] == "windows_org":
                        args.extend(["--clang-bin", "internal-llvm/bin"])
                    with patch.object(build.subprocess, "run") as run_mock:
                        self.assertEqual(build.main(args), 0)
                        run_mock.assert_not_called()

    def test_profiles_are_filtered_explicitly(self):
        row = json.loads(matrix.MATRIX_PATH.read_text())[0]
        open_row = {**row, "target": "DEBUG", "repositories": ["open"]}
        closed_row = {**row, "target": "RELEASE", "repositories": ["closed"], "shipping": False, "package_suffix": "XRCV"}
        with patch.object(Path, "read_text", return_value=json.dumps([open_row, closed_row])):
            self.assertEqual([item["id"] for item in matrix.select_builds("open")], [matrix.build_id(open_row)])
            self.assertEqual([item["id"] for item in matrix.select_builds("closed")], [matrix.build_id(closed_row)])

    def test_coverage_and_derived_identifiers(self):
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
        self.assertEqual({tuple(row[field] for field in matrix.FLAVOR_CHOICES) for row in rows}, expected)
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

    def test_provider_formats_preserve_same_flavors(self):
        for repository in matrix.REPOSITORIES:
            rows = matrix.select_builds(repository)
            for format_name in ("json", "github", "ado"):
                with self.subTest(repository=repository, format=format_name):
                    output = io.StringIO()
                    with redirect_stdout(output):
                        matrix.main(["--repo", repository, "--format", format_name])
                    result = json.loads(output.getvalue())
                    if format_name == "github":
                        result = result["include"]
                    elif format_name == "ado":
                        result = [{"id": identifier, **flavor} for identifier, flavor in result.items()]
                    self.assertEqual(result, rows)

    def test_invalid_matrix_is_rejected(self):
        row = json.loads(matrix.MATRIX_PATH.read_text())[0]
        invalid_documents = [
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

    def test_repository_mode_is_required(self):
        for args in ([], ["--repo", "auto"]):
            with self.subTest(args=args), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    matrix.main(args)
                self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
