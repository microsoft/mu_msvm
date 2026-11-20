Some sources of truth:

  GitHub pipeline: https://github.com/microsoft/mu_msvm/blob/main/.github/workflows/Platform-Build.yml
  Internal users can refer to other pipelines.

# Git

Other than the obvious:
  git clone https://github.com/microsoft/mu_msvm

You might need more, due to submodules:

  git config --global credential.useHttpPath true

With --global. Errors will mention useHttpPath but not the --global part.

And credential.helper=/mnt/c/Program\ Files/Git/mingw64/bin/git-credential-manager.exe
on WSL2, and maybe similar on Windows. (How to configure native Linux?)

# Build

Python virtual env is recommended, but CI does not use it, so not required.
How to work locally without virtualenv is TBD, since I could not get it to work, despite pipelines being that way.

  # Setup Build

    # Linux:
      sudo apt-get install -y acpica-tools mono-complete clang lld llvm
    # Without mono-complete, Nuget download will fail.
    # If you are interested in gcc: sudo apt-get install -y acpica-tools mono-complete clang lld llvm binutils-x86-64-linux-gnu gcc-x86-64-linux-gnu binutils-aarch64-linux-gnu gcc-aarch64-linux-gnu

    # make virtual env, venv1 is an example name.
    python3 -m venv venv1

    # enter virtual env (every session)
    Linux: source venv1/bin/activate
    Windows: venv1\Scripts\activate.bat

    # Some documentation misnames the pip .txt file. If not sure, see what is there.
    # Older branches: pip3 install --upgrade -r pip_requirement_stable.txt
    pip3 install --upgrade -r pip-requirements.txt
    python3 MsvmPkg/PlatformBuild.py --setup
    python3 MsvmPkg/PlatformBuild.py --update

    # Install Clang for Windows.

    There are a few options.
      1. VS2022 will install it. This can be automated.
      2. You can download releases from GitHub.
      3. You can build it yourself.
      4. There is an internally supported version, i.e. with bug fixes for "more demanding" code bases. UEFI is a fairly
         small portable code base and for private development, almost anything will work.

   e.g.:
    set CLANG_BIN=C:\Program Files\LLVM\bin\

  with the trailing slash.
  It is quite possible build will find the VS2022 install without setting an environment variable.
  Or via %PATH% search.
  Set CLANG_BIN per session.

  # Activate Python virtual environment (each session).

    Linux: source venv1/bin/activate
    Windows: venv1\Scripts\activate.bat

  # Inner Build Loop

    # VS2022 is going away or gone, prefer CLANGPDB, which ideally defaults
    # Of course this will vary per branch/commit. Older revisions only have VS2022.
    python3 MsvmPkg/PlatformBuild.py TARGET=DEBUG TOOL_CHAIN_TAG=VS2022
or:
    python3 MsvmPkg/PlatformBuild.py TARGET=DEBUG TOOL_CHAIN_TAG=CLANGPDB 
or:
    python3 MsvmPkg/PlatformBuild.py TARGET=DEBUG TOOL_CHAIN_TAG=CLANGPDB BUILD_ARCH=AARCH64
  etc.

   If there are build failures, see Build/BUILDLOG.txt

# Deploy

  The main output is the .fd file.
    dir /s/b *.fd
    find . | grep fd$

  Powershell:
    Set-ItemProperty "HKLM:/Software/Microsoft/Windows NT/CurrentVersion/Virtualization" -Name "AllowFirmwareLoadFromFile" -Value 1 -Type DWORD | Out-Null
  e.g.:
   copy /y Build\MsvmX64\DEBUG_CLANGPDB\FV\MSVM.fd %windir%\System32
   Note that WSL has very good file system interoperation with Windows, so copying from a build in WSL to system32 is convenient also.

  In the production system, MSVM.fd is communicated via "vpack" (similar to nuget) to a Win32 resource in vmfirmware.dll.
  If you have such powers, you can replace it there. i.e. something like:
    bcdedit /set testsigning on
    reboot: shutdown -r -t 1
    cd %windi%\system32
    takeown /f %windir%\system32 /a
    cacls %windir%\system32 /g %username%:f /e
    takeown /f %windir%\system32\vmfirmware.dll /a
    cacls %windir%\system32\vmfirmware.dll /g %username%:f /e
    mkdir backup1
    move vmfirmware.dll backup1
    copy /y backup1 .
    etc.

  It is plain to see (open as resources in VisualStudio), there are two or more resources, one is just ACPI tables, others are MSVM.fd.
  ACPI tables are Build\***\Dsdt.aml in resource 13506.
  There are multiple versions of MSVM.fd (0x1901, 0x1902, 0x1A01), so that memory map is stable across suspend/resume/migrate.
  The mainline/head one is 13505.

# Test

  Booting is one common test.

# Debug

  Choices:
    1. Print to serial port.
    2. Print to Hypervisor debugger.
       #define MkDebugHex(_Input_) MkArm64InvokeSmcDbgi(DBGI_FUNCTION_CODE_OUT_HEX, __LINE__, _Input_)
    3. Legacy closed source AMD64 debugger.
    4. Open source architecture-portable debugger1
    5. Open source architecture-portable debugger2

 There are many commands required for various options of debugging or printing over serial/namedpipe.
 There are also many build options, e.g. BLD_*_DEBUG_NOISY=1 BLD_*_DEBUGLIB_SERIAL=1
 Details are needed here.
 Internal users, see https://eng.ms/docs/windows/devices-drivers-hardware/drivers-firmware/hyper-v-virtual-machine-firmware-development-instructions
