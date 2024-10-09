# MsvmPkg

## Loading private UEFI binaries
Type the following command to a powershell window, which allows loading UEFI via a file (MSVM.fd) in `C:\Windows\System32`:

```powershell
Set-ItemProperty "HKLM:/Software/Microsoft/Windows NT/CurrentVersion/Virtualization" -Name "AllowFirmwareLoadFromFile" -Value 1 -Type DWORD | Out-Null
```

Next, after running a successful build, copy the MSVM.fd binary from the FV folder (e.g. `{root}\Build\MsvmX64\DEBUG_VS2022\FV\MSVM.fd`) to `C:\Windows\System32`. Your standard generation 2 VMs will now use this firmware when booting.



## Connecting to the debugger
MsvmPkg uses the MU Feature Debugger, whose full documentation can be found [here](https://github.com/microsoft/mu_feature_debugger/blob/HEAD/DebuggerFeaturePkg/Readme.md?plain=1). 

For these instructions, it is assumed that Windbg is already installed onto the system. WinDbg documentation can be found [here](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/).

### Building
You must build your UEFI with `BLD_*_DEBUGGER_ENABLED=1` to enable the debugger. For example:

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BLD_*_DEBUGGER_ENABLED=1
```


### Setting up serial
Before connecting to the debugger, you must initialize named pipe transports. Using a Hyper-V virtual machine, the following commands setup a named pipe through COM1:

```powershell
Set-VMComPort -VmName "myvm" -Number 1 -Path \\.\pipe\port1 -DebuggerMode On
```


### Install Python and related packages
The MU Feature Debugger repository contains a `ComToTcpServer.py` script to support COM and named pipe based transports to forward traffic between a serial device and a TCP server. 

- First, install the latest version of python for your platform
- Next, in a command window or terminal, run `pip install pyserial` and `pip install pywin32`. This gives access to the Windows COM and named pipe devices
- Copy the `ComToTcpServer.py` script to some location on your computer.

NOTE: If you are running Windows Server, you may get an error when installing python along the lines of "This installation is Forbidden by System Policy." Run the following to get past this:

```powershell
# Modify the system registry
$registryPath = "HKLM:\SOFTWARE\Policies\Microsoft\Windows"
$registryKey = "Installer"
$registryValueName = "DisableMSI"
$registryValue = 0

# Create the registry key if it doesn't exist
if (-not (Test-Path "$registryPath\$registryKey")) {
    New-Item -Path $registryPath -Name $registryKey -Force
}

# Set the registry value
Set-ItemProperty -Path "$registryPath\$registryKey" -Name $registryValueName -Value $registryValue
```


### UEFI Extension
In order to load UEFI symbols and modules through the MU Feature Debugger, you will need the `uefiext.dll`. Full documentation of this extension and its capabilities can be found [here](https://github.com/microsoft/mu_feature_debugger/blob/HEAD/UefiDbgExt/readme.md).

You can get this from one of the MU Feature Debugger pipeline artifacts [here](https://github.com/microsoft/mu_feature_debugger/actions/workflows/Build-UefiExt.yaml).

Once downloaded, place the DLL in a place where all Windbg sessions have access, e.g.
```console
C:\Users\<user>\AppData\Local\dbg\EngineExtensions
```


### Connecting
With all the initial setup in place, follow along each time you make changes to your UEFI firmware build:

1. Build with `stuart_build -c MsvmPkg\PlatformBuild.py` (build with `stuart_build -c MsvmPkg\PlatformBuild.py BUILD_ARCH=AARCH64` for ARM64 platforms)
2. Copy your PDBs over to your computer
3. Run `py ComToTcpServer.py -v -n \\.\pipe\port1 -p 5556` (you don't need `-v`, but the verbosity helps to ensure that the connection to serial is working)
4. Start your VM. You should see that you VM is running, but in a black screen. This is because we are broken in and have not started the UEFI process yet.
5. Launch Windbg and press `Ctrl-K` to get Kernel Debugger options, then click on the EXDI tab. Fill in the fields as appropriate to your platform (an example for X64 in the picture below):
    > ![Windbgx EXDI UEFI](./Docs/Images/windbgx_uefi.png)
    - NOTE: You do not have to set break on connection
6. For debug builds, the `uefiext` should be able to locate private symbols on the local host. In case you have built your firmware on a different machine than what you are testing on, add your symbols to windbg's symbols path, e.g.
```console
.sympath+ C:\project\build\PlatformPkg\PDB
```
7. Start the UEFI extension by typing into the Windbg command window: `!uefiext.init`
8. You can resolve the loaded binaries by using `!uefiext.findmodule` and `uefiext.findall` commands
9. You can use the UI buttons on top of Windbg or the command window to go, break in, step over/into etc. now.

More debugging configurations and connection scenarios can be found in the official MU Feature Debugger documentation linked at the top of this section. 



## Advanced Logger

MsvmPkg uses the Advanced Logger to output all `DEBUG()` macro statements through emulated serial on COM2. Full documentation can be found [here](https://github.com/microsoft/mu_plus/tree/HEAD/AdvLoggerPkg/Docs).

NOTE: For ARM64 platforms, we do not have SEC. Therefore, you will not see any SEC output through the advanced logger.


### Building
You must build your UEFI with `BLD_*_DEBUGLIB_SERIAL=1` to enable serial output. For example:

```powershell
stuart_build -c .\MsvmPkg\PlatformBuild.py BLD_*_DEBUGLIB_SERIAL=1
```


### Connecting
Hyper-V comes with a command line utility program `hvc`. In order to see advanced logger output through COM2, run this command: 

```powershell
hvc serial -rcp 2 <VM-name>
```

To exit out of hvc, type `ctrl+a` then `q` right after.