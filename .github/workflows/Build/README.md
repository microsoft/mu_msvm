# Build pipeline

Builds the mu_msvm UEFI firmware for every variant in `data/matrix.yml`,
in parallel on GitHub-hosted runners.

## Layout

```
.github/workflows/Build/
├── data/
│   └── matrix.yml                  # single source of truth for build variants
└── scripts/
    ├── ci_common.py                # shared types + helpers
    ├── generate_matrix.py          # matrix.yml -> GH Actions matrix JSON
    ├── install_vs_components.py    # vs_installer modify on the runner
    ├── invoke_setup.py             # pip install + stuart_setup + stuart_update
    ├── invoke_build.py             # the single stuart_build invocation
    ├── stage_artifacts.py          # lays out MSVM.fd / MAP / PDB / logs for upload
    ├── invoke_ci_locally.py        # entry point for local-CI runs
    └── tests/
        ├── test_pipeline.py        # pytest unit tests
        └── verify_ci.py            # full local sweep (yaml + py + pytest + matrix)
```

Triggered by `.github/workflows/Build.yml`.

## Running locally

```pwsh
# List variants
python .github/workflows/Build/scripts/invoke_ci_locally.py --list

# Build one variant (skips stuart setup/update on repeat runs):
python .github/workflows/Build/scripts/invoke_ci_locally.py `
    --arch X64 --target DEBUG --tools VS2022

# Re-run a build without re-running stuart setup/update or staging:
python .github/workflows/Build/scripts/invoke_ci_locally.py `
    --arch X64 --target DEBUG --tools VS2022 --skip-setup --skip-stage

# Build every variant sequentially:
python .github/workflows/Build/scripts/invoke_ci_locally.py --all
```

Note: local runs assume your dev box already has the Visual Studio components
listed in `install_vs_components.py`. They are not installed locally.

## Verifying changes before pushing

```pwsh
python .github/workflows/Build/scripts/tests/verify_ci.py
```

## Adding / removing variants

Edit `data/matrix.yml`. Do **not** edit `Build.yml`; the workflow reads the
matrix from that file via `generate_matrix.py`.
