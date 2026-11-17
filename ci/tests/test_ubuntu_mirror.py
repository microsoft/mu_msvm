"""Preserve upstream OneBranch Ubuntu mirror behavior without touching host apt."""

import unittest
import os
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.tests.test_ado_poc import load_yaml


def linux_setup_script() -> str:
    job = load_yaml(".azuredevops/templates/build.yml")["jobs"][1]["${{ each host in parameters.hosts }}"][0]
    steps = next(branch["${{ if eq(host, 'linux') }}"] for branch in job["steps"]
                 if "${{ if eq(host, 'linux') }}" in branch)
    return next(str(step["bash"]) for step in steps if "bash" in step)


def configure_ubuntu_mirror(apt: Path) -> None:
    script = linux_setup_script().split("apt-get update", 1)[0]
    subprocess.run(["bash", "-c", script.replace("/etc/apt", str(apt))], check=True)


@unittest.skipIf(os.name == "nt", "Executes the Linux Bash adapter")
class UbuntuMirrorTests(unittest.TestCase):
    def test_supported_source_formats_and_idempotence(self) -> None:
        with TemporaryDirectory() as temporary:
            apt = Path(temporary)
            (apt / "sources.list.d").mkdir()
            contents = {
                "sources.list": "deb http://archive.ubuntu.com/ubuntu noble main\n",
                "sources.list.d/extra.list": "deb https://archive.ubuntu.com/ubuntu noble-updates main\n",
                "sources.list.d/ubuntu.sources": (
                    "Types: deb\nURIs: http://archive.ubuntu.com/ubuntu\nSuites: noble noble-updates\n"
                ),
                "sources.list.d/security.list": "deb http://security.ubuntu.com/ubuntu noble-security main\n",
                "sources.list.d/microsoft.sources": "URIs: https://packages.microsoft.com/ubuntu/24.04/prod\n",
                "sources.list.d/ubuntu.sources.save": "URIs: http://archive.ubuntu.com/ubuntu\n",
            }
            for name, content in contents.items():
                (apt / name).write_text(content, encoding="utf-8")
            configure_ubuntu_mirror(apt)
            configure_ubuntu_mirror(apt)
            for name, content in contents.items():
                expected = content if name.endswith(".save") else content.replace(
                    "://archive.ubuntu.com/", "://azure.archive.ubuntu.com/"
                )
                self.assertEqual((apt / name).read_text(encoding="utf-8"), expected)

    def test_missing_sources_are_not_created(self) -> None:
        with TemporaryDirectory() as temporary:
            configure_ubuntu_mirror(Path(temporary))
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_linux_build_configures_mirror_before_apt(self) -> None:
        script = linux_setup_script()
        self.assertLess(script.index("sed -i"), script.index("apt-get update"))


if __name__ == "__main__":
    unittest.main()
