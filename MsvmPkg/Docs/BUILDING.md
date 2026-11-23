# Building MsvmPkg

This guide covers cloning and building X64 and AARCH64 UEFI firmware from a Windows x64 host using PowerShell.
The commands below build the C DXE core with CLANGPDB, as used by platform CI.

## Prerequisites

- Git for Windows, with Git available on `PATH`.
- Python 3.12 x64, including pip and venv (the version used by CI).
- Visual Studio 2022 or Visual Studio 2026 with the **Desktop development with C++** workload, including the
  MSVC x64/x86 build tools and a Windows SDK. CLANGPDB uses Visual Studio's NMake and SDK even when Clang is installed separately.
- LLVM/Clang, including `clang.exe`, `clang-cl.exe`, `lld-link.exe`, and `llvm-lib.exe`.
  Select **C++ Clang tools for Windows** in Visual Studio Installer, or use standalone LLVM as described below.
- Internet access to GitHub and the package feeds used by pip and Stuart's external dependencies.

CI uses Visual Studio 2022's bundled Clang. Visual Studio 2026 Enterprise 18.12 with MSVC 14.52 and standalone
LLVM 20.1.8 was also validated for both X64 and AARCH64 RELEASE builds. Use `TOOL_CHAIN_TAG=CLANGPDB` with either
Visual Studio version; the pinned build definitions do not have a `VS2026` toolchain tag.

Python can be installed for the current user, for example:

```powershell
winget install --id Python.Python.3.12 --exact --scope user --silent --accept-package-agreements --accept-source-agreements
```

### Clone and Prepare Python

Use a short checkout path to avoid Windows path-length problems. Disable Git line-ending conversion for this checkout
before initializing submodules; source checks expect the repository's original line endings.

```powershell
git -c core.autocrlf=false clone https://github.com/microsoft/mu_msvm.git
Set-Location mu_msvm
git config core.autocrlf false
git config core.longpaths true

# Keep the virtual environment outside the checkout so build metadata stays clean.
$venv = Join-Path (Split-Path $PWD.Path -Parent) 'mu_msvm-venv'
py -3.12 -m venv $venv
if ($LASTEXITCODE -ne 0) { throw 'Python virtual environment creation failed' }
$env:PATH = "$(Join-Path $venv 'Scripts');$env:PATH"
python -m pip install -r pip-requirements.txt
if ($LASTEXITCODE -ne 0) { throw 'Python dependency installation failed' }
```

If `py` is unavailable, replace `py -3.12` with the full path to Python 3.12, typically
`& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"`. A newly installed Python may not appear on `PATH`
until a new terminal is opened. Adding the venv's `Scripts` directory to `PATH` avoids needing PowerShell activation
scripts or changing execution policy. Repeat that `PATH` assignment in each new terminal. Recreate rather than move
a venv: Windows console entry points embed its original path.

### Select Clang

Stuart discovers Visual Studio's host tools automatically; a Developer Command Prompt is not required for CLANGPDB.
Choose the Clang installation explicitly and include a trailing directory separator:

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'Visual Studio C++ tools were not found' }
$env:CLANG_BIN = Join-Path $vs 'VC\Tools\Llvm\x64\bin\'
foreach ($tool in @('clang.exe', 'clang-cl.exe', 'lld-link.exe', 'llvm-lib.exe')) {
	if (-not (Test-Path (Join-Path $env:CLANG_BIN $tool))) { throw "Missing $tool in CLANG_BIN; install Clang or select standalone LLVM" }
}
& (Join-Path $env:CLANG_BIN 'clang.exe') --version
```

Having `clang-format.exe` and `clang-tidy.exe` alone is not sufficient. For standalone LLVM, set
`$env:CLANG_BIN = 'C:\Program Files\LLVM\bin\'` instead, or point it at an extracted portable distribution.
For a non-elevated setup, download the **x86_64-pc-windows-msvc** archive from
[LLVM 20.1.8](https://github.com/llvm/llvm-project/releases/tag/llvmorg-20.1.8), verify the published SHA-256 digest,
and extract it outside the checkout with `tar -xJf <archive> -C <tools-directory>`. Point `CLANG_BIN` at the extracted
`bin\` directory and rerun the executable checks above. The host archive is x86_64 even when building AARCH64 firmware.

## Build Both Release Images

Run from the repository root, in the same terminal where Python and `CLANG_BIN` were configured:

```powershell
$ErrorActionPreference = 'Stop'
$env:SOURCE_ORIGIN = 'open'
foreach ($arch in @('X64', 'AARCH64')) {
	$flavor = @("BUILD_ARCH=$arch", 'TARGET=RELEASE', 'TOOL_CHAIN_TAG=CLANGPDB',
		'BLD_*_USE_LEGACY_C_CORE=TRUE')
	foreach ($tool in @('stuart_setup', 'stuart_update', 'stuart_build')) {
		$buildOptions = @()
		if ($tool -eq 'stuart_build') {
			$buildOptions = @('LaunchLogOnSuccess=FALSE')
		}
		& $tool -c MsvmPkg/PlatformBuild.py @flavor @buildOptions
		if ($LASTEXITCODE -ne 0) { throw "$tool failed for $arch with exit code $LASTEXITCODE" }
	}
	$image = "Build/Msvm$arch/RELEASE_CLANGPDB/FV/MSVM.fd"
	if (-not (Test-Path $image)) { throw "Missing firmware image: $image" }
	Get-Item $image | Select-Object FullName, Length, LastWriteTime
}
```

`stuart_setup` initializes the required pinned submodules. `stuart_update` downloads pinned external tools, including
BaseTools, NASM, and the ACPI compiler; these do not need separate manual installation. Keep the flavor arguments
consistent across setup, update, and build. To build only one architecture, use `@('X64')` or `@('AARCH64')` in the outer loop.
Quote arguments containing `*` or spaces as shown. Explicitly checking `$LASTEXITCODE` is necessary because
`$ErrorActionPreference` alone does not reliably catch native command failures.

The default architecture is X64 and the default toolchain is VS2022, so an unqualified `stuart_build` is not equivalent
to this recipe. `BUILD_ARCH=AARCH64` selects the ARM platform; `-a` is the source-check architecture selector, not its replacement.
`LaunchLogOnSuccess=FALSE` suppresses Notepad after successful builds; omit it for interactive log viewing.

### Development Loop

After setup/update, rerun only the build for an ordinary source change:

```powershell
stuart_build -c MsvmPkg/PlatformBuild.py BUILD_ARCH=X64 TARGET=RELEASE TOOL_CHAIN_TAG=CLANGPDB `
	'BLD_*_USE_LEGACY_C_CORE=TRUE' LaunchLogOnSuccess=FALSE
if ($LASTEXITCODE -ne 0) { throw 'X64 release build failed' }
```

Replace `BUILD_ARCH=X64` with `BUILD_ARCH=AARCH64` for ARM64, or `TARGET=RELEASE` with `TARGET=DEBUG` for debug builds.
Rerun setup/update after changing branches, submodule pins, external dependency descriptors, or build flavor.
For Patina, set `'BLD_*_USE_LEGACY_C_CORE=FALSE'` throughout setup/update/build; Stuart obtains the prebuilt core.
C and Patina use the same local output directory, so preserve artifacts before switching cores.

## Running CI Checks Locally

Run the source checks from the repository root before opening a pull request. Install the Python dependencies and download the
tools used by the checks on a new checkout or whenever their configuration changes:

```powershell
python -m pip install -r pip-requirements.txt
stuart_setup -c .\.pytool\CISettings.py
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

These examples assume the Python and Clang environment above. They use X64 DEBUG with CLANGPDB;
check `$LASTEXITCODE` after each build before continuing.

### Debug-Enabled Build

For interactive debugging with WinDbg:

```powershell
stuart_build -c MsvmPkg/PlatformBuild.py TOOL_CHAIN_TAG=CLANGPDB 'BLD_*_DEBUGGER_ENABLED=1' `
	LaunchLogOnSuccess=FALSE
```

### Serial Logging Build

For serial port debug output:

```powershell
stuart_build -c MsvmPkg/PlatformBuild.py TOOL_CHAIN_TAG=CLANGPDB 'BLD_*_DEBUGLIB_SERIAL=1' `
	LaunchLogOnSuccess=FALSE
```

### Combined Debug + Serial Build

```powershell
stuart_build -c MsvmPkg/PlatformBuild.py TOOL_CHAIN_TAG=CLANGPDB `
	'BLD_*_DEBUGGER_ENABLED=1' 'BLD_*_DEBUGLIB_SERIAL=1' LaunchLogOnSuccess=FALSE
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
- `{toolchain}` = CLANGPDB for the recipe above, or VS2022 for the default X64 build

**Examples:**
- `Build\MsvmX64\RELEASE_CLANGPDB\FV\MSVM.fd`
- `Build\MsvmAARCH64\RELEASE_CLANGPDB\FV\MSVM.fd`

`MAP\` and `PDB\` contain flattened symbol files. `BUILD_TOOLS_REPORT.*` records tool versions.
To generate `BUILD_REPORT.TXT`, add `BUILDREPORTING=TRUE` and
`'BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY'` to `stuart_build`.
`Build\BUILDLOG.txt`, `Build\SETUPLOG.txt`, and
`Build\UPDATE_LOG.txt` are overwritten by subsequent invocations; preserve them before switching architectures
when diagnosing a failure. Reports and symbols remain in their architecture-specific output directories.

## Troubleshooting

- **Dependency downloads fail:** Check network/proxy access, then rerun setup/update with the same flavor arguments.
- **Line-ending check fails on untouched files:** Ensure `core.autocrlf=false` was applied before checkout and submodule
	initialization. Prefer a fresh correctly configured clone rather than rewriting a working tree with local edits.
- **Dirty-worktree warning:** Firmware version metadata records uncommitted and untracked files, including editor
	settings. This is not itself a build failure.
- **Build failure:** Read the last errors in `Build\BUILDLOG.txt` and the corresponding EDK II log.

## Next Steps

A successful build validates compilation and firmware-image checks, not guest boot behavior. Deploy the image for runtime testing:

- **Deploy Firmware:** See [FIRMWARE-DEPLOYMENT.md](FIRMWARE-DEPLOYMENT.md)
- **Enable Debugging:** See [DEBUGGING.md](DEBUGGING.md)
- **Collect Logs:** See [LOGGING.md](LOGGING.md)
