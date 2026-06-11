# Sync pipeline (placeholder)

Future home of the `Sync-MuMsvm-HyperVUEFI` pipeline.

## Purpose (future)

Triggered when changes land in `mu_msvm/main`, this pipeline kicks off a
corresponding pipeline in the internal `hyperv.uefi` repo that integrates
the mu_msvm changes downstream.

## Status

Not yet implemented. This folder exists to reserve the layout so the
`workflows/` tree matches the closed-source counterpart at
`.azuredevops/pipelines/Sync/`.

## Planned layout (when implemented)

```
.github/workflows/Sync/
├── data/
│   └── targets.yml           # which downstream pipeline(s) to dispatch
└── scripts/
    ├── sync_common.py
    ├── dispatch_downstream.py
    └── tests/
```

Triggered by `.github/workflows/Sync.yml` (to be added).# Sync pipeline (placeholder)

Future home of the `Sync-MuMsvm-HyperVUEFI` pipeline.

## Purpose (future)

Triggered when changes land in `mu_msvm/main`, this pipeline kicks off a
corresponding pipeline in the internal `hyperv.uefi` repo that integrates
the mu_msvm changes downstream.

## Status

Not yet implemented. This folder exists to reserve the layout so the
`workflows/` tree matches the closed-source counterpart at
`.azuredevops/pipelines/Sync/`.

## Planned layout (when implemented)

```
.github/workflows/Sync/
├── data/
│   └── targets.yml           # which downstream pipeline(s) to dispatch
└── scripts/
    ├── sync_common.py
    ├── dispatch_downstream.py
    └── tests/
```

Triggered by `.github/workflows/Sync.yml` (to be added).
