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

## Running CI Checks Locally

Run the source checks from the repository root before opening a pull request. Install the Python dependencies and download the
tools used by the checks on a new checkout or whenever their configuration changes:

```powershell
python -m pip install -r pip-requirements.txt
stuart_update -c .\.pytool\CISettings.py
```

Run the same source checks enforced by GitHub Actions:

```powershell
stuart_ci_build `
	-c .\.pytool\CISettings.py `
	-p MsvmPkg `
	-t NO-TARGET `
	-a X64,AARCH64 `
	--disable-all `
	GuidCheck=run `
	LineEndingCheck=run `
	UncrustifyCheck=run
```

The `--disable-all` option disables the default plugin set; each `Check=run` argument enables a check used by this repository.
To run one check while iterating, specify only that check. For example:

```powershell
stuart_ci_build -c .\.pytool\CISettings.py -p MsvmPkg -t NO-TARGET -a X64,AARCH64 --disable-all GuidCheck=run
```

### Fixing Uncrustify Failures

Run Uncrustify in correction mode to update incorrectly formatted files in place:

```powershell
stuart_ci_build `
	-c .\.pytool\CISettings.py `
	-p MsvmPkg `
	-t NO-TARGET `
	-a X64,AARCH64 `
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
