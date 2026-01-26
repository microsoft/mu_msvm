/** @file
  ACPI DSDT table source

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

// Establish local define for architecture

#if defined(MDE_CPU_X64) || defined(MDE_CPU_IA32)

#define _DSDT_INTEL_

#elif defined(MDE_CPU_AARCH64)

#define _DSDT_ARM_

#else

#error Unsupported Architecture!

#endif

// Define DSDT

DefinitionBlock (
    "Dsdt.aml",
    "DSDT",
    0x02,       // DSDT Compliance revision.
                // A Revision field value greater than or equal to 2 signifies that integers
                // declared within the Definition Block are to be evaluated as 64-bit values
    "MSFTVM",   // OEM ID (6 byte string)
    "DSDT01",   // OEM table ID  (8 byte string)
    0x01        // OEM version of DSDT table (4 byte Integer)
    )
{
    // The following operation region provides for a block of system memory
    // that can be referenced by this ASL code. The memory block contains
    // configuration parameters that are filled in at runtime by the UEFI C code in:
    // MsvmPkg/AcpiPlatformDxe/Dsdt.c DsdtAllocateAmlData. This definition of the
    // OperationRegion must match the signature in the above C code. As well
    // the definition of the fields must match the DSDT_AML_DATA struct in the above
    // C code.

    OperationRegion(BIOS, SystemMemory, 0xffffff00, 0xff)
    Field(BIOS, ByteAcc, NoLock, Preserve)
    {
        MG2B,32,        // Base of first MMIO region in bytes.
        MG2L,32,        // Length of first MMIO region in bytes.
        HMIB,32,        // Base of the second MMIO region in MB.
        HMIL,32,        // Length of the second MMIO region in MB.
        GCAL,32,        // Lower 32 bit address of Generation counter
        GCAH,32,        // Upper 32 bit address of Generation counter
        PCNT,32,        // Processor count
        NVDA,32,        // NVDIMM Method buffer address
        SCFG,8,         // Serial controllers enabled/disabled
        TCFG,8,         // TPM enabled/disabled
        PCFG,8,         // OEMP table load enabled/disabled
        HCFG,8,         // Hibernation enabled/disabled
        NCFG,8,         // PMEM (NVDIMMs) enabled/disabled
        BCFG,8,         // Virtual Battery enabled/disabled
        SGXE,8,         // SGX Memory enabled/disabled
        PADE,8,         // Processor Aggregator Device enabled/disabled
        CCFG,8,         // CXL memory support enabled/disabled
        SINT,8,         // HV SINT PPI device enabled
        NCNT,16,        // NVDIMM count
    }

    // Supported machine sleep states =========================================

#if defined(_DSDT_INTEL_)

    // Define the S0 running state. This package is not used, but it is required
    // to exist by the specification.

    Name(\_S0, Package(2){0, 0})

    // Define the S5 powered off state. The first package value is the value to
    // write to PM1A_CNT.SLP_TYPE to cause the machine to power off. The second
    // value is for PM1B_CNT.SLP_TYPE, which is not supported by the Hyper-V
    // PM device.

    Name(\_S5, Package(2){0, 0})

#endif

    // Define the S4 hibernate state only if configured.

    If(LGreater(HCFG, 0))
    {
        Name(\_S4, Package(2){1, 0})
    }

    Scope(\_SB)
    {
        // ARM needs to use the strict check to determine if the hibernate state is present.

        Method(_DSM, 4, NotSerialized)
        {
            // Comparing DSM UUID to ACPI DSM UUID for S4 toggle (STRICT_S4_CHECK_DSM_UUID)
            If (LEqual(Arg0, ToUUID ("713E539D-E06E-4AE9-A75D-21EB34112B7E")))
            {
                 // Function 0: Query function, return based on revision
                If (LEqual(ToInteger(Arg2), 0))
                {
                    // Revision 0: Function 1 supported
                    If (LEqual(ToInteger(Arg1), 0))
                    {
                        Return (Buffer () {0x03})
                    }
                }

                // Function 1 : Strict S4 enforcement toggle function
                If (LEqual(ToInteger(Arg2), 1))
                {
                    Return(0x0001)
                }
            }
            Return (Buffer () {0x00})
        }
    }



    // VMOD ==================================================================

    // ACPI module device that holds the VMBus device. A module device is
    // useful here because Windows automatically provides a resource arbiter
    // for module devices that have resources. This allows VMBus's child devices
    // (including virtual PCI and its children) to claim memory resources
    // without VMBus having to provide its own arbiter implementation.
    // Without this, Windows looks for memory space on any ISA bus it can find,
    // which means that the PCI bus arbiter gets used. However, since the PCI
    // bus is typically not present in UEFI Hyper-V VMs, this is not an option.
    // An alternative would be to write an arbiter implementation for VMBus
    // but this would prevent inbox Windows 7 and Windows 8 VMBus drivers from
    // claiming memory space which would prevent synthetic video and SR-IOV
    // devices from working.

    Device(\_SB.VMOD)
    {
        Name(_HID, "ACPI0004")
        Name(_UID, 0)
        Name(_CRS,
            ResourceTemplate()
            {
                // MMIO space below 4GB.
                DWORDMemory(ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                // Granularity Min Max Translation Range (Length = Max-Min+1)
                   0,          0,  0,  0,          0,,,
                   MEM6)   // Name declaration for this descriptor
                // MMIO above 4GB
                QWORDMemory( ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                //  Granularity Min Max Translation Range (Length = Max-Min+1)
                    0,          0,  0,  0,          0,,,
                    MEM7)
            }
        )

        CreateDwordField(_CRS, MEM6._MIN, MIN6)  // Min
        CreateDwordField(_CRS, MEM6._MAX, MAX6)  // Max
        CreateDwordField(_CRS, MEM6._LEN, LEN6)  // Memory length

        CreateQwordField(_CRS, MEM7._MIN, MIN7)  // Min
        CreateQwordField(_CRS, MEM7._MAX, MAX7)  // Max
        CreateQwordField(_CRS, MEM7._LEN, LEN7)  // Memory length

        Method(_INI, 0)
        {
            // Update the DWORDMemory resource descriptor with the low MMIO region.
            Store(MG2B, MIN6)
            Store(MG2L, LEN6)
            Store(MG2L, Local0)
            Add(MIN6, Decrement(Local0), MAX6)

            // Update the QWORDMemory resource descriptor with the high MMIO region.
            ShiftLeft (HMIB, 20, Local1)
            ShiftLeft (HMIL, 20, Local2)
            Store(Local1, MIN7)
            Store(Local2, LEN7)
            Store(Local2, Local0)
            Add(MIN7, Decrement(Local0), MAX7)
        }
    }

    // BIOS Registers =========================================================

    Scope(\_SB)
    {

#if defined(_DSDT_INTEL_)

        OperationRegion(BIOB, SystemIO, FixedPcdGet32(PcdBiosBaseAddress), 0x8)

#elif defined(_DSDT_ARM_)

        OperationRegion(BIOB, SystemMemory, FixedPcdGet32(PcdBiosBaseAddress), 0x1000)

#endif

        Field (BIOB, DWordAcc, NoLock, Preserve)
        {
            BADR, 32,      // address
            BDAT, 32,      // data
        }
    }

    // APIC ===================================================================

#if defined(_DSDT_INTEL_)

    Device(\_SB.VMOD.APIC)
    {
        Name(_HID, EISAID("PNP0003"))
        Name(_CRS,
            ResourceTemplate()
            {
                // Local APIC - hard coded architectural address
                Memory32Fixed(ReadWrite, 0xfee00000, 0x1000)
                // I/O APIC - hard coded architectural address
                Memory32Fixed(ReadWrite, 0xfec00000, 0x1000)
            })
    }

#endif

    // Serial Ports =======================================================

    If(LGreater(SCFG, 0))
    {

#if defined(_DSDT_INTEL_)

        // COM1 (SerialControllerDevice.cpp).
        Device(\_SB.UAR1)
        {
            Name(_HID, EISAID("PNP0501")) // 16550A-compatible COM port
            Name(_DDN, "COM1")
            Name(_UID, 1)
            Name(_CRS, ResourceTemplate()
            {
                IO(Decode16, FixedPcdGet32(PcdCom1RegisterBase), FixedPcdGet32(PcdCom1RegisterBase), 1, 8)
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdCom1Vector)}
            })
        }

        // COM2 (SerialControllerDevice.cpp).
        Device(\_SB.UAR2)
        {
            Name(_HID, EISAID("PNP0501")) // 16550A-compatible COM port
            Name(_DDN, "COM2")
            Name(_UID, 2)
            Name(_CRS, ResourceTemplate()
            {
                IO(Decode16, FixedPcdGet32(PcdCom2RegisterBase), FixedPcdGet32(PcdCom2RegisterBase), 1, 8)
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdCom2Vector)}
            })
        }

#elif defined(_DSDT_ARM_)

        // COM1 (PL011Device.cpp)
        Device(\_SB.VMOD.UAR1)
        {
            Name(_HID, "ARMH0011") // ARM SBSA PL011 UART
            Name(_DDN, "COM1")
            Name(_UID, 1)
            Name(_CRS, ResourceTemplate()
            {
                Memory32Fixed(ReadWrite, FixedPcdGet32(PcdCom1RegisterBase), 0x1000)
                Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdCom1Vector)}
            })
            Name(_DSD, Package() {
                ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package() {
                    Package(2) {"SerCx-FriendlyName", "UART1"}
                }
            })
        }

        // COM2 (PL011Device.cpp)
        Device(\_SB.VMOD.UAR2)
        {
            Name(_HID, "ARMH0011") // ARM SBSA PL011 UART
            Name(_DDN, "COM2")
            Name(_UID, 2)
            Name(_CRS, ResourceTemplate()
            {
                Memory32Fixed(ReadWrite, FixedPcdGet32(PcdCom2RegisterBase), 0x1000)
                Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdCom2Vector)}
            })
            Name(_DSD, Package() {
                ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package() {
                    Package(2) {"SerCx-FriendlyName", "UART2"}
                }
            })
        }

#endif

    }

    // VMBus ==================================================================

    Device(\_SB.VMOD.VMBS)
    {
        Name(STA, 0xF)
        Name(_ADR, 0x00)
#if defined(_DSDT_ARM_)
        Name(_CCA, One)
#endif
        Name(_DDN, "VMBUS")
        Name(_HID, "MSFT1000")
        Name(_CID, "VMBus")
        Name(_UID, 0)
        Method(_DIS, 0) { And(STA, 0xD, STA) }
        Method(_PS0, 0) { Or(STA, 0xF, STA) }
        Method(_STA, 0)
        {
            return(STA)
        }

        // Older versions of this DSDT implemented _PS3 improperly, as:
        //     Name(_PS3, 0)
        // This is intentionally a do-nothing method in case any version of Windows requires _PS3 to be implemented

        Method(_PS3, 0) { return(STA) }

        // TODO: SPIs are not available to the guest on AARCH64, which is what
        // PcdVmbusVector is currently defined as. Supposedly it should use a PPI,
        // but those are strange because they're reserved for hypervisor devices.
        //
        // Windows doesn't boot when VmBus is given an SPI, since it's unable to
        // allocate any since none exist in guests. Thus, leave it out on AARCH64
        // for now.
        //
        // Linux may need this field if it's not hardcoded, unsure.
        //
        // Additionally, no Interrupt-Signaled event devices currently work either,
        // due to SPIs not being available to guests.
        Name(_CRS,

            // Include an interrupt resource so that Linux VMs can get IDT
            // entries.
            //
            // N.B. All Windows VMs that support UEFI also support
            // getting IDT entries via other mechanisms, so this is not
            // necessary for Windows.

            ResourceTemplate()
            {

#if defined(_DSDT_INTEL_)

                // Older Linux kernels like RHEL/CentOS don't seem to be able to
                // parse the new Extended Interrupt Descriptor resource type (see ACPI Section 6.4.3.6),
                // so we instead use the old legacy IRQ description which
                // becomes the short form of Interrupt Descriptor (ACPI Section 5.4.2.1)
                // which only supports legacy PIC devices to describe up to 15
                // interrupts. VMBUS is interrupt 5 on X64, so this is okay.
                IRQ(Edge,ActiveHigh,Exclusive)
                    {FixedPcdGet8(PcdVmbusVector)}

#else
                // On AArch64, we select a PPI (16) because Linux expects it to
                // be available to all CPUs.
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                   {FixedPcdGet8(PcdVmbusVector)}

#endif
            }
        )
    }

#if defined(_DSDT_ARM_)
    // Intercept SINT =========================================================
    // Exposes a PPI for Linux L1VH to use for hypervisor intercept SINTs.
    // Only enabled when the loader sets the HvSintEnabled flag.

    If(LGreater(SINT, 0))
    {
        Device(\_SB.VMOD.SINT)
        {
            Name(_HID, "MSFT1003")
            Name(_UID, 0)
            Name(_DDN, "Hyper-V SINTs")
            Name(_CRS, ResourceTemplate()
            {
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdInterceptSintVector)}
            })
            // _STA: Return status bitmap
            // Bit 0: Present, Bit 1: Enabled, Bit 3: Functioning
            // Bit 2 NOT set: Do not show in UI (prevents "unknown device" in Device Manager)
            Method(_STA, 0, NotSerialized)
            {
                Return (0x0B)
            }
        }
    }
#endif

    // TPM ====================================================================

    If(LGreater(TCFG, 0))
    {
        Device(\_SB.VMOD.TPM2)
        {
            Name(_ADR, 0x00)
            Name(_HID, "MSFT1001")
            Name(_CID, Package (2) { "MSFT0101", "VTPM0101" })
            Name(_UID, 0x01)
            Name(_DDN, "Microsoft Virtual TPM 2.0")
            Name(_STR, Unicode ("Microsoft Virtual TPM 2.0"))
            Name(_CRS, ResourceTemplate () {
                Memory32Fixed(ReadWrite, FixedPcdGet32(PcdTpmBaseAddress), 0x1000)
            })
            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }

#if defined(_DSDT_INTEL_)

            // Operational region for TPM general IO port access
            OperationRegion (GIO, SystemIO, 0x1040, 8)
            Field (GIO, DWordAcc, NoLock, Preserve)
            {
                CTLP, 32,      // 32-bit control port
                DATP, 32,      // 32-bit data port
            }

#elif defined(_DSDT_ARM_)

            // MMIO region for Virtual TPM
            OperationRegion(GIO, SystemMemory, FixedPcdGet32(PcdTpmBaseAddress) + 0x80, 8)
            Field(GIO, DWordAcc, NoLock, WriteAsZeros)
            {
                CTLP, 32,      // 32-bit control port
                DATP, 32,      // 32-bit data port
            }

#endif

            // TCG Physical Presence Interface
            Method (TPPI, 2, Serialized)
            {
                Name(PKG2, Package(){0, 0})
                Name(PKG3, Package(){0, 0, 0})

                // Switch by function index
                Switch (ToInteger(Arg0))
                {
                    Case (0)
                    {
                        // Func 0 - Standard query, supports function 1-8
                        Return (Buffer () {0xFF, 0x01})
                    }
                    Case (1)
                    {
                        // Func 1 - Get Physical Presence Interface Version
                        Return ("1.3")
                    }
                    Case (2)
                    {
                        // Func 2 - Submit TPM Operation Request to Pre-OS Environment (Deprecated, Not implemented)
                        Return (3)
                    }
                    Case (3)
                    {
                        // Func 3 - Get Pending TPM Operation Requested By the OS
                        // Process the request in vDev. IO command is identical to Function Id
                        Store (3, CTLP)
                        Store (0, Index (PKG2, 0))
                        Store (DATP, Index (PKG2, 1))
                        Return (PKG2)
                    }
                    Case (4)
                    {
                        // Func 4 - Get Platform-Specific Action to Transition to Pre-OS Environment
                        // Return reboot.
                        Return (2)
                    }
                    Case (5)
                    {
                        // Func 5 - Return TPM Operation Response to OS Environment
                        // Process the request in vDev. IO command is identical to Function Id
                        // Get operation value
                        Store (5, CTLP)
                        Store (DATP, Index (PKG3, 1))
                        // Get operation result
                        Store (6, CTLP)
                        Store (DATP, Index (PKG3, 2))
                        // Set succeed
                        Store (0, Index (PKG3, 0))
                        Return (PKG3)
                    }
                    Case (6)
                    {
                        // Func 6 - Submit preferred user language (Not implemented)
                        Return (3)
                    }
                    Case (7)
                    {
                        // Func 7 - Submit TPM Operation Request to Pre-OS Environment 2
                        // Process the request in vDev. IO command is identical to Function Id
                        Store (7, CTLP)
                        Store (DerefOf (Index (Arg1, 0)), DATP)
                        Return (DATP)
                    }
                    Case (8)
                    {
                        // Func 8 - Get User Confirmation Status for Operation
                        // Process in vDev. IO command is identical to Function Id
                        Store (8, CTLP)
                        Store (DerefOf (Index (Arg1, 0)), DATP)
                        Return (DATP)
                    }
                }
                Return (1)
            }

            Method (TMCI, 2, Serialized)
            {
                // Switch by function index
                Switch (ToInteger (Arg0))
                {
                    Case (0)
                    {
                        // Standard query, supports function 1-1
                        Return (Buffer () {0x03})
                    }
                    Case (1)
                    {
                        Store (0x31, BADR) // 0x32 is BiosConfigMorSetVariable
                        Store (DerefOf (Index (Arg1, 0)), BDAT)
                        return (BDAT)
                    }
                }
                Return (1)
            }

            Method (_DSM, 4, Serialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
            {
                // TCG Physical Presence Interface
                If (LEqual(Arg0, ToUUID ("3dddfaa6-361b-4eb4-a424-8d10089d1653")))
                {
                    Return (TPPI(Arg2, Arg3))
                }

                // TCG Memory Clear Interface
                If (LEqual(Arg0, ToUUID ("376054ed-cc13-4675-901c-4756d7f2d45d")))
                {
                    Return (TMCI (Arg2, Arg3))
                }

                // If not one of the function identifiers we recognize, then return a buffer
                // with bit 0 set to 0 indicating no functions supported.
                Return (Buffer () {0})
            }

        }
    }

    // SGX ====================================================================

    // The Enclave Page Cache aka SGX memory device. This is intentionally not
    // Intel spec compliant in that it doesn't have any memory regions described
    // in the _CRS. Existence of this device will trigger a guest kernel to load
    // a device driver. That device driver will use other mechanisms (cpuid) to
    // discover the SGX memory regions.

#if defined (_DSDT_INTEL_)

    If(LGreater(SGXE, 0))
    {
        Device(\_SB.EPC)
        {
            Name(_HID, EISAID("INT0E0C"))
            Name(_STR, Unicode ("Enclave Page Cache 1.0"))
            Name(_CRS, ResourceTemplate()
            {
                // This is dummy data to make the _CRS not empty.
                VendorShort() { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
            })
            Method(_STA, 0x0)
            {
                Return(0xF)
            }
        }
    }

#endif

    // Generation Counter =====================================================

    Device(\_SB.GENC)
    {
        Name(_HID, "MSFT1002")
        Name(_CID, Package (3) { "VM_Gen_Counter", "VMGENCTR", "Hyper_V_Gen_Counter_V1" })
        Name(_UID, 0)
        Name(_DDN, "VM_Gen_Counter")
        Method(ADDR, 0)
        {
            Name(LPKG, Package(){0, 0})
            Store(GCAL, Index(LPKG, 0))
            Store(GCAH, Index(LPKG, 1))
            Return(LPKG)
        }
    }

#if defined(_DSDT_INTEL_)


    // GPE method for generation counter
    Scope(\_GPE)
    {
        // Method for notifying external changes to the generation counter:
        //      E  - This event is edge triggered
        //      00 - Use bit 0 in the General Purpose Event register described
        //           in the FADT
        Method(_E00)
        {
            Notify(\_SB.GENC, 0x80)
        }
    }

#elif defined(_DSDT_ARM_)

    // Interrupt signalled event for generation counter
    Device (\_SB.GED1)
    {
        Name(_HID, "ACPI0013")
        Name(_UID, 1)
        Name(_CRS,
            ResourceTemplate()
            {
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdGenCountEventVector)}
            }
        )
        Method(_EVT, 1)
        {
            Notify(\_SB.GENC, 0x80) // counter value changed event
        }
    }
#endif

    // RealTime Clock (RTC) ===================================================

#if defined(_DSDT_INTEL_)
    Device(\_SB.RTC0)
    {
        Name(_HID, EISAID("PNP0B00")) // AT real-time clock
        Name(_UID, 0)
        Name(_CRS, ResourceTemplate()
        {
            IO(Decode16, FixedPcdGet32(PcdRtcRegisterBase), FixedPcdGet32(PcdRtcRegisterBase), 0, 0x2)
            Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive) {FixedPcdGet8(PcdRtcVector)}
        })
    }
#endif

    // Processor Aggregator Device  ===========================================

    If(LGreater(PADE, 0))
    {
        // MMIO region for Processor Aggregator Device
        OperationRegion(PADM, SystemMemory, FixedPcdGet32(PcdProcIdleBase), 0x1000)
        Field(PADM, DWordAcc, NoLock, WriteAsZeros)
        {
            PRID,32, // Number of processors to idle
            OST1,32, // _OST Arg1 Status Code (success 0, no action 1)
            OST2,32, // _OST Arg2 Number processors idled
        }

        Device(\_SB.VMOD.PAD1)
        {
            Name(_CID, "Virtual Processor Aggregator Device")
            Name(_HID, "ACPI000C")

            Method(_PUR, 0)
            {
                Name(PUR, Package ()
                {
                    1, // RevisionID 1
                    0  // Number of processors to idle
                })

                Store(PRID, Index(PUR, 1))
                Return(PUR)
            }

            Method(_OST, 3, Serialized)
            {
                If (LEqual(Arg1, 0x0))
                {
                    Store(Arg2, OST2)
                }
                Store(Arg1, OST1)
            }
        }

#if defined (_DSDT_INTEL_)

        // GPE method for Processor Aggregator
        Scope(\_GPE)
        {
            // Method for notifying external changes to the Processor Aggregator Device:
            //      E  - This event is edge triggered
            //      0B - Use bit B in the General Purpose Event register described
            //           in the FADT
            Method(_E0B)
            {

#elif defined (_DSDT_ARM_)

        // Interrupt signalled event device for Processor Aggregator
        Device(\_SB.GED4)
        {
            Name(_HID,"ACPI0013")
            Name(_UID, 4)
            Name(_CRS, ResourceTemplate()
            {
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdProcIdleEventVector)}
            })
            Method(_EVT, 1)
            {
#endif
                // Notify OSPM of new idle request
                Notify(\_SB.VMOD.PAD1, 0x80)
            }
        }
    }

    // Battery ================================================================

    If(LGreater(BCFG, 0))
    {

        // MMIO region for Virtual Battery
        OperationRegion(BATM, SystemMemory, FixedPcdGet32(PcdBatteryBase), 0x1000)
        Field(BATM, DWordAcc, NoLock, WriteAsZeros)
        {
            BSTA,32, // Battery Status
            BSTE,32, // Battery State
            BRAT,32, // Battery Present Rate
            BCAP,32, // Battery Remaining Capacity
            ACPS,32, // AC Adapter PSR Status
            BNST,32, // Battery Notify Status
            BNCL,32  // Battery Notify Clear
        }

        Device(\_SB.VMOD.BAT1)
        {
            Name(BIX, Package () {
                0, // Revision - 0
                0x0, // Power unit - 0 is mWh
                5000, // Design Capacity - 5000 mWh
                5000, // Last Full Charge Capacity - 5000 mWh
                0x00000001, // Battery Technology - Secondary (Rechargeable)
                0x1388, // Design Voltage - 5 Volts
                500, // Design capacity of warning - 10%
                250, // Design capacity of low - 5%
                0xFFFFFFFF, // Cycle count - Unknown
                10000, // Measurement Accuracy - 100%
                0xFFFFFFFF, // Max Sampling Time - Unknown
                0xFFFFFFFF, // Min Sampling Time - Unknown
                1000, // Max Averaging Internval - 1000ms
                100, // Min Averaging
                10, // Battery Capacity Granularity 1
                10, // Battery Capacity Granularity 2
                "Microsoft Hyper-V Virtual Battery",
                "Virtual",
                "Virtual Battery",
                ""
            })

            Name(BIXE, Package () {
                0, // Revision - 0
                0x0, // Power unit - 0 is mWh
                0xFFFFFFFF, // Design Capacity - Unknown
                0xFFFFFFFF, // Last Full Charge Capacity - Unknown
                0x00000001, // Battery Technology - Secondary (Rechargeable)
                0xFFFFFFFF, // Design Voltage - Unknown
                0, // Design capacity of warning - 10%
                0, // Design capacity of low - 5%
                0xFFFFFFFF, // Cycle count - Unknown
                10000, // Measurement Accuracy - 100%
                0xFFFFFFFF, // Max Sampling Time - Unknown
                0xFFFFFFFF, // Min Sampling Time - Unknown
                1000, // Max Averaging Internval - 1000ms
                100, // Min Averaging
                10, // Battery Capacity Granularity 1
                10, // Battery Capacity Granularity 2
                "",
                "",
                "",
                ""
            })

            Name(_CID, "Virtual Battery")
            Name(_HID, "PNP0C0A")
            Method(_BIX, 0)
            {
                // Check if battery is present
                If (LEqual(BSTA, 0x1F))
                {
                    // If so return BIX with battery static capacity info
                    Return(BIX)
                }
                Else
                {
                    // Otherwise return battery empty BIX
                    Return(BIXE)
                }
            }

            Method(_BST, 0)
            {
                Name(BST, Package () {
                    0x0, // Battery State
                    0x0, // Battery Present Rate (discharge rate)
                    0x0, // Battery Remaining Capacity
                    0x1388  // Battery Present Voltage - 5 volts
                })

                Store(BSTE, Index(BST, 0))
                Store(BRAT, Index(BST, 1))
                Store(BCAP, Index(BST, 2))

                // Check if battery isn't present
                If (LEqual(BSTA, 0xF))
                {
                    // If so, report unknown voltage
                    Store(0xFFFFFFFF, Index(BST, 3))
                }

                Return(BST)
            }

            Method(_STA, 0)
            {
                Return(BSTA)
            }

            Name(_PCL, Package() {
                \_SB // All nodes under SB are powered by this device
            })
        }

        Device(\_SB.VMOD.AC1)
        {
            Name(_HID, "ACPI0003")
            Name(_CID, "Virtual AC Adapter")

            Name(_PCL, Package () {
                \_SB // All nodes under SB are powered by this device
            })

            Method(_PSR, 0)
            {
                Return(ACPS)
            }
        }

#if defined (_DSDT_INTEL_)

        // GPE method for battery
        Scope(\_GPE)
        {
            // Method for notifying external changes to the battery device:
            //      E  - This event is edge triggered
            //      09 - Use bit 9 in the General Purpose Event register described
            //           in the FADT
            Method(_E09)
            {

#elif defined (_DSDT_ARM_)

        // Interrupt signalled event device for battery
        Device(\_SB.GED2)
        {
            Name(_HID,"ACPI0013")
            Name(_UID, 2)
            Name(_CRS, ResourceTemplate()
            {
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                    {FixedPcdGet32(PcdBatteryEventVector)}
            })
            Method(_EVT, 1)
            {

#endif

                // Read battery notify type into local
                Store(BNST, Local0)

                // Break status into different bits
                // Local1 is 0x80 status at bit 0
                // Local2 is 0x81 status at bit 1
                And(Local0, 0x1, Local1)
                And(Local0, 0x2, Local2)

                // For Windows and Linux, sending a notify of 0x81 forces a recheck
                // of both _BIX and _BST. Thus we can let Notify 0x81 take priority
                // if we see both bits set.
                If (LEqual(Local2, 0x2))
                {
                    // Note that since 0x81 takes priority, force a recheck of AC
                    // _PSR as well.
                    Notify(\_SB.VMOD.BAT1, 0x81)
                    Notify(\_SB.VMOD.AC1, 0x80)
                }
                ElseIf(LEqual(Local1, 0x1))
                {
                    // Notify OSPM that both battery _BST and AC _PSR have changed
                    Notify(\_SB.VMOD.BAT1, 0x80)
                    Notify(\_SB.VMOD.AC1, 0x80)
                }

                // Clear whatever bits we read as set by writing to the clear
                // location
                Store(Local0, BNCL)
            }
        }
    }

    // NVDIMM root and child devices

    If(LGreater(NCFG, 0))
    {
        Device(\_SB.NVDR)
        {
            // 4K Operation Region used for MMIO to signal to the host vdev that
            // a method is being called. We split up the MMIO region into offsets
            // to indicate to the host which method is being called on which device.
            OperationRegion(NVIO, SystemMemory, FixedPcdGet32(PcdPmemRegisterBase), 4096)
            Field(NVIO, DWordAcc, NoLock, WriteAsZeros)
            {
                RDSM,32, // Root  _DSM
                CDSM,32, // Child _DSM
                CLSI,32, // Child _LSI
                CLSR,32, // Child _LSR
                NEV0,32, // NEV# is a NVDIMM Event Register.
                NEV1,32, // NEV0 - NEV3 are NFIT Health Event Notifications
                NEV2,32, //                 (0x81 on NVDIMM Child Device)
                NEV3,32, //
                NEV4,32, // NVDIMM Root Device notifications.
            }

            // 4K Operation Region for Method I/O buffer between the ACPI NVDIMM
            // devices and the vPMEM vdev on the Host. The actual address (NVDA)
            // comes from the BIOS operation region that gets updated from by the
            // UEFI firmware during ACPI table initialization.
            OperationRegion(NVDB, SystemMemory, NVDA, 4096)
            Field(NVDB, AnyAcc, NoLock, WriteAsZeros)
            {
                MBUF,32736, // Raw buffer that can be returned to callers (4k bytes - 4).
                MBFL,32,    // Size of the data that has been returned.
            }

            Name (_HID, "ACPI0012")
            Name (_STA, 0xF)

            // This mutex protects the above NVDB OperationRegion.
            // It should be used as follows.
            // 1. Acquire NMTX
            // 2. Store method arguments in MBUF.
            // 3. Signal the vdev via the NVIO MMIO page.
            // 4. Read the return values from MBUF into scratch space (of length MBFL).
            // 5. Release NMTX.
            Mutex(NMTX, 0)


            // _DSM Device Specific Method
            // Arg0: UUID Unique function identifier
            // Arg1: Integer Revision Level
            // Arg2: Integer Function Index (0 = Return Supported Functions)
            // Arg3: Package Parameters
            Method (_DSM, 4, Serialized, 0, {IntObj,BuffObj}, )
            {
                Switch (ToBuffer(Arg0))
                {
                    Case (ToUUID("2f10e7a4-9e91-11e4-89d3-123b93f75cba"))
                    {
                        Switch (ToInteger(Arg1))
                        {
                            Case (1)
                            {
                                Switch (ToInteger(Arg2))
                                {
                                    // Function 0: Return supported functions.
                                    Case (0)
                                    {
                                        // Read from the MMIO page to get the supported functions.
                                        Return (RDSM)
                                    }
                                    // For all other functions, call into the vdev.
                                    Default
                                    {
                                        Acquire (NMTX, 0xFFFF)

                                        // Copy the arguments into the method I/O buffer.
                                        Store (DeRefOf(Index(Arg3,0)), MBUF)

                                        // Write the function index to the MMIO Page
                                        // to signal the vdev.
                                        Store (Arg2, RDSM)

                                        // Copy the contents of the method I/O Buffer.
                                        Name (RBUF, Buffer(MBFL) {})
                                        Store (MBUF, RBUF)

                                        Release (NMTX)
                                        Return (RBUF)
                                    }
                                }
                            }
                            Default
                            {
                                // Return a buffer with bit 0 set to 0 indicating no functions supported
                                // if we don't recognize the revision level.
                                Return (Buffer(){0})
                            }
                        }
                    }
                }

                // Return a buffer with bit 0 set to 0 indicating no functions supported
                // if we don't recognize the UUID.
                Return (Buffer(){0})
            }

            // CDSF - Generic Method for Child _DSMs.
            // Arg0: UUID Unique function identifier
            // Arg1: Integer Revision Level
            // Arg2: Integer Function Index (0 = Return Supported Functions)
            // Arg3: Package Parameters
            // Arg4: Integer Device Index
            Method (CDSF, 5, Serialized, 0, {IntObj,BuffObj})
            {
                Switch (ToBuffer(Arg0))
                {
                    Case (ToUUID("5746C5F2-A9A2-4264-AD0E-E4DDC9E09E80"))
                    {
                        Switch (ToInteger(Arg1))
                        {
                            Case (1)
                            {
                                Switch (ToInteger(Arg2))
                                {
                                    // Function 0: Return supported functions.
                                    Case (0)
                                    {
                                        // Read from the MMIO page to get the supported functions.
                                        Return (CDSM)
                                    }
                                    // For all other functions, call into the vdev.
                                    Default
                                    {
                                        // We need to pack the function index and device index into a DWORD.
                                        Name (INDX, Buffer(4) {})
                                        CreateField (INDX, 0, 16, FIND) // Space for Function Index
                                        CreateField(INDX, 16, 16, DIND) // Space for Device Index
                                        Store (Arg2, FIND)
                                        Store (Arg4, DIND)

                                        Acquire (NMTX, 0xFFFF)

                                        // Copy the arguments into the method I/O buffer.
                                        Store (DeRefOf(Index(Arg3,0)), MBUF)

                                        // Write the function and device indices
                                        // to the MMIO Page to signal the vdev.
                                        Store (INDX, CDSM)

                                        // Copy the contents of the method I/O Buffer.
                                        Name (RBUF, Buffer(MBFL) {})
                                        Store (MBUF, RBUF)

                                        Release (NMTX)
                                        Return (RBUF)
                                    }
                                }
                            }
                            Default
                            {
                                // Return a buffer with bit 0 set to 0 indicating no functions supported
                                // if we don't recognize the revision level.
                                Return (Buffer(){0})
                            }
                        }
                    }
                }

                // Return a buffer with bit 0 set to 0 indicating no functions supported
                // if we don't recognize the UUID.
                Return (Buffer(){0})
            }

            // LSIM - Generic Method for Child _LSIs.
            //
            // Arg0: Integer Device Index
            Method (LSIM, 1, Serialized, 0, {PkgObj})
            {
                Acquire (NMTX, 0xFFFF)

                // Write the device index
                // to the MMIO Page to signal the vdev.
                Store (Arg0, CLSI)

                // Copy the contents of the method I/O Buffer.
                Name (RBUF, Buffer(MBFL) {})
                Store (MBUF, RBUF)

                Release (NMTX)

                CreateDWordField (RBUF, 0, DWD0)
                CreateDWordField (RBUF, 4, DWD1)
                CreateDWordField (RBUF, 8, DWD2)
                Name (PKGI, Package(3) {0, 0, 0})
                Store (DWD0, Index(PKGI, 0))
                Store (DWD1, Index(PKGI, 1))
                Store (DWD2, Index(PKGI, 2))
                Return (PKGI)
            }

            // LSRM - Generic Method for Child _LSRs.
            // Arg0: Integer(DWORD) Byte Offset.
            // Arg1: Integer(DWORD) Tranfer Byte Length.
            // Arg2: Integer Device Index.
            Method (LSRM, 3, Serialized, 0, {PkgObj})
            {
                // Pack up the arguments.
                // Make INPT the same size as MBUF, so ASL does not do source
                // and destination type conversion, to guarantee that the resulting
                // store copies INPT byte for byte into MBUF.
                Name (INPT, Buffer(0xffc) {})
                CreateField (INPT, 0, 32, BTOF) // Space for Byte Offset
                CreateField (INPT, 32, 32, TFLT) // Space for Transfer Length
                Store (Arg0, BTOF)
                Store (Arg1, TFLT)

                Acquire (NMTX, 0xFFFF)

                // Copy the arguments.
                Store (INPT, MBUF)

                // Write the device index
                // to the MMIO Page to signal the vdev.
                Store (Arg2, CLSR)

                // Copy the contents of the method I/O Buffer.
                Name (RBUF, Buffer(MBFL) {})
                Multiply (MBFL, 8, Local0)
                Store (MBUF, RBUF)

                Release (NMTX)

                CreateDWordField (RBUF, 0, DWD0)
                Store (Subtract(Local0, 32), Local1) // size of the data buffer in bits
                CreateField (RBUF, 32, Local1, LBLD) // label data buffer
                Name (PKGR, Package(2) {0, Buffer(){0}})
                Store (DWD0, Index (PKGR, 0))
                Store (LBLD, Index (PKGR, 1))
                Return (PKGR)
            }
        }

        // NVDIMM Child Devices

        If(LLessEqual(1, NCNT))
        {
            Device(\_SB.NVDR.N000)
            {
                Name(_ADR, 0)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(2, NCNT))
        {
            Device(\_SB.NVDR.N001)
            {
                Name(_ADR, 1)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(3, NCNT))
        {
            Device(\_SB.NVDR.N002)
            {
                Name(_ADR, 2)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(4, NCNT))
        {
            Device(\_SB.NVDR.N003)
            {
                Name(_ADR, 3)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(5, NCNT))
        {
            Device(\_SB.NVDR.N004)
            {
                Name(_ADR, 4)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(6, NCNT))
        {
            Device(\_SB.NVDR.N005)
            {
                Name(_ADR, 5)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(7, NCNT))
        {
            Device(\_SB.NVDR.N006)
            {
                Name(_ADR, 6)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(8, NCNT))
        {
            Device(\_SB.NVDR.N007)
            {
                Name(_ADR, 7)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(9, NCNT))
        {
            Device(\_SB.NVDR.N008)
            {
                Name(_ADR, 8)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(10, NCNT))
        {
            Device(\_SB.NVDR.N009)
            {
                Name(_ADR, 9)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(11, NCNT))
        {
            Device(\_SB.NVDR.N010)
            {
                Name(_ADR, 10)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(12, NCNT))
        {
            Device(\_SB.NVDR.N011)
            {
                Name(_ADR, 11)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(13, NCNT))
        {
            Device(\_SB.NVDR.N012)
            {
                Name(_ADR, 12)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(14, NCNT))
        {
            Device(\_SB.NVDR.N013)
            {
                Name(_ADR, 13)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(15, NCNT))
        {
            Device(\_SB.NVDR.N014)
            {
                Name(_ADR, 14)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(16, NCNT))
        {
            Device(\_SB.NVDR.N015)
            {
                Name(_ADR, 15)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(17, NCNT))
        {
            Device(\_SB.NVDR.N016)
            {
                Name(_ADR, 16)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(18, NCNT))
        {
            Device(\_SB.NVDR.N017)
            {
                Name(_ADR, 17)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(19, NCNT))
        {
            Device(\_SB.NVDR.N018)
            {
                Name(_ADR, 18)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(20, NCNT))
        {
            Device(\_SB.NVDR.N019)
            {
                Name(_ADR, 19)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(21, NCNT))
        {
            Device(\_SB.NVDR.N020)
            {
                Name(_ADR, 20)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(22, NCNT))
        {
            Device(\_SB.NVDR.N021)
            {
                Name(_ADR, 21)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(23, NCNT))
        {
            Device(\_SB.NVDR.N022)
            {
                Name(_ADR, 22)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(24, NCNT))
        {
            Device(\_SB.NVDR.N023)
            {
                Name(_ADR, 23)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(25, NCNT))
        {
            Device(\_SB.NVDR.N024)
            {
                Name(_ADR, 24)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(26, NCNT))
        {
            Device(\_SB.NVDR.N025)
            {
                Name(_ADR, 25)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(27, NCNT))
        {
            Device(\_SB.NVDR.N026)
            {
                Name(_ADR, 26)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(28, NCNT))
        {
            Device(\_SB.NVDR.N027)
            {
                Name(_ADR, 27)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(29, NCNT))
        {
            Device(\_SB.NVDR.N028)
            {
                Name(_ADR, 28)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(30, NCNT))
        {
            Device(\_SB.NVDR.N029)
            {
                Name(_ADR, 29)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(31, NCNT))
        {
            Device(\_SB.NVDR.N030)
            {
                Name(_ADR, 30)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(32, NCNT))
        {
            Device(\_SB.NVDR.N031)
            {
                Name(_ADR, 31)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(33, NCNT))
        {
            Device(\_SB.NVDR.N032)
            {
                Name(_ADR, 32)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(34, NCNT))
        {
            Device(\_SB.NVDR.N033)
            {
                Name(_ADR, 33)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(35, NCNT))
        {
            Device(\_SB.NVDR.N034)
            {
                Name(_ADR, 34)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(36, NCNT))
        {
            Device(\_SB.NVDR.N035)
            {
                Name(_ADR, 35)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(37, NCNT))
        {
            Device(\_SB.NVDR.N036)
            {
                Name(_ADR, 36)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(38, NCNT))
        {
            Device(\_SB.NVDR.N037)
            {
                Name(_ADR, 37)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(39, NCNT))
        {
            Device(\_SB.NVDR.N038)
            {
                Name(_ADR, 38)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(40, NCNT))
        {
            Device(\_SB.NVDR.N039)
            {
                Name(_ADR, 39)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(41, NCNT))
        {
            Device(\_SB.NVDR.N040)
            {
                Name(_ADR, 40)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(42, NCNT))
        {
            Device(\_SB.NVDR.N041)
            {
                Name(_ADR, 41)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(43, NCNT))
        {
            Device(\_SB.NVDR.N042)
            {
                Name(_ADR, 42)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(44, NCNT))
        {
            Device(\_SB.NVDR.N043)
            {
                Name(_ADR, 43)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(45, NCNT))
        {
            Device(\_SB.NVDR.N044)
            {
                Name(_ADR, 44)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(46, NCNT))
        {
            Device(\_SB.NVDR.N045)
            {
                Name(_ADR, 45)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(47, NCNT))
        {
            Device(\_SB.NVDR.N046)
            {
                Name(_ADR, 46)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(48, NCNT))
        {
            Device(\_SB.NVDR.N047)
            {
                Name(_ADR, 47)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(49, NCNT))
        {
            Device(\_SB.NVDR.N048)
            {
                Name(_ADR, 48)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(50, NCNT))
        {
            Device(\_SB.NVDR.N049)
            {
                Name(_ADR, 49)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(51, NCNT))
        {
            Device(\_SB.NVDR.N050)
            {
                Name(_ADR, 50)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(52, NCNT))
        {
            Device(\_SB.NVDR.N051)
            {
                Name(_ADR, 51)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(53, NCNT))
        {
            Device(\_SB.NVDR.N052)
            {
                Name(_ADR, 52)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(54, NCNT))
        {
            Device(\_SB.NVDR.N053)
            {
                Name(_ADR, 53)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(55, NCNT))
        {
            Device(\_SB.NVDR.N054)
            {
                Name(_ADR, 54)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(56, NCNT))
        {
            Device(\_SB.NVDR.N055)
            {
                Name(_ADR, 55)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(57, NCNT))
        {
            Device(\_SB.NVDR.N056)
            {
                Name(_ADR, 56)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(58, NCNT))
        {
            Device(\_SB.NVDR.N057)
            {
                Name(_ADR, 57)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(59, NCNT))
        {
            Device(\_SB.NVDR.N058)
            {
                Name(_ADR, 58)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(60, NCNT))
        {
            Device(\_SB.NVDR.N059)
            {
                Name(_ADR, 59)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(61, NCNT))
        {
            Device(\_SB.NVDR.N060)
            {
                Name(_ADR, 60)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(62, NCNT))
        {
            Device(\_SB.NVDR.N061)
            {
                Name(_ADR, 61)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(63, NCNT))
        {
            Device(\_SB.NVDR.N062)
            {
                Name(_ADR, 62)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(64, NCNT))
        {
            Device(\_SB.NVDR.N063)
            {
                Name(_ADR, 63)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(65, NCNT))
        {
            Device(\_SB.NVDR.N064)
            {
                Name(_ADR, 64)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(66, NCNT))
        {
            Device(\_SB.NVDR.N065)
            {
                Name(_ADR, 65)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(67, NCNT))
        {
            Device(\_SB.NVDR.N066)
            {
                Name(_ADR, 66)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(68, NCNT))
        {
            Device(\_SB.NVDR.N067)
            {
                Name(_ADR, 67)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(69, NCNT))
        {
            Device(\_SB.NVDR.N068)
            {
                Name(_ADR, 68)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(70, NCNT))
        {
            Device(\_SB.NVDR.N069)
            {
                Name(_ADR, 69)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(71, NCNT))
        {
            Device(\_SB.NVDR.N070)
            {
                Name(_ADR, 70)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(72, NCNT))
        {
            Device(\_SB.NVDR.N071)
            {
                Name(_ADR, 71)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(73, NCNT))
        {
            Device(\_SB.NVDR.N072)
            {
                Name(_ADR, 72)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(74, NCNT))
        {
            Device(\_SB.NVDR.N073)
            {
                Name(_ADR, 73)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(75, NCNT))
        {
            Device(\_SB.NVDR.N074)
            {
                Name(_ADR, 74)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(76, NCNT))
        {
            Device(\_SB.NVDR.N075)
            {
                Name(_ADR, 75)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(77, NCNT))
        {
            Device(\_SB.NVDR.N076)
            {
                Name(_ADR, 76)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(78, NCNT))
        {
            Device(\_SB.NVDR.N077)
            {
                Name(_ADR, 77)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(79, NCNT))
        {
            Device(\_SB.NVDR.N078)
            {
                Name(_ADR, 78)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(80, NCNT))
        {
            Device(\_SB.NVDR.N079)
            {
                Name(_ADR, 79)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(81, NCNT))
        {
            Device(\_SB.NVDR.N080)
            {
                Name(_ADR, 80)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(82, NCNT))
        {
            Device(\_SB.NVDR.N081)
            {
                Name(_ADR, 81)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(83, NCNT))
        {
            Device(\_SB.NVDR.N082)
            {
                Name(_ADR, 82)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(84, NCNT))
        {
            Device(\_SB.NVDR.N083)
            {
                Name(_ADR, 83)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(85, NCNT))
        {
            Device(\_SB.NVDR.N084)
            {
                Name(_ADR, 84)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(86, NCNT))
        {
            Device(\_SB.NVDR.N085)
            {
                Name(_ADR, 85)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(87, NCNT))
        {
            Device(\_SB.NVDR.N086)
            {
                Name(_ADR, 86)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(88, NCNT))
        {
            Device(\_SB.NVDR.N087)
            {
                Name(_ADR, 87)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(89, NCNT))
        {
            Device(\_SB.NVDR.N088)
            {
                Name(_ADR, 88)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(90, NCNT))
        {
            Device(\_SB.NVDR.N089)
            {
                Name(_ADR, 89)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(91, NCNT))
        {
            Device(\_SB.NVDR.N090)
            {
                Name(_ADR, 90)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(92, NCNT))
        {
            Device(\_SB.NVDR.N091)
            {
                Name(_ADR, 91)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(93, NCNT))
        {
            Device(\_SB.NVDR.N092)
            {
                Name(_ADR, 92)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(94, NCNT))
        {
            Device(\_SB.NVDR.N093)
            {
                Name(_ADR, 93)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(95, NCNT))
        {
            Device(\_SB.NVDR.N094)
            {
                Name(_ADR, 94)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(96, NCNT))
        {
            Device(\_SB.NVDR.N095)
            {
                Name(_ADR, 95)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(97, NCNT))
        {
            Device(\_SB.NVDR.N096)
            {
                Name(_ADR, 96)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(98, NCNT))
        {
            Device(\_SB.NVDR.N097)
            {
                Name(_ADR, 97)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(99, NCNT))
        {
            Device(\_SB.NVDR.N098)
            {
                Name(_ADR, 98)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(100, NCNT))
        {
            Device(\_SB.NVDR.N099)
            {
                Name(_ADR, 99)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(101, NCNT))
        {
            Device(\_SB.NVDR.N100)
            {
                Name(_ADR, 100)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(102, NCNT))
        {
            Device(\_SB.NVDR.N101)
            {
                Name(_ADR, 101)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(103, NCNT))
        {
            Device(\_SB.NVDR.N102)
            {
                Name(_ADR, 102)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(104, NCNT))
        {
            Device(\_SB.NVDR.N103)
            {
                Name(_ADR, 103)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(105, NCNT))
        {
            Device(\_SB.NVDR.N104)
            {
                Name(_ADR, 104)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(106, NCNT))
        {
            Device(\_SB.NVDR.N105)
            {
                Name(_ADR, 105)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(107, NCNT))
        {
            Device(\_SB.NVDR.N106)
            {
                Name(_ADR, 106)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(108, NCNT))
        {
            Device(\_SB.NVDR.N107)
            {
                Name(_ADR, 107)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(109, NCNT))
        {
            Device(\_SB.NVDR.N108)
            {
                Name(_ADR, 108)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(110, NCNT))
        {
            Device(\_SB.NVDR.N109)
            {
                Name(_ADR, 109)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(111, NCNT))
        {
            Device(\_SB.NVDR.N110)
            {
                Name(_ADR, 110)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(112, NCNT))
        {
            Device(\_SB.NVDR.N111)
            {
                Name(_ADR, 111)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(113, NCNT))
        {
            Device(\_SB.NVDR.N112)
            {
                Name(_ADR, 112)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(114, NCNT))
        {
            Device(\_SB.NVDR.N113)
            {
                Name(_ADR, 113)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(115, NCNT))
        {
            Device(\_SB.NVDR.N114)
            {
                Name(_ADR, 114)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(116, NCNT))
        {
            Device(\_SB.NVDR.N115)
            {
                Name(_ADR, 115)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(117, NCNT))
        {
            Device(\_SB.NVDR.N116)
            {
                Name(_ADR, 116)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(118, NCNT))
        {
            Device(\_SB.NVDR.N117)
            {
                Name(_ADR, 117)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(119, NCNT))
        {
            Device(\_SB.NVDR.N118)
            {
                Name(_ADR, 118)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(120, NCNT))
        {
            Device(\_SB.NVDR.N119)
            {
                Name(_ADR, 119)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(121, NCNT))
        {
            Device(\_SB.NVDR.N120)
            {
                Name(_ADR, 120)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(122, NCNT))
        {
            Device(\_SB.NVDR.N121)
            {
                Name(_ADR, 121)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(123, NCNT))
        {
            Device(\_SB.NVDR.N122)
            {
                Name(_ADR, 122)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(124, NCNT))
        {
            Device(\_SB.NVDR.N123)
            {
                Name(_ADR, 123)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(125, NCNT))
        {
            Device(\_SB.NVDR.N124)
            {
                Name(_ADR, 124)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(126, NCNT))
        {
            Device(\_SB.NVDR.N125)
            {
                Name(_ADR, 125)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(127, NCNT))
        {
            Device(\_SB.NVDR.N126)
            {
                Name(_ADR, 126)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }
        If(LLessEqual(128, NCNT))
        {
            Device(\_SB.NVDR.N127)
            {
                Name(_ADR, 127)
                Method(_DSM, 4, NotSerialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
                    { Return (CDSF(Arg0, Arg1, Arg2, Arg3, _ADR)) }
                Function (_LSI, {PkgObj}) { Return (LSIM(_ADR)) }
                Function (_LSR, {PkgObj}, {IntObj, IntObj}) { Return (LSRM(Arg0, Arg1, _ADR)) }
            }
        }


#if defined (_DSDT_INTEL_)

        // GPE method for PMEM
        Scope(\_GPE)
        {
            // Method for notifying external changes to the PMEM device:
            //      E  - This event is edge triggered
            //      0A - Use bit A in the General Purpose Event register described
            //           in the FADT
            Method(_E0A)
            {

#elif defined (_DSDT_ARM_)

        // Interrupt signalled event device for PMEM
        Device(\_SB.GED3)
        {
            Name(_HID,"ACPI0013")
            Name(_UID, 3)
            Name(_CRS, ResourceTemplate()
            {
                Interrupt(ResourceConsumer, Edge, ActiveHigh, Exclusive)
                    {FixedPcdGet8(PcdPmemEventVector)}
            })
            Method(_EVT, 1)
            {

#endif
                // Read the Event registers.
                Store(\_SB.NVDR.NEV0, Local0)
                Store(\_SB.NVDR.NEV1, Local1)
                Store(\_SB.NVDR.NEV2, Local2)
                Store(\_SB.NVDR.NEV3, Local3)
                Store(\_SB.NVDR.NEV4, Local4)

                // Go through each event register to see what events were signalled.
                // For NEV0-3, each bit corresponds to a 0x81 event on a different NVDIMM
                // child device.
                // For NEV4, bit 0 corresponds to 0x80 (NFIT Update Notification), and
                // bit 1 corresponds to 0x81 (Unconsumed Uncorrectable Memory Error Detected),
                // both on the NVDIMM root device.
                if (LNotEqual(Local0, 0))
                {
                    if (LNotEqual( And(Local0, 0x00000001), 0)) { Notify (\_SB.NVDR.N000, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000002), 0)) { Notify (\_SB.NVDR.N001, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000004), 0)) { Notify (\_SB.NVDR.N002, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000008), 0)) { Notify (\_SB.NVDR.N003, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000010), 0)) { Notify (\_SB.NVDR.N004, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000020), 0)) { Notify (\_SB.NVDR.N005, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000040), 0)) { Notify (\_SB.NVDR.N006, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000080), 0)) { Notify (\_SB.NVDR.N007, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000100), 0)) { Notify (\_SB.NVDR.N008, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000200), 0)) { Notify (\_SB.NVDR.N009, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000400), 0)) { Notify (\_SB.NVDR.N010, 0x81) }
                    if (LNotEqual( And(Local0, 0x00000800), 0)) { Notify (\_SB.NVDR.N011, 0x81) }
                    if (LNotEqual( And(Local0, 0x00001000), 0)) { Notify (\_SB.NVDR.N012, 0x81) }
                    if (LNotEqual( And(Local0, 0x00002000), 0)) { Notify (\_SB.NVDR.N013, 0x81) }
                    if (LNotEqual( And(Local0, 0x00004000), 0)) { Notify (\_SB.NVDR.N014, 0x81) }
                    if (LNotEqual( And(Local0, 0x00008000), 0)) { Notify (\_SB.NVDR.N015, 0x81) }
                    if (LNotEqual( And(Local0, 0x00010000), 0)) { Notify (\_SB.NVDR.N016, 0x81) }
                    if (LNotEqual( And(Local0, 0x00020000), 0)) { Notify (\_SB.NVDR.N017, 0x81) }
                    if (LNotEqual( And(Local0, 0x00040000), 0)) { Notify (\_SB.NVDR.N018, 0x81) }
                    if (LNotEqual( And(Local0, 0x00080000), 0)) { Notify (\_SB.NVDR.N019, 0x81) }
                    if (LNotEqual( And(Local0, 0x00100000), 0)) { Notify (\_SB.NVDR.N020, 0x81) }
                    if (LNotEqual( And(Local0, 0x00200000), 0)) { Notify (\_SB.NVDR.N021, 0x81) }
                    if (LNotEqual( And(Local0, 0x00400000), 0)) { Notify (\_SB.NVDR.N022, 0x81) }
                    if (LNotEqual( And(Local0, 0x00800000), 0)) { Notify (\_SB.NVDR.N023, 0x81) }
                    if (LNotEqual( And(Local0, 0x01000000), 0)) { Notify (\_SB.NVDR.N024, 0x81) }
                    if (LNotEqual( And(Local0, 0x02000000), 0)) { Notify (\_SB.NVDR.N025, 0x81) }
                    if (LNotEqual( And(Local0, 0x04000000), 0)) { Notify (\_SB.NVDR.N026, 0x81) }
                    if (LNotEqual( And(Local0, 0x08000000), 0)) { Notify (\_SB.NVDR.N027, 0x81) }
                    if (LNotEqual( And(Local0, 0x10000000), 0)) { Notify (\_SB.NVDR.N028, 0x81) }
                    if (LNotEqual( And(Local0, 0x20000000), 0)) { Notify (\_SB.NVDR.N029, 0x81) }
                    if (LNotEqual( And(Local0, 0x40000000), 0)) { Notify (\_SB.NVDR.N030, 0x81) }
                    if (LNotEqual( And(Local0, 0x80000000), 0)) { Notify (\_SB.NVDR.N031, 0x81) }
                    // Clear the event register.
                    Store (Local0, \_SB.NVDR.NEV0)
                }
                if (LNotEqual(Local1, 0))
                {
                    if (LNotEqual( And(Local1, 0x00000001), 0)) { Notify (\_SB.NVDR.N032, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000002), 0)) { Notify (\_SB.NVDR.N033, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000004), 0)) { Notify (\_SB.NVDR.N034, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000008), 0)) { Notify (\_SB.NVDR.N035, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000010), 0)) { Notify (\_SB.NVDR.N036, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000020), 0)) { Notify (\_SB.NVDR.N037, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000040), 0)) { Notify (\_SB.NVDR.N038, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000080), 0)) { Notify (\_SB.NVDR.N039, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000100), 0)) { Notify (\_SB.NVDR.N040, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000200), 0)) { Notify (\_SB.NVDR.N041, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000400), 0)) { Notify (\_SB.NVDR.N042, 0x81) }
                    if (LNotEqual( And(Local1, 0x00000800), 0)) { Notify (\_SB.NVDR.N043, 0x81) }
                    if (LNotEqual( And(Local1, 0x00001000), 0)) { Notify (\_SB.NVDR.N044, 0x81) }
                    if (LNotEqual( And(Local1, 0x00002000), 0)) { Notify (\_SB.NVDR.N045, 0x81) }
                    if (LNotEqual( And(Local1, 0x00004000), 0)) { Notify (\_SB.NVDR.N046, 0x81) }
                    if (LNotEqual( And(Local1, 0x00008000), 0)) { Notify (\_SB.NVDR.N047, 0x81) }
                    if (LNotEqual( And(Local1, 0x00010000), 0)) { Notify (\_SB.NVDR.N048, 0x81) }
                    if (LNotEqual( And(Local1, 0x00020000), 0)) { Notify (\_SB.NVDR.N049, 0x81) }
                    if (LNotEqual( And(Local1, 0x00040000), 0)) { Notify (\_SB.NVDR.N050, 0x81) }
                    if (LNotEqual( And(Local1, 0x00080000), 0)) { Notify (\_SB.NVDR.N051, 0x81) }
                    if (LNotEqual( And(Local1, 0x00100000), 0)) { Notify (\_SB.NVDR.N052, 0x81) }
                    if (LNotEqual( And(Local1, 0x00200000), 0)) { Notify (\_SB.NVDR.N053, 0x81) }
                    if (LNotEqual( And(Local1, 0x00400000), 0)) { Notify (\_SB.NVDR.N054, 0x81) }
                    if (LNotEqual( And(Local1, 0x00800000), 0)) { Notify (\_SB.NVDR.N055, 0x81) }
                    if (LNotEqual( And(Local1, 0x01000000), 0)) { Notify (\_SB.NVDR.N056, 0x81) }
                    if (LNotEqual( And(Local1, 0x02000000), 0)) { Notify (\_SB.NVDR.N057, 0x81) }
                    if (LNotEqual( And(Local1, 0x04000000), 0)) { Notify (\_SB.NVDR.N058, 0x81) }
                    if (LNotEqual( And(Local1, 0x08000000), 0)) { Notify (\_SB.NVDR.N059, 0x81) }
                    if (LNotEqual( And(Local1, 0x10000000), 0)) { Notify (\_SB.NVDR.N060, 0x81) }
                    if (LNotEqual( And(Local1, 0x20000000), 0)) { Notify (\_SB.NVDR.N061, 0x81) }
                    if (LNotEqual( And(Local1, 0x40000000), 0)) { Notify (\_SB.NVDR.N062, 0x81) }
                    if (LNotEqual( And(Local1, 0x80000000), 0)) { Notify (\_SB.NVDR.N063, 0x81) }
                    // Clear the event register.
                    Store (Local1, \_SB.NVDR.NEV1)
                }
                if (LNotEqual(Local2, 0))
                {
                    if (LNotEqual( And(Local2, 0x00000001), 0)) { Notify (\_SB.NVDR.N064, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000002), 0)) { Notify (\_SB.NVDR.N065, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000004), 0)) { Notify (\_SB.NVDR.N066, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000008), 0)) { Notify (\_SB.NVDR.N067, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000010), 0)) { Notify (\_SB.NVDR.N068, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000020), 0)) { Notify (\_SB.NVDR.N069, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000040), 0)) { Notify (\_SB.NVDR.N070, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000080), 0)) { Notify (\_SB.NVDR.N071, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000100), 0)) { Notify (\_SB.NVDR.N072, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000200), 0)) { Notify (\_SB.NVDR.N073, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000400), 0)) { Notify (\_SB.NVDR.N074, 0x81) }
                    if (LNotEqual( And(Local2, 0x00000800), 0)) { Notify (\_SB.NVDR.N075, 0x81) }
                    if (LNotEqual( And(Local2, 0x00001000), 0)) { Notify (\_SB.NVDR.N076, 0x81) }
                    if (LNotEqual( And(Local2, 0x00002000), 0)) { Notify (\_SB.NVDR.N077, 0x81) }
                    if (LNotEqual( And(Local2, 0x00004000), 0)) { Notify (\_SB.NVDR.N078, 0x81) }
                    if (LNotEqual( And(Local2, 0x00008000), 0)) { Notify (\_SB.NVDR.N079, 0x81) }
                    if (LNotEqual( And(Local2, 0x00010000), 0)) { Notify (\_SB.NVDR.N080, 0x81) }
                    if (LNotEqual( And(Local2, 0x00020000), 0)) { Notify (\_SB.NVDR.N081, 0x81) }
                    if (LNotEqual( And(Local2, 0x00040000), 0)) { Notify (\_SB.NVDR.N082, 0x81) }
                    if (LNotEqual( And(Local2, 0x00080000), 0)) { Notify (\_SB.NVDR.N083, 0x81) }
                    if (LNotEqual( And(Local2, 0x00100000), 0)) { Notify (\_SB.NVDR.N084, 0x81) }
                    if (LNotEqual( And(Local2, 0x00200000), 0)) { Notify (\_SB.NVDR.N085, 0x81) }
                    if (LNotEqual( And(Local2, 0x00400000), 0)) { Notify (\_SB.NVDR.N086, 0x81) }
                    if (LNotEqual( And(Local2, 0x00800000), 0)) { Notify (\_SB.NVDR.N087, 0x81) }
                    if (LNotEqual( And(Local2, 0x01000000), 0)) { Notify (\_SB.NVDR.N088, 0x81) }
                    if (LNotEqual( And(Local2, 0x02000000), 0)) { Notify (\_SB.NVDR.N089, 0x81) }
                    if (LNotEqual( And(Local2, 0x04000000), 0)) { Notify (\_SB.NVDR.N090, 0x81) }
                    if (LNotEqual( And(Local2, 0x08000000), 0)) { Notify (\_SB.NVDR.N091, 0x81) }
                    if (LNotEqual( And(Local2, 0x10000000), 0)) { Notify (\_SB.NVDR.N092, 0x81) }
                    if (LNotEqual( And(Local2, 0x20000000), 0)) { Notify (\_SB.NVDR.N093, 0x81) }
                    if (LNotEqual( And(Local2, 0x40000000), 0)) { Notify (\_SB.NVDR.N094, 0x81) }
                    if (LNotEqual( And(Local2, 0x80000000), 0)) { Notify (\_SB.NVDR.N095, 0x81) }
                    // Clear the event register.
                    Store (Local2, \_SB.NVDR.NEV2)
                }
                if (LNotEqual(Local3, 0))
                {
                    if (LNotEqual( And(Local3, 0x00000001), 0)) { Notify (\_SB.NVDR.N096, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000002), 0)) { Notify (\_SB.NVDR.N097, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000004), 0)) { Notify (\_SB.NVDR.N098, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000008), 0)) { Notify (\_SB.NVDR.N099, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000010), 0)) { Notify (\_SB.NVDR.N100, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000020), 0)) { Notify (\_SB.NVDR.N101, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000040), 0)) { Notify (\_SB.NVDR.N102, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000080), 0)) { Notify (\_SB.NVDR.N103, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000100), 0)) { Notify (\_SB.NVDR.N104, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000200), 0)) { Notify (\_SB.NVDR.N105, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000400), 0)) { Notify (\_SB.NVDR.N106, 0x81) }
                    if (LNotEqual( And(Local3, 0x00000800), 0)) { Notify (\_SB.NVDR.N107, 0x81) }
                    if (LNotEqual( And(Local3, 0x00001000), 0)) { Notify (\_SB.NVDR.N108, 0x81) }
                    if (LNotEqual( And(Local3, 0x00002000), 0)) { Notify (\_SB.NVDR.N109, 0x81) }
                    if (LNotEqual( And(Local3, 0x00004000), 0)) { Notify (\_SB.NVDR.N110, 0x81) }
                    if (LNotEqual( And(Local3, 0x00008000), 0)) { Notify (\_SB.NVDR.N111, 0x81) }
                    if (LNotEqual( And(Local3, 0x00010000), 0)) { Notify (\_SB.NVDR.N112, 0x81) }
                    if (LNotEqual( And(Local3, 0x00020000), 0)) { Notify (\_SB.NVDR.N113, 0x81) }
                    if (LNotEqual( And(Local3, 0x00040000), 0)) { Notify (\_SB.NVDR.N114, 0x81) }
                    if (LNotEqual( And(Local3, 0x00080000), 0)) { Notify (\_SB.NVDR.N115, 0x81) }
                    if (LNotEqual( And(Local3, 0x00100000), 0)) { Notify (\_SB.NVDR.N116, 0x81) }
                    if (LNotEqual( And(Local3, 0x00200000), 0)) { Notify (\_SB.NVDR.N117, 0x81) }
                    if (LNotEqual( And(Local3, 0x00400000), 0)) { Notify (\_SB.NVDR.N118, 0x81) }
                    if (LNotEqual( And(Local3, 0x00800000), 0)) { Notify (\_SB.NVDR.N119, 0x81) }
                    if (LNotEqual( And(Local3, 0x01000000), 0)) { Notify (\_SB.NVDR.N120, 0x81) }
                    if (LNotEqual( And(Local3, 0x02000000), 0)) { Notify (\_SB.NVDR.N121, 0x81) }
                    if (LNotEqual( And(Local3, 0x04000000), 0)) { Notify (\_SB.NVDR.N122, 0x81) }
                    if (LNotEqual( And(Local3, 0x08000000), 0)) { Notify (\_SB.NVDR.N123, 0x81) }
                    if (LNotEqual( And(Local3, 0x10000000), 0)) { Notify (\_SB.NVDR.N124, 0x81) }
                    if (LNotEqual( And(Local3, 0x20000000), 0)) { Notify (\_SB.NVDR.N125, 0x81) }
                    if (LNotEqual( And(Local3, 0x40000000), 0)) { Notify (\_SB.NVDR.N126, 0x81) }
                    if (LNotEqual( And(Local3, 0x80000000), 0)) { Notify (\_SB.NVDR.N127, 0x81) }
                    // Clear the event register.
                    Store (Local3, \_SB.NVDR.NEV3)
                }
                if (LNotEqual(Local4, 0))
                {
                    if (LNotEqual(And(Local4, 0x1), 0)) { Notify (\_SB.NVDR, 0x80) }
                    if (LNotEqual(And(Local4, 0x2), 0)) { Notify (\_SB.NVDR, 0x81) }
                    // Clear the event register.
                    Store (Local4, \_SB.NVDR.NEV4)
                }
            }
        }
    }

    // CXL Root.

    If(LGreater(CCFG, 0))
    {
        Device(\_SB.CXL)
        {
            Name (_HID, "ACPI0017")
            Name (_STA, 0xF)

            // _DSM Device Specific Method. See ACPI spec 3.1, section 9.1.1 for details.
            // Arg0: UUID Unique function identifier
            // Arg1: Integer Revision Level
            // Arg2: Integer Function Index (0 = Return Supported Functions)
            // Arg3: Package Parameters
            Method (_DSM, 4, Serialized, 0, UnknownObj, {BuffObj, IntObj, IntObj, PkgObj})
            {
                Switch (ToBuffer(Arg0))
                {
                    Case (ToUUID("f365f9a6-a7de-4071-a66a-b40c0b4f8e52"))
                    {
                        Switch (ToInteger(Arg1))
                        {
                            Case (1)
                            {
                                Switch (ToInteger(Arg2))
                                {

                                    Case (0)
                                    {
                                        // Function 0 for _DSM methods always returns a bitfield
                                        // indicating which function indexes are supported, starting
                                        // with zero. We support indexes 0 and 1.
                                        return (Buffer() {0x3})
                                    }
                                    Case (1)
                                    {
                                        // Function index 1 is used to retrieve information on the
                                        // supported QTG IDs that the platform supports, as well
                                        // as which QTG ID(s) are recommended for a given set of
                                        // memory device performance characteristics.
                                        //
                                        // Arg3: The memory device performance characteristics, in
                                        //     the format Package() {ReadLatency, WriteLatency, ReadBandwidth, WriteBandwidth}
                                        //     All performance metrics are expressed as IntObj
                                        //
                                        // Return: Package() {MaxQtgId, Package(){RecommendedQtgId1, RecommendedQtgId2, ...}}
                                        //
                                        // Refer to CXL spec 3.1, section 9.18.3.1 for more details.
                                        //

                                        // Currently always return a dummy result until a vDEV is
                                        // implemented that can get better results from the host.
                                        // Always set max ID to 1, and recommend the caller use that.

                                        return (Package() {1, Package() {1}})
                                    }
                                    Default
                                    {
                                        // An invalid function index was supplied, indicating a bug
                                        // in the caller.
                                        BreakPoint
                                    }
                                }
                            }
                            Default
                            {
                                // We don't support this revision level, return a buffer with bit
                                // 0 set to 0 indicating no functions supported.
                                Return (Buffer(){0})
                            }
                        }
                    }
                    Default
                    {
                        // This is not a UUID we support, return a buffer with bit 0 set to 0
                        // indicating no functions supported.
                        return(Buffer(){0})
                    }
                }
            }
        }
    }

    // Processor devices ======================================================

    If(LLessEqual(1, PCNT))
    {
        Device(P001) { Name(_HID, "ACPI0007") Name(_UID, 1) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2, PCNT))
    {
        Device(P002) { Name(_HID, "ACPI0007") Name(_UID, 2) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(3, PCNT))
    {
        Device(P003) { Name(_HID, "ACPI0007") Name(_UID, 3) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(4, PCNT))
    {
        Device(P004) { Name(_HID, "ACPI0007") Name(_UID, 4) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(5, PCNT))
    {
        Device(P005) { Name(_HID, "ACPI0007") Name(_UID, 5) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(6, PCNT))
    {
        Device(P006) { Name(_HID, "ACPI0007") Name(_UID, 6) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(7, PCNT))
    {
        Device(P007) { Name(_HID, "ACPI0007") Name(_UID, 7) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(8, PCNT))
    {
        Device(P008) { Name(_HID, "ACPI0007") Name(_UID, 8) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(9, PCNT))
    {
        Device(P009) { Name(_HID, "ACPI0007") Name(_UID, 9) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(10, PCNT))
    {
        Device(P010) { Name(_HID, "ACPI0007") Name(_UID, 10) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(11, PCNT))
    {
        Device(P011) { Name(_HID, "ACPI0007") Name(_UID, 11) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(12, PCNT))
    {
        Device(P012) { Name(_HID, "ACPI0007") Name(_UID, 12) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(13, PCNT))
    {
        Device(P013) { Name(_HID, "ACPI0007") Name(_UID, 13) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(14, PCNT))
    {
        Device(P014) { Name(_HID, "ACPI0007") Name(_UID, 14) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(15, PCNT))
    {
        Device(P015) { Name(_HID, "ACPI0007") Name(_UID, 15) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(16, PCNT))
    {
        Device(P016) { Name(_HID, "ACPI0007") Name(_UID, 16) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(17, PCNT))
    {
        Device(P017) { Name(_HID, "ACPI0007") Name(_UID, 17) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(18, PCNT))
    {
        Device(P018) { Name(_HID, "ACPI0007") Name(_UID, 18) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(19, PCNT))
    {
        Device(P019) { Name(_HID, "ACPI0007") Name(_UID, 19) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(20, PCNT))
    {
        Device(P020) { Name(_HID, "ACPI0007") Name(_UID, 20) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(21, PCNT))
    {
        Device(P021) { Name(_HID, "ACPI0007") Name(_UID, 21) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(22, PCNT))
    {
        Device(P022) { Name(_HID, "ACPI0007") Name(_UID, 22) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(23, PCNT))
    {
        Device(P023) { Name(_HID, "ACPI0007") Name(_UID, 23) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(24, PCNT))
    {
        Device(P024) { Name(_HID, "ACPI0007") Name(_UID, 24) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(25, PCNT))
    {
        Device(P025) { Name(_HID, "ACPI0007") Name(_UID, 25) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(26, PCNT))
    {
        Device(P026) { Name(_HID, "ACPI0007") Name(_UID, 26) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(27, PCNT))
    {
        Device(P027) { Name(_HID, "ACPI0007") Name(_UID, 27) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(28, PCNT))
    {
        Device(P028) { Name(_HID, "ACPI0007") Name(_UID, 28) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(29, PCNT))
    {
        Device(P029) { Name(_HID, "ACPI0007") Name(_UID, 29) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(30, PCNT))
    {
        Device(P030) { Name(_HID, "ACPI0007") Name(_UID, 30) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(31, PCNT))
    {
        Device(P031) { Name(_HID, "ACPI0007") Name(_UID, 31) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(32, PCNT))
    {
        Device(P032) { Name(_HID, "ACPI0007") Name(_UID, 32) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(33, PCNT))
    {
        Device(P033) { Name(_HID, "ACPI0007") Name(_UID, 33) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(34, PCNT))
    {
        Device(P034) { Name(_HID, "ACPI0007") Name(_UID, 34) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(35, PCNT))
    {
        Device(P035) { Name(_HID, "ACPI0007") Name(_UID, 35) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(36, PCNT))
    {
        Device(P036) { Name(_HID, "ACPI0007") Name(_UID, 36) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(37, PCNT))
    {
        Device(P037) { Name(_HID, "ACPI0007") Name(_UID, 37) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(38, PCNT))
    {
        Device(P038) { Name(_HID, "ACPI0007") Name(_UID, 38) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(39, PCNT))
    {
        Device(P039) { Name(_HID, "ACPI0007") Name(_UID, 39) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(40, PCNT))
    {
        Device(P040) { Name(_HID, "ACPI0007") Name(_UID, 40) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(41, PCNT))
    {
        Device(P041) { Name(_HID, "ACPI0007") Name(_UID, 41) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(42, PCNT))
    {
        Device(P042) { Name(_HID, "ACPI0007") Name(_UID, 42) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(43, PCNT))
    {
        Device(P043) { Name(_HID, "ACPI0007") Name(_UID, 43) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(44, PCNT))
    {
        Device(P044) { Name(_HID, "ACPI0007") Name(_UID, 44) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(45, PCNT))
    {
        Device(P045) { Name(_HID, "ACPI0007") Name(_UID, 45) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(46, PCNT))
    {
        Device(P046) { Name(_HID, "ACPI0007") Name(_UID, 46) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(47, PCNT))
    {
        Device(P047) { Name(_HID, "ACPI0007") Name(_UID, 47) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(48, PCNT))
    {
        Device(P048) { Name(_HID, "ACPI0007") Name(_UID, 48) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(49, PCNT))
    {
        Device(P049) { Name(_HID, "ACPI0007") Name(_UID, 49) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(50, PCNT))
    {
        Device(P050) { Name(_HID, "ACPI0007") Name(_UID, 50) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(51, PCNT))
    {
        Device(P051) { Name(_HID, "ACPI0007") Name(_UID, 51) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(52, PCNT))
    {
        Device(P052) { Name(_HID, "ACPI0007") Name(_UID, 52) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(53, PCNT))
    {
        Device(P053) { Name(_HID, "ACPI0007") Name(_UID, 53) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(54, PCNT))
    {
        Device(P054) { Name(_HID, "ACPI0007") Name(_UID, 54) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(55, PCNT))
    {
        Device(P055) { Name(_HID, "ACPI0007") Name(_UID, 55) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(56, PCNT))
    {
        Device(P056) { Name(_HID, "ACPI0007") Name(_UID, 56) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(57, PCNT))
    {
        Device(P057) { Name(_HID, "ACPI0007") Name(_UID, 57) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(58, PCNT))
    {
        Device(P058) { Name(_HID, "ACPI0007") Name(_UID, 58) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(59, PCNT))
    {
        Device(P059) { Name(_HID, "ACPI0007") Name(_UID, 59) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(60, PCNT))
    {
        Device(P060) { Name(_HID, "ACPI0007") Name(_UID, 60) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(61, PCNT))
    {
        Device(P061) { Name(_HID, "ACPI0007") Name(_UID, 61) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(62, PCNT))
    {
        Device(P062) { Name(_HID, "ACPI0007") Name(_UID, 62) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(63, PCNT))
    {
        Device(P063) { Name(_HID, "ACPI0007") Name(_UID, 63) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(64, PCNT))
    {
        Device(P064) { Name(_HID, "ACPI0007") Name(_UID, 64) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(65, PCNT))
    {
        Device(P065) { Name(_HID, "ACPI0007") Name(_UID, 65) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(66, PCNT))
    {
        Device(P066) { Name(_HID, "ACPI0007") Name(_UID, 66) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(67, PCNT))
    {
        Device(P067) { Name(_HID, "ACPI0007") Name(_UID, 67) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(68, PCNT))
    {
        Device(P068) { Name(_HID, "ACPI0007") Name(_UID, 68) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(69, PCNT))
    {
        Device(P069) { Name(_HID, "ACPI0007") Name(_UID, 69) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(70, PCNT))
    {
        Device(P070) { Name(_HID, "ACPI0007") Name(_UID, 70) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(71, PCNT))
    {
        Device(P071) { Name(_HID, "ACPI0007") Name(_UID, 71) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(72, PCNT))
    {
        Device(P072) { Name(_HID, "ACPI0007") Name(_UID, 72) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(73, PCNT))
    {
        Device(P073) { Name(_HID, "ACPI0007") Name(_UID, 73) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(74, PCNT))
    {
        Device(P074) { Name(_HID, "ACPI0007") Name(_UID, 74) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(75, PCNT))
    {
        Device(P075) { Name(_HID, "ACPI0007") Name(_UID, 75) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(76, PCNT))
    {
        Device(P076) { Name(_HID, "ACPI0007") Name(_UID, 76) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(77, PCNT))
    {
        Device(P077) { Name(_HID, "ACPI0007") Name(_UID, 77) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(78, PCNT))
    {
        Device(P078) { Name(_HID, "ACPI0007") Name(_UID, 78) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(79, PCNT))
    {
        Device(P079) { Name(_HID, "ACPI0007") Name(_UID, 79) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(80, PCNT))
    {
        Device(P080) { Name(_HID, "ACPI0007") Name(_UID, 80) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(81, PCNT))
    {
        Device(P081) { Name(_HID, "ACPI0007") Name(_UID, 81) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(82, PCNT))
    {
        Device(P082) { Name(_HID, "ACPI0007") Name(_UID, 82) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(83, PCNT))
    {
        Device(P083) { Name(_HID, "ACPI0007") Name(_UID, 83) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(84, PCNT))
    {
        Device(P084) { Name(_HID, "ACPI0007") Name(_UID, 84) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(85, PCNT))
    {
        Device(P085) { Name(_HID, "ACPI0007") Name(_UID, 85) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(86, PCNT))
    {
        Device(P086) { Name(_HID, "ACPI0007") Name(_UID, 86) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(87, PCNT))
    {
        Device(P087) { Name(_HID, "ACPI0007") Name(_UID, 87) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(88, PCNT))
    {
        Device(P088) { Name(_HID, "ACPI0007") Name(_UID, 88) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(89, PCNT))
    {
        Device(P089) { Name(_HID, "ACPI0007") Name(_UID, 89) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(90, PCNT))
    {
        Device(P090) { Name(_HID, "ACPI0007") Name(_UID, 90) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(91, PCNT))
    {
        Device(P091) { Name(_HID, "ACPI0007") Name(_UID, 91) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(92, PCNT))
    {
        Device(P092) { Name(_HID, "ACPI0007") Name(_UID, 92) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(93, PCNT))
    {
        Device(P093) { Name(_HID, "ACPI0007") Name(_UID, 93) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(94, PCNT))
    {
        Device(P094) { Name(_HID, "ACPI0007") Name(_UID, 94) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(95, PCNT))
    {
        Device(P095) { Name(_HID, "ACPI0007") Name(_UID, 95) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(96, PCNT))
    {
        Device(P096) { Name(_HID, "ACPI0007") Name(_UID, 96) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(97, PCNT))
    {
        Device(P097) { Name(_HID, "ACPI0007") Name(_UID, 97) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(98, PCNT))
    {
        Device(P098) { Name(_HID, "ACPI0007") Name(_UID, 98) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(99, PCNT))
    {
        Device(P099) { Name(_HID, "ACPI0007") Name(_UID, 99) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(100, PCNT))
    {
        Device(P100) { Name(_HID, "ACPI0007") Name(_UID, 100) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(101, PCNT))
    {
        Device(P101) { Name(_HID, "ACPI0007") Name(_UID, 101) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(102, PCNT))
    {
        Device(P102) { Name(_HID, "ACPI0007") Name(_UID, 102) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(103, PCNT))
    {
        Device(P103) { Name(_HID, "ACPI0007") Name(_UID, 103) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(104, PCNT))
    {
        Device(P104) { Name(_HID, "ACPI0007") Name(_UID, 104) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(105, PCNT))
    {
        Device(P105) { Name(_HID, "ACPI0007") Name(_UID, 105) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(106, PCNT))
    {
        Device(P106) { Name(_HID, "ACPI0007") Name(_UID, 106) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(107, PCNT))
    {
        Device(P107) { Name(_HID, "ACPI0007") Name(_UID, 107) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(108, PCNT))
    {
        Device(P108) { Name(_HID, "ACPI0007") Name(_UID, 108) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(109, PCNT))
    {
        Device(P109) { Name(_HID, "ACPI0007") Name(_UID, 109) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(110, PCNT))
    {
        Device(P110) { Name(_HID, "ACPI0007") Name(_UID, 110) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(111, PCNT))
    {
        Device(P111) { Name(_HID, "ACPI0007") Name(_UID, 111) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(112, PCNT))
    {
        Device(P112) { Name(_HID, "ACPI0007") Name(_UID, 112) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(113, PCNT))
    {
        Device(P113) { Name(_HID, "ACPI0007") Name(_UID, 113) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(114, PCNT))
    {
        Device(P114) { Name(_HID, "ACPI0007") Name(_UID, 114) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(115, PCNT))
    {
        Device(P115) { Name(_HID, "ACPI0007") Name(_UID, 115) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(116, PCNT))
    {
        Device(P116) { Name(_HID, "ACPI0007") Name(_UID, 116) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(117, PCNT))
    {
        Device(P117) { Name(_HID, "ACPI0007") Name(_UID, 117) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(118, PCNT))
    {
        Device(P118) { Name(_HID, "ACPI0007") Name(_UID, 118) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(119, PCNT))
    {
        Device(P119) { Name(_HID, "ACPI0007") Name(_UID, 119) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(120, PCNT))
    {
        Device(P120) { Name(_HID, "ACPI0007") Name(_UID, 120) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(121, PCNT))
    {
        Device(P121) { Name(_HID, "ACPI0007") Name(_UID, 121) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(122, PCNT))
    {
        Device(P122) { Name(_HID, "ACPI0007") Name(_UID, 122) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(123, PCNT))
    {
        Device(P123) { Name(_HID, "ACPI0007") Name(_UID, 123) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(124, PCNT))
    {
        Device(P124) { Name(_HID, "ACPI0007") Name(_UID, 124) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(125, PCNT))
    {
        Device(P125) { Name(_HID, "ACPI0007") Name(_UID, 125) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(126, PCNT))
    {
        Device(P126) { Name(_HID, "ACPI0007") Name(_UID, 126) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(127, PCNT))
    {
        Device(P127) { Name(_HID, "ACPI0007") Name(_UID, 127) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(128, PCNT))
    {
        Device(P128) { Name(_HID, "ACPI0007") Name(_UID, 128) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(129, PCNT))
    {
        Device(P129) { Name(_HID, "ACPI0007") Name(_UID, 129) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(130, PCNT))
    {
        Device(P130) { Name(_HID, "ACPI0007") Name(_UID, 130) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(131, PCNT))
    {
        Device(P131) { Name(_HID, "ACPI0007") Name(_UID, 131) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(132, PCNT))
    {
        Device(P132) { Name(_HID, "ACPI0007") Name(_UID, 132) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(133, PCNT))
    {
        Device(P133) { Name(_HID, "ACPI0007") Name(_UID, 133) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(134, PCNT))
    {
        Device(P134) { Name(_HID, "ACPI0007") Name(_UID, 134) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(135, PCNT))
    {
        Device(P135) { Name(_HID, "ACPI0007") Name(_UID, 135) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(136, PCNT))
    {
        Device(P136) { Name(_HID, "ACPI0007") Name(_UID, 136) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(137, PCNT))
    {
        Device(P137) { Name(_HID, "ACPI0007") Name(_UID, 137) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(138, PCNT))
    {
        Device(P138) { Name(_HID, "ACPI0007") Name(_UID, 138) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(139, PCNT))
    {
        Device(P139) { Name(_HID, "ACPI0007") Name(_UID, 139) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(140, PCNT))
    {
        Device(P140) { Name(_HID, "ACPI0007") Name(_UID, 140) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(141, PCNT))
    {
        Device(P141) { Name(_HID, "ACPI0007") Name(_UID, 141) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(142, PCNT))
    {
        Device(P142) { Name(_HID, "ACPI0007") Name(_UID, 142) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(143, PCNT))
    {
        Device(P143) { Name(_HID, "ACPI0007") Name(_UID, 143) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(144, PCNT))
    {
        Device(P144) { Name(_HID, "ACPI0007") Name(_UID, 144) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(145, PCNT))
    {
        Device(P145) { Name(_HID, "ACPI0007") Name(_UID, 145) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(146, PCNT))
    {
        Device(P146) { Name(_HID, "ACPI0007") Name(_UID, 146) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(147, PCNT))
    {
        Device(P147) { Name(_HID, "ACPI0007") Name(_UID, 147) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(148, PCNT))
    {
        Device(P148) { Name(_HID, "ACPI0007") Name(_UID, 148) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(149, PCNT))
    {
        Device(P149) { Name(_HID, "ACPI0007") Name(_UID, 149) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(150, PCNT))
    {
        Device(P150) { Name(_HID, "ACPI0007") Name(_UID, 150) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(151, PCNT))
    {
        Device(P151) { Name(_HID, "ACPI0007") Name(_UID, 151) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(152, PCNT))
    {
        Device(P152) { Name(_HID, "ACPI0007") Name(_UID, 152) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(153, PCNT))
    {
        Device(P153) { Name(_HID, "ACPI0007") Name(_UID, 153) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(154, PCNT))
    {
        Device(P154) { Name(_HID, "ACPI0007") Name(_UID, 154) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(155, PCNT))
    {
        Device(P155) { Name(_HID, "ACPI0007") Name(_UID, 155) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(156, PCNT))
    {
        Device(P156) { Name(_HID, "ACPI0007") Name(_UID, 156) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(157, PCNT))
    {
        Device(P157) { Name(_HID, "ACPI0007") Name(_UID, 157) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(158, PCNT))
    {
        Device(P158) { Name(_HID, "ACPI0007") Name(_UID, 158) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(159, PCNT))
    {
        Device(P159) { Name(_HID, "ACPI0007") Name(_UID, 159) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(160, PCNT))
    {
        Device(P160) { Name(_HID, "ACPI0007") Name(_UID, 160) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(161, PCNT))
    {
        Device(P161) { Name(_HID, "ACPI0007") Name(_UID, 161) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(162, PCNT))
    {
        Device(P162) { Name(_HID, "ACPI0007") Name(_UID, 162) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(163, PCNT))
    {
        Device(P163) { Name(_HID, "ACPI0007") Name(_UID, 163) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(164, PCNT))
    {
        Device(P164) { Name(_HID, "ACPI0007") Name(_UID, 164) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(165, PCNT))
    {
        Device(P165) { Name(_HID, "ACPI0007") Name(_UID, 165) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(166, PCNT))
    {
        Device(P166) { Name(_HID, "ACPI0007") Name(_UID, 166) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(167, PCNT))
    {
        Device(P167) { Name(_HID, "ACPI0007") Name(_UID, 167) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(168, PCNT))
    {
        Device(P168) { Name(_HID, "ACPI0007") Name(_UID, 168) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(169, PCNT))
    {
        Device(P169) { Name(_HID, "ACPI0007") Name(_UID, 169) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(170, PCNT))
    {
        Device(P170) { Name(_HID, "ACPI0007") Name(_UID, 170) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(171, PCNT))
    {
        Device(P171) { Name(_HID, "ACPI0007") Name(_UID, 171) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(172, PCNT))
    {
        Device(P172) { Name(_HID, "ACPI0007") Name(_UID, 172) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(173, PCNT))
    {
        Device(P173) { Name(_HID, "ACPI0007") Name(_UID, 173) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(174, PCNT))
    {
        Device(P174) { Name(_HID, "ACPI0007") Name(_UID, 174) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(175, PCNT))
    {
        Device(P175) { Name(_HID, "ACPI0007") Name(_UID, 175) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(176, PCNT))
    {
        Device(P176) { Name(_HID, "ACPI0007") Name(_UID, 176) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(177, PCNT))
    {
        Device(P177) { Name(_HID, "ACPI0007") Name(_UID, 177) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(178, PCNT))
    {
        Device(P178) { Name(_HID, "ACPI0007") Name(_UID, 178) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(179, PCNT))
    {
        Device(P179) { Name(_HID, "ACPI0007") Name(_UID, 179) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(180, PCNT))
    {
        Device(P180) { Name(_HID, "ACPI0007") Name(_UID, 180) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(181, PCNT))
    {
        Device(P181) { Name(_HID, "ACPI0007") Name(_UID, 181) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(182, PCNT))
    {
        Device(P182) { Name(_HID, "ACPI0007") Name(_UID, 182) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(183, PCNT))
    {
        Device(P183) { Name(_HID, "ACPI0007") Name(_UID, 183) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(184, PCNT))
    {
        Device(P184) { Name(_HID, "ACPI0007") Name(_UID, 184) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(185, PCNT))
    {
        Device(P185) { Name(_HID, "ACPI0007") Name(_UID, 185) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(186, PCNT))
    {
        Device(P186) { Name(_HID, "ACPI0007") Name(_UID, 186) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(187, PCNT))
    {
        Device(P187) { Name(_HID, "ACPI0007") Name(_UID, 187) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(188, PCNT))
    {
        Device(P188) { Name(_HID, "ACPI0007") Name(_UID, 188) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(189, PCNT))
    {
        Device(P189) { Name(_HID, "ACPI0007") Name(_UID, 189) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(190, PCNT))
    {
        Device(P190) { Name(_HID, "ACPI0007") Name(_UID, 190) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(191, PCNT))
    {
        Device(P191) { Name(_HID, "ACPI0007") Name(_UID, 191) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(192, PCNT))
    {
        Device(P192) { Name(_HID, "ACPI0007") Name(_UID, 192) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(193, PCNT))
    {
        Device(P193) { Name(_HID, "ACPI0007") Name(_UID, 193) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(194, PCNT))
    {
        Device(P194) { Name(_HID, "ACPI0007") Name(_UID, 194) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(195, PCNT))
    {
        Device(P195) { Name(_HID, "ACPI0007") Name(_UID, 195) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(196, PCNT))
    {
        Device(P196) { Name(_HID, "ACPI0007") Name(_UID, 196) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(197, PCNT))
    {
        Device(P197) { Name(_HID, "ACPI0007") Name(_UID, 197) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(198, PCNT))
    {
        Device(P198) { Name(_HID, "ACPI0007") Name(_UID, 198) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(199, PCNT))
    {
        Device(P199) { Name(_HID, "ACPI0007") Name(_UID, 199) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(200, PCNT))
    {
        Device(P200) { Name(_HID, "ACPI0007") Name(_UID, 200) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(201, PCNT))
    {
        Device(P201) { Name(_HID, "ACPI0007") Name(_UID, 201) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(202, PCNT))
    {
        Device(P202) { Name(_HID, "ACPI0007") Name(_UID, 202) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(203, PCNT))
    {
        Device(P203) { Name(_HID, "ACPI0007") Name(_UID, 203) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(204, PCNT))
    {
        Device(P204) { Name(_HID, "ACPI0007") Name(_UID, 204) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(205, PCNT))
    {
        Device(P205) { Name(_HID, "ACPI0007") Name(_UID, 205) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(206, PCNT))
    {
        Device(P206) { Name(_HID, "ACPI0007") Name(_UID, 206) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(207, PCNT))
    {
        Device(P207) { Name(_HID, "ACPI0007") Name(_UID, 207) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(208, PCNT))
    {
        Device(P208) { Name(_HID, "ACPI0007") Name(_UID, 208) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(209, PCNT))
    {
        Device(P209) { Name(_HID, "ACPI0007") Name(_UID, 209) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(210, PCNT))
    {
        Device(P210) { Name(_HID, "ACPI0007") Name(_UID, 210) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(211, PCNT))
    {
        Device(P211) { Name(_HID, "ACPI0007") Name(_UID, 211) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(212, PCNT))
    {
        Device(P212) { Name(_HID, "ACPI0007") Name(_UID, 212) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(213, PCNT))
    {
        Device(P213) { Name(_HID, "ACPI0007") Name(_UID, 213) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(214, PCNT))
    {
        Device(P214) { Name(_HID, "ACPI0007") Name(_UID, 214) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(215, PCNT))
    {
        Device(P215) { Name(_HID, "ACPI0007") Name(_UID, 215) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(216, PCNT))
    {
        Device(P216) { Name(_HID, "ACPI0007") Name(_UID, 216) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(217, PCNT))
    {
        Device(P217) { Name(_HID, "ACPI0007") Name(_UID, 217) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(218, PCNT))
    {
        Device(P218) { Name(_HID, "ACPI0007") Name(_UID, 218) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(219, PCNT))
    {
        Device(P219) { Name(_HID, "ACPI0007") Name(_UID, 219) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(220, PCNT))
    {
        Device(P220) { Name(_HID, "ACPI0007") Name(_UID, 220) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(221, PCNT))
    {
        Device(P221) { Name(_HID, "ACPI0007") Name(_UID, 221) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(222, PCNT))
    {
        Device(P222) { Name(_HID, "ACPI0007") Name(_UID, 222) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(223, PCNT))
    {
        Device(P223) { Name(_HID, "ACPI0007") Name(_UID, 223) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(224, PCNT))
    {
        Device(P224) { Name(_HID, "ACPI0007") Name(_UID, 224) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(225, PCNT))
    {
        Device(P225) { Name(_HID, "ACPI0007") Name(_UID, 225) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(226, PCNT))
    {
        Device(P226) { Name(_HID, "ACPI0007") Name(_UID, 226) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(227, PCNT))
    {
        Device(P227) { Name(_HID, "ACPI0007") Name(_UID, 227) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(228, PCNT))
    {
        Device(P228) { Name(_HID, "ACPI0007") Name(_UID, 228) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(229, PCNT))
    {
        Device(P229) { Name(_HID, "ACPI0007") Name(_UID, 229) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(230, PCNT))
    {
        Device(P230) { Name(_HID, "ACPI0007") Name(_UID, 230) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(231, PCNT))
    {
        Device(P231) { Name(_HID, "ACPI0007") Name(_UID, 231) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(232, PCNT))
    {
        Device(P232) { Name(_HID, "ACPI0007") Name(_UID, 232) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(233, PCNT))
    {
        Device(P233) { Name(_HID, "ACPI0007") Name(_UID, 233) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(234, PCNT))
    {
        Device(P234) { Name(_HID, "ACPI0007") Name(_UID, 234) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(235, PCNT))
    {
        Device(P235) { Name(_HID, "ACPI0007") Name(_UID, 235) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(236, PCNT))
    {
        Device(P236) { Name(_HID, "ACPI0007") Name(_UID, 236) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(237, PCNT))
    {
        Device(P237) { Name(_HID, "ACPI0007") Name(_UID, 237) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(238, PCNT))
    {
        Device(P238) { Name(_HID, "ACPI0007") Name(_UID, 238) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(239, PCNT))
    {
        Device(P239) { Name(_HID, "ACPI0007") Name(_UID, 239) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(240, PCNT))
    {
        Device(P240) { Name(_HID, "ACPI0007") Name(_UID, 240) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(241, PCNT))
    {
        Device(P241) { Name(_HID, "ACPI0007") Name(_UID, 241) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(242, PCNT))
    {
        Device(P242) { Name(_HID, "ACPI0007") Name(_UID, 242) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(243, PCNT))
    {
        Device(P243) { Name(_HID, "ACPI0007") Name(_UID, 243) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(244, PCNT))
    {
        Device(P244) { Name(_HID, "ACPI0007") Name(_UID, 244) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(245, PCNT))
    {
        Device(P245) { Name(_HID, "ACPI0007") Name(_UID, 245) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(246, PCNT))
    {
        Device(P246) { Name(_HID, "ACPI0007") Name(_UID, 246) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(247, PCNT))
    {
        Device(P247) { Name(_HID, "ACPI0007") Name(_UID, 247) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(248, PCNT))
    {
        Device(P248) { Name(_HID, "ACPI0007") Name(_UID, 248) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(249, PCNT))
    {
        Device(P249) { Name(_HID, "ACPI0007") Name(_UID, 249) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(250, PCNT))
    {
        Device(P250) { Name(_HID, "ACPI0007") Name(_UID, 250) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(251, PCNT))
    {
        Device(P251) { Name(_HID, "ACPI0007") Name(_UID, 251) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(252, PCNT))
    {
        Device(P252) { Name(_HID, "ACPI0007") Name(_UID, 252) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(253, PCNT))
    {
        Device(P253) { Name(_HID, "ACPI0007") Name(_UID, 253) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(254, PCNT))
    {
        Device(P254) { Name(_HID, "ACPI0007") Name(_UID, 254) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(255, PCNT))
    {
        Device(P255) { Name(_HID, "ACPI0007") Name(_UID, 255) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(256, PCNT))
    {
        Device(P256) { Name(_HID, "ACPI0007") Name(_UID, 256) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(257, PCNT))
    {
        Device(P257) { Name(_HID, "ACPI0007") Name(_UID, 257) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(258, PCNT))
    {
        Device(P258) { Name(_HID, "ACPI0007") Name(_UID, 258) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(259, PCNT))
    {
        Device(P259) { Name(_HID, "ACPI0007") Name(_UID, 259) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(260, PCNT))
    {
        Device(P260) { Name(_HID, "ACPI0007") Name(_UID, 260) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(261, PCNT))
    {
        Device(P261) { Name(_HID, "ACPI0007") Name(_UID, 261) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(262, PCNT))
    {
        Device(P262) { Name(_HID, "ACPI0007") Name(_UID, 262) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(263, PCNT))
    {
        Device(P263) { Name(_HID, "ACPI0007") Name(_UID, 263) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(264, PCNT))
    {
        Device(P264) { Name(_HID, "ACPI0007") Name(_UID, 264) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(265, PCNT))
    {
        Device(P265) { Name(_HID, "ACPI0007") Name(_UID, 265) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(266, PCNT))
    {
        Device(P266) { Name(_HID, "ACPI0007") Name(_UID, 266) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(267, PCNT))
    {
        Device(P267) { Name(_HID, "ACPI0007") Name(_UID, 267) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(268, PCNT))
    {
        Device(P268) { Name(_HID, "ACPI0007") Name(_UID, 268) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(269, PCNT))
    {
        Device(P269) { Name(_HID, "ACPI0007") Name(_UID, 269) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(270, PCNT))
    {
        Device(P270) { Name(_HID, "ACPI0007") Name(_UID, 270) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(271, PCNT))
    {
        Device(P271) { Name(_HID, "ACPI0007") Name(_UID, 271) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(272, PCNT))
    {
        Device(P272) { Name(_HID, "ACPI0007") Name(_UID, 272) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(273, PCNT))
    {
        Device(P273) { Name(_HID, "ACPI0007") Name(_UID, 273) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(274, PCNT))
    {
        Device(P274) { Name(_HID, "ACPI0007") Name(_UID, 274) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(275, PCNT))
    {
        Device(P275) { Name(_HID, "ACPI0007") Name(_UID, 275) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(276, PCNT))
    {
        Device(P276) { Name(_HID, "ACPI0007") Name(_UID, 276) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(277, PCNT))
    {
        Device(P277) { Name(_HID, "ACPI0007") Name(_UID, 277) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(278, PCNT))
    {
        Device(P278) { Name(_HID, "ACPI0007") Name(_UID, 278) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(279, PCNT))
    {
        Device(P279) { Name(_HID, "ACPI0007") Name(_UID, 279) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(280, PCNT))
    {
        Device(P280) { Name(_HID, "ACPI0007") Name(_UID, 280) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(281, PCNT))
    {
        Device(P281) { Name(_HID, "ACPI0007") Name(_UID, 281) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(282, PCNT))
    {
        Device(P282) { Name(_HID, "ACPI0007") Name(_UID, 282) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(283, PCNT))
    {
        Device(P283) { Name(_HID, "ACPI0007") Name(_UID, 283) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(284, PCNT))
    {
        Device(P284) { Name(_HID, "ACPI0007") Name(_UID, 284) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(285, PCNT))
    {
        Device(P285) { Name(_HID, "ACPI0007") Name(_UID, 285) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(286, PCNT))
    {
        Device(P286) { Name(_HID, "ACPI0007") Name(_UID, 286) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(287, PCNT))
    {
        Device(P287) { Name(_HID, "ACPI0007") Name(_UID, 287) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(288, PCNT))
    {
        Device(P288) { Name(_HID, "ACPI0007") Name(_UID, 288) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(289, PCNT))
    {
        Device(P289) { Name(_HID, "ACPI0007") Name(_UID, 289) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(290, PCNT))
    {
        Device(P290) { Name(_HID, "ACPI0007") Name(_UID, 290) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(291, PCNT))
    {
        Device(P291) { Name(_HID, "ACPI0007") Name(_UID, 291) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(292, PCNT))
    {
        Device(P292) { Name(_HID, "ACPI0007") Name(_UID, 292) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(293, PCNT))
    {
        Device(P293) { Name(_HID, "ACPI0007") Name(_UID, 293) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(294, PCNT))
    {
        Device(P294) { Name(_HID, "ACPI0007") Name(_UID, 294) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(295, PCNT))
    {
        Device(P295) { Name(_HID, "ACPI0007") Name(_UID, 295) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(296, PCNT))
    {
        Device(P296) { Name(_HID, "ACPI0007") Name(_UID, 296) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(297, PCNT))
    {
        Device(P297) { Name(_HID, "ACPI0007") Name(_UID, 297) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(298, PCNT))
    {
        Device(P298) { Name(_HID, "ACPI0007") Name(_UID, 298) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(299, PCNT))
    {
        Device(P299) { Name(_HID, "ACPI0007") Name(_UID, 299) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(300, PCNT))
    {
        Device(P300) { Name(_HID, "ACPI0007") Name(_UID, 300) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(301, PCNT))
    {
        Device(P301) { Name(_HID, "ACPI0007") Name(_UID, 301) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(302, PCNT))
    {
        Device(P302) { Name(_HID, "ACPI0007") Name(_UID, 302) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(303, PCNT))
    {
        Device(P303) { Name(_HID, "ACPI0007") Name(_UID, 303) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(304, PCNT))
    {
        Device(P304) { Name(_HID, "ACPI0007") Name(_UID, 304) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(305, PCNT))
    {
        Device(P305) { Name(_HID, "ACPI0007") Name(_UID, 305) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(306, PCNT))
    {
        Device(P306) { Name(_HID, "ACPI0007") Name(_UID, 306) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(307, PCNT))
    {
        Device(P307) { Name(_HID, "ACPI0007") Name(_UID, 307) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(308, PCNT))
    {
        Device(P308) { Name(_HID, "ACPI0007") Name(_UID, 308) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(309, PCNT))
    {
        Device(P309) { Name(_HID, "ACPI0007") Name(_UID, 309) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(310, PCNT))
    {
        Device(P310) { Name(_HID, "ACPI0007") Name(_UID, 310) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(311, PCNT))
    {
        Device(P311) { Name(_HID, "ACPI0007") Name(_UID, 311) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(312, PCNT))
    {
        Device(P312) { Name(_HID, "ACPI0007") Name(_UID, 312) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(313, PCNT))
    {
        Device(P313) { Name(_HID, "ACPI0007") Name(_UID, 313) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(314, PCNT))
    {
        Device(P314) { Name(_HID, "ACPI0007") Name(_UID, 314) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(315, PCNT))
    {
        Device(P315) { Name(_HID, "ACPI0007") Name(_UID, 315) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(316, PCNT))
    {
        Device(P316) { Name(_HID, "ACPI0007") Name(_UID, 316) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(317, PCNT))
    {
        Device(P317) { Name(_HID, "ACPI0007") Name(_UID, 317) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(318, PCNT))
    {
        Device(P318) { Name(_HID, "ACPI0007") Name(_UID, 318) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(319, PCNT))
    {
        Device(P319) { Name(_HID, "ACPI0007") Name(_UID, 319) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(320, PCNT))
    {
        Device(P320) { Name(_HID, "ACPI0007") Name(_UID, 320) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(321, PCNT))
    {
        Device(P321) { Name(_HID, "ACPI0007") Name(_UID, 321) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(322, PCNT))
    {
        Device(P322) { Name(_HID, "ACPI0007") Name(_UID, 322) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(323, PCNT))
    {
        Device(P323) { Name(_HID, "ACPI0007") Name(_UID, 323) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(324, PCNT))
    {
        Device(P324) { Name(_HID, "ACPI0007") Name(_UID, 324) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(325, PCNT))
    {
        Device(P325) { Name(_HID, "ACPI0007") Name(_UID, 325) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(326, PCNT))
    {
        Device(P326) { Name(_HID, "ACPI0007") Name(_UID, 326) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(327, PCNT))
    {
        Device(P327) { Name(_HID, "ACPI0007") Name(_UID, 327) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(328, PCNT))
    {
        Device(P328) { Name(_HID, "ACPI0007") Name(_UID, 328) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(329, PCNT))
    {
        Device(P329) { Name(_HID, "ACPI0007") Name(_UID, 329) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(330, PCNT))
    {
        Device(P330) { Name(_HID, "ACPI0007") Name(_UID, 330) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(331, PCNT))
    {
        Device(P331) { Name(_HID, "ACPI0007") Name(_UID, 331) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(332, PCNT))
    {
        Device(P332) { Name(_HID, "ACPI0007") Name(_UID, 332) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(333, PCNT))
    {
        Device(P333) { Name(_HID, "ACPI0007") Name(_UID, 333) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(334, PCNT))
    {
        Device(P334) { Name(_HID, "ACPI0007") Name(_UID, 334) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(335, PCNT))
    {
        Device(P335) { Name(_HID, "ACPI0007") Name(_UID, 335) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(336, PCNT))
    {
        Device(P336) { Name(_HID, "ACPI0007") Name(_UID, 336) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(337, PCNT))
    {
        Device(P337) { Name(_HID, "ACPI0007") Name(_UID, 337) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(338, PCNT))
    {
        Device(P338) { Name(_HID, "ACPI0007") Name(_UID, 338) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(339, PCNT))
    {
        Device(P339) { Name(_HID, "ACPI0007") Name(_UID, 339) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(340, PCNT))
    {
        Device(P340) { Name(_HID, "ACPI0007") Name(_UID, 340) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(341, PCNT))
    {
        Device(P341) { Name(_HID, "ACPI0007") Name(_UID, 341) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(342, PCNT))
    {
        Device(P342) { Name(_HID, "ACPI0007") Name(_UID, 342) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(343, PCNT))
    {
        Device(P343) { Name(_HID, "ACPI0007") Name(_UID, 343) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(344, PCNT))
    {
        Device(P344) { Name(_HID, "ACPI0007") Name(_UID, 344) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(345, PCNT))
    {
        Device(P345) { Name(_HID, "ACPI0007") Name(_UID, 345) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(346, PCNT))
    {
        Device(P346) { Name(_HID, "ACPI0007") Name(_UID, 346) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(347, PCNT))
    {
        Device(P347) { Name(_HID, "ACPI0007") Name(_UID, 347) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(348, PCNT))
    {
        Device(P348) { Name(_HID, "ACPI0007") Name(_UID, 348) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(349, PCNT))
    {
        Device(P349) { Name(_HID, "ACPI0007") Name(_UID, 349) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(350, PCNT))
    {
        Device(P350) { Name(_HID, "ACPI0007") Name(_UID, 350) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(351, PCNT))
    {
        Device(P351) { Name(_HID, "ACPI0007") Name(_UID, 351) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(352, PCNT))
    {
        Device(P352) { Name(_HID, "ACPI0007") Name(_UID, 352) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(353, PCNT))
    {
        Device(P353) { Name(_HID, "ACPI0007") Name(_UID, 353) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(354, PCNT))
    {
        Device(P354) { Name(_HID, "ACPI0007") Name(_UID, 354) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(355, PCNT))
    {
        Device(P355) { Name(_HID, "ACPI0007") Name(_UID, 355) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(356, PCNT))
    {
        Device(P356) { Name(_HID, "ACPI0007") Name(_UID, 356) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(357, PCNT))
    {
        Device(P357) { Name(_HID, "ACPI0007") Name(_UID, 357) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(358, PCNT))
    {
        Device(P358) { Name(_HID, "ACPI0007") Name(_UID, 358) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(359, PCNT))
    {
        Device(P359) { Name(_HID, "ACPI0007") Name(_UID, 359) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(360, PCNT))
    {
        Device(P360) { Name(_HID, "ACPI0007") Name(_UID, 360) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(361, PCNT))
    {
        Device(P361) { Name(_HID, "ACPI0007") Name(_UID, 361) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(362, PCNT))
    {
        Device(P362) { Name(_HID, "ACPI0007") Name(_UID, 362) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(363, PCNT))
    {
        Device(P363) { Name(_HID, "ACPI0007") Name(_UID, 363) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(364, PCNT))
    {
        Device(P364) { Name(_HID, "ACPI0007") Name(_UID, 364) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(365, PCNT))
    {
        Device(P365) { Name(_HID, "ACPI0007") Name(_UID, 365) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(366, PCNT))
    {
        Device(P366) { Name(_HID, "ACPI0007") Name(_UID, 366) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(367, PCNT))
    {
        Device(P367) { Name(_HID, "ACPI0007") Name(_UID, 367) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(368, PCNT))
    {
        Device(P368) { Name(_HID, "ACPI0007") Name(_UID, 368) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(369, PCNT))
    {
        Device(P369) { Name(_HID, "ACPI0007") Name(_UID, 369) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(370, PCNT))
    {
        Device(P370) { Name(_HID, "ACPI0007") Name(_UID, 370) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(371, PCNT))
    {
        Device(P371) { Name(_HID, "ACPI0007") Name(_UID, 371) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(372, PCNT))
    {
        Device(P372) { Name(_HID, "ACPI0007") Name(_UID, 372) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(373, PCNT))
    {
        Device(P373) { Name(_HID, "ACPI0007") Name(_UID, 373) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(374, PCNT))
    {
        Device(P374) { Name(_HID, "ACPI0007") Name(_UID, 374) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(375, PCNT))
    {
        Device(P375) { Name(_HID, "ACPI0007") Name(_UID, 375) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(376, PCNT))
    {
        Device(P376) { Name(_HID, "ACPI0007") Name(_UID, 376) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(377, PCNT))
    {
        Device(P377) { Name(_HID, "ACPI0007") Name(_UID, 377) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(378, PCNT))
    {
        Device(P378) { Name(_HID, "ACPI0007") Name(_UID, 378) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(379, PCNT))
    {
        Device(P379) { Name(_HID, "ACPI0007") Name(_UID, 379) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(380, PCNT))
    {
        Device(P380) { Name(_HID, "ACPI0007") Name(_UID, 380) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(381, PCNT))
    {
        Device(P381) { Name(_HID, "ACPI0007") Name(_UID, 381) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(382, PCNT))
    {
        Device(P382) { Name(_HID, "ACPI0007") Name(_UID, 382) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(383, PCNT))
    {
        Device(P383) { Name(_HID, "ACPI0007") Name(_UID, 383) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(384, PCNT))
    {
        Device(P384) { Name(_HID, "ACPI0007") Name(_UID, 384) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(385, PCNT))
    {
        Device(P385) { Name(_HID, "ACPI0007") Name(_UID, 385) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(386, PCNT))
    {
        Device(P386) { Name(_HID, "ACPI0007") Name(_UID, 386) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(387, PCNT))
    {
        Device(P387) { Name(_HID, "ACPI0007") Name(_UID, 387) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(388, PCNT))
    {
        Device(P388) { Name(_HID, "ACPI0007") Name(_UID, 388) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(389, PCNT))
    {
        Device(P389) { Name(_HID, "ACPI0007") Name(_UID, 389) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(390, PCNT))
    {
        Device(P390) { Name(_HID, "ACPI0007") Name(_UID, 390) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(391, PCNT))
    {
        Device(P391) { Name(_HID, "ACPI0007") Name(_UID, 391) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(392, PCNT))
    {
        Device(P392) { Name(_HID, "ACPI0007") Name(_UID, 392) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(393, PCNT))
    {
        Device(P393) { Name(_HID, "ACPI0007") Name(_UID, 393) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(394, PCNT))
    {
        Device(P394) { Name(_HID, "ACPI0007") Name(_UID, 394) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(395, PCNT))
    {
        Device(P395) { Name(_HID, "ACPI0007") Name(_UID, 395) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(396, PCNT))
    {
        Device(P396) { Name(_HID, "ACPI0007") Name(_UID, 396) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(397, PCNT))
    {
        Device(P397) { Name(_HID, "ACPI0007") Name(_UID, 397) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(398, PCNT))
    {
        Device(P398) { Name(_HID, "ACPI0007") Name(_UID, 398) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(399, PCNT))
    {
        Device(P399) { Name(_HID, "ACPI0007") Name(_UID, 399) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(400, PCNT))
    {
        Device(P400) { Name(_HID, "ACPI0007") Name(_UID, 400) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(401, PCNT))
    {
        Device(P401) { Name(_HID, "ACPI0007") Name(_UID, 401) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(402, PCNT))
    {
        Device(P402) { Name(_HID, "ACPI0007") Name(_UID, 402) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(403, PCNT))
    {
        Device(P403) { Name(_HID, "ACPI0007") Name(_UID, 403) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(404, PCNT))
    {
        Device(P404) { Name(_HID, "ACPI0007") Name(_UID, 404) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(405, PCNT))
    {
        Device(P405) { Name(_HID, "ACPI0007") Name(_UID, 405) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(406, PCNT))
    {
        Device(P406) { Name(_HID, "ACPI0007") Name(_UID, 406) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(407, PCNT))
    {
        Device(P407) { Name(_HID, "ACPI0007") Name(_UID, 407) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(408, PCNT))
    {
        Device(P408) { Name(_HID, "ACPI0007") Name(_UID, 408) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(409, PCNT))
    {
        Device(P409) { Name(_HID, "ACPI0007") Name(_UID, 409) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(410, PCNT))
    {
        Device(P410) { Name(_HID, "ACPI0007") Name(_UID, 410) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(411, PCNT))
    {
        Device(P411) { Name(_HID, "ACPI0007") Name(_UID, 411) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(412, PCNT))
    {
        Device(P412) { Name(_HID, "ACPI0007") Name(_UID, 412) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(413, PCNT))
    {
        Device(P413) { Name(_HID, "ACPI0007") Name(_UID, 413) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(414, PCNT))
    {
        Device(P414) { Name(_HID, "ACPI0007") Name(_UID, 414) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(415, PCNT))
    {
        Device(P415) { Name(_HID, "ACPI0007") Name(_UID, 415) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(416, PCNT))
    {
        Device(P416) { Name(_HID, "ACPI0007") Name(_UID, 416) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(417, PCNT))
    {
        Device(P417) { Name(_HID, "ACPI0007") Name(_UID, 417) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(418, PCNT))
    {
        Device(P418) { Name(_HID, "ACPI0007") Name(_UID, 418) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(419, PCNT))
    {
        Device(P419) { Name(_HID, "ACPI0007") Name(_UID, 419) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(420, PCNT))
    {
        Device(P420) { Name(_HID, "ACPI0007") Name(_UID, 420) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(421, PCNT))
    {
        Device(P421) { Name(_HID, "ACPI0007") Name(_UID, 421) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(422, PCNT))
    {
        Device(P422) { Name(_HID, "ACPI0007") Name(_UID, 422) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(423, PCNT))
    {
        Device(P423) { Name(_HID, "ACPI0007") Name(_UID, 423) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(424, PCNT))
    {
        Device(P424) { Name(_HID, "ACPI0007") Name(_UID, 424) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(425, PCNT))
    {
        Device(P425) { Name(_HID, "ACPI0007") Name(_UID, 425) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(426, PCNT))
    {
        Device(P426) { Name(_HID, "ACPI0007") Name(_UID, 426) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(427, PCNT))
    {
        Device(P427) { Name(_HID, "ACPI0007") Name(_UID, 427) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(428, PCNT))
    {
        Device(P428) { Name(_HID, "ACPI0007") Name(_UID, 428) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(429, PCNT))
    {
        Device(P429) { Name(_HID, "ACPI0007") Name(_UID, 429) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(430, PCNT))
    {
        Device(P430) { Name(_HID, "ACPI0007") Name(_UID, 430) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(431, PCNT))
    {
        Device(P431) { Name(_HID, "ACPI0007") Name(_UID, 431) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(432, PCNT))
    {
        Device(P432) { Name(_HID, "ACPI0007") Name(_UID, 432) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(433, PCNT))
    {
        Device(P433) { Name(_HID, "ACPI0007") Name(_UID, 433) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(434, PCNT))
    {
        Device(P434) { Name(_HID, "ACPI0007") Name(_UID, 434) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(435, PCNT))
    {
        Device(P435) { Name(_HID, "ACPI0007") Name(_UID, 435) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(436, PCNT))
    {
        Device(P436) { Name(_HID, "ACPI0007") Name(_UID, 436) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(437, PCNT))
    {
        Device(P437) { Name(_HID, "ACPI0007") Name(_UID, 437) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(438, PCNT))
    {
        Device(P438) { Name(_HID, "ACPI0007") Name(_UID, 438) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(439, PCNT))
    {
        Device(P439) { Name(_HID, "ACPI0007") Name(_UID, 439) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(440, PCNT))
    {
        Device(P440) { Name(_HID, "ACPI0007") Name(_UID, 440) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(441, PCNT))
    {
        Device(P441) { Name(_HID, "ACPI0007") Name(_UID, 441) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(442, PCNT))
    {
        Device(P442) { Name(_HID, "ACPI0007") Name(_UID, 442) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(443, PCNT))
    {
        Device(P443) { Name(_HID, "ACPI0007") Name(_UID, 443) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(444, PCNT))
    {
        Device(P444) { Name(_HID, "ACPI0007") Name(_UID, 444) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(445, PCNT))
    {
        Device(P445) { Name(_HID, "ACPI0007") Name(_UID, 445) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(446, PCNT))
    {
        Device(P446) { Name(_HID, "ACPI0007") Name(_UID, 446) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(447, PCNT))
    {
        Device(P447) { Name(_HID, "ACPI0007") Name(_UID, 447) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(448, PCNT))
    {
        Device(P448) { Name(_HID, "ACPI0007") Name(_UID, 448) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(449, PCNT))
    {
        Device(P449) { Name(_HID, "ACPI0007") Name(_UID, 449) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(450, PCNT))
    {
        Device(P450) { Name(_HID, "ACPI0007") Name(_UID, 450) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(451, PCNT))
    {
        Device(P451) { Name(_HID, "ACPI0007") Name(_UID, 451) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(452, PCNT))
    {
        Device(P452) { Name(_HID, "ACPI0007") Name(_UID, 452) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(453, PCNT))
    {
        Device(P453) { Name(_HID, "ACPI0007") Name(_UID, 453) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(454, PCNT))
    {
        Device(P454) { Name(_HID, "ACPI0007") Name(_UID, 454) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(455, PCNT))
    {
        Device(P455) { Name(_HID, "ACPI0007") Name(_UID, 455) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(456, PCNT))
    {
        Device(P456) { Name(_HID, "ACPI0007") Name(_UID, 456) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(457, PCNT))
    {
        Device(P457) { Name(_HID, "ACPI0007") Name(_UID, 457) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(458, PCNT))
    {
        Device(P458) { Name(_HID, "ACPI0007") Name(_UID, 458) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(459, PCNT))
    {
        Device(P459) { Name(_HID, "ACPI0007") Name(_UID, 459) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(460, PCNT))
    {
        Device(P460) { Name(_HID, "ACPI0007") Name(_UID, 460) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(461, PCNT))
    {
        Device(P461) { Name(_HID, "ACPI0007") Name(_UID, 461) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(462, PCNT))
    {
        Device(P462) { Name(_HID, "ACPI0007") Name(_UID, 462) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(463, PCNT))
    {
        Device(P463) { Name(_HID, "ACPI0007") Name(_UID, 463) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(464, PCNT))
    {
        Device(P464) { Name(_HID, "ACPI0007") Name(_UID, 464) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(465, PCNT))
    {
        Device(P465) { Name(_HID, "ACPI0007") Name(_UID, 465) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(466, PCNT))
    {
        Device(P466) { Name(_HID, "ACPI0007") Name(_UID, 466) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(467, PCNT))
    {
        Device(P467) { Name(_HID, "ACPI0007") Name(_UID, 467) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(468, PCNT))
    {
        Device(P468) { Name(_HID, "ACPI0007") Name(_UID, 468) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(469, PCNT))
    {
        Device(P469) { Name(_HID, "ACPI0007") Name(_UID, 469) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(470, PCNT))
    {
        Device(P470) { Name(_HID, "ACPI0007") Name(_UID, 470) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(471, PCNT))
    {
        Device(P471) { Name(_HID, "ACPI0007") Name(_UID, 471) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(472, PCNT))
    {
        Device(P472) { Name(_HID, "ACPI0007") Name(_UID, 472) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(473, PCNT))
    {
        Device(P473) { Name(_HID, "ACPI0007") Name(_UID, 473) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(474, PCNT))
    {
        Device(P474) { Name(_HID, "ACPI0007") Name(_UID, 474) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(475, PCNT))
    {
        Device(P475) { Name(_HID, "ACPI0007") Name(_UID, 475) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(476, PCNT))
    {
        Device(P476) { Name(_HID, "ACPI0007") Name(_UID, 476) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(477, PCNT))
    {
        Device(P477) { Name(_HID, "ACPI0007") Name(_UID, 477) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(478, PCNT))
    {
        Device(P478) { Name(_HID, "ACPI0007") Name(_UID, 478) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(479, PCNT))
    {
        Device(P479) { Name(_HID, "ACPI0007") Name(_UID, 479) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(480, PCNT))
    {
        Device(P480) { Name(_HID, "ACPI0007") Name(_UID, 480) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(481, PCNT))
    {
        Device(P481) { Name(_HID, "ACPI0007") Name(_UID, 481) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(482, PCNT))
    {
        Device(P482) { Name(_HID, "ACPI0007") Name(_UID, 482) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(483, PCNT))
    {
        Device(P483) { Name(_HID, "ACPI0007") Name(_UID, 483) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(484, PCNT))
    {
        Device(P484) { Name(_HID, "ACPI0007") Name(_UID, 484) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(485, PCNT))
    {
        Device(P485) { Name(_HID, "ACPI0007") Name(_UID, 485) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(486, PCNT))
    {
        Device(P486) { Name(_HID, "ACPI0007") Name(_UID, 486) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(487, PCNT))
    {
        Device(P487) { Name(_HID, "ACPI0007") Name(_UID, 487) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(488, PCNT))
    {
        Device(P488) { Name(_HID, "ACPI0007") Name(_UID, 488) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(489, PCNT))
    {
        Device(P489) { Name(_HID, "ACPI0007") Name(_UID, 489) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(490, PCNT))
    {
        Device(P490) { Name(_HID, "ACPI0007") Name(_UID, 490) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(491, PCNT))
    {
        Device(P491) { Name(_HID, "ACPI0007") Name(_UID, 491) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(492, PCNT))
    {
        Device(P492) { Name(_HID, "ACPI0007") Name(_UID, 492) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(493, PCNT))
    {
        Device(P493) { Name(_HID, "ACPI0007") Name(_UID, 493) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(494, PCNT))
    {
        Device(P494) { Name(_HID, "ACPI0007") Name(_UID, 494) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(495, PCNT))
    {
        Device(P495) { Name(_HID, "ACPI0007") Name(_UID, 495) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(496, PCNT))
    {
        Device(P496) { Name(_HID, "ACPI0007") Name(_UID, 496) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(497, PCNT))
    {
        Device(P497) { Name(_HID, "ACPI0007") Name(_UID, 497) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(498, PCNT))
    {
        Device(P498) { Name(_HID, "ACPI0007") Name(_UID, 498) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(499, PCNT))
    {
        Device(P499) { Name(_HID, "ACPI0007") Name(_UID, 499) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(500, PCNT))
    {
        Device(P500) { Name(_HID, "ACPI0007") Name(_UID, 500) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(501, PCNT))
    {
        Device(P501) { Name(_HID, "ACPI0007") Name(_UID, 501) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(502, PCNT))
    {
        Device(P502) { Name(_HID, "ACPI0007") Name(_UID, 502) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(503, PCNT))
    {
        Device(P503) { Name(_HID, "ACPI0007") Name(_UID, 503) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(504, PCNT))
    {
        Device(P504) { Name(_HID, "ACPI0007") Name(_UID, 504) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(505, PCNT))
    {
        Device(P505) { Name(_HID, "ACPI0007") Name(_UID, 505) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(506, PCNT))
    {
        Device(P506) { Name(_HID, "ACPI0007") Name(_UID, 506) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(507, PCNT))
    {
        Device(P507) { Name(_HID, "ACPI0007") Name(_UID, 507) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(508, PCNT))
    {
        Device(P508) { Name(_HID, "ACPI0007") Name(_UID, 508) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(509, PCNT))
    {
        Device(P509) { Name(_HID, "ACPI0007") Name(_UID, 509) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(510, PCNT))
    {
        Device(P510) { Name(_HID, "ACPI0007") Name(_UID, 510) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(511, PCNT))
    {
        Device(P511) { Name(_HID, "ACPI0007") Name(_UID, 511) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(512, PCNT))
    {
        Device(P512) { Name(_HID, "ACPI0007") Name(_UID, 512) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(513, PCNT))
    {
        Device(P513) { Name(_HID, "ACPI0007") Name(_UID, 513) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(514, PCNT))
    {
        Device(P514) { Name(_HID, "ACPI0007") Name(_UID, 514) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(515, PCNT))
    {
        Device(P515) { Name(_HID, "ACPI0007") Name(_UID, 515) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(516, PCNT))
    {
        Device(P516) { Name(_HID, "ACPI0007") Name(_UID, 516) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(517, PCNT))
    {
        Device(P517) { Name(_HID, "ACPI0007") Name(_UID, 517) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(518, PCNT))
    {
        Device(P518) { Name(_HID, "ACPI0007") Name(_UID, 518) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(519, PCNT))
    {
        Device(P519) { Name(_HID, "ACPI0007") Name(_UID, 519) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(520, PCNT))
    {
        Device(P520) { Name(_HID, "ACPI0007") Name(_UID, 520) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(521, PCNT))
    {
        Device(P521) { Name(_HID, "ACPI0007") Name(_UID, 521) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(522, PCNT))
    {
        Device(P522) { Name(_HID, "ACPI0007") Name(_UID, 522) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(523, PCNT))
    {
        Device(P523) { Name(_HID, "ACPI0007") Name(_UID, 523) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(524, PCNT))
    {
        Device(P524) { Name(_HID, "ACPI0007") Name(_UID, 524) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(525, PCNT))
    {
        Device(P525) { Name(_HID, "ACPI0007") Name(_UID, 525) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(526, PCNT))
    {
        Device(P526) { Name(_HID, "ACPI0007") Name(_UID, 526) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(527, PCNT))
    {
        Device(P527) { Name(_HID, "ACPI0007") Name(_UID, 527) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(528, PCNT))
    {
        Device(P528) { Name(_HID, "ACPI0007") Name(_UID, 528) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(529, PCNT))
    {
        Device(P529) { Name(_HID, "ACPI0007") Name(_UID, 529) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(530, PCNT))
    {
        Device(P530) { Name(_HID, "ACPI0007") Name(_UID, 530) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(531, PCNT))
    {
        Device(P531) { Name(_HID, "ACPI0007") Name(_UID, 531) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(532, PCNT))
    {
        Device(P532) { Name(_HID, "ACPI0007") Name(_UID, 532) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(533, PCNT))
    {
        Device(P533) { Name(_HID, "ACPI0007") Name(_UID, 533) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(534, PCNT))
    {
        Device(P534) { Name(_HID, "ACPI0007") Name(_UID, 534) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(535, PCNT))
    {
        Device(P535) { Name(_HID, "ACPI0007") Name(_UID, 535) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(536, PCNT))
    {
        Device(P536) { Name(_HID, "ACPI0007") Name(_UID, 536) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(537, PCNT))
    {
        Device(P537) { Name(_HID, "ACPI0007") Name(_UID, 537) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(538, PCNT))
    {
        Device(P538) { Name(_HID, "ACPI0007") Name(_UID, 538) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(539, PCNT))
    {
        Device(P539) { Name(_HID, "ACPI0007") Name(_UID, 539) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(540, PCNT))
    {
        Device(P540) { Name(_HID, "ACPI0007") Name(_UID, 540) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(541, PCNT))
    {
        Device(P541) { Name(_HID, "ACPI0007") Name(_UID, 541) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(542, PCNT))
    {
        Device(P542) { Name(_HID, "ACPI0007") Name(_UID, 542) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(543, PCNT))
    {
        Device(P543) { Name(_HID, "ACPI0007") Name(_UID, 543) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(544, PCNT))
    {
        Device(P544) { Name(_HID, "ACPI0007") Name(_UID, 544) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(545, PCNT))
    {
        Device(P545) { Name(_HID, "ACPI0007") Name(_UID, 545) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(546, PCNT))
    {
        Device(P546) { Name(_HID, "ACPI0007") Name(_UID, 546) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(547, PCNT))
    {
        Device(P547) { Name(_HID, "ACPI0007") Name(_UID, 547) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(548, PCNT))
    {
        Device(P548) { Name(_HID, "ACPI0007") Name(_UID, 548) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(549, PCNT))
    {
        Device(P549) { Name(_HID, "ACPI0007") Name(_UID, 549) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(550, PCNT))
    {
        Device(P550) { Name(_HID, "ACPI0007") Name(_UID, 550) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(551, PCNT))
    {
        Device(P551) { Name(_HID, "ACPI0007") Name(_UID, 551) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(552, PCNT))
    {
        Device(P552) { Name(_HID, "ACPI0007") Name(_UID, 552) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(553, PCNT))
    {
        Device(P553) { Name(_HID, "ACPI0007") Name(_UID, 553) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(554, PCNT))
    {
        Device(P554) { Name(_HID, "ACPI0007") Name(_UID, 554) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(555, PCNT))
    {
        Device(P555) { Name(_HID, "ACPI0007") Name(_UID, 555) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(556, PCNT))
    {
        Device(P556) { Name(_HID, "ACPI0007") Name(_UID, 556) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(557, PCNT))
    {
        Device(P557) { Name(_HID, "ACPI0007") Name(_UID, 557) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(558, PCNT))
    {
        Device(P558) { Name(_HID, "ACPI0007") Name(_UID, 558) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(559, PCNT))
    {
        Device(P559) { Name(_HID, "ACPI0007") Name(_UID, 559) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(560, PCNT))
    {
        Device(P560) { Name(_HID, "ACPI0007") Name(_UID, 560) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(561, PCNT))
    {
        Device(P561) { Name(_HID, "ACPI0007") Name(_UID, 561) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(562, PCNT))
    {
        Device(P562) { Name(_HID, "ACPI0007") Name(_UID, 562) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(563, PCNT))
    {
        Device(P563) { Name(_HID, "ACPI0007") Name(_UID, 563) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(564, PCNT))
    {
        Device(P564) { Name(_HID, "ACPI0007") Name(_UID, 564) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(565, PCNT))
    {
        Device(P565) { Name(_HID, "ACPI0007") Name(_UID, 565) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(566, PCNT))
    {
        Device(P566) { Name(_HID, "ACPI0007") Name(_UID, 566) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(567, PCNT))
    {
        Device(P567) { Name(_HID, "ACPI0007") Name(_UID, 567) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(568, PCNT))
    {
        Device(P568) { Name(_HID, "ACPI0007") Name(_UID, 568) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(569, PCNT))
    {
        Device(P569) { Name(_HID, "ACPI0007") Name(_UID, 569) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(570, PCNT))
    {
        Device(P570) { Name(_HID, "ACPI0007") Name(_UID, 570) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(571, PCNT))
    {
        Device(P571) { Name(_HID, "ACPI0007") Name(_UID, 571) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(572, PCNT))
    {
        Device(P572) { Name(_HID, "ACPI0007") Name(_UID, 572) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(573, PCNT))
    {
        Device(P573) { Name(_HID, "ACPI0007") Name(_UID, 573) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(574, PCNT))
    {
        Device(P574) { Name(_HID, "ACPI0007") Name(_UID, 574) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(575, PCNT))
    {
        Device(P575) { Name(_HID, "ACPI0007") Name(_UID, 575) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(576, PCNT))
    {
        Device(P576) { Name(_HID, "ACPI0007") Name(_UID, 576) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(577, PCNT))
    {
        Device(P577) { Name(_HID, "ACPI0007") Name(_UID, 577) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(578, PCNT))
    {
        Device(P578) { Name(_HID, "ACPI0007") Name(_UID, 578) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(579, PCNT))
    {
        Device(P579) { Name(_HID, "ACPI0007") Name(_UID, 579) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(580, PCNT))
    {
        Device(P580) { Name(_HID, "ACPI0007") Name(_UID, 580) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(581, PCNT))
    {
        Device(P581) { Name(_HID, "ACPI0007") Name(_UID, 581) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(582, PCNT))
    {
        Device(P582) { Name(_HID, "ACPI0007") Name(_UID, 582) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(583, PCNT))
    {
        Device(P583) { Name(_HID, "ACPI0007") Name(_UID, 583) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(584, PCNT))
    {
        Device(P584) { Name(_HID, "ACPI0007") Name(_UID, 584) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(585, PCNT))
    {
        Device(P585) { Name(_HID, "ACPI0007") Name(_UID, 585) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(586, PCNT))
    {
        Device(P586) { Name(_HID, "ACPI0007") Name(_UID, 586) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(587, PCNT))
    {
        Device(P587) { Name(_HID, "ACPI0007") Name(_UID, 587) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(588, PCNT))
    {
        Device(P588) { Name(_HID, "ACPI0007") Name(_UID, 588) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(589, PCNT))
    {
        Device(P589) { Name(_HID, "ACPI0007") Name(_UID, 589) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(590, PCNT))
    {
        Device(P590) { Name(_HID, "ACPI0007") Name(_UID, 590) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(591, PCNT))
    {
        Device(P591) { Name(_HID, "ACPI0007") Name(_UID, 591) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(592, PCNT))
    {
        Device(P592) { Name(_HID, "ACPI0007") Name(_UID, 592) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(593, PCNT))
    {
        Device(P593) { Name(_HID, "ACPI0007") Name(_UID, 593) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(594, PCNT))
    {
        Device(P594) { Name(_HID, "ACPI0007") Name(_UID, 594) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(595, PCNT))
    {
        Device(P595) { Name(_HID, "ACPI0007") Name(_UID, 595) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(596, PCNT))
    {
        Device(P596) { Name(_HID, "ACPI0007") Name(_UID, 596) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(597, PCNT))
    {
        Device(P597) { Name(_HID, "ACPI0007") Name(_UID, 597) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(598, PCNT))
    {
        Device(P598) { Name(_HID, "ACPI0007") Name(_UID, 598) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(599, PCNT))
    {
        Device(P599) { Name(_HID, "ACPI0007") Name(_UID, 599) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(600, PCNT))
    {
        Device(P600) { Name(_HID, "ACPI0007") Name(_UID, 600) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(601, PCNT))
    {
        Device(P601) { Name(_HID, "ACPI0007") Name(_UID, 601) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(602, PCNT))
    {
        Device(P602) { Name(_HID, "ACPI0007") Name(_UID, 602) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(603, PCNT))
    {
        Device(P603) { Name(_HID, "ACPI0007") Name(_UID, 603) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(604, PCNT))
    {
        Device(P604) { Name(_HID, "ACPI0007") Name(_UID, 604) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(605, PCNT))
    {
        Device(P605) { Name(_HID, "ACPI0007") Name(_UID, 605) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(606, PCNT))
    {
        Device(P606) { Name(_HID, "ACPI0007") Name(_UID, 606) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(607, PCNT))
    {
        Device(P607) { Name(_HID, "ACPI0007") Name(_UID, 607) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(608, PCNT))
    {
        Device(P608) { Name(_HID, "ACPI0007") Name(_UID, 608) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(609, PCNT))
    {
        Device(P609) { Name(_HID, "ACPI0007") Name(_UID, 609) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(610, PCNT))
    {
        Device(P610) { Name(_HID, "ACPI0007") Name(_UID, 610) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(611, PCNT))
    {
        Device(P611) { Name(_HID, "ACPI0007") Name(_UID, 611) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(612, PCNT))
    {
        Device(P612) { Name(_HID, "ACPI0007") Name(_UID, 612) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(613, PCNT))
    {
        Device(P613) { Name(_HID, "ACPI0007") Name(_UID, 613) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(614, PCNT))
    {
        Device(P614) { Name(_HID, "ACPI0007") Name(_UID, 614) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(615, PCNT))
    {
        Device(P615) { Name(_HID, "ACPI0007") Name(_UID, 615) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(616, PCNT))
    {
        Device(P616) { Name(_HID, "ACPI0007") Name(_UID, 616) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(617, PCNT))
    {
        Device(P617) { Name(_HID, "ACPI0007") Name(_UID, 617) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(618, PCNT))
    {
        Device(P618) { Name(_HID, "ACPI0007") Name(_UID, 618) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(619, PCNT))
    {
        Device(P619) { Name(_HID, "ACPI0007") Name(_UID, 619) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(620, PCNT))
    {
        Device(P620) { Name(_HID, "ACPI0007") Name(_UID, 620) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(621, PCNT))
    {
        Device(P621) { Name(_HID, "ACPI0007") Name(_UID, 621) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(622, PCNT))
    {
        Device(P622) { Name(_HID, "ACPI0007") Name(_UID, 622) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(623, PCNT))
    {
        Device(P623) { Name(_HID, "ACPI0007") Name(_UID, 623) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(624, PCNT))
    {
        Device(P624) { Name(_HID, "ACPI0007") Name(_UID, 624) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(625, PCNT))
    {
        Device(P625) { Name(_HID, "ACPI0007") Name(_UID, 625) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(626, PCNT))
    {
        Device(P626) { Name(_HID, "ACPI0007") Name(_UID, 626) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(627, PCNT))
    {
        Device(P627) { Name(_HID, "ACPI0007") Name(_UID, 627) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(628, PCNT))
    {
        Device(P628) { Name(_HID, "ACPI0007") Name(_UID, 628) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(629, PCNT))
    {
        Device(P629) { Name(_HID, "ACPI0007") Name(_UID, 629) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(630, PCNT))
    {
        Device(P630) { Name(_HID, "ACPI0007") Name(_UID, 630) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(631, PCNT))
    {
        Device(P631) { Name(_HID, "ACPI0007") Name(_UID, 631) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(632, PCNT))
    {
        Device(P632) { Name(_HID, "ACPI0007") Name(_UID, 632) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(633, PCNT))
    {
        Device(P633) { Name(_HID, "ACPI0007") Name(_UID, 633) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(634, PCNT))
    {
        Device(P634) { Name(_HID, "ACPI0007") Name(_UID, 634) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(635, PCNT))
    {
        Device(P635) { Name(_HID, "ACPI0007") Name(_UID, 635) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(636, PCNT))
    {
        Device(P636) { Name(_HID, "ACPI0007") Name(_UID, 636) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(637, PCNT))
    {
        Device(P637) { Name(_HID, "ACPI0007") Name(_UID, 637) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(638, PCNT))
    {
        Device(P638) { Name(_HID, "ACPI0007") Name(_UID, 638) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(639, PCNT))
    {
        Device(P639) { Name(_HID, "ACPI0007") Name(_UID, 639) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(640, PCNT))
    {
        Device(P640) { Name(_HID, "ACPI0007") Name(_UID, 640) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(641, PCNT))
    {
        Device(P641) { Name(_HID, "ACPI0007") Name(_UID, 641) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(642, PCNT))
    {
        Device(P642) { Name(_HID, "ACPI0007") Name(_UID, 642) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(643, PCNT))
    {
        Device(P643) { Name(_HID, "ACPI0007") Name(_UID, 643) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(644, PCNT))
    {
        Device(P644) { Name(_HID, "ACPI0007") Name(_UID, 644) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(645, PCNT))
    {
        Device(P645) { Name(_HID, "ACPI0007") Name(_UID, 645) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(646, PCNT))
    {
        Device(P646) { Name(_HID, "ACPI0007") Name(_UID, 646) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(647, PCNT))
    {
        Device(P647) { Name(_HID, "ACPI0007") Name(_UID, 647) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(648, PCNT))
    {
        Device(P648) { Name(_HID, "ACPI0007") Name(_UID, 648) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(649, PCNT))
    {
        Device(P649) { Name(_HID, "ACPI0007") Name(_UID, 649) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(650, PCNT))
    {
        Device(P650) { Name(_HID, "ACPI0007") Name(_UID, 650) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(651, PCNT))
    {
        Device(P651) { Name(_HID, "ACPI0007") Name(_UID, 651) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(652, PCNT))
    {
        Device(P652) { Name(_HID, "ACPI0007") Name(_UID, 652) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(653, PCNT))
    {
        Device(P653) { Name(_HID, "ACPI0007") Name(_UID, 653) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(654, PCNT))
    {
        Device(P654) { Name(_HID, "ACPI0007") Name(_UID, 654) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(655, PCNT))
    {
        Device(P655) { Name(_HID, "ACPI0007") Name(_UID, 655) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(656, PCNT))
    {
        Device(P656) { Name(_HID, "ACPI0007") Name(_UID, 656) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(657, PCNT))
    {
        Device(P657) { Name(_HID, "ACPI0007") Name(_UID, 657) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(658, PCNT))
    {
        Device(P658) { Name(_HID, "ACPI0007") Name(_UID, 658) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(659, PCNT))
    {
        Device(P659) { Name(_HID, "ACPI0007") Name(_UID, 659) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(660, PCNT))
    {
        Device(P660) { Name(_HID, "ACPI0007") Name(_UID, 660) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(661, PCNT))
    {
        Device(P661) { Name(_HID, "ACPI0007") Name(_UID, 661) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(662, PCNT))
    {
        Device(P662) { Name(_HID, "ACPI0007") Name(_UID, 662) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(663, PCNT))
    {
        Device(P663) { Name(_HID, "ACPI0007") Name(_UID, 663) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(664, PCNT))
    {
        Device(P664) { Name(_HID, "ACPI0007") Name(_UID, 664) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(665, PCNT))
    {
        Device(P665) { Name(_HID, "ACPI0007") Name(_UID, 665) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(666, PCNT))
    {
        Device(P666) { Name(_HID, "ACPI0007") Name(_UID, 666) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(667, PCNT))
    {
        Device(P667) { Name(_HID, "ACPI0007") Name(_UID, 667) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(668, PCNT))
    {
        Device(P668) { Name(_HID, "ACPI0007") Name(_UID, 668) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(669, PCNT))
    {
        Device(P669) { Name(_HID, "ACPI0007") Name(_UID, 669) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(670, PCNT))
    {
        Device(P670) { Name(_HID, "ACPI0007") Name(_UID, 670) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(671, PCNT))
    {
        Device(P671) { Name(_HID, "ACPI0007") Name(_UID, 671) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(672, PCNT))
    {
        Device(P672) { Name(_HID, "ACPI0007") Name(_UID, 672) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(673, PCNT))
    {
        Device(P673) { Name(_HID, "ACPI0007") Name(_UID, 673) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(674, PCNT))
    {
        Device(P674) { Name(_HID, "ACPI0007") Name(_UID, 674) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(675, PCNT))
    {
        Device(P675) { Name(_HID, "ACPI0007") Name(_UID, 675) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(676, PCNT))
    {
        Device(P676) { Name(_HID, "ACPI0007") Name(_UID, 676) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(677, PCNT))
    {
        Device(P677) { Name(_HID, "ACPI0007") Name(_UID, 677) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(678, PCNT))
    {
        Device(P678) { Name(_HID, "ACPI0007") Name(_UID, 678) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(679, PCNT))
    {
        Device(P679) { Name(_HID, "ACPI0007") Name(_UID, 679) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(680, PCNT))
    {
        Device(P680) { Name(_HID, "ACPI0007") Name(_UID, 680) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(681, PCNT))
    {
        Device(P681) { Name(_HID, "ACPI0007") Name(_UID, 681) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(682, PCNT))
    {
        Device(P682) { Name(_HID, "ACPI0007") Name(_UID, 682) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(683, PCNT))
    {
        Device(P683) { Name(_HID, "ACPI0007") Name(_UID, 683) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(684, PCNT))
    {
        Device(P684) { Name(_HID, "ACPI0007") Name(_UID, 684) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(685, PCNT))
    {
        Device(P685) { Name(_HID, "ACPI0007") Name(_UID, 685) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(686, PCNT))
    {
        Device(P686) { Name(_HID, "ACPI0007") Name(_UID, 686) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(687, PCNT))
    {
        Device(P687) { Name(_HID, "ACPI0007") Name(_UID, 687) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(688, PCNT))
    {
        Device(P688) { Name(_HID, "ACPI0007") Name(_UID, 688) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(689, PCNT))
    {
        Device(P689) { Name(_HID, "ACPI0007") Name(_UID, 689) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(690, PCNT))
    {
        Device(P690) { Name(_HID, "ACPI0007") Name(_UID, 690) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(691, PCNT))
    {
        Device(P691) { Name(_HID, "ACPI0007") Name(_UID, 691) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(692, PCNT))
    {
        Device(P692) { Name(_HID, "ACPI0007") Name(_UID, 692) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(693, PCNT))
    {
        Device(P693) { Name(_HID, "ACPI0007") Name(_UID, 693) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(694, PCNT))
    {
        Device(P694) { Name(_HID, "ACPI0007") Name(_UID, 694) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(695, PCNT))
    {
        Device(P695) { Name(_HID, "ACPI0007") Name(_UID, 695) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(696, PCNT))
    {
        Device(P696) { Name(_HID, "ACPI0007") Name(_UID, 696) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(697, PCNT))
    {
        Device(P697) { Name(_HID, "ACPI0007") Name(_UID, 697) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(698, PCNT))
    {
        Device(P698) { Name(_HID, "ACPI0007") Name(_UID, 698) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(699, PCNT))
    {
        Device(P699) { Name(_HID, "ACPI0007") Name(_UID, 699) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(700, PCNT))
    {
        Device(P700) { Name(_HID, "ACPI0007") Name(_UID, 700) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(701, PCNT))
    {
        Device(P701) { Name(_HID, "ACPI0007") Name(_UID, 701) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(702, PCNT))
    {
        Device(P702) { Name(_HID, "ACPI0007") Name(_UID, 702) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(703, PCNT))
    {
        Device(P703) { Name(_HID, "ACPI0007") Name(_UID, 703) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(704, PCNT))
    {
        Device(P704) { Name(_HID, "ACPI0007") Name(_UID, 704) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(705, PCNT))
    {
        Device(P705) { Name(_HID, "ACPI0007") Name(_UID, 705) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(706, PCNT))
    {
        Device(P706) { Name(_HID, "ACPI0007") Name(_UID, 706) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(707, PCNT))
    {
        Device(P707) { Name(_HID, "ACPI0007") Name(_UID, 707) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(708, PCNT))
    {
        Device(P708) { Name(_HID, "ACPI0007") Name(_UID, 708) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(709, PCNT))
    {
        Device(P709) { Name(_HID, "ACPI0007") Name(_UID, 709) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(710, PCNT))
    {
        Device(P710) { Name(_HID, "ACPI0007") Name(_UID, 710) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(711, PCNT))
    {
        Device(P711) { Name(_HID, "ACPI0007") Name(_UID, 711) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(712, PCNT))
    {
        Device(P712) { Name(_HID, "ACPI0007") Name(_UID, 712) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(713, PCNT))
    {
        Device(P713) { Name(_HID, "ACPI0007") Name(_UID, 713) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(714, PCNT))
    {
        Device(P714) { Name(_HID, "ACPI0007") Name(_UID, 714) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(715, PCNT))
    {
        Device(P715) { Name(_HID, "ACPI0007") Name(_UID, 715) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(716, PCNT))
    {
        Device(P716) { Name(_HID, "ACPI0007") Name(_UID, 716) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(717, PCNT))
    {
        Device(P717) { Name(_HID, "ACPI0007") Name(_UID, 717) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(718, PCNT))
    {
        Device(P718) { Name(_HID, "ACPI0007") Name(_UID, 718) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(719, PCNT))
    {
        Device(P719) { Name(_HID, "ACPI0007") Name(_UID, 719) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(720, PCNT))
    {
        Device(P720) { Name(_HID, "ACPI0007") Name(_UID, 720) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(721, PCNT))
    {
        Device(P721) { Name(_HID, "ACPI0007") Name(_UID, 721) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(722, PCNT))
    {
        Device(P722) { Name(_HID, "ACPI0007") Name(_UID, 722) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(723, PCNT))
    {
        Device(P723) { Name(_HID, "ACPI0007") Name(_UID, 723) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(724, PCNT))
    {
        Device(P724) { Name(_HID, "ACPI0007") Name(_UID, 724) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(725, PCNT))
    {
        Device(P725) { Name(_HID, "ACPI0007") Name(_UID, 725) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(726, PCNT))
    {
        Device(P726) { Name(_HID, "ACPI0007") Name(_UID, 726) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(727, PCNT))
    {
        Device(P727) { Name(_HID, "ACPI0007") Name(_UID, 727) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(728, PCNT))
    {
        Device(P728) { Name(_HID, "ACPI0007") Name(_UID, 728) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(729, PCNT))
    {
        Device(P729) { Name(_HID, "ACPI0007") Name(_UID, 729) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(730, PCNT))
    {
        Device(P730) { Name(_HID, "ACPI0007") Name(_UID, 730) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(731, PCNT))
    {
        Device(P731) { Name(_HID, "ACPI0007") Name(_UID, 731) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(732, PCNT))
    {
        Device(P732) { Name(_HID, "ACPI0007") Name(_UID, 732) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(733, PCNT))
    {
        Device(P733) { Name(_HID, "ACPI0007") Name(_UID, 733) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(734, PCNT))
    {
        Device(P734) { Name(_HID, "ACPI0007") Name(_UID, 734) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(735, PCNT))
    {
        Device(P735) { Name(_HID, "ACPI0007") Name(_UID, 735) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(736, PCNT))
    {
        Device(P736) { Name(_HID, "ACPI0007") Name(_UID, 736) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(737, PCNT))
    {
        Device(P737) { Name(_HID, "ACPI0007") Name(_UID, 737) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(738, PCNT))
    {
        Device(P738) { Name(_HID, "ACPI0007") Name(_UID, 738) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(739, PCNT))
    {
        Device(P739) { Name(_HID, "ACPI0007") Name(_UID, 739) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(740, PCNT))
    {
        Device(P740) { Name(_HID, "ACPI0007") Name(_UID, 740) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(741, PCNT))
    {
        Device(P741) { Name(_HID, "ACPI0007") Name(_UID, 741) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(742, PCNT))
    {
        Device(P742) { Name(_HID, "ACPI0007") Name(_UID, 742) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(743, PCNT))
    {
        Device(P743) { Name(_HID, "ACPI0007") Name(_UID, 743) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(744, PCNT))
    {
        Device(P744) { Name(_HID, "ACPI0007") Name(_UID, 744) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(745, PCNT))
    {
        Device(P745) { Name(_HID, "ACPI0007") Name(_UID, 745) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(746, PCNT))
    {
        Device(P746) { Name(_HID, "ACPI0007") Name(_UID, 746) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(747, PCNT))
    {
        Device(P747) { Name(_HID, "ACPI0007") Name(_UID, 747) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(748, PCNT))
    {
        Device(P748) { Name(_HID, "ACPI0007") Name(_UID, 748) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(749, PCNT))
    {
        Device(P749) { Name(_HID, "ACPI0007") Name(_UID, 749) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(750, PCNT))
    {
        Device(P750) { Name(_HID, "ACPI0007") Name(_UID, 750) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(751, PCNT))
    {
        Device(P751) { Name(_HID, "ACPI0007") Name(_UID, 751) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(752, PCNT))
    {
        Device(P752) { Name(_HID, "ACPI0007") Name(_UID, 752) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(753, PCNT))
    {
        Device(P753) { Name(_HID, "ACPI0007") Name(_UID, 753) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(754, PCNT))
    {
        Device(P754) { Name(_HID, "ACPI0007") Name(_UID, 754) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(755, PCNT))
    {
        Device(P755) { Name(_HID, "ACPI0007") Name(_UID, 755) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(756, PCNT))
    {
        Device(P756) { Name(_HID, "ACPI0007") Name(_UID, 756) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(757, PCNT))
    {
        Device(P757) { Name(_HID, "ACPI0007") Name(_UID, 757) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(758, PCNT))
    {
        Device(P758) { Name(_HID, "ACPI0007") Name(_UID, 758) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(759, PCNT))
    {
        Device(P759) { Name(_HID, "ACPI0007") Name(_UID, 759) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(760, PCNT))
    {
        Device(P760) { Name(_HID, "ACPI0007") Name(_UID, 760) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(761, PCNT))
    {
        Device(P761) { Name(_HID, "ACPI0007") Name(_UID, 761) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(762, PCNT))
    {
        Device(P762) { Name(_HID, "ACPI0007") Name(_UID, 762) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(763, PCNT))
    {
        Device(P763) { Name(_HID, "ACPI0007") Name(_UID, 763) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(764, PCNT))
    {
        Device(P764) { Name(_HID, "ACPI0007") Name(_UID, 764) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(765, PCNT))
    {
        Device(P765) { Name(_HID, "ACPI0007") Name(_UID, 765) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(766, PCNT))
    {
        Device(P766) { Name(_HID, "ACPI0007") Name(_UID, 766) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(767, PCNT))
    {
        Device(P767) { Name(_HID, "ACPI0007") Name(_UID, 767) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(768, PCNT))
    {
        Device(P768) { Name(_HID, "ACPI0007") Name(_UID, 768) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(769, PCNT))
    {
        Device(P769) { Name(_HID, "ACPI0007") Name(_UID, 769) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(770, PCNT))
    {
        Device(P770) { Name(_HID, "ACPI0007") Name(_UID, 770) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(771, PCNT))
    {
        Device(P771) { Name(_HID, "ACPI0007") Name(_UID, 771) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(772, PCNT))
    {
        Device(P772) { Name(_HID, "ACPI0007") Name(_UID, 772) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(773, PCNT))
    {
        Device(P773) { Name(_HID, "ACPI0007") Name(_UID, 773) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(774, PCNT))
    {
        Device(P774) { Name(_HID, "ACPI0007") Name(_UID, 774) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(775, PCNT))
    {
        Device(P775) { Name(_HID, "ACPI0007") Name(_UID, 775) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(776, PCNT))
    {
        Device(P776) { Name(_HID, "ACPI0007") Name(_UID, 776) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(777, PCNT))
    {
        Device(P777) { Name(_HID, "ACPI0007") Name(_UID, 777) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(778, PCNT))
    {
        Device(P778) { Name(_HID, "ACPI0007") Name(_UID, 778) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(779, PCNT))
    {
        Device(P779) { Name(_HID, "ACPI0007") Name(_UID, 779) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(780, PCNT))
    {
        Device(P780) { Name(_HID, "ACPI0007") Name(_UID, 780) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(781, PCNT))
    {
        Device(P781) { Name(_HID, "ACPI0007") Name(_UID, 781) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(782, PCNT))
    {
        Device(P782) { Name(_HID, "ACPI0007") Name(_UID, 782) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(783, PCNT))
    {
        Device(P783) { Name(_HID, "ACPI0007") Name(_UID, 783) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(784, PCNT))
    {
        Device(P784) { Name(_HID, "ACPI0007") Name(_UID, 784) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(785, PCNT))
    {
        Device(P785) { Name(_HID, "ACPI0007") Name(_UID, 785) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(786, PCNT))
    {
        Device(P786) { Name(_HID, "ACPI0007") Name(_UID, 786) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(787, PCNT))
    {
        Device(P787) { Name(_HID, "ACPI0007") Name(_UID, 787) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(788, PCNT))
    {
        Device(P788) { Name(_HID, "ACPI0007") Name(_UID, 788) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(789, PCNT))
    {
        Device(P789) { Name(_HID, "ACPI0007") Name(_UID, 789) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(790, PCNT))
    {
        Device(P790) { Name(_HID, "ACPI0007") Name(_UID, 790) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(791, PCNT))
    {
        Device(P791) { Name(_HID, "ACPI0007") Name(_UID, 791) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(792, PCNT))
    {
        Device(P792) { Name(_HID, "ACPI0007") Name(_UID, 792) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(793, PCNT))
    {
        Device(P793) { Name(_HID, "ACPI0007") Name(_UID, 793) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(794, PCNT))
    {
        Device(P794) { Name(_HID, "ACPI0007") Name(_UID, 794) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(795, PCNT))
    {
        Device(P795) { Name(_HID, "ACPI0007") Name(_UID, 795) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(796, PCNT))
    {
        Device(P796) { Name(_HID, "ACPI0007") Name(_UID, 796) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(797, PCNT))
    {
        Device(P797) { Name(_HID, "ACPI0007") Name(_UID, 797) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(798, PCNT))
    {
        Device(P798) { Name(_HID, "ACPI0007") Name(_UID, 798) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(799, PCNT))
    {
        Device(P799) { Name(_HID, "ACPI0007") Name(_UID, 799) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(800, PCNT))
    {
        Device(P800) { Name(_HID, "ACPI0007") Name(_UID, 800) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(801, PCNT))
    {
        Device(P801) { Name(_HID, "ACPI0007") Name(_UID, 801) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(802, PCNT))
    {
        Device(P802) { Name(_HID, "ACPI0007") Name(_UID, 802) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(803, PCNT))
    {
        Device(P803) { Name(_HID, "ACPI0007") Name(_UID, 803) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(804, PCNT))
    {
        Device(P804) { Name(_HID, "ACPI0007") Name(_UID, 804) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(805, PCNT))
    {
        Device(P805) { Name(_HID, "ACPI0007") Name(_UID, 805) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(806, PCNT))
    {
        Device(P806) { Name(_HID, "ACPI0007") Name(_UID, 806) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(807, PCNT))
    {
        Device(P807) { Name(_HID, "ACPI0007") Name(_UID, 807) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(808, PCNT))
    {
        Device(P808) { Name(_HID, "ACPI0007") Name(_UID, 808) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(809, PCNT))
    {
        Device(P809) { Name(_HID, "ACPI0007") Name(_UID, 809) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(810, PCNT))
    {
        Device(P810) { Name(_HID, "ACPI0007") Name(_UID, 810) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(811, PCNT))
    {
        Device(P811) { Name(_HID, "ACPI0007") Name(_UID, 811) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(812, PCNT))
    {
        Device(P812) { Name(_HID, "ACPI0007") Name(_UID, 812) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(813, PCNT))
    {
        Device(P813) { Name(_HID, "ACPI0007") Name(_UID, 813) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(814, PCNT))
    {
        Device(P814) { Name(_HID, "ACPI0007") Name(_UID, 814) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(815, PCNT))
    {
        Device(P815) { Name(_HID, "ACPI0007") Name(_UID, 815) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(816, PCNT))
    {
        Device(P816) { Name(_HID, "ACPI0007") Name(_UID, 816) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(817, PCNT))
    {
        Device(P817) { Name(_HID, "ACPI0007") Name(_UID, 817) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(818, PCNT))
    {
        Device(P818) { Name(_HID, "ACPI0007") Name(_UID, 818) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(819, PCNT))
    {
        Device(P819) { Name(_HID, "ACPI0007") Name(_UID, 819) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(820, PCNT))
    {
        Device(P820) { Name(_HID, "ACPI0007") Name(_UID, 820) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(821, PCNT))
    {
        Device(P821) { Name(_HID, "ACPI0007") Name(_UID, 821) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(822, PCNT))
    {
        Device(P822) { Name(_HID, "ACPI0007") Name(_UID, 822) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(823, PCNT))
    {
        Device(P823) { Name(_HID, "ACPI0007") Name(_UID, 823) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(824, PCNT))
    {
        Device(P824) { Name(_HID, "ACPI0007") Name(_UID, 824) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(825, PCNT))
    {
        Device(P825) { Name(_HID, "ACPI0007") Name(_UID, 825) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(826, PCNT))
    {
        Device(P826) { Name(_HID, "ACPI0007") Name(_UID, 826) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(827, PCNT))
    {
        Device(P827) { Name(_HID, "ACPI0007") Name(_UID, 827) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(828, PCNT))
    {
        Device(P828) { Name(_HID, "ACPI0007") Name(_UID, 828) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(829, PCNT))
    {
        Device(P829) { Name(_HID, "ACPI0007") Name(_UID, 829) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(830, PCNT))
    {
        Device(P830) { Name(_HID, "ACPI0007") Name(_UID, 830) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(831, PCNT))
    {
        Device(P831) { Name(_HID, "ACPI0007") Name(_UID, 831) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(832, PCNT))
    {
        Device(P832) { Name(_HID, "ACPI0007") Name(_UID, 832) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(833, PCNT))
    {
        Device(P833) { Name(_HID, "ACPI0007") Name(_UID, 833) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(834, PCNT))
    {
        Device(P834) { Name(_HID, "ACPI0007") Name(_UID, 834) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(835, PCNT))
    {
        Device(P835) { Name(_HID, "ACPI0007") Name(_UID, 835) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(836, PCNT))
    {
        Device(P836) { Name(_HID, "ACPI0007") Name(_UID, 836) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(837, PCNT))
    {
        Device(P837) { Name(_HID, "ACPI0007") Name(_UID, 837) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(838, PCNT))
    {
        Device(P838) { Name(_HID, "ACPI0007") Name(_UID, 838) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(839, PCNT))
    {
        Device(P839) { Name(_HID, "ACPI0007") Name(_UID, 839) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(840, PCNT))
    {
        Device(P840) { Name(_HID, "ACPI0007") Name(_UID, 840) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(841, PCNT))
    {
        Device(P841) { Name(_HID, "ACPI0007") Name(_UID, 841) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(842, PCNT))
    {
        Device(P842) { Name(_HID, "ACPI0007") Name(_UID, 842) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(843, PCNT))
    {
        Device(P843) { Name(_HID, "ACPI0007") Name(_UID, 843) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(844, PCNT))
    {
        Device(P844) { Name(_HID, "ACPI0007") Name(_UID, 844) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(845, PCNT))
    {
        Device(P845) { Name(_HID, "ACPI0007") Name(_UID, 845) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(846, PCNT))
    {
        Device(P846) { Name(_HID, "ACPI0007") Name(_UID, 846) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(847, PCNT))
    {
        Device(P847) { Name(_HID, "ACPI0007") Name(_UID, 847) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(848, PCNT))
    {
        Device(P848) { Name(_HID, "ACPI0007") Name(_UID, 848) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(849, PCNT))
    {
        Device(P849) { Name(_HID, "ACPI0007") Name(_UID, 849) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(850, PCNT))
    {
        Device(P850) { Name(_HID, "ACPI0007") Name(_UID, 850) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(851, PCNT))
    {
        Device(P851) { Name(_HID, "ACPI0007") Name(_UID, 851) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(852, PCNT))
    {
        Device(P852) { Name(_HID, "ACPI0007") Name(_UID, 852) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(853, PCNT))
    {
        Device(P853) { Name(_HID, "ACPI0007") Name(_UID, 853) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(854, PCNT))
    {
        Device(P854) { Name(_HID, "ACPI0007") Name(_UID, 854) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(855, PCNT))
    {
        Device(P855) { Name(_HID, "ACPI0007") Name(_UID, 855) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(856, PCNT))
    {
        Device(P856) { Name(_HID, "ACPI0007") Name(_UID, 856) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(857, PCNT))
    {
        Device(P857) { Name(_HID, "ACPI0007") Name(_UID, 857) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(858, PCNT))
    {
        Device(P858) { Name(_HID, "ACPI0007") Name(_UID, 858) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(859, PCNT))
    {
        Device(P859) { Name(_HID, "ACPI0007") Name(_UID, 859) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(860, PCNT))
    {
        Device(P860) { Name(_HID, "ACPI0007") Name(_UID, 860) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(861, PCNT))
    {
        Device(P861) { Name(_HID, "ACPI0007") Name(_UID, 861) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(862, PCNT))
    {
        Device(P862) { Name(_HID, "ACPI0007") Name(_UID, 862) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(863, PCNT))
    {
        Device(P863) { Name(_HID, "ACPI0007") Name(_UID, 863) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(864, PCNT))
    {
        Device(P864) { Name(_HID, "ACPI0007") Name(_UID, 864) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(865, PCNT))
    {
        Device(P865) { Name(_HID, "ACPI0007") Name(_UID, 865) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(866, PCNT))
    {
        Device(P866) { Name(_HID, "ACPI0007") Name(_UID, 866) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(867, PCNT))
    {
        Device(P867) { Name(_HID, "ACPI0007") Name(_UID, 867) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(868, PCNT))
    {
        Device(P868) { Name(_HID, "ACPI0007") Name(_UID, 868) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(869, PCNT))
    {
        Device(P869) { Name(_HID, "ACPI0007") Name(_UID, 869) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(870, PCNT))
    {
        Device(P870) { Name(_HID, "ACPI0007") Name(_UID, 870) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(871, PCNT))
    {
        Device(P871) { Name(_HID, "ACPI0007") Name(_UID, 871) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(872, PCNT))
    {
        Device(P872) { Name(_HID, "ACPI0007") Name(_UID, 872) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(873, PCNT))
    {
        Device(P873) { Name(_HID, "ACPI0007") Name(_UID, 873) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(874, PCNT))
    {
        Device(P874) { Name(_HID, "ACPI0007") Name(_UID, 874) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(875, PCNT))
    {
        Device(P875) { Name(_HID, "ACPI0007") Name(_UID, 875) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(876, PCNT))
    {
        Device(P876) { Name(_HID, "ACPI0007") Name(_UID, 876) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(877, PCNT))
    {
        Device(P877) { Name(_HID, "ACPI0007") Name(_UID, 877) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(878, PCNT))
    {
        Device(P878) { Name(_HID, "ACPI0007") Name(_UID, 878) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(879, PCNT))
    {
        Device(P879) { Name(_HID, "ACPI0007") Name(_UID, 879) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(880, PCNT))
    {
        Device(P880) { Name(_HID, "ACPI0007") Name(_UID, 880) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(881, PCNT))
    {
        Device(P881) { Name(_HID, "ACPI0007") Name(_UID, 881) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(882, PCNT))
    {
        Device(P882) { Name(_HID, "ACPI0007") Name(_UID, 882) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(883, PCNT))
    {
        Device(P883) { Name(_HID, "ACPI0007") Name(_UID, 883) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(884, PCNT))
    {
        Device(P884) { Name(_HID, "ACPI0007") Name(_UID, 884) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(885, PCNT))
    {
        Device(P885) { Name(_HID, "ACPI0007") Name(_UID, 885) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(886, PCNT))
    {
        Device(P886) { Name(_HID, "ACPI0007") Name(_UID, 886) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(887, PCNT))
    {
        Device(P887) { Name(_HID, "ACPI0007") Name(_UID, 887) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(888, PCNT))
    {
        Device(P888) { Name(_HID, "ACPI0007") Name(_UID, 888) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(889, PCNT))
    {
        Device(P889) { Name(_HID, "ACPI0007") Name(_UID, 889) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(890, PCNT))
    {
        Device(P890) { Name(_HID, "ACPI0007") Name(_UID, 890) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(891, PCNT))
    {
        Device(P891) { Name(_HID, "ACPI0007") Name(_UID, 891) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(892, PCNT))
    {
        Device(P892) { Name(_HID, "ACPI0007") Name(_UID, 892) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(893, PCNT))
    {
        Device(P893) { Name(_HID, "ACPI0007") Name(_UID, 893) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(894, PCNT))
    {
        Device(P894) { Name(_HID, "ACPI0007") Name(_UID, 894) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(895, PCNT))
    {
        Device(P895) { Name(_HID, "ACPI0007") Name(_UID, 895) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(896, PCNT))
    {
        Device(P896) { Name(_HID, "ACPI0007") Name(_UID, 896) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(897, PCNT))
    {
        Device(P897) { Name(_HID, "ACPI0007") Name(_UID, 897) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(898, PCNT))
    {
        Device(P898) { Name(_HID, "ACPI0007") Name(_UID, 898) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(899, PCNT))
    {
        Device(P899) { Name(_HID, "ACPI0007") Name(_UID, 899) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(900, PCNT))
    {
        Device(P900) { Name(_HID, "ACPI0007") Name(_UID, 900) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(901, PCNT))
    {
        Device(P901) { Name(_HID, "ACPI0007") Name(_UID, 901) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(902, PCNT))
    {
        Device(P902) { Name(_HID, "ACPI0007") Name(_UID, 902) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(903, PCNT))
    {
        Device(P903) { Name(_HID, "ACPI0007") Name(_UID, 903) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(904, PCNT))
    {
        Device(P904) { Name(_HID, "ACPI0007") Name(_UID, 904) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(905, PCNT))
    {
        Device(P905) { Name(_HID, "ACPI0007") Name(_UID, 905) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(906, PCNT))
    {
        Device(P906) { Name(_HID, "ACPI0007") Name(_UID, 906) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(907, PCNT))
    {
        Device(P907) { Name(_HID, "ACPI0007") Name(_UID, 907) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(908, PCNT))
    {
        Device(P908) { Name(_HID, "ACPI0007") Name(_UID, 908) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(909, PCNT))
    {
        Device(P909) { Name(_HID, "ACPI0007") Name(_UID, 909) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(910, PCNT))
    {
        Device(P910) { Name(_HID, "ACPI0007") Name(_UID, 910) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(911, PCNT))
    {
        Device(P911) { Name(_HID, "ACPI0007") Name(_UID, 911) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(912, PCNT))
    {
        Device(P912) { Name(_HID, "ACPI0007") Name(_UID, 912) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(913, PCNT))
    {
        Device(P913) { Name(_HID, "ACPI0007") Name(_UID, 913) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(914, PCNT))
    {
        Device(P914) { Name(_HID, "ACPI0007") Name(_UID, 914) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(915, PCNT))
    {
        Device(P915) { Name(_HID, "ACPI0007") Name(_UID, 915) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(916, PCNT))
    {
        Device(P916) { Name(_HID, "ACPI0007") Name(_UID, 916) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(917, PCNT))
    {
        Device(P917) { Name(_HID, "ACPI0007") Name(_UID, 917) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(918, PCNT))
    {
        Device(P918) { Name(_HID, "ACPI0007") Name(_UID, 918) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(919, PCNT))
    {
        Device(P919) { Name(_HID, "ACPI0007") Name(_UID, 919) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(920, PCNT))
    {
        Device(P920) { Name(_HID, "ACPI0007") Name(_UID, 920) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(921, PCNT))
    {
        Device(P921) { Name(_HID, "ACPI0007") Name(_UID, 921) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(922, PCNT))
    {
        Device(P922) { Name(_HID, "ACPI0007") Name(_UID, 922) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(923, PCNT))
    {
        Device(P923) { Name(_HID, "ACPI0007") Name(_UID, 923) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(924, PCNT))
    {
        Device(P924) { Name(_HID, "ACPI0007") Name(_UID, 924) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(925, PCNT))
    {
        Device(P925) { Name(_HID, "ACPI0007") Name(_UID, 925) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(926, PCNT))
    {
        Device(P926) { Name(_HID, "ACPI0007") Name(_UID, 926) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(927, PCNT))
    {
        Device(P927) { Name(_HID, "ACPI0007") Name(_UID, 927) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(928, PCNT))
    {
        Device(P928) { Name(_HID, "ACPI0007") Name(_UID, 928) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(929, PCNT))
    {
        Device(P929) { Name(_HID, "ACPI0007") Name(_UID, 929) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(930, PCNT))
    {
        Device(P930) { Name(_HID, "ACPI0007") Name(_UID, 930) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(931, PCNT))
    {
        Device(P931) { Name(_HID, "ACPI0007") Name(_UID, 931) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(932, PCNT))
    {
        Device(P932) { Name(_HID, "ACPI0007") Name(_UID, 932) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(933, PCNT))
    {
        Device(P933) { Name(_HID, "ACPI0007") Name(_UID, 933) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(934, PCNT))
    {
        Device(P934) { Name(_HID, "ACPI0007") Name(_UID, 934) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(935, PCNT))
    {
        Device(P935) { Name(_HID, "ACPI0007") Name(_UID, 935) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(936, PCNT))
    {
        Device(P936) { Name(_HID, "ACPI0007") Name(_UID, 936) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(937, PCNT))
    {
        Device(P937) { Name(_HID, "ACPI0007") Name(_UID, 937) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(938, PCNT))
    {
        Device(P938) { Name(_HID, "ACPI0007") Name(_UID, 938) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(939, PCNT))
    {
        Device(P939) { Name(_HID, "ACPI0007") Name(_UID, 939) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(940, PCNT))
    {
        Device(P940) { Name(_HID, "ACPI0007") Name(_UID, 940) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(941, PCNT))
    {
        Device(P941) { Name(_HID, "ACPI0007") Name(_UID, 941) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(942, PCNT))
    {
        Device(P942) { Name(_HID, "ACPI0007") Name(_UID, 942) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(943, PCNT))
    {
        Device(P943) { Name(_HID, "ACPI0007") Name(_UID, 943) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(944, PCNT))
    {
        Device(P944) { Name(_HID, "ACPI0007") Name(_UID, 944) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(945, PCNT))
    {
        Device(P945) { Name(_HID, "ACPI0007") Name(_UID, 945) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(946, PCNT))
    {
        Device(P946) { Name(_HID, "ACPI0007") Name(_UID, 946) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(947, PCNT))
    {
        Device(P947) { Name(_HID, "ACPI0007") Name(_UID, 947) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(948, PCNT))
    {
        Device(P948) { Name(_HID, "ACPI0007") Name(_UID, 948) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(949, PCNT))
    {
        Device(P949) { Name(_HID, "ACPI0007") Name(_UID, 949) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(950, PCNT))
    {
        Device(P950) { Name(_HID, "ACPI0007") Name(_UID, 950) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(951, PCNT))
    {
        Device(P951) { Name(_HID, "ACPI0007") Name(_UID, 951) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(952, PCNT))
    {
        Device(P952) { Name(_HID, "ACPI0007") Name(_UID, 952) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(953, PCNT))
    {
        Device(P953) { Name(_HID, "ACPI0007") Name(_UID, 953) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(954, PCNT))
    {
        Device(P954) { Name(_HID, "ACPI0007") Name(_UID, 954) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(955, PCNT))
    {
        Device(P955) { Name(_HID, "ACPI0007") Name(_UID, 955) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(956, PCNT))
    {
        Device(P956) { Name(_HID, "ACPI0007") Name(_UID, 956) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(957, PCNT))
    {
        Device(P957) { Name(_HID, "ACPI0007") Name(_UID, 957) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(958, PCNT))
    {
        Device(P958) { Name(_HID, "ACPI0007") Name(_UID, 958) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(959, PCNT))
    {
        Device(P959) { Name(_HID, "ACPI0007") Name(_UID, 959) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(960, PCNT))
    {
        Device(P960) { Name(_HID, "ACPI0007") Name(_UID, 960) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(961, PCNT))
    {
        Device(P961) { Name(_HID, "ACPI0007") Name(_UID, 961) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(962, PCNT))
    {
        Device(P962) { Name(_HID, "ACPI0007") Name(_UID, 962) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(963, PCNT))
    {
        Device(P963) { Name(_HID, "ACPI0007") Name(_UID, 963) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(964, PCNT))
    {
        Device(P964) { Name(_HID, "ACPI0007") Name(_UID, 964) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(965, PCNT))
    {
        Device(P965) { Name(_HID, "ACPI0007") Name(_UID, 965) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(966, PCNT))
    {
        Device(P966) { Name(_HID, "ACPI0007") Name(_UID, 966) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(967, PCNT))
    {
        Device(P967) { Name(_HID, "ACPI0007") Name(_UID, 967) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(968, PCNT))
    {
        Device(P968) { Name(_HID, "ACPI0007") Name(_UID, 968) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(969, PCNT))
    {
        Device(P969) { Name(_HID, "ACPI0007") Name(_UID, 969) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(970, PCNT))
    {
        Device(P970) { Name(_HID, "ACPI0007") Name(_UID, 970) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(971, PCNT))
    {
        Device(P971) { Name(_HID, "ACPI0007") Name(_UID, 971) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(972, PCNT))
    {
        Device(P972) { Name(_HID, "ACPI0007") Name(_UID, 972) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(973, PCNT))
    {
        Device(P973) { Name(_HID, "ACPI0007") Name(_UID, 973) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(974, PCNT))
    {
        Device(P974) { Name(_HID, "ACPI0007") Name(_UID, 974) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(975, PCNT))
    {
        Device(P975) { Name(_HID, "ACPI0007") Name(_UID, 975) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(976, PCNT))
    {
        Device(P976) { Name(_HID, "ACPI0007") Name(_UID, 976) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(977, PCNT))
    {
        Device(P977) { Name(_HID, "ACPI0007") Name(_UID, 977) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(978, PCNT))
    {
        Device(P978) { Name(_HID, "ACPI0007") Name(_UID, 978) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(979, PCNT))
    {
        Device(P979) { Name(_HID, "ACPI0007") Name(_UID, 979) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(980, PCNT))
    {
        Device(P980) { Name(_HID, "ACPI0007") Name(_UID, 980) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(981, PCNT))
    {
        Device(P981) { Name(_HID, "ACPI0007") Name(_UID, 981) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(982, PCNT))
    {
        Device(P982) { Name(_HID, "ACPI0007") Name(_UID, 982) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(983, PCNT))
    {
        Device(P983) { Name(_HID, "ACPI0007") Name(_UID, 983) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(984, PCNT))
    {
        Device(P984) { Name(_HID, "ACPI0007") Name(_UID, 984) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(985, PCNT))
    {
        Device(P985) { Name(_HID, "ACPI0007") Name(_UID, 985) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(986, PCNT))
    {
        Device(P986) { Name(_HID, "ACPI0007") Name(_UID, 986) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(987, PCNT))
    {
        Device(P987) { Name(_HID, "ACPI0007") Name(_UID, 987) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(988, PCNT))
    {
        Device(P988) { Name(_HID, "ACPI0007") Name(_UID, 988) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(989, PCNT))
    {
        Device(P989) { Name(_HID, "ACPI0007") Name(_UID, 989) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(990, PCNT))
    {
        Device(P990) { Name(_HID, "ACPI0007") Name(_UID, 990) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(991, PCNT))
    {
        Device(P991) { Name(_HID, "ACPI0007") Name(_UID, 991) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(992, PCNT))
    {
        Device(P992) { Name(_HID, "ACPI0007") Name(_UID, 992) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(993, PCNT))
    {
        Device(P993) { Name(_HID, "ACPI0007") Name(_UID, 993) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(994, PCNT))
    {
        Device(P994) { Name(_HID, "ACPI0007") Name(_UID, 994) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(995, PCNT))
    {
        Device(P995) { Name(_HID, "ACPI0007") Name(_UID, 995) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(996, PCNT))
    {
        Device(P996) { Name(_HID, "ACPI0007") Name(_UID, 996) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(997, PCNT))
    {
        Device(P997) { Name(_HID, "ACPI0007") Name(_UID, 997) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(998, PCNT))
    {
        Device(P998) { Name(_HID, "ACPI0007") Name(_UID, 998) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(999, PCNT))
    {
        Device(P999) { Name(_HID, "ACPI0007") Name(_UID, 999) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1000, PCNT))
    {
        Device(Q000) { Name(_HID, "ACPI0007") Name(_UID, 1000) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1001, PCNT))
    {
        Device(Q001) { Name(_HID, "ACPI0007") Name(_UID, 1001) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1002, PCNT))
    {
        Device(Q002) { Name(_HID, "ACPI0007") Name(_UID, 1002) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1003, PCNT))
    {
        Device(Q003) { Name(_HID, "ACPI0007") Name(_UID, 1003) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1004, PCNT))
    {
        Device(Q004) { Name(_HID, "ACPI0007") Name(_UID, 1004) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1005, PCNT))
    {
        Device(Q005) { Name(_HID, "ACPI0007") Name(_UID, 1005) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1006, PCNT))
    {
        Device(Q006) { Name(_HID, "ACPI0007") Name(_UID, 1006) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1007, PCNT))
    {
        Device(Q007) { Name(_HID, "ACPI0007") Name(_UID, 1007) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1008, PCNT))
    {
        Device(Q008) { Name(_HID, "ACPI0007") Name(_UID, 1008) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1009, PCNT))
    {
        Device(Q009) { Name(_HID, "ACPI0007") Name(_UID, 1009) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1010, PCNT))
    {
        Device(Q010) { Name(_HID, "ACPI0007") Name(_UID, 1010) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1011, PCNT))
    {
        Device(Q011) { Name(_HID, "ACPI0007") Name(_UID, 1011) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1012, PCNT))
    {
        Device(Q012) { Name(_HID, "ACPI0007") Name(_UID, 1012) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1013, PCNT))
    {
        Device(Q013) { Name(_HID, "ACPI0007") Name(_UID, 1013) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1014, PCNT))
    {
        Device(Q014) { Name(_HID, "ACPI0007") Name(_UID, 1014) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1015, PCNT))
    {
        Device(Q015) { Name(_HID, "ACPI0007") Name(_UID, 1015) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1016, PCNT))
    {
        Device(Q016) { Name(_HID, "ACPI0007") Name(_UID, 1016) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1017, PCNT))
    {
        Device(Q017) { Name(_HID, "ACPI0007") Name(_UID, 1017) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1018, PCNT))
    {
        Device(Q018) { Name(_HID, "ACPI0007") Name(_UID, 1018) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1019, PCNT))
    {
        Device(Q019) { Name(_HID, "ACPI0007") Name(_UID, 1019) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1020, PCNT))
    {
        Device(Q020) { Name(_HID, "ACPI0007") Name(_UID, 1020) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1021, PCNT))
    {
        Device(Q021) { Name(_HID, "ACPI0007") Name(_UID, 1021) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1022, PCNT))
    {
        Device(Q022) { Name(_HID, "ACPI0007") Name(_UID, 1022) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1023, PCNT))
    {
        Device(Q023) { Name(_HID, "ACPI0007") Name(_UID, 1023) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1024, PCNT))
    {
        Device(Q024) { Name(_HID, "ACPI0007") Name(_UID, 1024) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1025, PCNT))
    {
        Device(Q025) { Name(_HID, "ACPI0007") Name(_UID, 1025) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1026, PCNT))
    {
        Device(Q026) { Name(_HID, "ACPI0007") Name(_UID, 1026) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1027, PCNT))
    {
        Device(Q027) { Name(_HID, "ACPI0007") Name(_UID, 1027) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1028, PCNT))
    {
        Device(Q028) { Name(_HID, "ACPI0007") Name(_UID, 1028) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1029, PCNT))
    {
        Device(Q029) { Name(_HID, "ACPI0007") Name(_UID, 1029) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1030, PCNT))
    {
        Device(Q030) { Name(_HID, "ACPI0007") Name(_UID, 1030) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1031, PCNT))
    {
        Device(Q031) { Name(_HID, "ACPI0007") Name(_UID, 1031) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1032, PCNT))
    {
        Device(Q032) { Name(_HID, "ACPI0007") Name(_UID, 1032) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1033, PCNT))
    {
        Device(Q033) { Name(_HID, "ACPI0007") Name(_UID, 1033) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1034, PCNT))
    {
        Device(Q034) { Name(_HID, "ACPI0007") Name(_UID, 1034) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1035, PCNT))
    {
        Device(Q035) { Name(_HID, "ACPI0007") Name(_UID, 1035) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1036, PCNT))
    {
        Device(Q036) { Name(_HID, "ACPI0007") Name(_UID, 1036) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1037, PCNT))
    {
        Device(Q037) { Name(_HID, "ACPI0007") Name(_UID, 1037) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1038, PCNT))
    {
        Device(Q038) { Name(_HID, "ACPI0007") Name(_UID, 1038) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1039, PCNT))
    {
        Device(Q039) { Name(_HID, "ACPI0007") Name(_UID, 1039) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1040, PCNT))
    {
        Device(Q040) { Name(_HID, "ACPI0007") Name(_UID, 1040) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1041, PCNT))
    {
        Device(Q041) { Name(_HID, "ACPI0007") Name(_UID, 1041) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1042, PCNT))
    {
        Device(Q042) { Name(_HID, "ACPI0007") Name(_UID, 1042) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1043, PCNT))
    {
        Device(Q043) { Name(_HID, "ACPI0007") Name(_UID, 1043) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1044, PCNT))
    {
        Device(Q044) { Name(_HID, "ACPI0007") Name(_UID, 1044) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1045, PCNT))
    {
        Device(Q045) { Name(_HID, "ACPI0007") Name(_UID, 1045) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1046, PCNT))
    {
        Device(Q046) { Name(_HID, "ACPI0007") Name(_UID, 1046) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1047, PCNT))
    {
        Device(Q047) { Name(_HID, "ACPI0007") Name(_UID, 1047) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1048, PCNT))
    {
        Device(Q048) { Name(_HID, "ACPI0007") Name(_UID, 1048) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1049, PCNT))
    {
        Device(Q049) { Name(_HID, "ACPI0007") Name(_UID, 1049) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1050, PCNT))
    {
        Device(Q050) { Name(_HID, "ACPI0007") Name(_UID, 1050) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1051, PCNT))
    {
        Device(Q051) { Name(_HID, "ACPI0007") Name(_UID, 1051) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1052, PCNT))
    {
        Device(Q052) { Name(_HID, "ACPI0007") Name(_UID, 1052) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1053, PCNT))
    {
        Device(Q053) { Name(_HID, "ACPI0007") Name(_UID, 1053) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1054, PCNT))
    {
        Device(Q054) { Name(_HID, "ACPI0007") Name(_UID, 1054) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1055, PCNT))
    {
        Device(Q055) { Name(_HID, "ACPI0007") Name(_UID, 1055) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1056, PCNT))
    {
        Device(Q056) { Name(_HID, "ACPI0007") Name(_UID, 1056) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1057, PCNT))
    {
        Device(Q057) { Name(_HID, "ACPI0007") Name(_UID, 1057) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1058, PCNT))
    {
        Device(Q058) { Name(_HID, "ACPI0007") Name(_UID, 1058) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1059, PCNT))
    {
        Device(Q059) { Name(_HID, "ACPI0007") Name(_UID, 1059) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1060, PCNT))
    {
        Device(Q060) { Name(_HID, "ACPI0007") Name(_UID, 1060) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1061, PCNT))
    {
        Device(Q061) { Name(_HID, "ACPI0007") Name(_UID, 1061) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1062, PCNT))
    {
        Device(Q062) { Name(_HID, "ACPI0007") Name(_UID, 1062) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1063, PCNT))
    {
        Device(Q063) { Name(_HID, "ACPI0007") Name(_UID, 1063) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1064, PCNT))
    {
        Device(Q064) { Name(_HID, "ACPI0007") Name(_UID, 1064) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1065, PCNT))
    {
        Device(Q065) { Name(_HID, "ACPI0007") Name(_UID, 1065) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1066, PCNT))
    {
        Device(Q066) { Name(_HID, "ACPI0007") Name(_UID, 1066) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1067, PCNT))
    {
        Device(Q067) { Name(_HID, "ACPI0007") Name(_UID, 1067) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1068, PCNT))
    {
        Device(Q068) { Name(_HID, "ACPI0007") Name(_UID, 1068) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1069, PCNT))
    {
        Device(Q069) { Name(_HID, "ACPI0007") Name(_UID, 1069) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1070, PCNT))
    {
        Device(Q070) { Name(_HID, "ACPI0007") Name(_UID, 1070) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1071, PCNT))
    {
        Device(Q071) { Name(_HID, "ACPI0007") Name(_UID, 1071) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1072, PCNT))
    {
        Device(Q072) { Name(_HID, "ACPI0007") Name(_UID, 1072) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1073, PCNT))
    {
        Device(Q073) { Name(_HID, "ACPI0007") Name(_UID, 1073) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1074, PCNT))
    {
        Device(Q074) { Name(_HID, "ACPI0007") Name(_UID, 1074) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1075, PCNT))
    {
        Device(Q075) { Name(_HID, "ACPI0007") Name(_UID, 1075) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1076, PCNT))
    {
        Device(Q076) { Name(_HID, "ACPI0007") Name(_UID, 1076) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1077, PCNT))
    {
        Device(Q077) { Name(_HID, "ACPI0007") Name(_UID, 1077) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1078, PCNT))
    {
        Device(Q078) { Name(_HID, "ACPI0007") Name(_UID, 1078) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1079, PCNT))
    {
        Device(Q079) { Name(_HID, "ACPI0007") Name(_UID, 1079) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1080, PCNT))
    {
        Device(Q080) { Name(_HID, "ACPI0007") Name(_UID, 1080) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1081, PCNT))
    {
        Device(Q081) { Name(_HID, "ACPI0007") Name(_UID, 1081) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1082, PCNT))
    {
        Device(Q082) { Name(_HID, "ACPI0007") Name(_UID, 1082) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1083, PCNT))
    {
        Device(Q083) { Name(_HID, "ACPI0007") Name(_UID, 1083) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1084, PCNT))
    {
        Device(Q084) { Name(_HID, "ACPI0007") Name(_UID, 1084) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1085, PCNT))
    {
        Device(Q085) { Name(_HID, "ACPI0007") Name(_UID, 1085) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1086, PCNT))
    {
        Device(Q086) { Name(_HID, "ACPI0007") Name(_UID, 1086) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1087, PCNT))
    {
        Device(Q087) { Name(_HID, "ACPI0007") Name(_UID, 1087) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1088, PCNT))
    {
        Device(Q088) { Name(_HID, "ACPI0007") Name(_UID, 1088) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1089, PCNT))
    {
        Device(Q089) { Name(_HID, "ACPI0007") Name(_UID, 1089) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1090, PCNT))
    {
        Device(Q090) { Name(_HID, "ACPI0007") Name(_UID, 1090) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1091, PCNT))
    {
        Device(Q091) { Name(_HID, "ACPI0007") Name(_UID, 1091) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1092, PCNT))
    {
        Device(Q092) { Name(_HID, "ACPI0007") Name(_UID, 1092) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1093, PCNT))
    {
        Device(Q093) { Name(_HID, "ACPI0007") Name(_UID, 1093) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1094, PCNT))
    {
        Device(Q094) { Name(_HID, "ACPI0007") Name(_UID, 1094) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1095, PCNT))
    {
        Device(Q095) { Name(_HID, "ACPI0007") Name(_UID, 1095) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1096, PCNT))
    {
        Device(Q096) { Name(_HID, "ACPI0007") Name(_UID, 1096) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1097, PCNT))
    {
        Device(Q097) { Name(_HID, "ACPI0007") Name(_UID, 1097) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1098, PCNT))
    {
        Device(Q098) { Name(_HID, "ACPI0007") Name(_UID, 1098) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1099, PCNT))
    {
        Device(Q099) { Name(_HID, "ACPI0007") Name(_UID, 1099) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1100, PCNT))
    {
        Device(Q100) { Name(_HID, "ACPI0007") Name(_UID, 1100) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1101, PCNT))
    {
        Device(Q101) { Name(_HID, "ACPI0007") Name(_UID, 1101) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1102, PCNT))
    {
        Device(Q102) { Name(_HID, "ACPI0007") Name(_UID, 1102) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1103, PCNT))
    {
        Device(Q103) { Name(_HID, "ACPI0007") Name(_UID, 1103) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1104, PCNT))
    {
        Device(Q104) { Name(_HID, "ACPI0007") Name(_UID, 1104) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1105, PCNT))
    {
        Device(Q105) { Name(_HID, "ACPI0007") Name(_UID, 1105) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1106, PCNT))
    {
        Device(Q106) { Name(_HID, "ACPI0007") Name(_UID, 1106) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1107, PCNT))
    {
        Device(Q107) { Name(_HID, "ACPI0007") Name(_UID, 1107) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1108, PCNT))
    {
        Device(Q108) { Name(_HID, "ACPI0007") Name(_UID, 1108) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1109, PCNT))
    {
        Device(Q109) { Name(_HID, "ACPI0007") Name(_UID, 1109) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1110, PCNT))
    {
        Device(Q110) { Name(_HID, "ACPI0007") Name(_UID, 1110) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1111, PCNT))
    {
        Device(Q111) { Name(_HID, "ACPI0007") Name(_UID, 1111) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1112, PCNT))
    {
        Device(Q112) { Name(_HID, "ACPI0007") Name(_UID, 1112) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1113, PCNT))
    {
        Device(Q113) { Name(_HID, "ACPI0007") Name(_UID, 1113) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1114, PCNT))
    {
        Device(Q114) { Name(_HID, "ACPI0007") Name(_UID, 1114) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1115, PCNT))
    {
        Device(Q115) { Name(_HID, "ACPI0007") Name(_UID, 1115) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1116, PCNT))
    {
        Device(Q116) { Name(_HID, "ACPI0007") Name(_UID, 1116) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1117, PCNT))
    {
        Device(Q117) { Name(_HID, "ACPI0007") Name(_UID, 1117) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1118, PCNT))
    {
        Device(Q118) { Name(_HID, "ACPI0007") Name(_UID, 1118) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1119, PCNT))
    {
        Device(Q119) { Name(_HID, "ACPI0007") Name(_UID, 1119) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1120, PCNT))
    {
        Device(Q120) { Name(_HID, "ACPI0007") Name(_UID, 1120) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1121, PCNT))
    {
        Device(Q121) { Name(_HID, "ACPI0007") Name(_UID, 1121) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1122, PCNT))
    {
        Device(Q122) { Name(_HID, "ACPI0007") Name(_UID, 1122) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1123, PCNT))
    {
        Device(Q123) { Name(_HID, "ACPI0007") Name(_UID, 1123) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1124, PCNT))
    {
        Device(Q124) { Name(_HID, "ACPI0007") Name(_UID, 1124) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1125, PCNT))
    {
        Device(Q125) { Name(_HID, "ACPI0007") Name(_UID, 1125) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1126, PCNT))
    {
        Device(Q126) { Name(_HID, "ACPI0007") Name(_UID, 1126) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1127, PCNT))
    {
        Device(Q127) { Name(_HID, "ACPI0007") Name(_UID, 1127) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1128, PCNT))
    {
        Device(Q128) { Name(_HID, "ACPI0007") Name(_UID, 1128) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1129, PCNT))
    {
        Device(Q129) { Name(_HID, "ACPI0007") Name(_UID, 1129) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1130, PCNT))
    {
        Device(Q130) { Name(_HID, "ACPI0007") Name(_UID, 1130) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1131, PCNT))
    {
        Device(Q131) { Name(_HID, "ACPI0007") Name(_UID, 1131) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1132, PCNT))
    {
        Device(Q132) { Name(_HID, "ACPI0007") Name(_UID, 1132) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1133, PCNT))
    {
        Device(Q133) { Name(_HID, "ACPI0007") Name(_UID, 1133) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1134, PCNT))
    {
        Device(Q134) { Name(_HID, "ACPI0007") Name(_UID, 1134) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1135, PCNT))
    {
        Device(Q135) { Name(_HID, "ACPI0007") Name(_UID, 1135) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1136, PCNT))
    {
        Device(Q136) { Name(_HID, "ACPI0007") Name(_UID, 1136) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1137, PCNT))
    {
        Device(Q137) { Name(_HID, "ACPI0007") Name(_UID, 1137) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1138, PCNT))
    {
        Device(Q138) { Name(_HID, "ACPI0007") Name(_UID, 1138) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1139, PCNT))
    {
        Device(Q139) { Name(_HID, "ACPI0007") Name(_UID, 1139) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1140, PCNT))
    {
        Device(Q140) { Name(_HID, "ACPI0007") Name(_UID, 1140) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1141, PCNT))
    {
        Device(Q141) { Name(_HID, "ACPI0007") Name(_UID, 1141) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1142, PCNT))
    {
        Device(Q142) { Name(_HID, "ACPI0007") Name(_UID, 1142) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1143, PCNT))
    {
        Device(Q143) { Name(_HID, "ACPI0007") Name(_UID, 1143) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1144, PCNT))
    {
        Device(Q144) { Name(_HID, "ACPI0007") Name(_UID, 1144) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1145, PCNT))
    {
        Device(Q145) { Name(_HID, "ACPI0007") Name(_UID, 1145) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1146, PCNT))
    {
        Device(Q146) { Name(_HID, "ACPI0007") Name(_UID, 1146) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1147, PCNT))
    {
        Device(Q147) { Name(_HID, "ACPI0007") Name(_UID, 1147) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1148, PCNT))
    {
        Device(Q148) { Name(_HID, "ACPI0007") Name(_UID, 1148) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1149, PCNT))
    {
        Device(Q149) { Name(_HID, "ACPI0007") Name(_UID, 1149) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1150, PCNT))
    {
        Device(Q150) { Name(_HID, "ACPI0007") Name(_UID, 1150) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1151, PCNT))
    {
        Device(Q151) { Name(_HID, "ACPI0007") Name(_UID, 1151) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1152, PCNT))
    {
        Device(Q152) { Name(_HID, "ACPI0007") Name(_UID, 1152) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1153, PCNT))
    {
        Device(Q153) { Name(_HID, "ACPI0007") Name(_UID, 1153) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1154, PCNT))
    {
        Device(Q154) { Name(_HID, "ACPI0007") Name(_UID, 1154) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1155, PCNT))
    {
        Device(Q155) { Name(_HID, "ACPI0007") Name(_UID, 1155) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1156, PCNT))
    {
        Device(Q156) { Name(_HID, "ACPI0007") Name(_UID, 1156) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1157, PCNT))
    {
        Device(Q157) { Name(_HID, "ACPI0007") Name(_UID, 1157) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1158, PCNT))
    {
        Device(Q158) { Name(_HID, "ACPI0007") Name(_UID, 1158) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1159, PCNT))
    {
        Device(Q159) { Name(_HID, "ACPI0007") Name(_UID, 1159) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1160, PCNT))
    {
        Device(Q160) { Name(_HID, "ACPI0007") Name(_UID, 1160) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1161, PCNT))
    {
        Device(Q161) { Name(_HID, "ACPI0007") Name(_UID, 1161) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1162, PCNT))
    {
        Device(Q162) { Name(_HID, "ACPI0007") Name(_UID, 1162) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1163, PCNT))
    {
        Device(Q163) { Name(_HID, "ACPI0007") Name(_UID, 1163) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1164, PCNT))
    {
        Device(Q164) { Name(_HID, "ACPI0007") Name(_UID, 1164) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1165, PCNT))
    {
        Device(Q165) { Name(_HID, "ACPI0007") Name(_UID, 1165) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1166, PCNT))
    {
        Device(Q166) { Name(_HID, "ACPI0007") Name(_UID, 1166) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1167, PCNT))
    {
        Device(Q167) { Name(_HID, "ACPI0007") Name(_UID, 1167) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1168, PCNT))
    {
        Device(Q168) { Name(_HID, "ACPI0007") Name(_UID, 1168) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1169, PCNT))
    {
        Device(Q169) { Name(_HID, "ACPI0007") Name(_UID, 1169) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1170, PCNT))
    {
        Device(Q170) { Name(_HID, "ACPI0007") Name(_UID, 1170) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1171, PCNT))
    {
        Device(Q171) { Name(_HID, "ACPI0007") Name(_UID, 1171) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1172, PCNT))
    {
        Device(Q172) { Name(_HID, "ACPI0007") Name(_UID, 1172) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1173, PCNT))
    {
        Device(Q173) { Name(_HID, "ACPI0007") Name(_UID, 1173) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1174, PCNT))
    {
        Device(Q174) { Name(_HID, "ACPI0007") Name(_UID, 1174) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1175, PCNT))
    {
        Device(Q175) { Name(_HID, "ACPI0007") Name(_UID, 1175) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1176, PCNT))
    {
        Device(Q176) { Name(_HID, "ACPI0007") Name(_UID, 1176) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1177, PCNT))
    {
        Device(Q177) { Name(_HID, "ACPI0007") Name(_UID, 1177) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1178, PCNT))
    {
        Device(Q178) { Name(_HID, "ACPI0007") Name(_UID, 1178) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1179, PCNT))
    {
        Device(Q179) { Name(_HID, "ACPI0007") Name(_UID, 1179) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1180, PCNT))
    {
        Device(Q180) { Name(_HID, "ACPI0007") Name(_UID, 1180) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1181, PCNT))
    {
        Device(Q181) { Name(_HID, "ACPI0007") Name(_UID, 1181) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1182, PCNT))
    {
        Device(Q182) { Name(_HID, "ACPI0007") Name(_UID, 1182) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1183, PCNT))
    {
        Device(Q183) { Name(_HID, "ACPI0007") Name(_UID, 1183) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1184, PCNT))
    {
        Device(Q184) { Name(_HID, "ACPI0007") Name(_UID, 1184) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1185, PCNT))
    {
        Device(Q185) { Name(_HID, "ACPI0007") Name(_UID, 1185) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1186, PCNT))
    {
        Device(Q186) { Name(_HID, "ACPI0007") Name(_UID, 1186) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1187, PCNT))
    {
        Device(Q187) { Name(_HID, "ACPI0007") Name(_UID, 1187) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1188, PCNT))
    {
        Device(Q188) { Name(_HID, "ACPI0007") Name(_UID, 1188) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1189, PCNT))
    {
        Device(Q189) { Name(_HID, "ACPI0007") Name(_UID, 1189) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1190, PCNT))
    {
        Device(Q190) { Name(_HID, "ACPI0007") Name(_UID, 1190) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1191, PCNT))
    {
        Device(Q191) { Name(_HID, "ACPI0007") Name(_UID, 1191) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1192, PCNT))
    {
        Device(Q192) { Name(_HID, "ACPI0007") Name(_UID, 1192) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1193, PCNT))
    {
        Device(Q193) { Name(_HID, "ACPI0007") Name(_UID, 1193) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1194, PCNT))
    {
        Device(Q194) { Name(_HID, "ACPI0007") Name(_UID, 1194) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1195, PCNT))
    {
        Device(Q195) { Name(_HID, "ACPI0007") Name(_UID, 1195) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1196, PCNT))
    {
        Device(Q196) { Name(_HID, "ACPI0007") Name(_UID, 1196) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1197, PCNT))
    {
        Device(Q197) { Name(_HID, "ACPI0007") Name(_UID, 1197) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1198, PCNT))
    {
        Device(Q198) { Name(_HID, "ACPI0007") Name(_UID, 1198) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1199, PCNT))
    {
        Device(Q199) { Name(_HID, "ACPI0007") Name(_UID, 1199) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1200, PCNT))
    {
        Device(Q200) { Name(_HID, "ACPI0007") Name(_UID, 1200) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1201, PCNT))
    {
        Device(Q201) { Name(_HID, "ACPI0007") Name(_UID, 1201) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1202, PCNT))
    {
        Device(Q202) { Name(_HID, "ACPI0007") Name(_UID, 1202) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1203, PCNT))
    {
        Device(Q203) { Name(_HID, "ACPI0007") Name(_UID, 1203) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1204, PCNT))
    {
        Device(Q204) { Name(_HID, "ACPI0007") Name(_UID, 1204) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1205, PCNT))
    {
        Device(Q205) { Name(_HID, "ACPI0007") Name(_UID, 1205) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1206, PCNT))
    {
        Device(Q206) { Name(_HID, "ACPI0007") Name(_UID, 1206) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1207, PCNT))
    {
        Device(Q207) { Name(_HID, "ACPI0007") Name(_UID, 1207) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1208, PCNT))
    {
        Device(Q208) { Name(_HID, "ACPI0007") Name(_UID, 1208) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1209, PCNT))
    {
        Device(Q209) { Name(_HID, "ACPI0007") Name(_UID, 1209) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1210, PCNT))
    {
        Device(Q210) { Name(_HID, "ACPI0007") Name(_UID, 1210) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1211, PCNT))
    {
        Device(Q211) { Name(_HID, "ACPI0007") Name(_UID, 1211) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1212, PCNT))
    {
        Device(Q212) { Name(_HID, "ACPI0007") Name(_UID, 1212) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1213, PCNT))
    {
        Device(Q213) { Name(_HID, "ACPI0007") Name(_UID, 1213) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1214, PCNT))
    {
        Device(Q214) { Name(_HID, "ACPI0007") Name(_UID, 1214) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1215, PCNT))
    {
        Device(Q215) { Name(_HID, "ACPI0007") Name(_UID, 1215) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1216, PCNT))
    {
        Device(Q216) { Name(_HID, "ACPI0007") Name(_UID, 1216) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1217, PCNT))
    {
        Device(Q217) { Name(_HID, "ACPI0007") Name(_UID, 1217) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1218, PCNT))
    {
        Device(Q218) { Name(_HID, "ACPI0007") Name(_UID, 1218) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1219, PCNT))
    {
        Device(Q219) { Name(_HID, "ACPI0007") Name(_UID, 1219) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1220, PCNT))
    {
        Device(Q220) { Name(_HID, "ACPI0007") Name(_UID, 1220) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1221, PCNT))
    {
        Device(Q221) { Name(_HID, "ACPI0007") Name(_UID, 1221) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1222, PCNT))
    {
        Device(Q222) { Name(_HID, "ACPI0007") Name(_UID, 1222) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1223, PCNT))
    {
        Device(Q223) { Name(_HID, "ACPI0007") Name(_UID, 1223) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1224, PCNT))
    {
        Device(Q224) { Name(_HID, "ACPI0007") Name(_UID, 1224) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1225, PCNT))
    {
        Device(Q225) { Name(_HID, "ACPI0007") Name(_UID, 1225) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1226, PCNT))
    {
        Device(Q226) { Name(_HID, "ACPI0007") Name(_UID, 1226) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1227, PCNT))
    {
        Device(Q227) { Name(_HID, "ACPI0007") Name(_UID, 1227) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1228, PCNT))
    {
        Device(Q228) { Name(_HID, "ACPI0007") Name(_UID, 1228) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1229, PCNT))
    {
        Device(Q229) { Name(_HID, "ACPI0007") Name(_UID, 1229) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1230, PCNT))
    {
        Device(Q230) { Name(_HID, "ACPI0007") Name(_UID, 1230) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1231, PCNT))
    {
        Device(Q231) { Name(_HID, "ACPI0007") Name(_UID, 1231) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1232, PCNT))
    {
        Device(Q232) { Name(_HID, "ACPI0007") Name(_UID, 1232) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1233, PCNT))
    {
        Device(Q233) { Name(_HID, "ACPI0007") Name(_UID, 1233) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1234, PCNT))
    {
        Device(Q234) { Name(_HID, "ACPI0007") Name(_UID, 1234) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1235, PCNT))
    {
        Device(Q235) { Name(_HID, "ACPI0007") Name(_UID, 1235) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1236, PCNT))
    {
        Device(Q236) { Name(_HID, "ACPI0007") Name(_UID, 1236) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1237, PCNT))
    {
        Device(Q237) { Name(_HID, "ACPI0007") Name(_UID, 1237) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1238, PCNT))
    {
        Device(Q238) { Name(_HID, "ACPI0007") Name(_UID, 1238) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1239, PCNT))
    {
        Device(Q239) { Name(_HID, "ACPI0007") Name(_UID, 1239) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1240, PCNT))
    {
        Device(Q240) { Name(_HID, "ACPI0007") Name(_UID, 1240) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1241, PCNT))
    {
        Device(Q241) { Name(_HID, "ACPI0007") Name(_UID, 1241) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1242, PCNT))
    {
        Device(Q242) { Name(_HID, "ACPI0007") Name(_UID, 1242) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1243, PCNT))
    {
        Device(Q243) { Name(_HID, "ACPI0007") Name(_UID, 1243) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1244, PCNT))
    {
        Device(Q244) { Name(_HID, "ACPI0007") Name(_UID, 1244) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1245, PCNT))
    {
        Device(Q245) { Name(_HID, "ACPI0007") Name(_UID, 1245) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1246, PCNT))
    {
        Device(Q246) { Name(_HID, "ACPI0007") Name(_UID, 1246) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1247, PCNT))
    {
        Device(Q247) { Name(_HID, "ACPI0007") Name(_UID, 1247) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1248, PCNT))
    {
        Device(Q248) { Name(_HID, "ACPI0007") Name(_UID, 1248) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1249, PCNT))
    {
        Device(Q249) { Name(_HID, "ACPI0007") Name(_UID, 1249) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1250, PCNT))
    {
        Device(Q250) { Name(_HID, "ACPI0007") Name(_UID, 1250) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1251, PCNT))
    {
        Device(Q251) { Name(_HID, "ACPI0007") Name(_UID, 1251) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1252, PCNT))
    {
        Device(Q252) { Name(_HID, "ACPI0007") Name(_UID, 1252) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1253, PCNT))
    {
        Device(Q253) { Name(_HID, "ACPI0007") Name(_UID, 1253) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1254, PCNT))
    {
        Device(Q254) { Name(_HID, "ACPI0007") Name(_UID, 1254) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1255, PCNT))
    {
        Device(Q255) { Name(_HID, "ACPI0007") Name(_UID, 1255) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1256, PCNT))
    {
        Device(Q256) { Name(_HID, "ACPI0007") Name(_UID, 1256) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1257, PCNT))
    {
        Device(Q257) { Name(_HID, "ACPI0007") Name(_UID, 1257) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1258, PCNT))
    {
        Device(Q258) { Name(_HID, "ACPI0007") Name(_UID, 1258) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1259, PCNT))
    {
        Device(Q259) { Name(_HID, "ACPI0007") Name(_UID, 1259) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1260, PCNT))
    {
        Device(Q260) { Name(_HID, "ACPI0007") Name(_UID, 1260) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1261, PCNT))
    {
        Device(Q261) { Name(_HID, "ACPI0007") Name(_UID, 1261) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1262, PCNT))
    {
        Device(Q262) { Name(_HID, "ACPI0007") Name(_UID, 1262) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1263, PCNT))
    {
        Device(Q263) { Name(_HID, "ACPI0007") Name(_UID, 1263) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1264, PCNT))
    {
        Device(Q264) { Name(_HID, "ACPI0007") Name(_UID, 1264) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1265, PCNT))
    {
        Device(Q265) { Name(_HID, "ACPI0007") Name(_UID, 1265) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1266, PCNT))
    {
        Device(Q266) { Name(_HID, "ACPI0007") Name(_UID, 1266) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1267, PCNT))
    {
        Device(Q267) { Name(_HID, "ACPI0007") Name(_UID, 1267) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1268, PCNT))
    {
        Device(Q268) { Name(_HID, "ACPI0007") Name(_UID, 1268) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1269, PCNT))
    {
        Device(Q269) { Name(_HID, "ACPI0007") Name(_UID, 1269) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1270, PCNT))
    {
        Device(Q270) { Name(_HID, "ACPI0007") Name(_UID, 1270) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1271, PCNT))
    {
        Device(Q271) { Name(_HID, "ACPI0007") Name(_UID, 1271) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1272, PCNT))
    {
        Device(Q272) { Name(_HID, "ACPI0007") Name(_UID, 1272) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1273, PCNT))
    {
        Device(Q273) { Name(_HID, "ACPI0007") Name(_UID, 1273) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1274, PCNT))
    {
        Device(Q274) { Name(_HID, "ACPI0007") Name(_UID, 1274) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1275, PCNT))
    {
        Device(Q275) { Name(_HID, "ACPI0007") Name(_UID, 1275) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1276, PCNT))
    {
        Device(Q276) { Name(_HID, "ACPI0007") Name(_UID, 1276) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1277, PCNT))
    {
        Device(Q277) { Name(_HID, "ACPI0007") Name(_UID, 1277) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1278, PCNT))
    {
        Device(Q278) { Name(_HID, "ACPI0007") Name(_UID, 1278) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1279, PCNT))
    {
        Device(Q279) { Name(_HID, "ACPI0007") Name(_UID, 1279) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1280, PCNT))
    {
        Device(Q280) { Name(_HID, "ACPI0007") Name(_UID, 1280) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1281, PCNT))
    {
        Device(Q281) { Name(_HID, "ACPI0007") Name(_UID, 1281) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1282, PCNT))
    {
        Device(Q282) { Name(_HID, "ACPI0007") Name(_UID, 1282) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1283, PCNT))
    {
        Device(Q283) { Name(_HID, "ACPI0007") Name(_UID, 1283) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1284, PCNT))
    {
        Device(Q284) { Name(_HID, "ACPI0007") Name(_UID, 1284) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1285, PCNT))
    {
        Device(Q285) { Name(_HID, "ACPI0007") Name(_UID, 1285) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1286, PCNT))
    {
        Device(Q286) { Name(_HID, "ACPI0007") Name(_UID, 1286) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1287, PCNT))
    {
        Device(Q287) { Name(_HID, "ACPI0007") Name(_UID, 1287) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1288, PCNT))
    {
        Device(Q288) { Name(_HID, "ACPI0007") Name(_UID, 1288) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1289, PCNT))
    {
        Device(Q289) { Name(_HID, "ACPI0007") Name(_UID, 1289) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1290, PCNT))
    {
        Device(Q290) { Name(_HID, "ACPI0007") Name(_UID, 1290) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1291, PCNT))
    {
        Device(Q291) { Name(_HID, "ACPI0007") Name(_UID, 1291) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1292, PCNT))
    {
        Device(Q292) { Name(_HID, "ACPI0007") Name(_UID, 1292) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1293, PCNT))
    {
        Device(Q293) { Name(_HID, "ACPI0007") Name(_UID, 1293) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1294, PCNT))
    {
        Device(Q294) { Name(_HID, "ACPI0007") Name(_UID, 1294) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1295, PCNT))
    {
        Device(Q295) { Name(_HID, "ACPI0007") Name(_UID, 1295) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1296, PCNT))
    {
        Device(Q296) { Name(_HID, "ACPI0007") Name(_UID, 1296) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1297, PCNT))
    {
        Device(Q297) { Name(_HID, "ACPI0007") Name(_UID, 1297) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1298, PCNT))
    {
        Device(Q298) { Name(_HID, "ACPI0007") Name(_UID, 1298) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1299, PCNT))
    {
        Device(Q299) { Name(_HID, "ACPI0007") Name(_UID, 1299) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1300, PCNT))
    {
        Device(Q300) { Name(_HID, "ACPI0007") Name(_UID, 1300) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1301, PCNT))
    {
        Device(Q301) { Name(_HID, "ACPI0007") Name(_UID, 1301) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1302, PCNT))
    {
        Device(Q302) { Name(_HID, "ACPI0007") Name(_UID, 1302) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1303, PCNT))
    {
        Device(Q303) { Name(_HID, "ACPI0007") Name(_UID, 1303) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1304, PCNT))
    {
        Device(Q304) { Name(_HID, "ACPI0007") Name(_UID, 1304) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1305, PCNT))
    {
        Device(Q305) { Name(_HID, "ACPI0007") Name(_UID, 1305) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1306, PCNT))
    {
        Device(Q306) { Name(_HID, "ACPI0007") Name(_UID, 1306) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1307, PCNT))
    {
        Device(Q307) { Name(_HID, "ACPI0007") Name(_UID, 1307) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1308, PCNT))
    {
        Device(Q308) { Name(_HID, "ACPI0007") Name(_UID, 1308) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1309, PCNT))
    {
        Device(Q309) { Name(_HID, "ACPI0007") Name(_UID, 1309) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1310, PCNT))
    {
        Device(Q310) { Name(_HID, "ACPI0007") Name(_UID, 1310) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1311, PCNT))
    {
        Device(Q311) { Name(_HID, "ACPI0007") Name(_UID, 1311) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1312, PCNT))
    {
        Device(Q312) { Name(_HID, "ACPI0007") Name(_UID, 1312) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1313, PCNT))
    {
        Device(Q313) { Name(_HID, "ACPI0007") Name(_UID, 1313) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1314, PCNT))
    {
        Device(Q314) { Name(_HID, "ACPI0007") Name(_UID, 1314) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1315, PCNT))
    {
        Device(Q315) { Name(_HID, "ACPI0007") Name(_UID, 1315) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1316, PCNT))
    {
        Device(Q316) { Name(_HID, "ACPI0007") Name(_UID, 1316) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1317, PCNT))
    {
        Device(Q317) { Name(_HID, "ACPI0007") Name(_UID, 1317) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1318, PCNT))
    {
        Device(Q318) { Name(_HID, "ACPI0007") Name(_UID, 1318) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1319, PCNT))
    {
        Device(Q319) { Name(_HID, "ACPI0007") Name(_UID, 1319) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1320, PCNT))
    {
        Device(Q320) { Name(_HID, "ACPI0007") Name(_UID, 1320) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1321, PCNT))
    {
        Device(Q321) { Name(_HID, "ACPI0007") Name(_UID, 1321) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1322, PCNT))
    {
        Device(Q322) { Name(_HID, "ACPI0007") Name(_UID, 1322) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1323, PCNT))
    {
        Device(Q323) { Name(_HID, "ACPI0007") Name(_UID, 1323) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1324, PCNT))
    {
        Device(Q324) { Name(_HID, "ACPI0007") Name(_UID, 1324) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1325, PCNT))
    {
        Device(Q325) { Name(_HID, "ACPI0007") Name(_UID, 1325) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1326, PCNT))
    {
        Device(Q326) { Name(_HID, "ACPI0007") Name(_UID, 1326) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1327, PCNT))
    {
        Device(Q327) { Name(_HID, "ACPI0007") Name(_UID, 1327) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1328, PCNT))
    {
        Device(Q328) { Name(_HID, "ACPI0007") Name(_UID, 1328) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1329, PCNT))
    {
        Device(Q329) { Name(_HID, "ACPI0007") Name(_UID, 1329) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1330, PCNT))
    {
        Device(Q330) { Name(_HID, "ACPI0007") Name(_UID, 1330) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1331, PCNT))
    {
        Device(Q331) { Name(_HID, "ACPI0007") Name(_UID, 1331) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1332, PCNT))
    {
        Device(Q332) { Name(_HID, "ACPI0007") Name(_UID, 1332) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1333, PCNT))
    {
        Device(Q333) { Name(_HID, "ACPI0007") Name(_UID, 1333) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1334, PCNT))
    {
        Device(Q334) { Name(_HID, "ACPI0007") Name(_UID, 1334) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1335, PCNT))
    {
        Device(Q335) { Name(_HID, "ACPI0007") Name(_UID, 1335) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1336, PCNT))
    {
        Device(Q336) { Name(_HID, "ACPI0007") Name(_UID, 1336) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1337, PCNT))
    {
        Device(Q337) { Name(_HID, "ACPI0007") Name(_UID, 1337) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1338, PCNT))
    {
        Device(Q338) { Name(_HID, "ACPI0007") Name(_UID, 1338) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1339, PCNT))
    {
        Device(Q339) { Name(_HID, "ACPI0007") Name(_UID, 1339) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1340, PCNT))
    {
        Device(Q340) { Name(_HID, "ACPI0007") Name(_UID, 1340) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1341, PCNT))
    {
        Device(Q341) { Name(_HID, "ACPI0007") Name(_UID, 1341) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1342, PCNT))
    {
        Device(Q342) { Name(_HID, "ACPI0007") Name(_UID, 1342) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1343, PCNT))
    {
        Device(Q343) { Name(_HID, "ACPI0007") Name(_UID, 1343) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1344, PCNT))
    {
        Device(Q344) { Name(_HID, "ACPI0007") Name(_UID, 1344) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1345, PCNT))
    {
        Device(Q345) { Name(_HID, "ACPI0007") Name(_UID, 1345) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1346, PCNT))
    {
        Device(Q346) { Name(_HID, "ACPI0007") Name(_UID, 1346) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1347, PCNT))
    {
        Device(Q347) { Name(_HID, "ACPI0007") Name(_UID, 1347) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1348, PCNT))
    {
        Device(Q348) { Name(_HID, "ACPI0007") Name(_UID, 1348) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1349, PCNT))
    {
        Device(Q349) { Name(_HID, "ACPI0007") Name(_UID, 1349) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1350, PCNT))
    {
        Device(Q350) { Name(_HID, "ACPI0007") Name(_UID, 1350) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1351, PCNT))
    {
        Device(Q351) { Name(_HID, "ACPI0007") Name(_UID, 1351) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1352, PCNT))
    {
        Device(Q352) { Name(_HID, "ACPI0007") Name(_UID, 1352) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1353, PCNT))
    {
        Device(Q353) { Name(_HID, "ACPI0007") Name(_UID, 1353) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1354, PCNT))
    {
        Device(Q354) { Name(_HID, "ACPI0007") Name(_UID, 1354) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1355, PCNT))
    {
        Device(Q355) { Name(_HID, "ACPI0007") Name(_UID, 1355) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1356, PCNT))
    {
        Device(Q356) { Name(_HID, "ACPI0007") Name(_UID, 1356) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1357, PCNT))
    {
        Device(Q357) { Name(_HID, "ACPI0007") Name(_UID, 1357) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1358, PCNT))
    {
        Device(Q358) { Name(_HID, "ACPI0007") Name(_UID, 1358) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1359, PCNT))
    {
        Device(Q359) { Name(_HID, "ACPI0007") Name(_UID, 1359) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1360, PCNT))
    {
        Device(Q360) { Name(_HID, "ACPI0007") Name(_UID, 1360) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1361, PCNT))
    {
        Device(Q361) { Name(_HID, "ACPI0007") Name(_UID, 1361) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1362, PCNT))
    {
        Device(Q362) { Name(_HID, "ACPI0007") Name(_UID, 1362) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1363, PCNT))
    {
        Device(Q363) { Name(_HID, "ACPI0007") Name(_UID, 1363) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1364, PCNT))
    {
        Device(Q364) { Name(_HID, "ACPI0007") Name(_UID, 1364) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1365, PCNT))
    {
        Device(Q365) { Name(_HID, "ACPI0007") Name(_UID, 1365) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1366, PCNT))
    {
        Device(Q366) { Name(_HID, "ACPI0007") Name(_UID, 1366) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1367, PCNT))
    {
        Device(Q367) { Name(_HID, "ACPI0007") Name(_UID, 1367) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1368, PCNT))
    {
        Device(Q368) { Name(_HID, "ACPI0007") Name(_UID, 1368) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1369, PCNT))
    {
        Device(Q369) { Name(_HID, "ACPI0007") Name(_UID, 1369) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1370, PCNT))
    {
        Device(Q370) { Name(_HID, "ACPI0007") Name(_UID, 1370) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1371, PCNT))
    {
        Device(Q371) { Name(_HID, "ACPI0007") Name(_UID, 1371) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1372, PCNT))
    {
        Device(Q372) { Name(_HID, "ACPI0007") Name(_UID, 1372) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1373, PCNT))
    {
        Device(Q373) { Name(_HID, "ACPI0007") Name(_UID, 1373) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1374, PCNT))
    {
        Device(Q374) { Name(_HID, "ACPI0007") Name(_UID, 1374) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1375, PCNT))
    {
        Device(Q375) { Name(_HID, "ACPI0007") Name(_UID, 1375) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1376, PCNT))
    {
        Device(Q376) { Name(_HID, "ACPI0007") Name(_UID, 1376) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1377, PCNT))
    {
        Device(Q377) { Name(_HID, "ACPI0007") Name(_UID, 1377) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1378, PCNT))
    {
        Device(Q378) { Name(_HID, "ACPI0007") Name(_UID, 1378) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1379, PCNT))
    {
        Device(Q379) { Name(_HID, "ACPI0007") Name(_UID, 1379) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1380, PCNT))
    {
        Device(Q380) { Name(_HID, "ACPI0007") Name(_UID, 1380) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1381, PCNT))
    {
        Device(Q381) { Name(_HID, "ACPI0007") Name(_UID, 1381) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1382, PCNT))
    {
        Device(Q382) { Name(_HID, "ACPI0007") Name(_UID, 1382) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1383, PCNT))
    {
        Device(Q383) { Name(_HID, "ACPI0007") Name(_UID, 1383) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1384, PCNT))
    {
        Device(Q384) { Name(_HID, "ACPI0007") Name(_UID, 1384) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1385, PCNT))
    {
        Device(Q385) { Name(_HID, "ACPI0007") Name(_UID, 1385) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1386, PCNT))
    {
        Device(Q386) { Name(_HID, "ACPI0007") Name(_UID, 1386) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1387, PCNT))
    {
        Device(Q387) { Name(_HID, "ACPI0007") Name(_UID, 1387) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1388, PCNT))
    {
        Device(Q388) { Name(_HID, "ACPI0007") Name(_UID, 1388) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1389, PCNT))
    {
        Device(Q389) { Name(_HID, "ACPI0007") Name(_UID, 1389) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1390, PCNT))
    {
        Device(Q390) { Name(_HID, "ACPI0007") Name(_UID, 1390) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1391, PCNT))
    {
        Device(Q391) { Name(_HID, "ACPI0007") Name(_UID, 1391) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1392, PCNT))
    {
        Device(Q392) { Name(_HID, "ACPI0007") Name(_UID, 1392) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1393, PCNT))
    {
        Device(Q393) { Name(_HID, "ACPI0007") Name(_UID, 1393) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1394, PCNT))
    {
        Device(Q394) { Name(_HID, "ACPI0007") Name(_UID, 1394) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1395, PCNT))
    {
        Device(Q395) { Name(_HID, "ACPI0007") Name(_UID, 1395) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1396, PCNT))
    {
        Device(Q396) { Name(_HID, "ACPI0007") Name(_UID, 1396) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1397, PCNT))
    {
        Device(Q397) { Name(_HID, "ACPI0007") Name(_UID, 1397) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1398, PCNT))
    {
        Device(Q398) { Name(_HID, "ACPI0007") Name(_UID, 1398) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1399, PCNT))
    {
        Device(Q399) { Name(_HID, "ACPI0007") Name(_UID, 1399) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1400, PCNT))
    {
        Device(Q400) { Name(_HID, "ACPI0007") Name(_UID, 1400) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1401, PCNT))
    {
        Device(Q401) { Name(_HID, "ACPI0007") Name(_UID, 1401) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1402, PCNT))
    {
        Device(Q402) { Name(_HID, "ACPI0007") Name(_UID, 1402) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1403, PCNT))
    {
        Device(Q403) { Name(_HID, "ACPI0007") Name(_UID, 1403) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1404, PCNT))
    {
        Device(Q404) { Name(_HID, "ACPI0007") Name(_UID, 1404) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1405, PCNT))
    {
        Device(Q405) { Name(_HID, "ACPI0007") Name(_UID, 1405) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1406, PCNT))
    {
        Device(Q406) { Name(_HID, "ACPI0007") Name(_UID, 1406) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1407, PCNT))
    {
        Device(Q407) { Name(_HID, "ACPI0007") Name(_UID, 1407) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1408, PCNT))
    {
        Device(Q408) { Name(_HID, "ACPI0007") Name(_UID, 1408) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1409, PCNT))
    {
        Device(Q409) { Name(_HID, "ACPI0007") Name(_UID, 1409) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1410, PCNT))
    {
        Device(Q410) { Name(_HID, "ACPI0007") Name(_UID, 1410) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1411, PCNT))
    {
        Device(Q411) { Name(_HID, "ACPI0007") Name(_UID, 1411) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1412, PCNT))
    {
        Device(Q412) { Name(_HID, "ACPI0007") Name(_UID, 1412) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1413, PCNT))
    {
        Device(Q413) { Name(_HID, "ACPI0007") Name(_UID, 1413) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1414, PCNT))
    {
        Device(Q414) { Name(_HID, "ACPI0007") Name(_UID, 1414) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1415, PCNT))
    {
        Device(Q415) { Name(_HID, "ACPI0007") Name(_UID, 1415) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1416, PCNT))
    {
        Device(Q416) { Name(_HID, "ACPI0007") Name(_UID, 1416) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1417, PCNT))
    {
        Device(Q417) { Name(_HID, "ACPI0007") Name(_UID, 1417) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1418, PCNT))
    {
        Device(Q418) { Name(_HID, "ACPI0007") Name(_UID, 1418) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1419, PCNT))
    {
        Device(Q419) { Name(_HID, "ACPI0007") Name(_UID, 1419) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1420, PCNT))
    {
        Device(Q420) { Name(_HID, "ACPI0007") Name(_UID, 1420) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1421, PCNT))
    {
        Device(Q421) { Name(_HID, "ACPI0007") Name(_UID, 1421) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1422, PCNT))
    {
        Device(Q422) { Name(_HID, "ACPI0007") Name(_UID, 1422) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1423, PCNT))
    {
        Device(Q423) { Name(_HID, "ACPI0007") Name(_UID, 1423) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1424, PCNT))
    {
        Device(Q424) { Name(_HID, "ACPI0007") Name(_UID, 1424) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1425, PCNT))
    {
        Device(Q425) { Name(_HID, "ACPI0007") Name(_UID, 1425) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1426, PCNT))
    {
        Device(Q426) { Name(_HID, "ACPI0007") Name(_UID, 1426) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1427, PCNT))
    {
        Device(Q427) { Name(_HID, "ACPI0007") Name(_UID, 1427) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1428, PCNT))
    {
        Device(Q428) { Name(_HID, "ACPI0007") Name(_UID, 1428) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1429, PCNT))
    {
        Device(Q429) { Name(_HID, "ACPI0007") Name(_UID, 1429) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1430, PCNT))
    {
        Device(Q430) { Name(_HID, "ACPI0007") Name(_UID, 1430) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1431, PCNT))
    {
        Device(Q431) { Name(_HID, "ACPI0007") Name(_UID, 1431) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1432, PCNT))
    {
        Device(Q432) { Name(_HID, "ACPI0007") Name(_UID, 1432) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1433, PCNT))
    {
        Device(Q433) { Name(_HID, "ACPI0007") Name(_UID, 1433) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1434, PCNT))
    {
        Device(Q434) { Name(_HID, "ACPI0007") Name(_UID, 1434) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1435, PCNT))
    {
        Device(Q435) { Name(_HID, "ACPI0007") Name(_UID, 1435) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1436, PCNT))
    {
        Device(Q436) { Name(_HID, "ACPI0007") Name(_UID, 1436) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1437, PCNT))
    {
        Device(Q437) { Name(_HID, "ACPI0007") Name(_UID, 1437) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1438, PCNT))
    {
        Device(Q438) { Name(_HID, "ACPI0007") Name(_UID, 1438) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1439, PCNT))
    {
        Device(Q439) { Name(_HID, "ACPI0007") Name(_UID, 1439) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1440, PCNT))
    {
        Device(Q440) { Name(_HID, "ACPI0007") Name(_UID, 1440) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1441, PCNT))
    {
        Device(Q441) { Name(_HID, "ACPI0007") Name(_UID, 1441) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1442, PCNT))
    {
        Device(Q442) { Name(_HID, "ACPI0007") Name(_UID, 1442) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1443, PCNT))
    {
        Device(Q443) { Name(_HID, "ACPI0007") Name(_UID, 1443) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1444, PCNT))
    {
        Device(Q444) { Name(_HID, "ACPI0007") Name(_UID, 1444) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1445, PCNT))
    {
        Device(Q445) { Name(_HID, "ACPI0007") Name(_UID, 1445) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1446, PCNT))
    {
        Device(Q446) { Name(_HID, "ACPI0007") Name(_UID, 1446) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1447, PCNT))
    {
        Device(Q447) { Name(_HID, "ACPI0007") Name(_UID, 1447) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1448, PCNT))
    {
        Device(Q448) { Name(_HID, "ACPI0007") Name(_UID, 1448) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1449, PCNT))
    {
        Device(Q449) { Name(_HID, "ACPI0007") Name(_UID, 1449) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1450, PCNT))
    {
        Device(Q450) { Name(_HID, "ACPI0007") Name(_UID, 1450) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1451, PCNT))
    {
        Device(Q451) { Name(_HID, "ACPI0007") Name(_UID, 1451) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1452, PCNT))
    {
        Device(Q452) { Name(_HID, "ACPI0007") Name(_UID, 1452) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1453, PCNT))
    {
        Device(Q453) { Name(_HID, "ACPI0007") Name(_UID, 1453) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1454, PCNT))
    {
        Device(Q454) { Name(_HID, "ACPI0007") Name(_UID, 1454) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1455, PCNT))
    {
        Device(Q455) { Name(_HID, "ACPI0007") Name(_UID, 1455) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1456, PCNT))
    {
        Device(Q456) { Name(_HID, "ACPI0007") Name(_UID, 1456) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1457, PCNT))
    {
        Device(Q457) { Name(_HID, "ACPI0007") Name(_UID, 1457) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1458, PCNT))
    {
        Device(Q458) { Name(_HID, "ACPI0007") Name(_UID, 1458) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1459, PCNT))
    {
        Device(Q459) { Name(_HID, "ACPI0007") Name(_UID, 1459) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1460, PCNT))
    {
        Device(Q460) { Name(_HID, "ACPI0007") Name(_UID, 1460) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1461, PCNT))
    {
        Device(Q461) { Name(_HID, "ACPI0007") Name(_UID, 1461) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1462, PCNT))
    {
        Device(Q462) { Name(_HID, "ACPI0007") Name(_UID, 1462) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1463, PCNT))
    {
        Device(Q463) { Name(_HID, "ACPI0007") Name(_UID, 1463) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1464, PCNT))
    {
        Device(Q464) { Name(_HID, "ACPI0007") Name(_UID, 1464) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1465, PCNT))
    {
        Device(Q465) { Name(_HID, "ACPI0007") Name(_UID, 1465) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1466, PCNT))
    {
        Device(Q466) { Name(_HID, "ACPI0007") Name(_UID, 1466) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1467, PCNT))
    {
        Device(Q467) { Name(_HID, "ACPI0007") Name(_UID, 1467) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1468, PCNT))
    {
        Device(Q468) { Name(_HID, "ACPI0007") Name(_UID, 1468) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1469, PCNT))
    {
        Device(Q469) { Name(_HID, "ACPI0007") Name(_UID, 1469) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1470, PCNT))
    {
        Device(Q470) { Name(_HID, "ACPI0007") Name(_UID, 1470) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1471, PCNT))
    {
        Device(Q471) { Name(_HID, "ACPI0007") Name(_UID, 1471) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1472, PCNT))
    {
        Device(Q472) { Name(_HID, "ACPI0007") Name(_UID, 1472) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1473, PCNT))
    {
        Device(Q473) { Name(_HID, "ACPI0007") Name(_UID, 1473) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1474, PCNT))
    {
        Device(Q474) { Name(_HID, "ACPI0007") Name(_UID, 1474) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1475, PCNT))
    {
        Device(Q475) { Name(_HID, "ACPI0007") Name(_UID, 1475) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1476, PCNT))
    {
        Device(Q476) { Name(_HID, "ACPI0007") Name(_UID, 1476) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1477, PCNT))
    {
        Device(Q477) { Name(_HID, "ACPI0007") Name(_UID, 1477) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1478, PCNT))
    {
        Device(Q478) { Name(_HID, "ACPI0007") Name(_UID, 1478) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1479, PCNT))
    {
        Device(Q479) { Name(_HID, "ACPI0007") Name(_UID, 1479) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1480, PCNT))
    {
        Device(Q480) { Name(_HID, "ACPI0007") Name(_UID, 1480) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1481, PCNT))
    {
        Device(Q481) { Name(_HID, "ACPI0007") Name(_UID, 1481) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1482, PCNT))
    {
        Device(Q482) { Name(_HID, "ACPI0007") Name(_UID, 1482) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1483, PCNT))
    {
        Device(Q483) { Name(_HID, "ACPI0007") Name(_UID, 1483) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1484, PCNT))
    {
        Device(Q484) { Name(_HID, "ACPI0007") Name(_UID, 1484) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1485, PCNT))
    {
        Device(Q485) { Name(_HID, "ACPI0007") Name(_UID, 1485) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1486, PCNT))
    {
        Device(Q486) { Name(_HID, "ACPI0007") Name(_UID, 1486) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1487, PCNT))
    {
        Device(Q487) { Name(_HID, "ACPI0007") Name(_UID, 1487) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1488, PCNT))
    {
        Device(Q488) { Name(_HID, "ACPI0007") Name(_UID, 1488) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1489, PCNT))
    {
        Device(Q489) { Name(_HID, "ACPI0007") Name(_UID, 1489) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1490, PCNT))
    {
        Device(Q490) { Name(_HID, "ACPI0007") Name(_UID, 1490) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1491, PCNT))
    {
        Device(Q491) { Name(_HID, "ACPI0007") Name(_UID, 1491) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1492, PCNT))
    {
        Device(Q492) { Name(_HID, "ACPI0007") Name(_UID, 1492) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1493, PCNT))
    {
        Device(Q493) { Name(_HID, "ACPI0007") Name(_UID, 1493) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1494, PCNT))
    {
        Device(Q494) { Name(_HID, "ACPI0007") Name(_UID, 1494) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1495, PCNT))
    {
        Device(Q495) { Name(_HID, "ACPI0007") Name(_UID, 1495) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1496, PCNT))
    {
        Device(Q496) { Name(_HID, "ACPI0007") Name(_UID, 1496) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1497, PCNT))
    {
        Device(Q497) { Name(_HID, "ACPI0007") Name(_UID, 1497) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1498, PCNT))
    {
        Device(Q498) { Name(_HID, "ACPI0007") Name(_UID, 1498) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1499, PCNT))
    {
        Device(Q499) { Name(_HID, "ACPI0007") Name(_UID, 1499) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1500, PCNT))
    {
        Device(Q500) { Name(_HID, "ACPI0007") Name(_UID, 1500) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1501, PCNT))
    {
        Device(Q501) { Name(_HID, "ACPI0007") Name(_UID, 1501) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1502, PCNT))
    {
        Device(Q502) { Name(_HID, "ACPI0007") Name(_UID, 1502) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1503, PCNT))
    {
        Device(Q503) { Name(_HID, "ACPI0007") Name(_UID, 1503) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1504, PCNT))
    {
        Device(Q504) { Name(_HID, "ACPI0007") Name(_UID, 1504) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1505, PCNT))
    {
        Device(Q505) { Name(_HID, "ACPI0007") Name(_UID, 1505) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1506, PCNT))
    {
        Device(Q506) { Name(_HID, "ACPI0007") Name(_UID, 1506) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1507, PCNT))
    {
        Device(Q507) { Name(_HID, "ACPI0007") Name(_UID, 1507) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1508, PCNT))
    {
        Device(Q508) { Name(_HID, "ACPI0007") Name(_UID, 1508) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1509, PCNT))
    {
        Device(Q509) { Name(_HID, "ACPI0007") Name(_UID, 1509) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1510, PCNT))
    {
        Device(Q510) { Name(_HID, "ACPI0007") Name(_UID, 1510) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1511, PCNT))
    {
        Device(Q511) { Name(_HID, "ACPI0007") Name(_UID, 1511) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1512, PCNT))
    {
        Device(Q512) { Name(_HID, "ACPI0007") Name(_UID, 1512) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1513, PCNT))
    {
        Device(Q513) { Name(_HID, "ACPI0007") Name(_UID, 1513) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1514, PCNT))
    {
        Device(Q514) { Name(_HID, "ACPI0007") Name(_UID, 1514) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1515, PCNT))
    {
        Device(Q515) { Name(_HID, "ACPI0007") Name(_UID, 1515) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1516, PCNT))
    {
        Device(Q516) { Name(_HID, "ACPI0007") Name(_UID, 1516) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1517, PCNT))
    {
        Device(Q517) { Name(_HID, "ACPI0007") Name(_UID, 1517) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1518, PCNT))
    {
        Device(Q518) { Name(_HID, "ACPI0007") Name(_UID, 1518) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1519, PCNT))
    {
        Device(Q519) { Name(_HID, "ACPI0007") Name(_UID, 1519) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1520, PCNT))
    {
        Device(Q520) { Name(_HID, "ACPI0007") Name(_UID, 1520) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1521, PCNT))
    {
        Device(Q521) { Name(_HID, "ACPI0007") Name(_UID, 1521) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1522, PCNT))
    {
        Device(Q522) { Name(_HID, "ACPI0007") Name(_UID, 1522) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1523, PCNT))
    {
        Device(Q523) { Name(_HID, "ACPI0007") Name(_UID, 1523) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1524, PCNT))
    {
        Device(Q524) { Name(_HID, "ACPI0007") Name(_UID, 1524) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1525, PCNT))
    {
        Device(Q525) { Name(_HID, "ACPI0007") Name(_UID, 1525) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1526, PCNT))
    {
        Device(Q526) { Name(_HID, "ACPI0007") Name(_UID, 1526) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1527, PCNT))
    {
        Device(Q527) { Name(_HID, "ACPI0007") Name(_UID, 1527) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1528, PCNT))
    {
        Device(Q528) { Name(_HID, "ACPI0007") Name(_UID, 1528) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1529, PCNT))
    {
        Device(Q529) { Name(_HID, "ACPI0007") Name(_UID, 1529) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1530, PCNT))
    {
        Device(Q530) { Name(_HID, "ACPI0007") Name(_UID, 1530) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1531, PCNT))
    {
        Device(Q531) { Name(_HID, "ACPI0007") Name(_UID, 1531) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1532, PCNT))
    {
        Device(Q532) { Name(_HID, "ACPI0007") Name(_UID, 1532) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1533, PCNT))
    {
        Device(Q533) { Name(_HID, "ACPI0007") Name(_UID, 1533) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1534, PCNT))
    {
        Device(Q534) { Name(_HID, "ACPI0007") Name(_UID, 1534) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1535, PCNT))
    {
        Device(Q535) { Name(_HID, "ACPI0007") Name(_UID, 1535) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1536, PCNT))
    {
        Device(Q536) { Name(_HID, "ACPI0007") Name(_UID, 1536) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1537, PCNT))
    {
        Device(Q537) { Name(_HID, "ACPI0007") Name(_UID, 1537) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1538, PCNT))
    {
        Device(Q538) { Name(_HID, "ACPI0007") Name(_UID, 1538) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1539, PCNT))
    {
        Device(Q539) { Name(_HID, "ACPI0007") Name(_UID, 1539) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1540, PCNT))
    {
        Device(Q540) { Name(_HID, "ACPI0007") Name(_UID, 1540) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1541, PCNT))
    {
        Device(Q541) { Name(_HID, "ACPI0007") Name(_UID, 1541) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1542, PCNT))
    {
        Device(Q542) { Name(_HID, "ACPI0007") Name(_UID, 1542) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1543, PCNT))
    {
        Device(Q543) { Name(_HID, "ACPI0007") Name(_UID, 1543) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1544, PCNT))
    {
        Device(Q544) { Name(_HID, "ACPI0007") Name(_UID, 1544) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1545, PCNT))
    {
        Device(Q545) { Name(_HID, "ACPI0007") Name(_UID, 1545) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1546, PCNT))
    {
        Device(Q546) { Name(_HID, "ACPI0007") Name(_UID, 1546) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1547, PCNT))
    {
        Device(Q547) { Name(_HID, "ACPI0007") Name(_UID, 1547) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1548, PCNT))
    {
        Device(Q548) { Name(_HID, "ACPI0007") Name(_UID, 1548) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1549, PCNT))
    {
        Device(Q549) { Name(_HID, "ACPI0007") Name(_UID, 1549) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1550, PCNT))
    {
        Device(Q550) { Name(_HID, "ACPI0007") Name(_UID, 1550) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1551, PCNT))
    {
        Device(Q551) { Name(_HID, "ACPI0007") Name(_UID, 1551) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1552, PCNT))
    {
        Device(Q552) { Name(_HID, "ACPI0007") Name(_UID, 1552) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1553, PCNT))
    {
        Device(Q553) { Name(_HID, "ACPI0007") Name(_UID, 1553) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1554, PCNT))
    {
        Device(Q554) { Name(_HID, "ACPI0007") Name(_UID, 1554) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1555, PCNT))
    {
        Device(Q555) { Name(_HID, "ACPI0007") Name(_UID, 1555) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1556, PCNT))
    {
        Device(Q556) { Name(_HID, "ACPI0007") Name(_UID, 1556) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1557, PCNT))
    {
        Device(Q557) { Name(_HID, "ACPI0007") Name(_UID, 1557) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1558, PCNT))
    {
        Device(Q558) { Name(_HID, "ACPI0007") Name(_UID, 1558) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1559, PCNT))
    {
        Device(Q559) { Name(_HID, "ACPI0007") Name(_UID, 1559) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1560, PCNT))
    {
        Device(Q560) { Name(_HID, "ACPI0007") Name(_UID, 1560) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1561, PCNT))
    {
        Device(Q561) { Name(_HID, "ACPI0007") Name(_UID, 1561) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1562, PCNT))
    {
        Device(Q562) { Name(_HID, "ACPI0007") Name(_UID, 1562) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1563, PCNT))
    {
        Device(Q563) { Name(_HID, "ACPI0007") Name(_UID, 1563) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1564, PCNT))
    {
        Device(Q564) { Name(_HID, "ACPI0007") Name(_UID, 1564) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1565, PCNT))
    {
        Device(Q565) { Name(_HID, "ACPI0007") Name(_UID, 1565) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1566, PCNT))
    {
        Device(Q566) { Name(_HID, "ACPI0007") Name(_UID, 1566) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1567, PCNT))
    {
        Device(Q567) { Name(_HID, "ACPI0007") Name(_UID, 1567) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1568, PCNT))
    {
        Device(Q568) { Name(_HID, "ACPI0007") Name(_UID, 1568) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1569, PCNT))
    {
        Device(Q569) { Name(_HID, "ACPI0007") Name(_UID, 1569) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1570, PCNT))
    {
        Device(Q570) { Name(_HID, "ACPI0007") Name(_UID, 1570) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1571, PCNT))
    {
        Device(Q571) { Name(_HID, "ACPI0007") Name(_UID, 1571) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1572, PCNT))
    {
        Device(Q572) { Name(_HID, "ACPI0007") Name(_UID, 1572) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1573, PCNT))
    {
        Device(Q573) { Name(_HID, "ACPI0007") Name(_UID, 1573) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1574, PCNT))
    {
        Device(Q574) { Name(_HID, "ACPI0007") Name(_UID, 1574) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1575, PCNT))
    {
        Device(Q575) { Name(_HID, "ACPI0007") Name(_UID, 1575) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1576, PCNT))
    {
        Device(Q576) { Name(_HID, "ACPI0007") Name(_UID, 1576) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1577, PCNT))
    {
        Device(Q577) { Name(_HID, "ACPI0007") Name(_UID, 1577) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1578, PCNT))
    {
        Device(Q578) { Name(_HID, "ACPI0007") Name(_UID, 1578) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1579, PCNT))
    {
        Device(Q579) { Name(_HID, "ACPI0007") Name(_UID, 1579) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1580, PCNT))
    {
        Device(Q580) { Name(_HID, "ACPI0007") Name(_UID, 1580) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1581, PCNT))
    {
        Device(Q581) { Name(_HID, "ACPI0007") Name(_UID, 1581) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1582, PCNT))
    {
        Device(Q582) { Name(_HID, "ACPI0007") Name(_UID, 1582) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1583, PCNT))
    {
        Device(Q583) { Name(_HID, "ACPI0007") Name(_UID, 1583) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1584, PCNT))
    {
        Device(Q584) { Name(_HID, "ACPI0007") Name(_UID, 1584) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1585, PCNT))
    {
        Device(Q585) { Name(_HID, "ACPI0007") Name(_UID, 1585) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1586, PCNT))
    {
        Device(Q586) { Name(_HID, "ACPI0007") Name(_UID, 1586) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1587, PCNT))
    {
        Device(Q587) { Name(_HID, "ACPI0007") Name(_UID, 1587) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1588, PCNT))
    {
        Device(Q588) { Name(_HID, "ACPI0007") Name(_UID, 1588) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1589, PCNT))
    {
        Device(Q589) { Name(_HID, "ACPI0007") Name(_UID, 1589) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1590, PCNT))
    {
        Device(Q590) { Name(_HID, "ACPI0007") Name(_UID, 1590) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1591, PCNT))
    {
        Device(Q591) { Name(_HID, "ACPI0007") Name(_UID, 1591) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1592, PCNT))
    {
        Device(Q592) { Name(_HID, "ACPI0007") Name(_UID, 1592) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1593, PCNT))
    {
        Device(Q593) { Name(_HID, "ACPI0007") Name(_UID, 1593) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1594, PCNT))
    {
        Device(Q594) { Name(_HID, "ACPI0007") Name(_UID, 1594) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1595, PCNT))
    {
        Device(Q595) { Name(_HID, "ACPI0007") Name(_UID, 1595) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1596, PCNT))
    {
        Device(Q596) { Name(_HID, "ACPI0007") Name(_UID, 1596) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1597, PCNT))
    {
        Device(Q597) { Name(_HID, "ACPI0007") Name(_UID, 1597) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1598, PCNT))
    {
        Device(Q598) { Name(_HID, "ACPI0007") Name(_UID, 1598) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1599, PCNT))
    {
        Device(Q599) { Name(_HID, "ACPI0007") Name(_UID, 1599) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1600, PCNT))
    {
        Device(Q600) { Name(_HID, "ACPI0007") Name(_UID, 1600) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1601, PCNT))
    {
        Device(Q601) { Name(_HID, "ACPI0007") Name(_UID, 1601) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1602, PCNT))
    {
        Device(Q602) { Name(_HID, "ACPI0007") Name(_UID, 1602) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1603, PCNT))
    {
        Device(Q603) { Name(_HID, "ACPI0007") Name(_UID, 1603) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1604, PCNT))
    {
        Device(Q604) { Name(_HID, "ACPI0007") Name(_UID, 1604) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1605, PCNT))
    {
        Device(Q605) { Name(_HID, "ACPI0007") Name(_UID, 1605) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1606, PCNT))
    {
        Device(Q606) { Name(_HID, "ACPI0007") Name(_UID, 1606) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1607, PCNT))
    {
        Device(Q607) { Name(_HID, "ACPI0007") Name(_UID, 1607) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1608, PCNT))
    {
        Device(Q608) { Name(_HID, "ACPI0007") Name(_UID, 1608) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1609, PCNT))
    {
        Device(Q609) { Name(_HID, "ACPI0007") Name(_UID, 1609) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1610, PCNT))
    {
        Device(Q610) { Name(_HID, "ACPI0007") Name(_UID, 1610) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1611, PCNT))
    {
        Device(Q611) { Name(_HID, "ACPI0007") Name(_UID, 1611) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1612, PCNT))
    {
        Device(Q612) { Name(_HID, "ACPI0007") Name(_UID, 1612) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1613, PCNT))
    {
        Device(Q613) { Name(_HID, "ACPI0007") Name(_UID, 1613) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1614, PCNT))
    {
        Device(Q614) { Name(_HID, "ACPI0007") Name(_UID, 1614) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1615, PCNT))
    {
        Device(Q615) { Name(_HID, "ACPI0007") Name(_UID, 1615) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1616, PCNT))
    {
        Device(Q616) { Name(_HID, "ACPI0007") Name(_UID, 1616) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1617, PCNT))
    {
        Device(Q617) { Name(_HID, "ACPI0007") Name(_UID, 1617) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1618, PCNT))
    {
        Device(Q618) { Name(_HID, "ACPI0007") Name(_UID, 1618) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1619, PCNT))
    {
        Device(Q619) { Name(_HID, "ACPI0007") Name(_UID, 1619) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1620, PCNT))
    {
        Device(Q620) { Name(_HID, "ACPI0007") Name(_UID, 1620) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1621, PCNT))
    {
        Device(Q621) { Name(_HID, "ACPI0007") Name(_UID, 1621) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1622, PCNT))
    {
        Device(Q622) { Name(_HID, "ACPI0007") Name(_UID, 1622) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1623, PCNT))
    {
        Device(Q623) { Name(_HID, "ACPI0007") Name(_UID, 1623) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1624, PCNT))
    {
        Device(Q624) { Name(_HID, "ACPI0007") Name(_UID, 1624) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1625, PCNT))
    {
        Device(Q625) { Name(_HID, "ACPI0007") Name(_UID, 1625) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1626, PCNT))
    {
        Device(Q626) { Name(_HID, "ACPI0007") Name(_UID, 1626) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1627, PCNT))
    {
        Device(Q627) { Name(_HID, "ACPI0007") Name(_UID, 1627) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1628, PCNT))
    {
        Device(Q628) { Name(_HID, "ACPI0007") Name(_UID, 1628) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1629, PCNT))
    {
        Device(Q629) { Name(_HID, "ACPI0007") Name(_UID, 1629) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1630, PCNT))
    {
        Device(Q630) { Name(_HID, "ACPI0007") Name(_UID, 1630) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1631, PCNT))
    {
        Device(Q631) { Name(_HID, "ACPI0007") Name(_UID, 1631) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1632, PCNT))
    {
        Device(Q632) { Name(_HID, "ACPI0007") Name(_UID, 1632) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1633, PCNT))
    {
        Device(Q633) { Name(_HID, "ACPI0007") Name(_UID, 1633) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1634, PCNT))
    {
        Device(Q634) { Name(_HID, "ACPI0007") Name(_UID, 1634) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1635, PCNT))
    {
        Device(Q635) { Name(_HID, "ACPI0007") Name(_UID, 1635) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1636, PCNT))
    {
        Device(Q636) { Name(_HID, "ACPI0007") Name(_UID, 1636) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1637, PCNT))
    {
        Device(Q637) { Name(_HID, "ACPI0007") Name(_UID, 1637) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1638, PCNT))
    {
        Device(Q638) { Name(_HID, "ACPI0007") Name(_UID, 1638) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1639, PCNT))
    {
        Device(Q639) { Name(_HID, "ACPI0007") Name(_UID, 1639) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1640, PCNT))
    {
        Device(Q640) { Name(_HID, "ACPI0007") Name(_UID, 1640) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1641, PCNT))
    {
        Device(Q641) { Name(_HID, "ACPI0007") Name(_UID, 1641) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1642, PCNT))
    {
        Device(Q642) { Name(_HID, "ACPI0007") Name(_UID, 1642) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1643, PCNT))
    {
        Device(Q643) { Name(_HID, "ACPI0007") Name(_UID, 1643) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1644, PCNT))
    {
        Device(Q644) { Name(_HID, "ACPI0007") Name(_UID, 1644) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1645, PCNT))
    {
        Device(Q645) { Name(_HID, "ACPI0007") Name(_UID, 1645) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1646, PCNT))
    {
        Device(Q646) { Name(_HID, "ACPI0007") Name(_UID, 1646) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1647, PCNT))
    {
        Device(Q647) { Name(_HID, "ACPI0007") Name(_UID, 1647) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1648, PCNT))
    {
        Device(Q648) { Name(_HID, "ACPI0007") Name(_UID, 1648) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1649, PCNT))
    {
        Device(Q649) { Name(_HID, "ACPI0007") Name(_UID, 1649) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1650, PCNT))
    {
        Device(Q650) { Name(_HID, "ACPI0007") Name(_UID, 1650) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1651, PCNT))
    {
        Device(Q651) { Name(_HID, "ACPI0007") Name(_UID, 1651) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1652, PCNT))
    {
        Device(Q652) { Name(_HID, "ACPI0007") Name(_UID, 1652) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1653, PCNT))
    {
        Device(Q653) { Name(_HID, "ACPI0007") Name(_UID, 1653) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1654, PCNT))
    {
        Device(Q654) { Name(_HID, "ACPI0007") Name(_UID, 1654) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1655, PCNT))
    {
        Device(Q655) { Name(_HID, "ACPI0007") Name(_UID, 1655) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1656, PCNT))
    {
        Device(Q656) { Name(_HID, "ACPI0007") Name(_UID, 1656) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1657, PCNT))
    {
        Device(Q657) { Name(_HID, "ACPI0007") Name(_UID, 1657) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1658, PCNT))
    {
        Device(Q658) { Name(_HID, "ACPI0007") Name(_UID, 1658) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1659, PCNT))
    {
        Device(Q659) { Name(_HID, "ACPI0007") Name(_UID, 1659) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1660, PCNT))
    {
        Device(Q660) { Name(_HID, "ACPI0007") Name(_UID, 1660) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1661, PCNT))
    {
        Device(Q661) { Name(_HID, "ACPI0007") Name(_UID, 1661) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1662, PCNT))
    {
        Device(Q662) { Name(_HID, "ACPI0007") Name(_UID, 1662) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1663, PCNT))
    {
        Device(Q663) { Name(_HID, "ACPI0007") Name(_UID, 1663) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1664, PCNT))
    {
        Device(Q664) { Name(_HID, "ACPI0007") Name(_UID, 1664) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1665, PCNT))
    {
        Device(Q665) { Name(_HID, "ACPI0007") Name(_UID, 1665) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1666, PCNT))
    {
        Device(Q666) { Name(_HID, "ACPI0007") Name(_UID, 1666) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1667, PCNT))
    {
        Device(Q667) { Name(_HID, "ACPI0007") Name(_UID, 1667) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1668, PCNT))
    {
        Device(Q668) { Name(_HID, "ACPI0007") Name(_UID, 1668) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1669, PCNT))
    {
        Device(Q669) { Name(_HID, "ACPI0007") Name(_UID, 1669) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1670, PCNT))
    {
        Device(Q670) { Name(_HID, "ACPI0007") Name(_UID, 1670) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1671, PCNT))
    {
        Device(Q671) { Name(_HID, "ACPI0007") Name(_UID, 1671) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1672, PCNT))
    {
        Device(Q672) { Name(_HID, "ACPI0007") Name(_UID, 1672) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1673, PCNT))
    {
        Device(Q673) { Name(_HID, "ACPI0007") Name(_UID, 1673) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1674, PCNT))
    {
        Device(Q674) { Name(_HID, "ACPI0007") Name(_UID, 1674) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1675, PCNT))
    {
        Device(Q675) { Name(_HID, "ACPI0007") Name(_UID, 1675) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1676, PCNT))
    {
        Device(Q676) { Name(_HID, "ACPI0007") Name(_UID, 1676) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1677, PCNT))
    {
        Device(Q677) { Name(_HID, "ACPI0007") Name(_UID, 1677) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1678, PCNT))
    {
        Device(Q678) { Name(_HID, "ACPI0007") Name(_UID, 1678) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1679, PCNT))
    {
        Device(Q679) { Name(_HID, "ACPI0007") Name(_UID, 1679) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1680, PCNT))
    {
        Device(Q680) { Name(_HID, "ACPI0007") Name(_UID, 1680) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1681, PCNT))
    {
        Device(Q681) { Name(_HID, "ACPI0007") Name(_UID, 1681) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1682, PCNT))
    {
        Device(Q682) { Name(_HID, "ACPI0007") Name(_UID, 1682) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1683, PCNT))
    {
        Device(Q683) { Name(_HID, "ACPI0007") Name(_UID, 1683) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1684, PCNT))
    {
        Device(Q684) { Name(_HID, "ACPI0007") Name(_UID, 1684) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1685, PCNT))
    {
        Device(Q685) { Name(_HID, "ACPI0007") Name(_UID, 1685) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1686, PCNT))
    {
        Device(Q686) { Name(_HID, "ACPI0007") Name(_UID, 1686) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1687, PCNT))
    {
        Device(Q687) { Name(_HID, "ACPI0007") Name(_UID, 1687) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1688, PCNT))
    {
        Device(Q688) { Name(_HID, "ACPI0007") Name(_UID, 1688) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1689, PCNT))
    {
        Device(Q689) { Name(_HID, "ACPI0007") Name(_UID, 1689) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1690, PCNT))
    {
        Device(Q690) { Name(_HID, "ACPI0007") Name(_UID, 1690) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1691, PCNT))
    {
        Device(Q691) { Name(_HID, "ACPI0007") Name(_UID, 1691) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1692, PCNT))
    {
        Device(Q692) { Name(_HID, "ACPI0007") Name(_UID, 1692) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1693, PCNT))
    {
        Device(Q693) { Name(_HID, "ACPI0007") Name(_UID, 1693) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1694, PCNT))
    {
        Device(Q694) { Name(_HID, "ACPI0007") Name(_UID, 1694) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1695, PCNT))
    {
        Device(Q695) { Name(_HID, "ACPI0007") Name(_UID, 1695) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1696, PCNT))
    {
        Device(Q696) { Name(_HID, "ACPI0007") Name(_UID, 1696) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1697, PCNT))
    {
        Device(Q697) { Name(_HID, "ACPI0007") Name(_UID, 1697) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1698, PCNT))
    {
        Device(Q698) { Name(_HID, "ACPI0007") Name(_UID, 1698) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1699, PCNT))
    {
        Device(Q699) { Name(_HID, "ACPI0007") Name(_UID, 1699) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1700, PCNT))
    {
        Device(Q700) { Name(_HID, "ACPI0007") Name(_UID, 1700) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1701, PCNT))
    {
        Device(Q701) { Name(_HID, "ACPI0007") Name(_UID, 1701) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1702, PCNT))
    {
        Device(Q702) { Name(_HID, "ACPI0007") Name(_UID, 1702) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1703, PCNT))
    {
        Device(Q703) { Name(_HID, "ACPI0007") Name(_UID, 1703) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1704, PCNT))
    {
        Device(Q704) { Name(_HID, "ACPI0007") Name(_UID, 1704) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1705, PCNT))
    {
        Device(Q705) { Name(_HID, "ACPI0007") Name(_UID, 1705) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1706, PCNT))
    {
        Device(Q706) { Name(_HID, "ACPI0007") Name(_UID, 1706) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1707, PCNT))
    {
        Device(Q707) { Name(_HID, "ACPI0007") Name(_UID, 1707) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1708, PCNT))
    {
        Device(Q708) { Name(_HID, "ACPI0007") Name(_UID, 1708) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1709, PCNT))
    {
        Device(Q709) { Name(_HID, "ACPI0007") Name(_UID, 1709) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1710, PCNT))
    {
        Device(Q710) { Name(_HID, "ACPI0007") Name(_UID, 1710) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1711, PCNT))
    {
        Device(Q711) { Name(_HID, "ACPI0007") Name(_UID, 1711) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1712, PCNT))
    {
        Device(Q712) { Name(_HID, "ACPI0007") Name(_UID, 1712) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1713, PCNT))
    {
        Device(Q713) { Name(_HID, "ACPI0007") Name(_UID, 1713) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1714, PCNT))
    {
        Device(Q714) { Name(_HID, "ACPI0007") Name(_UID, 1714) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1715, PCNT))
    {
        Device(Q715) { Name(_HID, "ACPI0007") Name(_UID, 1715) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1716, PCNT))
    {
        Device(Q716) { Name(_HID, "ACPI0007") Name(_UID, 1716) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1717, PCNT))
    {
        Device(Q717) { Name(_HID, "ACPI0007") Name(_UID, 1717) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1718, PCNT))
    {
        Device(Q718) { Name(_HID, "ACPI0007") Name(_UID, 1718) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1719, PCNT))
    {
        Device(Q719) { Name(_HID, "ACPI0007") Name(_UID, 1719) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1720, PCNT))
    {
        Device(Q720) { Name(_HID, "ACPI0007") Name(_UID, 1720) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1721, PCNT))
    {
        Device(Q721) { Name(_HID, "ACPI0007") Name(_UID, 1721) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1722, PCNT))
    {
        Device(Q722) { Name(_HID, "ACPI0007") Name(_UID, 1722) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1723, PCNT))
    {
        Device(Q723) { Name(_HID, "ACPI0007") Name(_UID, 1723) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1724, PCNT))
    {
        Device(Q724) { Name(_HID, "ACPI0007") Name(_UID, 1724) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1725, PCNT))
    {
        Device(Q725) { Name(_HID, "ACPI0007") Name(_UID, 1725) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1726, PCNT))
    {
        Device(Q726) { Name(_HID, "ACPI0007") Name(_UID, 1726) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1727, PCNT))
    {
        Device(Q727) { Name(_HID, "ACPI0007") Name(_UID, 1727) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1728, PCNT))
    {
        Device(Q728) { Name(_HID, "ACPI0007") Name(_UID, 1728) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1729, PCNT))
    {
        Device(Q729) { Name(_HID, "ACPI0007") Name(_UID, 1729) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1730, PCNT))
    {
        Device(Q730) { Name(_HID, "ACPI0007") Name(_UID, 1730) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1731, PCNT))
    {
        Device(Q731) { Name(_HID, "ACPI0007") Name(_UID, 1731) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1732, PCNT))
    {
        Device(Q732) { Name(_HID, "ACPI0007") Name(_UID, 1732) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1733, PCNT))
    {
        Device(Q733) { Name(_HID, "ACPI0007") Name(_UID, 1733) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1734, PCNT))
    {
        Device(Q734) { Name(_HID, "ACPI0007") Name(_UID, 1734) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1735, PCNT))
    {
        Device(Q735) { Name(_HID, "ACPI0007") Name(_UID, 1735) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1736, PCNT))
    {
        Device(Q736) { Name(_HID, "ACPI0007") Name(_UID, 1736) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1737, PCNT))
    {
        Device(Q737) { Name(_HID, "ACPI0007") Name(_UID, 1737) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1738, PCNT))
    {
        Device(Q738) { Name(_HID, "ACPI0007") Name(_UID, 1738) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1739, PCNT))
    {
        Device(Q739) { Name(_HID, "ACPI0007") Name(_UID, 1739) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1740, PCNT))
    {
        Device(Q740) { Name(_HID, "ACPI0007") Name(_UID, 1740) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1741, PCNT))
    {
        Device(Q741) { Name(_HID, "ACPI0007") Name(_UID, 1741) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1742, PCNT))
    {
        Device(Q742) { Name(_HID, "ACPI0007") Name(_UID, 1742) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1743, PCNT))
    {
        Device(Q743) { Name(_HID, "ACPI0007") Name(_UID, 1743) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1744, PCNT))
    {
        Device(Q744) { Name(_HID, "ACPI0007") Name(_UID, 1744) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1745, PCNT))
    {
        Device(Q745) { Name(_HID, "ACPI0007") Name(_UID, 1745) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1746, PCNT))
    {
        Device(Q746) { Name(_HID, "ACPI0007") Name(_UID, 1746) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1747, PCNT))
    {
        Device(Q747) { Name(_HID, "ACPI0007") Name(_UID, 1747) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1748, PCNT))
    {
        Device(Q748) { Name(_HID, "ACPI0007") Name(_UID, 1748) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1749, PCNT))
    {
        Device(Q749) { Name(_HID, "ACPI0007") Name(_UID, 1749) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1750, PCNT))
    {
        Device(Q750) { Name(_HID, "ACPI0007") Name(_UID, 1750) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1751, PCNT))
    {
        Device(Q751) { Name(_HID, "ACPI0007") Name(_UID, 1751) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1752, PCNT))
    {
        Device(Q752) { Name(_HID, "ACPI0007") Name(_UID, 1752) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1753, PCNT))
    {
        Device(Q753) { Name(_HID, "ACPI0007") Name(_UID, 1753) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1754, PCNT))
    {
        Device(Q754) { Name(_HID, "ACPI0007") Name(_UID, 1754) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1755, PCNT))
    {
        Device(Q755) { Name(_HID, "ACPI0007") Name(_UID, 1755) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1756, PCNT))
    {
        Device(Q756) { Name(_HID, "ACPI0007") Name(_UID, 1756) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1757, PCNT))
    {
        Device(Q757) { Name(_HID, "ACPI0007") Name(_UID, 1757) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1758, PCNT))
    {
        Device(Q758) { Name(_HID, "ACPI0007") Name(_UID, 1758) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1759, PCNT))
    {
        Device(Q759) { Name(_HID, "ACPI0007") Name(_UID, 1759) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1760, PCNT))
    {
        Device(Q760) { Name(_HID, "ACPI0007") Name(_UID, 1760) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1761, PCNT))
    {
        Device(Q761) { Name(_HID, "ACPI0007") Name(_UID, 1761) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1762, PCNT))
    {
        Device(Q762) { Name(_HID, "ACPI0007") Name(_UID, 1762) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1763, PCNT))
    {
        Device(Q763) { Name(_HID, "ACPI0007") Name(_UID, 1763) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1764, PCNT))
    {
        Device(Q764) { Name(_HID, "ACPI0007") Name(_UID, 1764) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1765, PCNT))
    {
        Device(Q765) { Name(_HID, "ACPI0007") Name(_UID, 1765) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1766, PCNT))
    {
        Device(Q766) { Name(_HID, "ACPI0007") Name(_UID, 1766) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1767, PCNT))
    {
        Device(Q767) { Name(_HID, "ACPI0007") Name(_UID, 1767) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1768, PCNT))
    {
        Device(Q768) { Name(_HID, "ACPI0007") Name(_UID, 1768) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1769, PCNT))
    {
        Device(Q769) { Name(_HID, "ACPI0007") Name(_UID, 1769) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1770, PCNT))
    {
        Device(Q770) { Name(_HID, "ACPI0007") Name(_UID, 1770) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1771, PCNT))
    {
        Device(Q771) { Name(_HID, "ACPI0007") Name(_UID, 1771) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1772, PCNT))
    {
        Device(Q772) { Name(_HID, "ACPI0007") Name(_UID, 1772) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1773, PCNT))
    {
        Device(Q773) { Name(_HID, "ACPI0007") Name(_UID, 1773) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1774, PCNT))
    {
        Device(Q774) { Name(_HID, "ACPI0007") Name(_UID, 1774) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1775, PCNT))
    {
        Device(Q775) { Name(_HID, "ACPI0007") Name(_UID, 1775) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1776, PCNT))
    {
        Device(Q776) { Name(_HID, "ACPI0007") Name(_UID, 1776) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1777, PCNT))
    {
        Device(Q777) { Name(_HID, "ACPI0007") Name(_UID, 1777) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1778, PCNT))
    {
        Device(Q778) { Name(_HID, "ACPI0007") Name(_UID, 1778) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1779, PCNT))
    {
        Device(Q779) { Name(_HID, "ACPI0007") Name(_UID, 1779) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1780, PCNT))
    {
        Device(Q780) { Name(_HID, "ACPI0007") Name(_UID, 1780) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1781, PCNT))
    {
        Device(Q781) { Name(_HID, "ACPI0007") Name(_UID, 1781) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1782, PCNT))
    {
        Device(Q782) { Name(_HID, "ACPI0007") Name(_UID, 1782) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1783, PCNT))
    {
        Device(Q783) { Name(_HID, "ACPI0007") Name(_UID, 1783) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1784, PCNT))
    {
        Device(Q784) { Name(_HID, "ACPI0007") Name(_UID, 1784) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1785, PCNT))
    {
        Device(Q785) { Name(_HID, "ACPI0007") Name(_UID, 1785) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1786, PCNT))
    {
        Device(Q786) { Name(_HID, "ACPI0007") Name(_UID, 1786) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1787, PCNT))
    {
        Device(Q787) { Name(_HID, "ACPI0007") Name(_UID, 1787) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1788, PCNT))
    {
        Device(Q788) { Name(_HID, "ACPI0007") Name(_UID, 1788) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1789, PCNT))
    {
        Device(Q789) { Name(_HID, "ACPI0007") Name(_UID, 1789) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1790, PCNT))
    {
        Device(Q790) { Name(_HID, "ACPI0007") Name(_UID, 1790) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1791, PCNT))
    {
        Device(Q791) { Name(_HID, "ACPI0007") Name(_UID, 1791) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1792, PCNT))
    {
        Device(Q792) { Name(_HID, "ACPI0007") Name(_UID, 1792) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1793, PCNT))
    {
        Device(Q793) { Name(_HID, "ACPI0007") Name(_UID, 1793) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1794, PCNT))
    {
        Device(Q794) { Name(_HID, "ACPI0007") Name(_UID, 1794) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1795, PCNT))
    {
        Device(Q795) { Name(_HID, "ACPI0007") Name(_UID, 1795) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1796, PCNT))
    {
        Device(Q796) { Name(_HID, "ACPI0007") Name(_UID, 1796) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1797, PCNT))
    {
        Device(Q797) { Name(_HID, "ACPI0007") Name(_UID, 1797) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1798, PCNT))
    {
        Device(Q798) { Name(_HID, "ACPI0007") Name(_UID, 1798) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1799, PCNT))
    {
        Device(Q799) { Name(_HID, "ACPI0007") Name(_UID, 1799) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1800, PCNT))
    {
        Device(Q800) { Name(_HID, "ACPI0007") Name(_UID, 1800) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1801, PCNT))
    {
        Device(Q801) { Name(_HID, "ACPI0007") Name(_UID, 1801) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1802, PCNT))
    {
        Device(Q802) { Name(_HID, "ACPI0007") Name(_UID, 1802) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1803, PCNT))
    {
        Device(Q803) { Name(_HID, "ACPI0007") Name(_UID, 1803) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1804, PCNT))
    {
        Device(Q804) { Name(_HID, "ACPI0007") Name(_UID, 1804) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1805, PCNT))
    {
        Device(Q805) { Name(_HID, "ACPI0007") Name(_UID, 1805) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1806, PCNT))
    {
        Device(Q806) { Name(_HID, "ACPI0007") Name(_UID, 1806) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1807, PCNT))
    {
        Device(Q807) { Name(_HID, "ACPI0007") Name(_UID, 1807) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1808, PCNT))
    {
        Device(Q808) { Name(_HID, "ACPI0007") Name(_UID, 1808) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1809, PCNT))
    {
        Device(Q809) { Name(_HID, "ACPI0007") Name(_UID, 1809) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1810, PCNT))
    {
        Device(Q810) { Name(_HID, "ACPI0007") Name(_UID, 1810) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1811, PCNT))
    {
        Device(Q811) { Name(_HID, "ACPI0007") Name(_UID, 1811) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1812, PCNT))
    {
        Device(Q812) { Name(_HID, "ACPI0007") Name(_UID, 1812) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1813, PCNT))
    {
        Device(Q813) { Name(_HID, "ACPI0007") Name(_UID, 1813) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1814, PCNT))
    {
        Device(Q814) { Name(_HID, "ACPI0007") Name(_UID, 1814) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1815, PCNT))
    {
        Device(Q815) { Name(_HID, "ACPI0007") Name(_UID, 1815) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1816, PCNT))
    {
        Device(Q816) { Name(_HID, "ACPI0007") Name(_UID, 1816) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1817, PCNT))
    {
        Device(Q817) { Name(_HID, "ACPI0007") Name(_UID, 1817) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1818, PCNT))
    {
        Device(Q818) { Name(_HID, "ACPI0007") Name(_UID, 1818) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1819, PCNT))
    {
        Device(Q819) { Name(_HID, "ACPI0007") Name(_UID, 1819) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1820, PCNT))
    {
        Device(Q820) { Name(_HID, "ACPI0007") Name(_UID, 1820) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1821, PCNT))
    {
        Device(Q821) { Name(_HID, "ACPI0007") Name(_UID, 1821) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1822, PCNT))
    {
        Device(Q822) { Name(_HID, "ACPI0007") Name(_UID, 1822) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1823, PCNT))
    {
        Device(Q823) { Name(_HID, "ACPI0007") Name(_UID, 1823) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1824, PCNT))
    {
        Device(Q824) { Name(_HID, "ACPI0007") Name(_UID, 1824) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1825, PCNT))
    {
        Device(Q825) { Name(_HID, "ACPI0007") Name(_UID, 1825) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1826, PCNT))
    {
        Device(Q826) { Name(_HID, "ACPI0007") Name(_UID, 1826) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1827, PCNT))
    {
        Device(Q827) { Name(_HID, "ACPI0007") Name(_UID, 1827) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1828, PCNT))
    {
        Device(Q828) { Name(_HID, "ACPI0007") Name(_UID, 1828) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1829, PCNT))
    {
        Device(Q829) { Name(_HID, "ACPI0007") Name(_UID, 1829) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1830, PCNT))
    {
        Device(Q830) { Name(_HID, "ACPI0007") Name(_UID, 1830) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1831, PCNT))
    {
        Device(Q831) { Name(_HID, "ACPI0007") Name(_UID, 1831) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1832, PCNT))
    {
        Device(Q832) { Name(_HID, "ACPI0007") Name(_UID, 1832) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1833, PCNT))
    {
        Device(Q833) { Name(_HID, "ACPI0007") Name(_UID, 1833) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1834, PCNT))
    {
        Device(Q834) { Name(_HID, "ACPI0007") Name(_UID, 1834) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1835, PCNT))
    {
        Device(Q835) { Name(_HID, "ACPI0007") Name(_UID, 1835) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1836, PCNT))
    {
        Device(Q836) { Name(_HID, "ACPI0007") Name(_UID, 1836) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1837, PCNT))
    {
        Device(Q837) { Name(_HID, "ACPI0007") Name(_UID, 1837) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1838, PCNT))
    {
        Device(Q838) { Name(_HID, "ACPI0007") Name(_UID, 1838) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1839, PCNT))
    {
        Device(Q839) { Name(_HID, "ACPI0007") Name(_UID, 1839) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1840, PCNT))
    {
        Device(Q840) { Name(_HID, "ACPI0007") Name(_UID, 1840) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1841, PCNT))
    {
        Device(Q841) { Name(_HID, "ACPI0007") Name(_UID, 1841) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1842, PCNT))
    {
        Device(Q842) { Name(_HID, "ACPI0007") Name(_UID, 1842) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1843, PCNT))
    {
        Device(Q843) { Name(_HID, "ACPI0007") Name(_UID, 1843) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1844, PCNT))
    {
        Device(Q844) { Name(_HID, "ACPI0007") Name(_UID, 1844) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1845, PCNT))
    {
        Device(Q845) { Name(_HID, "ACPI0007") Name(_UID, 1845) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1846, PCNT))
    {
        Device(Q846) { Name(_HID, "ACPI0007") Name(_UID, 1846) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1847, PCNT))
    {
        Device(Q847) { Name(_HID, "ACPI0007") Name(_UID, 1847) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1848, PCNT))
    {
        Device(Q848) { Name(_HID, "ACPI0007") Name(_UID, 1848) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1849, PCNT))
    {
        Device(Q849) { Name(_HID, "ACPI0007") Name(_UID, 1849) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1850, PCNT))
    {
        Device(Q850) { Name(_HID, "ACPI0007") Name(_UID, 1850) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1851, PCNT))
    {
        Device(Q851) { Name(_HID, "ACPI0007") Name(_UID, 1851) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1852, PCNT))
    {
        Device(Q852) { Name(_HID, "ACPI0007") Name(_UID, 1852) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1853, PCNT))
    {
        Device(Q853) { Name(_HID, "ACPI0007") Name(_UID, 1853) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1854, PCNT))
    {
        Device(Q854) { Name(_HID, "ACPI0007") Name(_UID, 1854) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1855, PCNT))
    {
        Device(Q855) { Name(_HID, "ACPI0007") Name(_UID, 1855) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1856, PCNT))
    {
        Device(Q856) { Name(_HID, "ACPI0007") Name(_UID, 1856) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1857, PCNT))
    {
        Device(Q857) { Name(_HID, "ACPI0007") Name(_UID, 1857) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1858, PCNT))
    {
        Device(Q858) { Name(_HID, "ACPI0007") Name(_UID, 1858) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1859, PCNT))
    {
        Device(Q859) { Name(_HID, "ACPI0007") Name(_UID, 1859) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1860, PCNT))
    {
        Device(Q860) { Name(_HID, "ACPI0007") Name(_UID, 1860) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1861, PCNT))
    {
        Device(Q861) { Name(_HID, "ACPI0007") Name(_UID, 1861) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1862, PCNT))
    {
        Device(Q862) { Name(_HID, "ACPI0007") Name(_UID, 1862) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1863, PCNT))
    {
        Device(Q863) { Name(_HID, "ACPI0007") Name(_UID, 1863) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1864, PCNT))
    {
        Device(Q864) { Name(_HID, "ACPI0007") Name(_UID, 1864) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1865, PCNT))
    {
        Device(Q865) { Name(_HID, "ACPI0007") Name(_UID, 1865) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1866, PCNT))
    {
        Device(Q866) { Name(_HID, "ACPI0007") Name(_UID, 1866) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1867, PCNT))
    {
        Device(Q867) { Name(_HID, "ACPI0007") Name(_UID, 1867) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1868, PCNT))
    {
        Device(Q868) { Name(_HID, "ACPI0007") Name(_UID, 1868) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1869, PCNT))
    {
        Device(Q869) { Name(_HID, "ACPI0007") Name(_UID, 1869) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1870, PCNT))
    {
        Device(Q870) { Name(_HID, "ACPI0007") Name(_UID, 1870) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1871, PCNT))
    {
        Device(Q871) { Name(_HID, "ACPI0007") Name(_UID, 1871) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1872, PCNT))
    {
        Device(Q872) { Name(_HID, "ACPI0007") Name(_UID, 1872) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1873, PCNT))
    {
        Device(Q873) { Name(_HID, "ACPI0007") Name(_UID, 1873) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1874, PCNT))
    {
        Device(Q874) { Name(_HID, "ACPI0007") Name(_UID, 1874) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1875, PCNT))
    {
        Device(Q875) { Name(_HID, "ACPI0007") Name(_UID, 1875) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1876, PCNT))
    {
        Device(Q876) { Name(_HID, "ACPI0007") Name(_UID, 1876) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1877, PCNT))
    {
        Device(Q877) { Name(_HID, "ACPI0007") Name(_UID, 1877) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1878, PCNT))
    {
        Device(Q878) { Name(_HID, "ACPI0007") Name(_UID, 1878) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1879, PCNT))
    {
        Device(Q879) { Name(_HID, "ACPI0007") Name(_UID, 1879) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1880, PCNT))
    {
        Device(Q880) { Name(_HID, "ACPI0007") Name(_UID, 1880) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1881, PCNT))
    {
        Device(Q881) { Name(_HID, "ACPI0007") Name(_UID, 1881) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1882, PCNT))
    {
        Device(Q882) { Name(_HID, "ACPI0007") Name(_UID, 1882) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1883, PCNT))
    {
        Device(Q883) { Name(_HID, "ACPI0007") Name(_UID, 1883) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1884, PCNT))
    {
        Device(Q884) { Name(_HID, "ACPI0007") Name(_UID, 1884) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1885, PCNT))
    {
        Device(Q885) { Name(_HID, "ACPI0007") Name(_UID, 1885) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1886, PCNT))
    {
        Device(Q886) { Name(_HID, "ACPI0007") Name(_UID, 1886) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1887, PCNT))
    {
        Device(Q887) { Name(_HID, "ACPI0007") Name(_UID, 1887) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1888, PCNT))
    {
        Device(Q888) { Name(_HID, "ACPI0007") Name(_UID, 1888) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1889, PCNT))
    {
        Device(Q889) { Name(_HID, "ACPI0007") Name(_UID, 1889) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1890, PCNT))
    {
        Device(Q890) { Name(_HID, "ACPI0007") Name(_UID, 1890) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1891, PCNT))
    {
        Device(Q891) { Name(_HID, "ACPI0007") Name(_UID, 1891) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1892, PCNT))
    {
        Device(Q892) { Name(_HID, "ACPI0007") Name(_UID, 1892) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1893, PCNT))
    {
        Device(Q893) { Name(_HID, "ACPI0007") Name(_UID, 1893) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1894, PCNT))
    {
        Device(Q894) { Name(_HID, "ACPI0007") Name(_UID, 1894) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1895, PCNT))
    {
        Device(Q895) { Name(_HID, "ACPI0007") Name(_UID, 1895) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1896, PCNT))
    {
        Device(Q896) { Name(_HID, "ACPI0007") Name(_UID, 1896) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1897, PCNT))
    {
        Device(Q897) { Name(_HID, "ACPI0007") Name(_UID, 1897) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1898, PCNT))
    {
        Device(Q898) { Name(_HID, "ACPI0007") Name(_UID, 1898) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1899, PCNT))
    {
        Device(Q899) { Name(_HID, "ACPI0007") Name(_UID, 1899) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1900, PCNT))
    {
        Device(Q900) { Name(_HID, "ACPI0007") Name(_UID, 1900) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1901, PCNT))
    {
        Device(Q901) { Name(_HID, "ACPI0007") Name(_UID, 1901) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1902, PCNT))
    {
        Device(Q902) { Name(_HID, "ACPI0007") Name(_UID, 1902) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1903, PCNT))
    {
        Device(Q903) { Name(_HID, "ACPI0007") Name(_UID, 1903) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1904, PCNT))
    {
        Device(Q904) { Name(_HID, "ACPI0007") Name(_UID, 1904) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1905, PCNT))
    {
        Device(Q905) { Name(_HID, "ACPI0007") Name(_UID, 1905) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1906, PCNT))
    {
        Device(Q906) { Name(_HID, "ACPI0007") Name(_UID, 1906) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1907, PCNT))
    {
        Device(Q907) { Name(_HID, "ACPI0007") Name(_UID, 1907) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1908, PCNT))
    {
        Device(Q908) { Name(_HID, "ACPI0007") Name(_UID, 1908) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1909, PCNT))
    {
        Device(Q909) { Name(_HID, "ACPI0007") Name(_UID, 1909) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1910, PCNT))
    {
        Device(Q910) { Name(_HID, "ACPI0007") Name(_UID, 1910) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1911, PCNT))
    {
        Device(Q911) { Name(_HID, "ACPI0007") Name(_UID, 1911) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1912, PCNT))
    {
        Device(Q912) { Name(_HID, "ACPI0007") Name(_UID, 1912) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1913, PCNT))
    {
        Device(Q913) { Name(_HID, "ACPI0007") Name(_UID, 1913) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1914, PCNT))
    {
        Device(Q914) { Name(_HID, "ACPI0007") Name(_UID, 1914) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1915, PCNT))
    {
        Device(Q915) { Name(_HID, "ACPI0007") Name(_UID, 1915) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1916, PCNT))
    {
        Device(Q916) { Name(_HID, "ACPI0007") Name(_UID, 1916) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1917, PCNT))
    {
        Device(Q917) { Name(_HID, "ACPI0007") Name(_UID, 1917) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1918, PCNT))
    {
        Device(Q918) { Name(_HID, "ACPI0007") Name(_UID, 1918) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1919, PCNT))
    {
        Device(Q919) { Name(_HID, "ACPI0007") Name(_UID, 1919) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1920, PCNT))
    {
        Device(Q920) { Name(_HID, "ACPI0007") Name(_UID, 1920) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1921, PCNT))
    {
        Device(Q921) { Name(_HID, "ACPI0007") Name(_UID, 1921) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1922, PCNT))
    {
        Device(Q922) { Name(_HID, "ACPI0007") Name(_UID, 1922) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1923, PCNT))
    {
        Device(Q923) { Name(_HID, "ACPI0007") Name(_UID, 1923) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1924, PCNT))
    {
        Device(Q924) { Name(_HID, "ACPI0007") Name(_UID, 1924) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1925, PCNT))
    {
        Device(Q925) { Name(_HID, "ACPI0007") Name(_UID, 1925) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1926, PCNT))
    {
        Device(Q926) { Name(_HID, "ACPI0007") Name(_UID, 1926) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1927, PCNT))
    {
        Device(Q927) { Name(_HID, "ACPI0007") Name(_UID, 1927) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1928, PCNT))
    {
        Device(Q928) { Name(_HID, "ACPI0007") Name(_UID, 1928) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1929, PCNT))
    {
        Device(Q929) { Name(_HID, "ACPI0007") Name(_UID, 1929) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1930, PCNT))
    {
        Device(Q930) { Name(_HID, "ACPI0007") Name(_UID, 1930) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1931, PCNT))
    {
        Device(Q931) { Name(_HID, "ACPI0007") Name(_UID, 1931) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1932, PCNT))
    {
        Device(Q932) { Name(_HID, "ACPI0007") Name(_UID, 1932) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1933, PCNT))
    {
        Device(Q933) { Name(_HID, "ACPI0007") Name(_UID, 1933) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1934, PCNT))
    {
        Device(Q934) { Name(_HID, "ACPI0007") Name(_UID, 1934) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1935, PCNT))
    {
        Device(Q935) { Name(_HID, "ACPI0007") Name(_UID, 1935) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1936, PCNT))
    {
        Device(Q936) { Name(_HID, "ACPI0007") Name(_UID, 1936) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1937, PCNT))
    {
        Device(Q937) { Name(_HID, "ACPI0007") Name(_UID, 1937) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1938, PCNT))
    {
        Device(Q938) { Name(_HID, "ACPI0007") Name(_UID, 1938) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1939, PCNT))
    {
        Device(Q939) { Name(_HID, "ACPI0007") Name(_UID, 1939) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1940, PCNT))
    {
        Device(Q940) { Name(_HID, "ACPI0007") Name(_UID, 1940) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1941, PCNT))
    {
        Device(Q941) { Name(_HID, "ACPI0007") Name(_UID, 1941) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1942, PCNT))
    {
        Device(Q942) { Name(_HID, "ACPI0007") Name(_UID, 1942) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1943, PCNT))
    {
        Device(Q943) { Name(_HID, "ACPI0007") Name(_UID, 1943) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1944, PCNT))
    {
        Device(Q944) { Name(_HID, "ACPI0007") Name(_UID, 1944) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1945, PCNT))
    {
        Device(Q945) { Name(_HID, "ACPI0007") Name(_UID, 1945) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1946, PCNT))
    {
        Device(Q946) { Name(_HID, "ACPI0007") Name(_UID, 1946) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1947, PCNT))
    {
        Device(Q947) { Name(_HID, "ACPI0007") Name(_UID, 1947) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1948, PCNT))
    {
        Device(Q948) { Name(_HID, "ACPI0007") Name(_UID, 1948) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1949, PCNT))
    {
        Device(Q949) { Name(_HID, "ACPI0007") Name(_UID, 1949) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1950, PCNT))
    {
        Device(Q950) { Name(_HID, "ACPI0007") Name(_UID, 1950) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1951, PCNT))
    {
        Device(Q951) { Name(_HID, "ACPI0007") Name(_UID, 1951) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1952, PCNT))
    {
        Device(Q952) { Name(_HID, "ACPI0007") Name(_UID, 1952) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1953, PCNT))
    {
        Device(Q953) { Name(_HID, "ACPI0007") Name(_UID, 1953) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1954, PCNT))
    {
        Device(Q954) { Name(_HID, "ACPI0007") Name(_UID, 1954) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1955, PCNT))
    {
        Device(Q955) { Name(_HID, "ACPI0007") Name(_UID, 1955) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1956, PCNT))
    {
        Device(Q956) { Name(_HID, "ACPI0007") Name(_UID, 1956) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1957, PCNT))
    {
        Device(Q957) { Name(_HID, "ACPI0007") Name(_UID, 1957) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1958, PCNT))
    {
        Device(Q958) { Name(_HID, "ACPI0007") Name(_UID, 1958) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1959, PCNT))
    {
        Device(Q959) { Name(_HID, "ACPI0007") Name(_UID, 1959) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1960, PCNT))
    {
        Device(Q960) { Name(_HID, "ACPI0007") Name(_UID, 1960) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1961, PCNT))
    {
        Device(Q961) { Name(_HID, "ACPI0007") Name(_UID, 1961) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1962, PCNT))
    {
        Device(Q962) { Name(_HID, "ACPI0007") Name(_UID, 1962) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1963, PCNT))
    {
        Device(Q963) { Name(_HID, "ACPI0007") Name(_UID, 1963) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1964, PCNT))
    {
        Device(Q964) { Name(_HID, "ACPI0007") Name(_UID, 1964) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1965, PCNT))
    {
        Device(Q965) { Name(_HID, "ACPI0007") Name(_UID, 1965) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1966, PCNT))
    {
        Device(Q966) { Name(_HID, "ACPI0007") Name(_UID, 1966) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1967, PCNT))
    {
        Device(Q967) { Name(_HID, "ACPI0007") Name(_UID, 1967) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1968, PCNT))
    {
        Device(Q968) { Name(_HID, "ACPI0007") Name(_UID, 1968) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1969, PCNT))
    {
        Device(Q969) { Name(_HID, "ACPI0007") Name(_UID, 1969) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1970, PCNT))
    {
        Device(Q970) { Name(_HID, "ACPI0007") Name(_UID, 1970) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1971, PCNT))
    {
        Device(Q971) { Name(_HID, "ACPI0007") Name(_UID, 1971) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1972, PCNT))
    {
        Device(Q972) { Name(_HID, "ACPI0007") Name(_UID, 1972) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1973, PCNT))
    {
        Device(Q973) { Name(_HID, "ACPI0007") Name(_UID, 1973) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1974, PCNT))
    {
        Device(Q974) { Name(_HID, "ACPI0007") Name(_UID, 1974) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1975, PCNT))
    {
        Device(Q975) { Name(_HID, "ACPI0007") Name(_UID, 1975) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1976, PCNT))
    {
        Device(Q976) { Name(_HID, "ACPI0007") Name(_UID, 1976) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1977, PCNT))
    {
        Device(Q977) { Name(_HID, "ACPI0007") Name(_UID, 1977) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1978, PCNT))
    {
        Device(Q978) { Name(_HID, "ACPI0007") Name(_UID, 1978) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1979, PCNT))
    {
        Device(Q979) { Name(_HID, "ACPI0007") Name(_UID, 1979) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1980, PCNT))
    {
        Device(Q980) { Name(_HID, "ACPI0007") Name(_UID, 1980) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1981, PCNT))
    {
        Device(Q981) { Name(_HID, "ACPI0007") Name(_UID, 1981) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1982, PCNT))
    {
        Device(Q982) { Name(_HID, "ACPI0007") Name(_UID, 1982) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1983, PCNT))
    {
        Device(Q983) { Name(_HID, "ACPI0007") Name(_UID, 1983) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1984, PCNT))
    {
        Device(Q984) { Name(_HID, "ACPI0007") Name(_UID, 1984) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1985, PCNT))
    {
        Device(Q985) { Name(_HID, "ACPI0007") Name(_UID, 1985) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1986, PCNT))
    {
        Device(Q986) { Name(_HID, "ACPI0007") Name(_UID, 1986) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1987, PCNT))
    {
        Device(Q987) { Name(_HID, "ACPI0007") Name(_UID, 1987) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1988, PCNT))
    {
        Device(Q988) { Name(_HID, "ACPI0007") Name(_UID, 1988) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1989, PCNT))
    {
        Device(Q989) { Name(_HID, "ACPI0007") Name(_UID, 1989) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1990, PCNT))
    {
        Device(Q990) { Name(_HID, "ACPI0007") Name(_UID, 1990) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1991, PCNT))
    {
        Device(Q991) { Name(_HID, "ACPI0007") Name(_UID, 1991) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1992, PCNT))
    {
        Device(Q992) { Name(_HID, "ACPI0007") Name(_UID, 1992) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1993, PCNT))
    {
        Device(Q993) { Name(_HID, "ACPI0007") Name(_UID, 1993) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1994, PCNT))
    {
        Device(Q994) { Name(_HID, "ACPI0007") Name(_UID, 1994) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1995, PCNT))
    {
        Device(Q995) { Name(_HID, "ACPI0007") Name(_UID, 1995) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1996, PCNT))
    {
        Device(Q996) { Name(_HID, "ACPI0007") Name(_UID, 1996) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1997, PCNT))
    {
        Device(Q997) { Name(_HID, "ACPI0007") Name(_UID, 1997) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1998, PCNT))
    {
        Device(Q998) { Name(_HID, "ACPI0007") Name(_UID, 1998) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(1999, PCNT))
    {
        Device(Q999) { Name(_HID, "ACPI0007") Name(_UID, 1999) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2000, PCNT))
    {
        Device(R000) { Name(_HID, "ACPI0007") Name(_UID, 2000) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2001, PCNT))
    {
        Device(R001) { Name(_HID, "ACPI0007") Name(_UID, 2001) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2002, PCNT))
    {
        Device(R002) { Name(_HID, "ACPI0007") Name(_UID, 2002) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2003, PCNT))
    {
        Device(R003) { Name(_HID, "ACPI0007") Name(_UID, 2003) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2004, PCNT))
    {
        Device(R004) { Name(_HID, "ACPI0007") Name(_UID, 2004) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2005, PCNT))
    {
        Device(R005) { Name(_HID, "ACPI0007") Name(_UID, 2005) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2006, PCNT))
    {
        Device(R006) { Name(_HID, "ACPI0007") Name(_UID, 2006) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2007, PCNT))
    {
        Device(R007) { Name(_HID, "ACPI0007") Name(_UID, 2007) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2008, PCNT))
    {
        Device(R008) { Name(_HID, "ACPI0007") Name(_UID, 2008) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2009, PCNT))
    {
        Device(R009) { Name(_HID, "ACPI0007") Name(_UID, 2009) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2010, PCNT))
    {
        Device(R010) { Name(_HID, "ACPI0007") Name(_UID, 2010) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2011, PCNT))
    {
        Device(R011) { Name(_HID, "ACPI0007") Name(_UID, 2011) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2012, PCNT))
    {
        Device(R012) { Name(_HID, "ACPI0007") Name(_UID, 2012) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2013, PCNT))
    {
        Device(R013) { Name(_HID, "ACPI0007") Name(_UID, 2013) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2014, PCNT))
    {
        Device(R014) { Name(_HID, "ACPI0007") Name(_UID, 2014) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2015, PCNT))
    {
        Device(R015) { Name(_HID, "ACPI0007") Name(_UID, 2015) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2016, PCNT))
    {
        Device(R016) { Name(_HID, "ACPI0007") Name(_UID, 2016) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2017, PCNT))
    {
        Device(R017) { Name(_HID, "ACPI0007") Name(_UID, 2017) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2018, PCNT))
    {
        Device(R018) { Name(_HID, "ACPI0007") Name(_UID, 2018) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2019, PCNT))
    {
        Device(R019) { Name(_HID, "ACPI0007") Name(_UID, 2019) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2020, PCNT))
    {
        Device(R020) { Name(_HID, "ACPI0007") Name(_UID, 2020) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2021, PCNT))
    {
        Device(R021) { Name(_HID, "ACPI0007") Name(_UID, 2021) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2022, PCNT))
    {
        Device(R022) { Name(_HID, "ACPI0007") Name(_UID, 2022) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2023, PCNT))
    {
        Device(R023) { Name(_HID, "ACPI0007") Name(_UID, 2023) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2024, PCNT))
    {
        Device(R024) { Name(_HID, "ACPI0007") Name(_UID, 2024) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2025, PCNT))
    {
        Device(R025) { Name(_HID, "ACPI0007") Name(_UID, 2025) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2026, PCNT))
    {
        Device(R026) { Name(_HID, "ACPI0007") Name(_UID, 2026) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2027, PCNT))
    {
        Device(R027) { Name(_HID, "ACPI0007") Name(_UID, 2027) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2028, PCNT))
    {
        Device(R028) { Name(_HID, "ACPI0007") Name(_UID, 2028) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2029, PCNT))
    {
        Device(R029) { Name(_HID, "ACPI0007") Name(_UID, 2029) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2030, PCNT))
    {
        Device(R030) { Name(_HID, "ACPI0007") Name(_UID, 2030) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2031, PCNT))
    {
        Device(R031) { Name(_HID, "ACPI0007") Name(_UID, 2031) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2032, PCNT))
    {
        Device(R032) { Name(_HID, "ACPI0007") Name(_UID, 2032) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2033, PCNT))
    {
        Device(R033) { Name(_HID, "ACPI0007") Name(_UID, 2033) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2034, PCNT))
    {
        Device(R034) { Name(_HID, "ACPI0007") Name(_UID, 2034) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2035, PCNT))
    {
        Device(R035) { Name(_HID, "ACPI0007") Name(_UID, 2035) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2036, PCNT))
    {
        Device(R036) { Name(_HID, "ACPI0007") Name(_UID, 2036) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2037, PCNT))
    {
        Device(R037) { Name(_HID, "ACPI0007") Name(_UID, 2037) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2038, PCNT))
    {
        Device(R038) { Name(_HID, "ACPI0007") Name(_UID, 2038) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2039, PCNT))
    {
        Device(R039) { Name(_HID, "ACPI0007") Name(_UID, 2039) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2040, PCNT))
    {
        Device(R040) { Name(_HID, "ACPI0007") Name(_UID, 2040) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2041, PCNT))
    {
        Device(R041) { Name(_HID, "ACPI0007") Name(_UID, 2041) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2042, PCNT))
    {
        Device(R042) { Name(_HID, "ACPI0007") Name(_UID, 2042) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2043, PCNT))
    {
        Device(R043) { Name(_HID, "ACPI0007") Name(_UID, 2043) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2044, PCNT))
    {
        Device(R044) { Name(_HID, "ACPI0007") Name(_UID, 2044) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2045, PCNT))
    {
        Device(R045) { Name(_HID, "ACPI0007") Name(_UID, 2045) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2046, PCNT))
    {
        Device(R046) { Name(_HID, "ACPI0007") Name(_UID, 2046) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2047, PCNT))
    {
        Device(R047) { Name(_HID, "ACPI0007") Name(_UID, 2047) Method(_STA, 0) { Return(0xF) } }
    }
    If(LLessEqual(2048, PCNT))
    {
        Device(R048) { Name(_HID, "ACPI0007") Name(_UID, 2048) Method(_STA, 0) { Return(0xF) } }
    }
}

