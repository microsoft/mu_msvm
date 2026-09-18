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
The closed profile still contains only X64 DEBUG CLANGPDB with the legacy core; it is an experimental
baseline, not a replacement for the full hyperv.uefi coverage policy.

IDs are derived as `<arch>_<target>_<tool_chain>_<core>`; do not enter IDs in the JSON.
Additional coverage can be added by editing the JSON, without duplicating flavor definitions in provider YAML.
The selector rejects duplicate flavors and fails when a requested repository has no builds.

`--format github` emits an `include` object; `--format ado` emits a job-leg name to variables mapping.
These commands only print JSON. They do not execute builds, install tools, or publish anything.
GitHub's platform workflow selects `open` coverage, feeds the result to its native `strategy.matrix`, and
calls the build action for each flavor. That action invokes the same `ci/scripts/build.py` used locally.
Builds depend only on matrix generation; source checks run independently. Build artifact publication is not
part of this step yet.

ADO build matrix integration remains unwired. Its JSON output format does not establish support for runtime
matrix expansion in the governed OneBranch templates; that requires validation before enabling ADO builds.

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
