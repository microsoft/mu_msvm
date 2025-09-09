# Firmware Deployment

This guide covers how to deploy custom UEFI firmware

## Hyper-V Deployment

> **⚠️ CRITICAL LIMITATION ⚠️**
> 
> **This method ONLY works for standard Generation 2 VMs with NO guest-state isolation.**
> 
> **❌ NOT SUPPORTED:**
> - Hardware-based Container and VM isolation (HCL)
> - Confidential Virtual Machines (CVM) 
> - VMs with guest-state isolation enabled
> - Any VM with security features that isolate guest state
>
> **If you are working with HCL or CVM environments, this method will NOT work. Use OpenHCL deployment instead.**

### Requirements

- Windows 11 24H2 or Windows Server 2025 (or newer)
- Administrator privileges
- Standard Generation 2 VMs only

### Deployment Steps

1. **Enable custom firmware loading** by running this PowerShell command as Administrator:

   ```powershell
   Set-ItemProperty "HKLM:\Software\Microsoft\Windows NT\CurrentVersion\Virtualization" -Name "AllowFirmwareLoadFromFile" -Value 1 -Type DWORD | Out-Null
   ```

2. **Deploy your firmware** by copying the MSVM.fd binary:

   ```powershell
   Copy-Item "{root}\Build\Msvm{architecture}\{flavor}_{toolchain}\FV\MSVM.fd" "C:\Windows\System32\MSVM.fd"
   ```

3. **Restart your Generation 2 VMs** - they will now use the custom firmware.

## OpenVMM Deployment

### Requirements

- OpenVMM built and functional
- WSL environment (typical setup)

### Direct Firmware Loading

Launch OpenVMM with your custom firmware:

```bash
cargo run -- --uefi --uefi-firmware /mnt/d/projects/uefi/ACTIVE/Build/Msvm{architecture}/{flavor}_{toolchain}/FV/MSVM.fd
```

**Note:** Adjust the path to match your Windows partition mount point in WSL.

## OpenHCL Deployment

OpenHCL is part of the OpenVMM repository, but uses a different firmware packaging mechanism that requires building an IGVM image.

### Requirements

- OpenVMM repository
- WSL environment (typical setup)

### IGVM Build Process

Build an OpenHCL firmware image with your custom UEFI firmware:

```bash
cargo xflowey build-igvm <RECIPE> --custom-uefi /mnt/d/projects/uefi/ACTIVE/Build/Msvm{architecture}/{flavor}_{toolchain}/FV/MSVM.fd
```

**Where:**
- `<RECIPE>` = OpenHCL configuration recipe (see options below)
- The resulting IGVM can be used with either OpenVMM or Hyper-V

**Available Recipe Options:**

| Recipe | Architecture | CVM Support | Dev Kernel | Description |
|--------|--------------|-------------|------------|-------------|
| `aarch64` | ARM64 | No | No | Standard ARM64 OpenHCL |
| `aarch64-devkern` | ARM64 | No | Yes | ARM64 OpenHCL with dev kernel in VTL2 |
| `x64` | X64 | No | No | Standard X64 OpenHCL |
| `x64-devkern` | X64 | No | Yes | X64 OpenHCL with dev kernel in VTL2 |
| `x64-cvm` | X64 | Yes | No | X64 OpenHCL with CVM support |
| `x64-cvm-devkern` | X64 | Yes | Yes | X64 OpenHCL with CVM support + dev kernel |

**Example:**
```bash
# For standard X64 OpenHCL
cargo xflowey build-igvm x64 --custom-uefi /mnt/d/projects/uefi/ACTIVE/Build/MsvmX64/DEBUG_VS2022/FV/MSVM.fd

# For X64 with CVM support
cargo xflowey build-igvm x64-cvm --custom-uefi /mnt/d/projects/uefi/ACTIVE/Build/MsvmX64/DEBUG_VS2022/FV/MSVM.fd
```

**Recipe Selection Guide:**
- Use `x64` or `aarch64` for standard OpenHCL testing
- Use `x64-cvm` for Confidential VM scenarios
- Use `*-devkern` variants for development/debugging with enhanced kernel features

### Using the IGVM Image

The built IGVM image can be deployed on:
- **OpenVMM:** Direct loading of the IGVM file
- **Hyper-V:** For HCL/CVM scenarios where standard deployment doesn't work

## Platform Compatibility Matrix

| Deployment Method | Standard VMs | HCL VMs | CVM | Notes |
|-------------------|--------------|---------|-----|-------|
| **Hyper-V Registry** | ✅ | ❌ | ❌ | Gen2 only, no isolation |
| **OpenVMM Direct** | ✅ | N/A | N/A | Development/testing |
| **OpenHCL IGVM** | ✅ | ✅ | ✅ | All isolation scenarios |

## Next Steps

After deploying your firmware, you may want to:

- **Set up debugging:** See [DEBUGGING.md](DEBUGGING.md)
- **Collect logs:** See [LOGGING.md](LOGGING.md)
- **Advanced features:** See [ADVANCED.md](ADVANCED.md)
