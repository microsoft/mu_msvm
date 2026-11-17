"""Use the Azure Ubuntu archive in OneBranch containers before running apt."""

from pathlib import Path


def configure_ubuntu_mirror(apt_dir: Path = Path("/etc/apt")) -> None:
    sources_dir = apt_dir / "sources.list.d"
    sources = [apt_dir / "sources.list", *sources_dir.glob("*.list"), *sources_dir.glob("*.sources")]
    for source in sources:
        if not source.is_file():
            continue
        original = source.read_text(encoding="utf-8")
        updated = original.replace("://archive.ubuntu.com/", "://azure.archive.ubuntu.com/")
        if updated != original:
            source.write_text(updated, encoding="utf-8")
            print(f"Configured Azure Ubuntu archive: {source}", flush=True)


if __name__ == "__main__":
    configure_ubuntu_mirror()
