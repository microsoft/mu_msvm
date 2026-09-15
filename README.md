# Project

This repo contains the Msvm firmware project for virtual machines running with
the Microsoft hypervisor.

This repository is built on top of Project Mu.  Please see Project Mu for details https://microsoft.github.io/mu

## Developer Guide

See the [developer guide](MsvmPkg/Docs/DEVGUIDE.md) and in particular the getting started section.

## Firmware Artifacts

Platform CI names firmware artifacts using this case-sensitive format:

```text
firmware-<TARGET>-<ARCH>-<TOOLCHAIN>[-patina]
```

GitHub release assets use the same name with a `.tar.gz` suffix. Consumers such as
OpenVMM should select an exact asset name from a specific release tag. For example:

```text
firmware-RELEASE-X64-CLANGPDB.tar.gz
firmware-RELEASE-X64-CLANGPDB-patina.tar.gz
```

The C DXE core uses the unsuffixed name. The Rust Patina DXE core adds `-patina`;
the brackets above indicate an optional suffix and are not part of the name.
`TOOLCHAIN` identifies the EDK II toolchain; the Patina core is supplied as a
prebuilt dependency. Both `DEBUG` and `RELEASE` targets are built for each of these
combinations:

| Architecture | Toolchain | DXE Core |
| --- | --- | --- |
| X64 | VS2022 | C |
| X64 | CLANGPDB | C, Patina |
| AARCH64 | CLANGPDB | C, Patina |

Each firmware archive contains `FV/MSVM.fd` and the `MAP/` and `PDB/` symbol
directories. CI build logs use the same variant suffix with a `logs-` prefix and
are not included in releases. Releases are published after successful main-branch
push validation, including OpenVMM boot tests of all X64 variants.

This restores unsuffixed C-core names from before the `-legacy-TRUE` naming and
replaces `-legacy-FALSE` with `-patina`. Consumers of the original unsuffixed names
can keep their asset selection and opt into Patina by adding `-patina`. Consumers
using boolean-suffixed names must update their selection for new releases.
Existing release assets retain their names. Archive contents are unchanged.

## Contributing

This project welcomes [contributions](MsvmPkg/Docs/CONTRIBUTING.md) and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit [Contributor License Agreements](https://cla.opensource.microsoft.com).

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.

