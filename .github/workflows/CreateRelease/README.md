# CreateRelease pipeline

Bundles artifacts produced by `Build` and publishes a GitHub Release.

## Layout

```
.github/workflows/CreateRelease/
├── data/
│   └── release.yml           # baseVersion + tagPrefix
└── scripts/
    ├── release_common.py     # shared helpers
    ├── compute_version.py    # next tag from baseVersion + existing releases
    ├── package_artifacts.py  # per-subdir -> *.tar.gz
    └── tests/
        └── test_release.py
```

Triggered by `.github/workflows/CreateRelease.yml`, which runs after the
`Build` workflow completes successfully on `main`.

## Bumping the version

Edit `data/release.yml`:

```yaml
baseVersion: "26.0"  # bump this to reset patch numbering
tagPrefix: "v"
```

Patch numbers auto-increment from existing GitHub releases that share the
`<tagPrefix><baseVersion>.` prefix.

## Testing locally

```pwsh
python -m pytest .github/workflows/CreateRelease/scripts/tests/

# Dry-run version computation (no `gh` needed)
python .github/workflows/CreateRelease/scripts/compute_version.py `
    --repo fake/repo --dry-run
```# CreateRelease pipeline

Bundles artifacts produced by `Build` and publishes a GitHub Release.

## Layout

```
.github/workflows/CreateRelease/
├── data/
│   └── release.yml           # baseVersion + tagPrefix
└── scripts/
    ├── release_common.py     # shared helpers
    ├── compute_version.py    # next tag from baseVersion + existing releases
    ├── package_artifacts.py  # per-subdir -> *.tar.gz
    └── tests/
        └── test_release.py
```

Triggered by `.github/workflows/CreateRelease.yml`, which runs after the
`Build` workflow completes successfully on `main`.

## Bumping the version

Edit `data/release.yml`:

```yaml
baseVersion: "26.0"  # bump this to reset patch numbering
tagPrefix: "v"
```

Patch numbers auto-increment from existing GitHub releases that share the
`<tagPrefix><baseVersion>.` prefix.

## Testing locally

```pwsh
python -m pytest .github/workflows/CreateRelease/scripts/tests/

# Dry-run version computation (no `gh` needed)
python .github/workflows/CreateRelease/scripts/compute_version.py `
    --repo fake/repo --dry-run
```
