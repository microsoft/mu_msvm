# Building MsvmPkg

This guide covers building UEFI firmware images for all supported platforms.

## Prerequisites

- Git with access to the repository's source submodules.
- Python **3.12**, matching the repository's CI configuration.
- On Windows, Visual Studio **2022** (including Build Tools) with **Desktop development with C++**,
  the MSVC x64/x86 tools, and a Windows SDK.
- Visual Studio **2026** can also be used for Windows X64 builds through the wrapper below. An X64 RELEASE
  C-core build with MSVC 14.51.36231 has been verified locally; VS2026 is not CI-certified or guest-boot-validated.

For general Project Mu build concepts, see the
[PlatformBuild instructions](https://microsoft.github.io/mu/CodeDevelopment/compile/).

## Windows Fresh-Clone Build

From the repository root in PowerShell:

```powershell
# If Python 3.12 is not installed, install it with the Python install manager.
py install 3.12
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r .\pip-requirements.txt

# Initialize the pinned source submodules, then download binary tools/dependencies.
.\.venv\Scripts\stuart_setup.exe -c .\MsvmPkg\PlatformBuild.py TOOL_CHAIN_TAG=VS2022
.\.venv\Scripts\stuart_update.exe -c .\MsvmPkg\PlatformBuild.py TOOL_CHAIN_TAG=VS2022

# Build X64 firmware with the default C DXE core.
.\Build.ps1 -Target RELEASE
```

If your launcher does not recognize `py install`, install Python 3.12 separately, then continue with
`py -3.12 -m venv .venv`. If PowerShell blocks script execution, use an execution policy permitted by your
organization; for example, invoke `powershell.exe -ExecutionPolicy Bypass -File .\Build.ps1 -Target RELEASE`
where permitted.

The resulting image is `Build\MsvmX64\RELEASE_VS2022\FV\MSVM.fd`. Use `-Target DEBUG` for a debug image.
Repeat setup/update when source or binary dependency configuration changes; subsequent builds only need the wrapper.
No virtual-environment activation is necessary: the wrapper prefers `.venv\Scripts\stuart_build.exe`, then
`stuart_build` on `PATH`.

The [Windows build wrapper](../../Build.ps1) prefers VS2022, falling back to VS2026. It initializes the
selected developer environment, derives `VS2022_PREFIX` from `VCToolsInstallDir`, and sets `VS2022_HOST`.
Compiler paths and versions are discovered rather than hard-coded. It preserves explicit `VS2022_PREFIX`,
`VS2022_HOST`, `VS170INSTALLPATH`, and `VS170TOOLVER` environment overrides and restores the calling
process's environment after the build. Set `VS2022_PREFIX` to an MSVC version directory with a trailing
backslash, not the Visual Studio installation root; if also setting `VS170INSTALLPATH`/`VS170TOOLVER`,
ensure they select that same compiler.

Additional Stuart arguments can be passed after the wrapper parameters:

```powershell
.\Build.ps1 -Target DEBUG 'BLD_*_DEBUGLIB_SERIAL=1'
```

The wrapper is limited to Windows X64 with the `VS2022` toolchain tag. Use Stuart directly for other
architectures/toolchains. The platform script and CI defaults remain unchanged.

### Troubleshooting Visual Studio Discovery

If direct `stuart_build` reports:

```text
VsWhere successfully executed, but could not find VS instance for vs2022.
Failed to configure environment for VS2022
```

the toolchain's automatic discovery searches for VS2022 (Visual Studio major version 17), not VS2026
(major version 18). Use `.\Build.ps1 -Target RELEASE`, or install VS2022 with the required C++ tools.
The wrapper supplies the toolchain plugin's explicit-prefix override after initializing the developer environment.
Setting only `VS170INSTALLPATH` to a VS2026 installation does not resolve the plugin's separate
VS2022-specific developer-environment lookup.

`VS2022` is the EDK II toolchain tag, so the output directory still contains `VS2022` even when using
VS2026's MSVC compiler. The wrapper reports the selected compiler directory and propagates Stuart's exit code.

## Basic Build Commands

The direct commands below assume Stuart is on `PATH` (for example, after activating `.venv`) and source/binary
dependencies have been initialized. For Windows X64, prefer the wrapper above to handle compiler discovery.

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
