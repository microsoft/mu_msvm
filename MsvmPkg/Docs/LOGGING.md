# UEFI Logging and Diagnostics

This guide covers collecting UEFI logs and diagnostics across different virtualization platforms.

## Overview

MsvmPkg uses the Advanced Logger to output all `DEBUG()` macro statements. Full documentation can be found [here](https://github.com/microsoft/mu_plus/tree/HEAD/AdvLoggerPkg/Docs).

**Note:** For ARM64 platforms, SEC phase output is not available through Advanced Logger.

## Platform Compatibility

Each platform provides different logging mechanisms due to their underlying architecture differences:

| Platform | Primary Method | Secondary Method | Notes |
|----------|---------------|------------------|-------|
| **Hyper-V** | ETW Tracing | Serial Port | ETW recommended for ease of use |
| **OpenVMM** | EfiDiagnostics | Serial Port | Automatic log display |
| **OpenHCL** | kmsg Buffer | Serial Port | Requires uhdiag.exe |

## Hyper-V Logging

> **⚠️ PLATFORM COMPATIBILITY ⚠️**
>
> **These instructions are specific to standard Hyper-V VMs without OpenHCL.**
>
> **For OpenVMM/OpenHCL environments, see the respective sections below.**

### Method 1: ETW Tracing (Recommended)

ETW (Event Tracing for Windows) provides high-performance log collection with minimal overhead.

**Requirements:**
- Windows 11 24H2 / Windows Server 2025 or newer
- PowerShell `PowerTest` modules installed

**Usage:**

1. **Start trace collection:**
   ```powershell
   Start-HyperVTraceProfile
   ```

2. **Run your VM** (boot, reproduce issue, etc.)

3. **Stop and save traces:**
   ```powershell
   Stop-HyperVTraceProfile -File "<your/path/to/save/trace.etl>"
   ```

**Advantages:**
- Low performance impact
- Captures all UEFI debug output
- Can be analyzed with Windows Performance Analyzer

### Method 2: Serial Port Logging

This method requires both firmware configuration and VM setup.

**Build Configuration:**

Build your firmware with serial debug output enabled:
```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BLD_*_DEBUGLIB_SERIAL=1
```

**VM Configuration:**

Configure your VM to use COM2 for logging output:
```powershell
Set-VMComPort -VmName "myvm" -Number 2 -Path \\.\pipe\port2
```

**Connecting to Output:**

Use the Hyper-V console utility to view Advanced Logger output:
```powershell
hvc serial -rcp 2 <VM-name>
```

**To exit:** Press `Ctrl+A`, then immediately press `Q`.

## OpenVMM Logging

> **⚠️ PLATFORM COMPATIBILITY ⚠️**
>
> **These instructions are specific to OpenVMM environments.**
>
> **For Hyper-V environments, see the section above.**

### Method 1: EfiDiagnostics via OpenVMM Logs (Recommended)

EfiDiagnostics enables OpenVMM to parse the Advanced Logger's shared memory buffer and display logs in real-time.

**How it works:**
- OpenVMM automatically reads the Advanced Logger's memory buffer
- Logs appear in the terminal window as the VM runs
- All logs are prefixed with `EFI log entry`

**Usage:**
```bash
cargo run -- --uefi --uefi-firmware /mnt/d/{root}/Build/Msvm{architecture}/{flavor}_{toolchain}/FV/MSVM.fd
```

**Advantages:**
- **No additional configuration required**
- Real-time log display
- No performance overhead
- Works out of the box

### Method 2: Serial Port Logging

Alternative method when you need serial port output specifically.

**Build Configuration:**

Build your firmware with serial debug output enabled:
```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BLD_*_DEBUGLIB_SERIAL=1
```

**OpenVMM Configuration:**

Launch OpenVMM with COM2 configured to output to stderr:
```bash
cargo run -- --uefi --uefi-firmware /mnt/d/{root}/Build/Msvm{architecture}/{flavor}_{toolchain}/FV/MSVM.fd --com2 stderr
```

## OpenHCL Logging

### Method 1: EfiDiagnostics via kmsg Buffer

This method is only available when using OpenHCL, as kmsg buffers do not exist in standard OpenVMM environments.

**How it works:**
- EfiDiagnostics logs are automatically sent to the kmsg buffer when OpenHCL is present
- Provides the same log content as OpenVMM's EfiDiagnostics but accessed through OpenHCL's diagnostic interface

**Usage:**
```cmd
uhdiag.exe <VM-NAME> kmsg
```

**Advantages:**
- Integrates with OpenHCL diagnostic infrastructure
- Same rich logging as OpenVMM EfiDiagnostics
- Suitable for automated testing and CI scenarios

### Method 2: Serial Port Logging

Same as other platforms - build with `BLD_*_DEBUGLIB_SERIAL=1` and configure serial output as needed for your OpenHCL setup.

## Next Steps

- **Combine with debugging:** See [DEBUGGING.md](DEBUGGING.md)
- **Advanced features:** See [ADVANCED.md](ADVANCED.md)
