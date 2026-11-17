# Building MsvmPkg

## Prerequisites

Follow the Project Mu [PlatformBuild instructions](https://microsoft.github.io/mu/CodeDevelopment/compile/).
Activate your Python environment and install the repository requirements:

```powershell
python -m pip install -r pip-requirements.txt
```

Provide the compiler locally. CI provisions Visual Studio Clang, Windows-org Clang or
distribution Clang/GCC in its provider-specific setup steps.

## Building Locally

Run native Stuart commands from the repository root. Pass the same flavor to setup, update and build:

```powershell
$flavor = @('BUILD_ARCH=X64', 'TARGET=DEBUG', 'TOOL_CHAIN_TAG=CLANGPDB',
    'BLD_*_USE_LEGACY_C_CORE=TRUE', 'BLD_*_LEGACY_DEBUGGER=0')
$env:SOURCE_ORIGIN = 'open' # Use 'closed' in hyperv.uefi.
foreach ($tool in @('stuart_setup', 'stuart_update', 'stuart_build')) {
    & $tool -c MsvmPkg/PlatformBuild.py @flavor
    if ($LASTEXITCODE -ne 0) { throw "$tool failed" }
}
```

Use `BUILD_ARCH=AARCH64` for ARM64, `TARGET=RELEASE` for a release build, and
`BLD_*_USE_LEGACY_C_CORE=FALSE` for Patina. ARM64 MSVC is unsupported.
Set `CLANG_BIN` to the selected Windows Clang bin directory, including its trailing slash.
Windows-org Clang requires its corresponding resource headers too.
Linux GCC requires `GCC_X64_PREFIX` or `GCC_AARCH64_PREFIX` for the installed cross-toolchain.

The closed X64 Clang DEBUG flavors use `BLD_*_LEGACY_DEBUGGER=1`.
Open-source, ARM64, GCC and Patina flavors do not enable that legacy debugger.

## Source Checks

The source checks use their own Stuart configuration:

```powershell
stuart_setup -c .pytool/CISettings.py
stuart_update -c .pytool/CISettings.py
stuart_ci_build -c .pytool/CISettings.py --disable-all GuidCheck=run LineEndingCheck=run UncrustifyCheck=run
```

Check each command's exit code before continuing. Add `--verbose` to setup when diagnosing
dependency initialization. To run one check, select only that check after `--disable-all`.
To apply Uncrustify corrections:

```powershell
stuart_ci_build -c .pytool/CISettings.py --disable-all UncrustifyCheck=run UNCRUSTIFY_IN_PLACE=TRUE
```

## Native CI Structure

GitHub actions and ADO templates follow the same sequence:

1. Prepare the runner, compiler, Python and credentials.
2. Install requirements; run Stuart setup, update and build (or source checks).
3. Verify and stage outputs, including diagnostics after failures.
4. Upload artifacts and run provider-specific packaging/publication.

The build and source-check command bodies match between providers. There is no custom
CI runner CLI or separate CI Python layer. Each provider reads its JSONC coverage in a
small native selection job before expanding its build matrix.
Firmware behavior remains in Stuart settings and build plugins. Provider YAML owns
credentials, package manifests, scheduling and uploads. The same provider files are
mirrored between mu_msvm and hyperv.uefi; private compatibility entrypoints remain in ADO.

Pipelines define jobs; actions/templates define steps:

| GitHub | ADO | Responsibility |
| --- | --- | --- |
| `.github/workflows/platform-ci.yml` | `.azuredevops/pipelines/platform.yml` | Jobs, build coverage, runner setup and publication |
| `.github/actions/build/action.yml` | `.azuredevops/templates/build.yml` | Build commands |
| `.github/actions/source-checks/action.yml` | `.azuredevops/templates/source-checks.yml` | Source-check commands |
| `.github/actions/stage-artifacts/action.yml` | `.azuredevops/templates/stage-artifacts.yml` | Verify/stage artifacts |

ADO's `platform-ci.yml` and `platform-pr.yml` entrypoints select Official or NonOfficial mode
for the common pipeline; the required OneBranch extension stays in that pipeline.

### Coverage

GitHub's `.github/build-matrix.jsonc` has eight Windows builds:
X64/AARCH64, DEBUG/RELEASE, legacy/Patina, all using Visual Studio Clang.

ADO's `.azuredevops/build-matrix.jsonc` has thirteen legacy-core builds:

| Host | Compiler | Coverage |
| --- | --- | --- |
| Windows | Visual Studio Clang | X64/AARCH64 DEBUG/RELEASE |
| Windows | Windows-org Clang | X64/AARCH64 DEBUG/RELEASE |
| Linux | Distribution Clang | X64/AARCH64 DEBUG/RELEASE |
| Linux | GCC | X64 RELEASE |

VS2022 builds are no longer scheduled. The three closed X64 Clang DEBUG variants enable
the legacy debugger. Shipping flavors are Windows-org Clang X64 DEBUG/RELEASE and
AARCH64 RELEASE. Both JSONC files use a flat `include` array and allow comments.
PowerShell 7 reads them and emits strict JSON for the native matrices. ADO follows `official/main`:
an empty `salt` retains the production name; a nonempty `salt` identifies a `.Test` variant.
There is no separate shipping flag or shipping suffix in job names.
Every ADO build stages its artifact and packs its NuGet within its own job.
OneBranch requires separate Windows/Linux pools, so the selection step routes the flat
list into host-specific runtime matrices, with up to four concurrent builds per host.
Matrix leg names follow `<TARGET>_<ARCH>_<TOOLCHAIN>[_<salt>]`, as in `official/main`;
OneBranch's job UI adds its fixed host-group prefix.

ADO Windows uses the image's default Python and PATH, matching Official main.
GitHub provisions Python 3.12. Linux uses a distribution Python virtual environment;
its setup changes Ubuntu archive URLs to `azure.archive.ubuntu.com` before apt.
ADO's reusable authentication template configures URL-scoped Git credentials and removes
them with `always()` cleanup. Do not use it against a developer's global Git configuration.

### Artifacts and Publication

GitHub preserves `firmware-<TARGET>-<ARCH>-CLANGPDB[-patina]` and matching `logs-...` names.
Firmware artifacts contain `FV/MSVM.fd`, `MAP/`, `PDB/` and `FwVersion/`.
Source checks upload `source-check-logs`. Failed builds also upload their full Build tree.

ADO preserves `drop_Build_<TARGET>_<ARCH>_<TOOLCHAIN>[_<salt>]`, with the historical
`Firmware Binary File ...`, `Firmware Map Files ...`, `Firmware PDB Files ...`,
`Build Logs <old job label>` and `Build/Msvm<ARCH>/<TARGET>_<TOOLCHAIN>/Bin/` folders.
Failures retain the full Build tree under `all/`. Source checks retain
`drop_Build_SourceChecks` with root logs, test results and `MsvmPkg/` diagnostics.
There are no intermediate `input-...` drops, generated build IDs, `BuildInfo.json`,
or separate package jobs.

The static `MsvmPkg/HyperVUefi.NuSpec` preserves legacy firmware/symbol/map package paths.
Sidecars are copied into `Build/PackageMetadata` for its fixed `FwVersion/` package entry.
NuGet IDs remain `<repo>.mscoreuefi.<PR. if PR><arch>.<target>[.<salt>][.Test]`.
Only Official shipping flavors omit `.Test`. Versions remain
`<release-prefix>-<branch-name>.<commit>`; OneBranch allocates VPack versions.

PR/NonOfficial jobs pack NuGets and upload CI artifacts but never publish to feeds,
VPack storage or symbol servers. Official builds require successful source checks,
an `official/*` branch and a non-PR run. OneBranch publishes NuGets to `OS/HyperVUefi`
and creates/pushes Windows VPack outputs. Linux stages the VPack-compatible payload
but does not run the Windows-only VPack task. Windows symbols are indexed in the
build workspace; symbol-server publication is Official-only.

GitHub releases require a push to main plus successful source checks, all builds and OpenVMM tests.
They retain `firmware-*.tar.gz` assets and `v<major>.<minor>.<patch>` tags.
The OpenVMM job boots Ubuntu with every X64 firmware artifact (DEBUG/RELEASE, legacy/Patina),
using the existing action from main. It runs on PRs and main; AARCH64 runtime tests are not included.
Native YAML cannot be fully validated locally; hosted builds and Official publication
still need validation before treating this POC as a production replacement.

## Firmware Versioning

`MsvmPkg/FirmwareVersion.toml` is the source of truth for the firmware interface
version and release prefix. The existing pre-build plugin writes matching PCDs,
an unchanged 80-byte version-1 binary record, and `FwVersion/FirmwareVersion.json`.
MAP/PDB collection remains owned by existing Stuart post-build plugins.

The interface major changes for incompatible firmware/VMM changes, and minor for
backward-compatible additions. It is independent of the binary record's struct version.
The embedded identity is the release prefix, Git commit and dirty/official flags;
a package version allocated after compilation is not embedded in that firmware.

`SOURCE_ORIGIN=open|closed|unknown` is sidecar metadata, not a firmware ABI extension.
GitHub selects `open`; ADO selects `closed`. Set it explicitly for local builds.
Origin and the official flag are declarations, not security attestations.

`BASE_VERSION` and `OFFICIAL_BUILD` use Stuart values first, then environment values,
then in-tree defaults. An empty Stuart base version suppresses the environment override.
The base version must fit 15 ASCII bytes plus NUL; interface fields must fit UINT16.
For official status, empty, `0` and case-insensitive `false` mean unofficial.
GitHub marks only pushes to main official; ADO uses the Official entrypoint.

Staging checks expected commit, origin, release prefix and official status.
Missing required outputs fail explicitly. Dirty PR test artifacts warn and retain their
provenance; Official packages and GitHub releases reject dirty firmware.

## Specialized Builds

```powershell
# Debugger, serial logging, or both:
stuart_build -c MsvmPkg/PlatformBuild.py BLD_*_DEBUGGER_ENABLED=1
stuart_build -c MsvmPkg/PlatformBuild.py BLD_*_DEBUGLIB_SERIAL=1
stuart_build -c MsvmPkg/PlatformBuild.py BLD_*_DEBUGGER_ENABLED=1 BLD_*_DEBUGLIB_SERIAL=1
```

Outputs are under `Build/Msvm<ARCH>/<TARGET>_<TOOLCHAIN>/`; the image is `FV/MSVM.fd`.
For deployment, debugging and logs, see [FIRMWARE-DEPLOYMENT.md](FIRMWARE-DEPLOYMENT.md),
[DEBUGGING.md](DEBUGGING.md) and [LOGGING.md](LOGGING.md).
