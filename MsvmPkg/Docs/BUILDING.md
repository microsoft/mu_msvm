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
python .\ci\scripts\build.py --arch X64 --target DEBUG --tool-chain CLANGPDB --core legacy
```

The script installs Python requirements, then runs `stuart_setup`, `stuart_update`, and `stuart_build`
with `MsvmPkg/PlatformBuild.py`. All three Stuart commands receive the selected flavor.
Architecture, target, toolchain, and core are required; use `--core patina` to select the Rust DXE core.
Stuart executables are resolved from the invoking Python environment, and execution stops on the first failure.

`--host windows|linux` defaults to the current host. `--compiler-source` distinguishes `visual_studio`,
`windows_org`, and `distribution`; it defaults to Visual Studio on Windows and distribution tools on Linux.
Use `--legacy-debugger 1` for the legacy-debugger variants. ARM64 MSVC is rejected before preparation.
Actual execution requires the selected host; `--dry-run` can preview either host's commands.

Windows-org Clang requires an explicit `--clang-bin` directory; it never falls back to Visual Studio Clang.
For example, with the internal compiler tools and matching resource headers already installed:

```powershell
python .\ci\scripts\build.py --arch AARCH64 --target RELEASE --tool-chain CLANGPDB --core legacy --host windows --compiler-source windows_org --clang-bin C:/tools/windows-clang/bin
```

The local runner does not download private compiler packages or install system compilers. ADO provisions
Windows-org LLVM tools and headers from the pinned internal feed; its Linux jobs install distribution tools
and create a Python virtual environment. Local developers provide the equivalent toolchain environment.

Add `--dry-run` to print the commands without installing dependencies or starting a build.
This command builds one flavor only; it does not select an open/closed repository matrix or publish artifacts.

### Selecting Repository Coverage

`ci/build-matrix.json` is the shared build list. Each row explicitly lists the repository modes that use it:
`open` for mu_msvm and `closed` for hyperv.uefi. Repository mode selects coverage, not source code,
credentials, official-build status, or publishing permissions.

```powershell
python .\ci\scripts\matrix.py --repo open
python .\ci\scripts\matrix.py --repo closed
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

Closed X64 DEBUG builds enable the legacy debugger. Open builds retain their existing debugger selection.
ARM64 MSVC is unsupported. Disabled GCC combinations are not reintroduced.

IDs are derived as `<host>_<arch>_<target>_<tool_chain>_<compiler_source>_<core>_<legacy_debugger>`;
do not enter IDs in the JSON. Identical flavors may belong to different repository profiles, but may not
occur twice within one profile.
Additional coverage can be added by editing the JSON, without duplicating flavor definitions in provider YAML.
The selector rejects duplicate flavors and fails when a requested repository has no builds.

`--format github` emits an `include` object; `--format ado` emits a job-leg name to variables mapping.
These commands only print JSON. They do not execute builds, install tools, or publish anything.
GitHub's platform workflow selects `open` coverage, feeds the result to its native `strategy.matrix`, and
calls the build action for each flavor. That action invokes the same `ci/scripts/build.py` used locally.
Builds depend only on matrix generation; source checks run independently. Build artifact publication is not
part of this step yet.

ADO's shared platform template selects `closed` coverage through its build job template. A `BuildMatrix`
job exports separate Windows and Linux matrices using `--host`. Dependent `BuildFirmware_windows` and
`BuildFirmware_linux` jobs consume them through native `strategy.matrix`, with at most four parallel
builds per host group. Host pools are selected at template expansion time, not by runtime matrix variables.
Each job runs inside OneBranch. The build stage
runs independently of source checks, and each flavor invokes the same `ci/scripts/build.py` as GitHub.
NuGet and VPack publishing remain disabled for these jobs.

The inspected OneBranch Windows job template forwards job strategies and dependencies. Local validation
checks our output binding and flavor arguments, but a hosted OneBranch run is still required to verify
the full governed-template expansion, runner tools, internal feed credentials, and repository access.

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
