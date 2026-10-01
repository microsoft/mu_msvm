# Building MsvmPkg

This guide covers building UEFI firmware images for all supported platforms.

## Prerequisites

Follow the basic instructions for PlatformBuild [here](https://microsoft.github.io/mu/CodeDevelopment/compile/).

## Basic Build Commands

### Default Debug Build

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py
```

### Release Build

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py TARGET=RELEASE
```

### ARM64 Build

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BUILD_ARCH=AARCH64
```

## Building One CI Flavor Locally

Use your activated Python virtual environment with the selected compiler installed and configured.
From the repository root, run:

```powershell
python .\ci\scripts\build.py --arch X64 --target DEBUG --tool-chain CLANGPDB --core legacy --source-origin open
```

The script installs Python requirements, then runs `stuart_setup`, `stuart_update`, and `stuart_build`
with `MsvmPkg/PlatformBuild.py`. All three Stuart commands receive the selected flavor.
Architecture, target, toolchain, and core are required; use `--core patina` to select the Rust DXE core.
Stuart executables are resolved from the invoking Python environment, and execution stops on the first failure.

`--host windows|linux` defaults to the current host. `--compiler-source` distinguishes `visual_studio`,
`windows_org`, and `distribution`; it defaults to Visual Studio on Windows and distribution tools on Linux.
`--legacy-debugger 1` requires closed-source X64 VS2022 or CLANGPDB with the legacy core.
CLANGPDB includes Windows Visual Studio Clang, Windows-org Clang, and Linux distribution Clang.
GitHub/open builds, GCC, and Patina builds must use `0`. ARM64 MSVC is rejected before preparation.
Actual execution requires the selected host; `--dry-run` can preview either host's commands.

Windows-org Clang requires an explicit `--clang-bin` directory; it never falls back to Visual Studio Clang.
For example, with the internal compiler tools and matching resource headers already installed:

```powershell
python .\ci\scripts\build.py --arch AARCH64 --target RELEASE --tool-chain CLANGPDB --core legacy --host windows --compiler-source windows_org --clang-bin C:/tools/windows-clang/bin --source-origin closed
```

The local runner does not download private compiler packages or install system compilers. ADO provisions
Windows-org LLVM tools and headers from the pinned internal feed; its Linux jobs install distribution tools
and create a Python virtual environment. Local developers provide the equivalent toolchain environment.

Add `--dry-run` to print the commands without installing dependencies or starting a build.
This command builds one flavor only; it does not select an open/closed repository matrix or publish artifacts.

### Selecting Repository Coverage

Coverage is maintained separately in `.github/build-matrix.json` (open-source mu_msvm) and
`.azuredevops/build-matrix.json` (closed-source hyperv.uefi). Both use the shared validator and build script.
Rows have no `repositories` field: the caller explicitly selects a file with `--matrix` instead of `--repo`.
The path selects coverage, not source code, credentials, official-build status, or publishing permissions.
Relative paths resolve from the current directory; either file can be selected locally.

```powershell
python .\ci\scripts\matrix.py --matrix .github/build-matrix.json
python .\ci\scripts\matrix.py --matrix .azuredevops/build-matrix.json
```

The open profile contains ten Windows-hosted builds: DEBUG and RELEASE for X64 VS2022 with the legacy core,
plus X64 and AARCH64 CLANGPDB with each of the legacy and Patina cores.
The closed profile restores 15 legacy-core builds from the preserved hyperv.uefi matrix:

| Host | Compiler source | Coverage |
| --- | --- | --- |
| Windows | VS2022 | X64 DEBUG and RELEASE |
| Windows | Visual Studio Clang | X64 and AARCH64, DEBUG and RELEASE |
| Windows | Windows-org Clang | X64 and AARCH64, DEBUG and RELEASE |
| Linux | Distribution Clang | X64 and AARCH64, DEBUG and RELEASE |
| Linux | Distribution GCC | X64 RELEASE only |

The four closed X64 DEBUG rows enable the legacy debugger: VS2022 and all three Clang compiler sources.
This matches the fetched ADO configuration; successful compilation and runtime debugging still require validation.
All open rows and all GCC rows disable the legacy debugger.
ARM64 MSVC is unsupported. Disabled GCC combinations are not reintroduced.

IDs are derived as `<host>_<arch>_<target>_<tool_chain>_<compiler_source>_<core>_<legacy_debugger>`;
do not enter IDs in the JSON. Identical flavors may occur in both files, but may not occur twice within one file.
Edit the respective JSON to change coverage independently, without editing shared Python or provider YAML.
The selector rejects duplicate flavors and fails when the supplied file or requested host has no builds.

`--format github` emits an `include` object; `--format ado` emits a job-leg name to variables mapping.
These commands only print JSON. They do not execute builds, install tools, or publish anything.
GitHub's platform workflow supplies `.github/build-matrix.json`, feeds the result to its native `strategy.matrix`, and
calls the build action for each flavor. That action invokes the same `ci/scripts/build.py` used locally.
Builds depend only on matrix generation; source checks run independently. Build artifact publication is not
part of this step yet.

ADO's shared platform template supplies `.azuredevops/build-matrix.json` through its build job template. A `BuildMatrix`
job exports separate Windows and Linux matrices using `--host`. Dependent `BuildFirmware_windows` and
`BuildFirmware_linux` jobs consume them through native `strategy.matrix`, with at most four parallel
builds per host group. Host pools are selected at template expansion time, not by runtime matrix variables.
Each job runs inside OneBranch. The build stage
runs independently of source checks, and each flavor invokes the same `ci/scripts/build.py` as GitHub.
NuGet and VPack publishing remain disabled for these jobs.

The inspected OneBranch Windows job template forwards job strategies and dependencies. Local validation
checks our output binding and flavor arguments, but a hosted OneBranch run is still required to verify
the full governed-template expansion, runner tools, internal feed credentials, and repository access.

### Job Labels and Artifact Names

Machine IDs remain stable and complete. They are not the human-facing job labels or package names.
Provider matrix output adds a `display_name` derived from the same flavor:

- GitHub: `DEBUG X64 CLANGPDB (Legacy)` or `RELEASE AARCH64 CLANGPDB (Patina)`.
- ADO: the same label followed by host and compiler source, such as `/ Windows WinOrg` or `/ Linux Distro`.
	Debugger-enabled rows also show `/ Legacy debugger`. ADO may additionally show its matrix leg key in the UI.

GitHub matrix output also supplies `artifact_name` and `logs_artifact_name` for the future upload steps.
They preserve the pre-refactor consumer names exactly:

| Flavor | Firmware artifact | Log artifact |
| --- | --- | --- |
| X64 DEBUG CLANGPDB legacy | `firmware-DEBUG-X64-CLANGPDB` | `logs-DEBUG-X64-CLANGPDB` |
| AARCH64 RELEASE CLANGPDB Patina | `firmware-RELEASE-AARCH64-CLANGPDB-patina` | `logs-RELEASE-AARCH64-CLANGPDB-patina` |

The pattern is `firmware-<TARGET>-<ARCH>-<TOOLCHAIN>[-patina]` (and `logs-` for logs).
Legacy has no suffix. Keep the literal `AARCH64` and `CLANGPDB` in artifact names for existing consumers.
If future host/compiler variants collide under this pattern, selection fails instead of silently renaming artifacts.
Any expanded naming scheme must be coordinated with consumers first.

ADO naming is a separate compatibility contract: historical staged folders were
`Firmware Binary File <TARGET>_<ARCH>`, `Firmware Map Files <TARGET>_<ARCH>`,
`Firmware PDB Files <TARGET>_<ARCH>`, and `Build Logs <job_name>` inside a per-job OneBranch artifact.
Those folder names alone are not globally unique. Changing job identity or log-folder labels can affect consumers
of the enclosing artifact and must be checked when staging/upload is restored. No ADO artifact names are inferred
from the new display label. Package names and suffixes below remain unchanged.

This change defines labels and names only; artifact upload and staging are not enabled yet.

### Closed Package Identity

Closed rows have explicit `shipping` and `package_suffix` metadata. Shipping variants are X64 DEBUG/RELEASE
VS2022 legacy and AARCH64 RELEASE Windows-org CLANGPDB legacy. These three keep their empty package suffix;
all other closed variants retain their historical test suffixes. Metadata describes eligibility, not permission
to publish, and does not mark a local or PR build official.

Build IDs are not package names. Future packaging must preserve the shared VPack/NuGet ID convention:
`<repo>.mscoreuefi.<PR. if PR><arch>.<target><.suffix if present><.Test if PR or non-shipping>`.
For example, the official ARM64 shipping ID remains `hyperv.uefi.mscoreuefi.AARCH64.RELEASE`, while its PR
ID is `hyperv.uefi.mscoreuefi.PR.AARCH64.RELEASE.Test`.
NuGet versions and OneBranch VPack version allocation remain separate contracts.
No package creation or publication is enabled here. In particular, runtime build matrices do not establish
that per-flavor OneBranch publishing policy can be expanded at runtime; validate or introduce a separate
compile-time publishing stage before enabling it.

## Firmware Versioning

Think of three different labels:

| Label | Example | What it answers |
| --- | --- | --- |
| Interface version | `1.0` | Can this firmware communicate correctly with this VMM? |
| Embedded build identity | `26.0` + Git SHA + flags | Which source produced this firmware? |
| Package version | GitHub release / NuGet / VPack version | Which published download is this? |

The first two come from `MsvmPkg/FirmwareVersion.toml` and the checkout. The last is assigned during publishing.
A package number assigned after compilation is not embedded in that firmware.

Inside the firmware today:

- **Interface version:** bump major for a breaking firmware/VMM contract change, minor for a compatible addition.
- **Release prefix:** `26.0`, not the final package number.
- **Git SHA:** the actual checkout built, including the merge commit for a PR merge checkout.
- **Dirty flag:** the source had uncommitted changes.
- **Official flag:** CI marked the build official. This does not mean signed or shipping.

The plugin writes matching firmware PCDs and an 80-byte version-1 record in the DXE FV. That binary layout
is unchanged. Future packaging must retain a mapping from package version to this build identity and flavor.

### Open Versus Closed Source

The build also writes `FwVersion/FirmwareVersion.json` under its build output directory. This sidecar
contains the same identity values plus **`source_origin`**: `open`, `closed`, or `unknown`.
GitHub explicitly selects `open`; ADO explicitly selects `closed`. Origin describes the source repository,
not the compiler supplier, shipping eligibility, or official status.

Locally, pass `--source-origin open` or `--source-origin closed` to `ci/scripts/build.py`.
Omitting it records `unknown`, rather than guessing from remotes or inheriting a stale shell setting.
For direct Stuart invocations, set `SOURCE_ORIGIN=open|closed|unknown` as a build or shell variable.
An explicit Stuart value wins over the shell; an unrecognized value is rejected before output is written.

Origin is currently **sidecar metadata, not embedded in MSVM.fd**. Keep the JSON with future build artifacts;
artifact upload is not wired yet. Identifying origin from the firmware file alone needs a separately reviewed
binary-record extension. Origin and the official flag are declarations, not security attestations.

### Overrides and CI Policy

`BASE_VERSION` uses an explicit Stuart value first, then the shell, then TOML. An empty Stuart value suppresses
the shell override and uses TOML. It must fit 15 ASCII bytes plus a NUL terminator; interface fields must fit UINT16.

`OFFICIAL_BUILD` uses Stuart-over-shell precedence too. Empty, `0`, and case-insensitive `false` mean unofficial;
other nonempty values mean official. Local builds default to unofficial unless explicitly overridden.

- GitHub sets `1` only for a push to `main`; PRs and manual dispatches get `0`.
- ADO sets `1` for the Official entry point and `0` for NonOfficial PR builds, on both build hosts.

An official pipeline can produce test-only flavors; a shipping flavor built locally is still unofficial.
Dirty and official flags can both be set. Public/private mirrors have different SHAs: source origin tells
you which repository to look in, not how to translate commits between them.

## Running CI Checks Locally

Run the same source-check workflow used by platform CI from the repository root:

```powershell
python .\ci\scripts\source_checks.py
```

The workflow installs the Python requirements, runs `stuart_setup` and `stuart_update`, and then runs the configured
`stuart_ci_build` checks. To run one check while iterating, invoke Stuart directly. For example:

```powershell
stuart_ci_build -c .\.pytool\CISettings.py --disable-all GuidCheck=run
```

### Fixing Uncrustify Failures

Run Uncrustify in correction mode to update incorrectly formatted files in place:

```powershell
stuart_ci_build `
	-c .\.pytool\CISettings.py `
	--disable-all `
	UncrustifyCheck=run `
	UNCRUSTIFY_IN_PLACE=TRUE
```

Review the resulting changes and rerun the full source-check command before committing them.

## Developing CI Python

Run these checks from the repository root in your activated virtual environment:

```powershell
python -m pip install -r ci/requirements-dev.txt
python -m mypy --config-file ci/mypy.ini
python -B -m unittest discover -s ci/tests -v
```

Strict type checking covers both CI scripts and tests. Keep parameter and return annotations complete;
document input/output contracts, side effects, and failure behavior in public API docstrings.
The matrix module validates JSON into typed flavor records before using it, keeping provider serialization
separate from the input definition. Runtime validation is still required: type annotations alone cannot
validate JSON or command-line input. Tests mock external commands and do not build or download anything.

These are local development checks; they have not been added to the hosted workflow yet.

## Specialized Builds

### Debug-Enabled Build

For interactive debugging with WinDbg:

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BLD_*_DEBUGGER_ENABLED=1
```

### Serial Logging Build

For serial port debug output:

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BLD_*_DEBUGLIB_SERIAL=1
```

### Combined Debug + Serial Build

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BLD_*_DEBUGGER_ENABLED=1 BLD_*_DEBUGLIB_SERIAL=1
```

## Build Output Structure

Build artifacts follow this path structure:

```
{root}\Build\Msvm{architecture}\{flavor}_{toolchain}\
├── FV\
│   └── MSVM.fd                 # Main firmware image
├── PDB\                        # Debug symbols
└── ...
```

**Path Variables:**
- `{root}` = Root directory of the UEFI project
- `{architecture}` = X64, AARCH64, etc.
- `{flavor}` = DEBUG or RELEASE
- `{toolchain}` = VS2022, GCC, etc.

**Examples:**
- `Build\MsvmX64\DEBUG_VS2022\FV\MSVM.fd`
- `Build\MsvmAARCH64\RELEASE_GCC\FV\MSVM.fd`

## Next Steps

After building, you'll need to deploy your firmware:

- **Deploy Firmware:** See [FIRMWARE-DEPLOYMENT.md](FIRMWARE-DEPLOYMENT.md)
- **Enable Debugging:** See [DEBUGGING.md](DEBUGGING.md)
- **Collect Logs:** See [LOGGING.md](LOGGING.md)
