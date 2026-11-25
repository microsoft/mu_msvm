# Copyright (C) Microsoft.
# SPDX-License-Identifier: BSD-2-Clause-Patent

<#
.SYNOPSIS
Builds Windows X64 MSVM firmware using an installed Visual Studio compiler.
.DESCRIPTION
Run the prerequisite and Stuart setup/update steps in MsvmPkg\Docs\BUILDING.md first.
This wrapper configures the compiler environment; it does not install dependencies.
.EXAMPLE
.\Build.ps1 -Target RELEASE
.EXAMPLE
.\Build.ps1 -Target DEBUG 'BLD_*_DEBUGLIB_SERIAL=1'
#>

[CmdletBinding()]
param (
    [ValidateSet('DEBUG', 'RELEASE')]
    [string]$Target = 'DEBUG',

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$BuildArguments = @()
)

$ErrorActionPreference = 'Stop'

try {
    if ($env:OS -ne 'Windows_NT') {
        throw 'Build.ps1 supports Windows X64 builds with the VS2022 toolchain tag only.'
    }
    foreach ($argument in $BuildArguments) {
        if ($argument -match '^(TARGET|BUILD_ARCH|TARGET_ARCH|ARCH|ACTIVE_PLATFORM|TOOL_CHAIN_TAG)=') {
            throw "Use -Target for the target. Use Stuart directly for other architectures or toolchains: $argument"
        }
    }

    $stuart = Join-Path $PSScriptRoot '.venv\Scripts\stuart_build.exe'
    if (-not (Test-Path -LiteralPath $stuart -PathType Leaf)) {
        $stuart = (Get-Command stuart_build -ErrorAction SilentlyContinue).Source
        if (-not $stuart) {
            throw 'Stuart is missing. Create a Python 3.12 virtual environment and install pip-requirements.txt; see MsvmPkg\Docs\BUILDING.md.'
        }
    }

    $installPath = $env:VS170INSTALLPATH
    $toolVersion = $env:VS170TOOLVER
    # Explicit compiler overrides take precedence over automatic Visual Studio discovery.
    if ($env:VS2022_PREFIX) {
        $prefix = Get-Item -LiteralPath $env:VS2022_PREFIX
        if (-not $prefix.PSIsContainer) {
            throw 'VS2022_PREFIX must point to an MSVC compiler directory.'
        }
        if (-not $installPath) {
            # The expected layout is <installation>\VC\Tools\MSVC\<version>.
            $installPath = $prefix.Parent.Parent.Parent.Parent.FullName
        }
        if (-not $toolVersion) {
            $toolVersion = $prefix.Name
        }
    }
    if (-not $installPath) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
            throw 'vswhere.exe is missing. Install Visual Studio 2022 or 2026 with Desktop development with C++ and a Windows SDK.'
        }
        $instancesJson = & $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json -utf8
        if ($LASTEXITCODE -ne 0) {
            throw "Visual Studio discovery failed with exit code $LASTEXITCODE."
        }
        # Prefer the CI toolchain (VS2022/17), then VS2026/18; choose the newest within each major.
        $instance = $instancesJson | ConvertFrom-Json |
            Where-Object { ([version]$_.installationVersion).Major -in 17, 18 } |
            Sort-Object @{ Expression = { ([version]$_.installationVersion).Major }; Ascending = $true },
                @{ Expression = { [version]$_.installationVersion }; Descending = $true } |
            Select-Object -First 1
        if (-not $instance) {
            throw 'No Visual Studio 2022 or 2026 installation with C++ tools found. Install Desktop development with C++ and a Windows SDK.'
        }
        $installPath = $instance.installationPath
        Write-Host "Using $($instance.displayName): $installPath"
        if (([version]$instance.installationVersion).Major -eq 18) {
            Write-Warning 'VS2026 is locally build-verified, but is not covered by the VS2022 CI configuration.'
        }
    } else {
        Write-Host "Using explicit Visual Studio installation: $installPath"
    }

    $hostArchitecture = $env:VS2022_HOST
    if (-not $hostArchitecture) {
        $hostArchitecture = $env:PROCESSOR_ARCHITECTURE
        if ($env:PROCESSOR_ARCHITEW6432) {
            $hostArchitecture = $env:PROCESSOR_ARCHITEW6432
        }
        if ($hostArchitecture -eq 'AMD64') { $hostArchitecture = 'x64' }
    }
    # The compiler host can vary, but the firmware target remains X64.
    $vcArchitecture = switch ($hostArchitecture.ToLowerInvariant()) {
        'x64' { 'amd64' }
        'x86' { 'x86_amd64' }
        'arm64' { 'arm64_amd64' }
        default { throw "Unsupported compiler host architecture: $hostArchitecture" }
    }
    $vcvars = Join-Path $installPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) {
        throw "Visual Studio developer environment script not found: $vcvars"
    }
    if ($toolVersion -and $toolVersion -notmatch '^\d+(\.\d+)*$') {
        throw 'VS170TOOLVER or the version in VS2022_PREFIX must be a numeric MSVC version.'
    }
    $versionArgument = ''
    if ($toolVersion) { $versionArgument = " -vcvars_ver=$toolVersion" }
    # A batch file cannot update PowerShell's environment directly, so capture its resulting variables.
    $developerEnvironment = & $env:ComSpec /d /s /c "call `"$vcvars`" $vcArchitecture$versionArgument >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio environment setup failed with exit code $LASTEXITCODE."
    }

    $savedEnvironment = @{}
    Get-ChildItem Env: | ForEach-Object { $savedEnvironment[$_.Name] = $_.Value }
    try {
        foreach ($line in $developerEnvironment) {
            if ($line -match '^([^=]+)=(.*)$') {
                $name = $Matches[1]
                $value = $Matches[2]
                # Leave unchanged values alone: setting an empty value can delete it on older runtimes.
                if (-not $savedEnvironment.ContainsKey($name) -or $savedEnvironment[$name] -cne $value) {
                    [Environment]::SetEnvironmentVariable($name, $value, 'Process')
                }
            }
        }
        # The explicit prefix bypasses Stuart's VS2022-only discovery, including when using VS2026.
        if (-not $env:VS2022_PREFIX) {
            $env:VS2022_PREFIX = $env:VCToolsInstallDir
        }
        if (-not $env:VS2022_HOST) {
            $env:VS2022_HOST = $hostArchitecture.ToLowerInvariant()
        }
        if (-not $env:VS2022_PREFIX -or -not (Test-Path -LiteralPath $env:VS2022_PREFIX -PathType Container)) {
            throw 'Visual Studio environment setup did not provide a valid MSVC compiler directory.'
        }
        Write-Host "MSVC compiler directory: $env:VS2022_PREFIX"

        Push-Location $PSScriptRoot
        try {
            & $stuart -c (Join-Path $PSScriptRoot 'MsvmPkg\PlatformBuild.py') `
                TOOL_CHAIN_TAG=VS2022 "TARGET=$Target" BUILD_ARCH=X64 `
                LaunchLogOnSuccess=False LaunchLogOnError=False @BuildArguments
            $buildExitCode = $LASTEXITCODE
            if ($buildExitCode -eq 0) {
                $firmware = Join-Path $PSScriptRoot "Build\MsvmX64\${Target}_VS2022\FV\MSVM.fd"
                if (-not (Test-Path -LiteralPath $firmware -PathType Leaf)) {
                    throw "Stuart succeeded but the firmware image is missing: $firmware"
                }
                Write-Host "Firmware: $firmware"
            }
        } finally {
            Pop-Location
        }
    } finally {
        # Limit compiler environment changes to this build, even if Stuart fails.
        $currentEnvironment = @{}
        Get-ChildItem Env: | ForEach-Object { $currentEnvironment[$_.Name] = $_.Value }
        foreach ($name in $currentEnvironment.Keys) {
            if (-not $savedEnvironment.ContainsKey($name)) {
                [Environment]::SetEnvironmentVariable($name, $null, 'Process')
            }
        }
        foreach ($name in $savedEnvironment.Keys) {
            if (-not $currentEnvironment.ContainsKey($name) -or
                $currentEnvironment[$name] -cne $savedEnvironment[$name]) {
                [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
            }
        }
    }
    exit $buildExitCode
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
