# Virtio (virtio-blk over PCI) — Upstream Provenance

The virtio-blk-over-PCI driver stack in MsvmPkg is **ported verbatim from EDK2
OvmfPkg**. This document records exactly where the code came from and what (if
anything) was changed, so that:

- Reviewers can mechanically answer "is this still verbatim / what diverged?"
- The next person picking up an upstream security fix or CVE patch has a precise
  base commit to diff against.

## Source coordinates

| | |
|---|---|
| Repository | <https://github.com/tianocore/edk2> |
| Tag        | `edk2-stable202511` |
| Commit     | `46548b1adac82211d8d11da12dd914f41e7aa775` |

## Porting rule

The **only** change applied to the upstream sources is retargeting each INF's
`[Packages]` section from `OvmfPkg/OvmfPkg.dec` to `MsvmPkg/MsvmPkg.dec`. All
`.c` and `.h` files are **verbatim** copies of the upstream sources at the commit
above.

The `gVirtioDeviceProtocolGuid` GUID value is preserved from OvmfPkg (defined in
`MsvmPkg/MsvmPkg.dec`).

## Divergence from upstream

Only the **modern (virtio 1.0+) non-transitional** transport is ported. The
**legacy/transitional** transport `OvmfPkg/VirtioPciDeviceDxe` is intentionally
**not** included, because:

- It performs no DMA translation (its `MapSharedBuffer` is an identity map and it
  allocates with plain `AllocatePages`), so it cannot bounce-buffer and is unsafe
  on hardware-isolated / confidential guests.
- It accesses the device through I/O-port BARs, which are not generally available
  on AARCH64.

The modern `Virtio10Dxe` transport negotiates `VIRTIO_F_VERSION_1` /
`VIRTIO_F_IOMMU_PLATFORM` and uses `PciIo->Map` / `PciIo->AllocateBuffer`, which
is the correct and safe path.

## Ported components

### Drivers

| MsvmPkg path | Upstream path |
|---|---|
| `MsvmPkg/VirtioBlkDxe` | `OvmfPkg/VirtioBlkDxe` |
| `MsvmPkg/Virtio10Dxe`  | `OvmfPkg/Virtio10Dxe` (modern 1.0 PCI transport) |

### Libraries

| MsvmPkg path | Upstream path |
|---|---|
| `MsvmPkg/Library/VirtioLib`          | `OvmfPkg/Library/VirtioLib` |
| `MsvmPkg/Library/BasePciCapLib`      | `OvmfPkg/Library/BasePciCapLib` |
| `MsvmPkg/Library/UefiPciCapPciIoLib` | `OvmfPkg/Library/UefiPciCapPciIoLib` |

### Headers

| MsvmPkg path | Upstream path |
|---|---|
| `MsvmPkg/Include/IndustryStandard/Virtio.h`     | `OvmfPkg/Include/IndustryStandard/Virtio.h` |
| `MsvmPkg/Include/IndustryStandard/Virtio095.h`  | `OvmfPkg/Include/IndustryStandard/Virtio095.h` |
| `MsvmPkg/Include/IndustryStandard/Virtio10.h`   | `OvmfPkg/Include/IndustryStandard/Virtio10.h` |
| `MsvmPkg/Include/IndustryStandard/VirtioBlk.h`  | `OvmfPkg/Include/IndustryStandard/VirtioBlk.h` |
| `MsvmPkg/Include/Protocol/VirtioDevice.h`       | `OvmfPkg/Include/Protocol/VirtioDevice.h` |
| `MsvmPkg/Include/Library/VirtioLib.h`           | `OvmfPkg/Include/Library/VirtioLib.h` |
| `MsvmPkg/Include/Library/PciCapLib.h`           | `OvmfPkg/Include/Library/PciCapLib.h` |
| `MsvmPkg/Include/Library/PciCapPciIoLib.h`      | `OvmfPkg/Include/Library/PciCapPciIoLib.h` |

> Note: `Virtio095.h` is named for the 0.9.5 spec but defines base structures
> (vrings, generic feature bits, etc.) that the modern stack also depends on — it
> is included by `Virtio10.h`. It is a shared header, not legacy-only.

## Refreshing from upstream

To pick up an upstream fix:

1. Diff the MsvmPkg copy of a file against the same file at the recorded commit in
   `tianocore/edk2` to confirm it is still verbatim (aside from the INF
   `[Packages]` retarget).
2. Apply the upstream change, re-applying only the `[Packages]` retarget to any
   INF.
3. Update the **Commit** (and **Tag**, if bumped) in the *Source coordinates*
   table above to the new base.

## Formatting / linting

These are foreign, verbatim upstream files. If a source-formatting gate (e.g.
uncrustify) is ever added to CI, exclude these paths so that reformatting does not
break the "verbatim" guarantee and complicate future upstream diffs.
