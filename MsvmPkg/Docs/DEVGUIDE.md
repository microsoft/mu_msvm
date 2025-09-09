# MsvmPkg Documentation

MsvmPkg provides UEFI firmware for Microsoft's virtualization platforms, including Hyper-V, OpenVMM, and OpenHCL environments.

## Getting Started

1. **[Building](BUILDING.md)** - Build UEFI firmware images
2. **[Firmware Deployment](FIRMWARE-DEPLOYMENT.md)** - Load custom firmware on different platforms
3. **[Debugging](DEBUGGING.md)** - Interactive UEFI debugging with WinDbg
4. **[Logging](LOGGING.md)** - Collect UEFI logs and diagnostics
5. **[Advanced](ADVANCED.md)** - Graphics, UEFI Shell, and experimental features

## Platform Support

| Platform | Deployment | Debugging | Logging |
|----------|------------|-----------|---------|
| **Hyper-V** | Registry + File Copy | Named Pipes + TCP | ETW Tracing via EfiDiagnostics, Serial |
| **OpenVMM** | Direct Loading | TCP | EfiDiagnostics, Serial |
| **OpenHCL** | IGVM Build | TCP | kmsg Buffer, Serial |

## When to Use Which Platform

**Use Hyper-V when:**
- Working with Windows production environments
- Testing on Windows 11 24H2+ / Server 2025+
- Need full Windows VM management features
- **Note:** Limited to standard Gen2 VMs (no HCL/CVM support)

**Use OpenVMM when:**
- Developing in WSL/Linux environments
- Need faster iteration cycles
- Want simpler debugging setup
- Testing experimental features

**Use OpenHCL when:**
- Working with hardware-based isolation
- Testing confidential computing scenarios
- Need compatibility with isolated workloads

## Getting Help

- **Build Issues:** See [BUILDING.md](BUILDING.md)
- **Debug Connection Problems:** See [DEBUGGING.md](DEBUGGING.md)
- **No Log Output:** See [LOGGING.md](LOGGING.md)
