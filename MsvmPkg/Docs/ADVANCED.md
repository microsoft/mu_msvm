# Advanced Features and Experimental Functionality

This guide covers additional features, experimental functionality, and advanced use cases.

## Graphics Output

### OpenVMM Graphics Support

Enable graphical UEFI display with VNC connectivity.

**Prerequisites:**
- TigerVNC viewer: Download from [TigerVNC on SourceForge](https://sourceforge.net/projects/tigervnc/)

**Setup:**

1. **Launch OpenVMM with graphics support:**
   ```bash
   cargo run -- --uefi --gfx --disk "C:\Users\vnowkakeane\Documents\virtualDrives\EfiShell1.vhdx"
   ```

2. **Connect to the graphical output:**
   - Open TigerVNC viewer
   - Connect to `localhost` (default VNC port)
   - The UEFI graphical interface will be displayed in the VNC window

**Use Cases:**
- Testing UEFI graphical applications
- Debugging display-related issues
- Demonstrating UEFI frontpage interfaces

## UEFI Shell Integration

### Attaching UEFI Shell (OpenVMM)

Test UEFI functionality with an interactive shell environment.

**Prerequisites:**

1. **Obtain the UEFI Shell VHDX file** from the internal test content location:
   ```
   \\sesdfs\1windows\TestContent\CORE\Base\HYP\MVM\uefi\EfiShell_udk.vhdx
   ```

2. **Copy to local storage** (example location):
   ```
   C:\Users\vnowkakeane\Documents\virtualDrives\EfiShell1.vhdx
   ```

**Usage:**

Launch OpenVMM with the shell disk attached:
```bash
cargo run -- --uefi --disk "C:\Users\vnowkakeane\Documents\virtualDrives\EfiShell1.vhdx" --uefi-console-mode com1
```

**Key Features:**
- `--uefi-console-mode com1` redirects shell output to the terminal for easy interaction
- Provides full UEFI Shell command environment
- Useful for testing UEFI applications and drivers

### Hyper-V UEFI Shell

For Hyper-V environments, attach the UEFI Shell VHDX as a standard disk to your Generation 2 VM and configure boot order to prioritize the shell disk.

## Next Steps

- **Return to basics:** [README.md](README.md)
- **Build fundamentals:** [BUILDING.md](BUILDING.md)
- **Production deployment:** [FIRMWARE-DEPLOYMENT.md](FIRMWARE-DEPLOYMENT.md)
