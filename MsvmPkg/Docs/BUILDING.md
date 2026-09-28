# Building MsvmPkg

This guide covers building UEFI firmware images for all supported platforms.

## Prerequisites

Follow the basic instructions for PlatformBuild [here](https://microsoft.github.io/mu/CodeDevelopment/compile/).

## Ubuntu and WSL

Install the Python dependencies, Mono (which Stuart uses to run `NuGet.exe`),
and the native LLVM toolchain used by CLANGPDB:

```bash
python3 -m pip install -r pip-requirements.txt
sudo apt update
sudo apt install -y mono-devel ca-certificates clang lld llvm
```

CLANGPDB runs the host's native LLVM tools on Linux; it does not invoke the
Windows LLVM installation from WSL. Verify that `clang`, `llvm-lib`, `lld-link`,
and `llvm-rc` are available on `PATH` before building.

From the repository root, set up and update the build environment:

```bash
stuart_setup \
  -c ./MsvmPkg/PlatformBuild.py \
  TOOL_CHAIN_TAG=CLANGPDB

stuart_update \
  -c ./MsvmPkg/PlatformBuild.py \
  TOOL_CHAIN_TAG=CLANGPDB
```

If the downloaded Linux tools lose their executable permissions, restore them
before building:

```bash
chmod +x \
  MsvmPkg/Tools/edk2-acpica-iasl_extdep/Linux-x86/iasl \
  MU_BASECORE/BaseTools/Bin/mu_nasm_extdep/Linux-x86-64/nasm \
  MU_BASECORE/BaseTools/Bin/mu_nasm_extdep/Linux-x86-64/ndisasm
```

Build an X64 debug image with build reporting, the CLANGPDB toolchain, and the
legacy C DXE core:

```bash
stuart_build \
  -c ./MsvmPkg/PlatformBuild.py \
  --verbose \
  TOOL_CHAIN_TAG=CLANGPDB \
  TARGET=DEBUG \
  BUILD_ARCH=X64 \
  BUILDREPORTING=TRUE \
  'BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY' \
  'BLD_*_USE_LEGACY_C_CORE=TRUE'
```

Use `TARGET=RELEASE` for a release build or `BUILD_ARCH=AARCH64` for an ARM64
build.

### Mono Certificate Failure

Some Mono installations have an empty or incompatible certificate store. In
that case, `stuart_update` reports that it cannot install `mu_nasm`, while
`Build/UPDATE_LOG.txt` contains `CERTIFICATE_VERIFY_FAILED`. The Azure Artifacts
feed is public; Azure CLI authentication is not required.

As a workaround, download the package with the system TLS stack and populate
NuGet's local cache:

```bash
mkdir -p ~/.nuget/packages/mu_nasm/20016.1.1

curl --fail --location \
  --output /tmp/mu_nasm.20016.1.1.nupkg \
  'https://pkgs.dev.azure.com/projectmu/622a787f-4403-4c43-b89f-e6634fa4b4a3/_packaging/d63724fd-115e-4847-b165-b57320974fce/nuget/v3/flat2/mu_nasm/20016.1.1/mu_nasm.20016.1.1.nupkg'

python3 -m zipfile -e \
  /tmp/mu_nasm.20016.1.1.nupkg \
  ~/.nuget/packages/mu_nasm/20016.1.1

chmod +x \
  ~/.nuget/packages/mu_nasm/20016.1.1/mu_nasm/Linux-x86-64/nasm \
  ~/.nuget/packages/mu_nasm/20016.1.1/mu_nasm/Linux-x86-64/ndisasm

rm /tmp/mu_nasm.20016.1.1.nupkg
```

The package version must match `MU_BASECORE/BaseTools/Bin/nasm_ext_dep.yaml`.
Rerun `stuart_update` after populating the cache.

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
