# Azure Integrated HSM (AziHsm) DXE Driver Design Document

## Overview

The Azure Integrated HSM (AziHsm) DXE driver is a UEFI driver that manages Hardware Security Modules (HSM) integrated into Azure virtual machines. The driver provides secure key management capabilities during the UEFI boot phase, with particular emphasis on protecting sensitive cryptographic material and ensuring proper cleanup before transitioning control to the operating system.

## Architecture

### Driver Type and Module Structure

- **Module Type**: `UEFI_DRIVER` 
- **Architecture Support**: X64, AARCH64
- **Entry Point**: `AziHsmDriverEntry`
- **Unload Function**: `AziHsmDriverUnload`

### Core Components

The driver is structured around several key functional areas:

1. **Driver Binding Protocol Implementation** - Standard UEFI driver lifecycle management
2. **PCI Device Management** - Hardware discovery and initialization
3. **HSM Communication Layer** - Host Controller Interface (HCI) for HSM operations
4. **Key Management System** - BKS3 (Boot Key Storage System 3) implementation
5. **Security and Cleanup System** - Sensitive data protection and cleanup mechanisms

## Module Layout and Organization

The AziHsmDxe driver is organized into 11 functional modules, each consisting of a `.c` implementation file and corresponding `.h` header file. This modular architecture provides clear separation of concerns and maintainability.

### Module Overview

#### Core Driver Module
- **AziHsmDxe.c/.h**
  - Main driver entry point (`AziHsmDriverEntry`)
  - UEFI Driver Binding Protocol implementation
  - Driver lifecycle management (Supported, Start, Stop)
  - Component Name Protocol support
  - Boot event handlers (Ready-to-Boot, Unable-to-Boot)
  - Global sensitive data cleanup coordination

#### Hardware Communication Modules
- **AziHsmPci.c/.h**
  - PCI interface management and configuration
  - Device identification (Vendor ID: 0x1414, Device ID: 0xC003)
  - PCI capability reading and attribute management
  - BAR (Base Address Register) operations

- **AziHsmHci.c/.h**
  - Host Controller Interface implementation
  - Doorbell register operations (SQ Tail, CQ Head)
  - Queue pair initialization and management
  - Low-level hardware command submission

- **AziHsmQueue.c/.h**
  - Submission Queue (SQ) and Completion Queue (CQ) management
  - Queue pair lifecycle (Initialize, Uninitialize)
  - Queue entry size configuration (SQE: 64 bytes, CQE: 16 bytes)
  - Phase bit tracking for command/completion matching

- **AziHsmDma.c/.h**
  - DMA buffer allocation and management
  - 64-bit address support for efficient data transfer
  - Memory mapping for PCI devices
  - Buffer cleanup and resource deallocation

#### Command Protocol Modules
- **AziHsmAdmin.c/.h**
  - Administrative commands (device identification, capabilities query)
  - Controller identity retrieval (serial number, firmware version)
  - API revision negotiation
  - Device health and status monitoring

- **AziHsmCp.c/.h**
  - Command Protocol layer
  - Command submission and completion handling
  - Status code interpretation
  - Command timeout and error handling

- **AziHsmDdi.c/.h**
  - Device Driver Interface (DDI) operations
  - MBOR (CBOR-based) message encoding/decoding
  - DDI command structures (GetApiRev, InitBks3, SetSealedBks3, GetSealedBks3)
  - Request/response header management

- **AziHsmDdiApi.c/.h**
  - High-level DDI API wrappers
  - Simplified interfaces for common operations
  - Error handling and status conversion
  - API version compatibility management

#### Cryptographic Operations Module
- **AziHsmBKS3.c/.h**
  - Boot Key Storage System 3 (BKS3) implementation
  - TPM 2.0 integration (Platform/Null hierarchy operations)
  - HKDF-Expand key derivation (RFC 5869)
  - TPM sealing/unsealing operations
  - Secure key material cleanup

#### Data Encoding Module
- **AziHsmMbor.c/.h**
  - MBOR (Message-Based Object Representation) encoding/decoding
  - CBOR-compatible serialization format
  - Structure marshalling for HSM communication
  - Field validation and error checking

### Module Dependencies

```
AziHsmDxe.c (Main Driver)
├── AziHsmPci.c (PCI Layer)
├── AziHsmHci.c (Hardware Interface)
│   ├── AziHsmQueue.c (Queue Management)
│   │   └── AziHsmDma.c (DMA Buffers)
│   └── AziHsmCp.c (Command Protocol)
│       └── AziHsmDdi.c (DDI Operations)
│           ├── AziHsmDdiApi.c (API Wrappers)
│           └── AziHsmMbor.c (Encoding/Decoding)
├── AziHsmAdmin.c (Admin Commands)
└── AziHsmBKS3.c (Cryptographic Operations)
```

### File Organization

```
MsvmPkg/AziHsmDxe/
├── AziHsmDxe.c/.h              (Main driver)
├── AziHsmPci.c/.h              (PCI interface)
├── AziHsmHci.c/.h              (Host Controller Interface)
├── AziHsmQueue.c/.h            (Queue management)
├── AziHsmDma.c/.h              (DMA operations)
├── AziHsmAdmin.c/.h            (Admin commands)
├── AziHsmCp.c/.h               (Command protocol)
├── AziHsmDdi.c/.h              (DDI operations)
├── AziHsmDdiApi.c/.h           (DDI API wrappers)
├── AziHsmBKS3.c/.h             (Crypto operations)
├── AziHsmMbor.c/.h             (MBOR encoding)
├── AziHsmDxe.inf               (Build configuration)
├── AziHsmDxe.uni               (Unicode strings)
└── AziHsmDesignReadMe.md       (This document)
```

---

## Driver Internals

This section provides concise details on the internal mechanisms and data structures used by the AziHsm driver.

### Controller State Management

The driver maintains per-device state using the `AZIHSM_CONTROLLER_STATE` structure:

```c
typedef struct _AZIHSM_CONTROLLER_STATE {
  UINT32                      Signature;           // 'AHSM' - validation marker
  EFI_HANDLE                  ControllerHandle;    // UEFI controller handle
  EFI_HANDLE                  ImageHandle;         // Driver image handle
  EFI_HANDLE                  DriverBindingHandle; // Binding protocol handle
  EFI_DEVICE_PATH_PROTOCOL    *ParentDevicePath;   // Device path
  EFI_PCI_IO_PROTOCOL         *PciIo;              // PCI I/O protocol
  UINT64                      PciAttributes;       // Saved PCI attributes
  AZIHSM_IO_QUEUE_PAIR        AdminQueue;          // Admin command queue
  AZIHSM_IO_QUEUE_PAIR        HsmQueue;            // HSM operation queue
  AZIHSM_PROTOCOL             AziHsmProtocol;      // Published protocol
  BOOLEAN                     HsmQueuesCreated;    // Queue init status
} AZIHSM_CONTROLLER_STATE;
```

**Key Fields:**
- **Signature**: Magic number (0x4148534D) for structure validation
- **Queue Pairs**: Separate queues for admin and HSM-specific operations
- **PciAttributes**: Saved for restoration during driver unload
- **HsmQueuesCreated**: Tracks initialization state for cleanup

### Queue Architecture

The driver uses a dual-queue architecture based on the NVMe submission/completion queue model:

#### Queue Pair Structure

```c
typedef struct _AZIHSM_IO_QUEUE_PAIR {
  EFI_PCI_IO_PROTOCOL    *PciIo;           // PCI I/O for DMA operations
  UINT16                 Id;               // Queue identifier (0=Admin, 1=HSM)
  UINT8                  Phase;            // Current phase bit (0 or 1)
  UINT16                 DoorbellStride;   // Doorbell register stride
  AZIHSM_IO_QUEUE        SubmissionQueue;  // Command submission
  AZIHSM_IO_QUEUE        CompletionQueue;  // Command completion
} AZIHSM_IO_QUEUE_PAIR;
```

#### Queue Operation Flow

```
1. Command Submission:
   ┌─────────────────────┐
   │ Prepare Command     │
   │ (64-byte SQE)       │
   └──────────┬──────────┘
              │
              ▼
   ┌─────────────────────┐
   │ Copy to SQ Buffer   │
   │ (DMA-mapped memory) │
   └──────────┬──────────┘
              │
              ▼
   ┌─────────────────────┐
   │ Increment SQ Tail   │
   │ (XOR with 1)        │
   └──────────┬──────────┘
              │
              ▼
   ┌─────────────────────┐
   │ Ring SQ Doorbell    │
   │ (Write to HW reg)   │
   └─────────────────────┘

2. Completion Polling:
   ┌─────────────────────┐
   │ Check CQ Phase Bit  │
   │ (Matches expected?) │
   └──────────┬──────────┘
              │
              ▼
   ┌─────────────────────┐
   │ Read CQE (16 bytes) │
   │ Extract Status      │
   └──────────┬──────────┘
              │
              ▼
   ┌─────────────────────┐
   │ Increment CQ Head   │
   │ (XOR with 1)        │
   └──────────┬──────────┘
              │
              ▼
   ┌─────────────────────┐
   │ Ring CQ Doorbell    │
   │ (Ack to hardware)   │
   └─────────────────────┘
```

**Key Concepts:**
- **Phase Bit**: Toggles between 0 and 1 to distinguish new completions from old ones
- **Doorbell Stride**: Hardware register offset between queue doorbells
- **Queue Size**: Currently fixed at 1 entry (single outstanding command model)

### DDI Command Flow

The Device Driver Interface (DDI) uses MBOR encoding for structured communication:

```
Client Code
    │
    ▼
┌──────────────────────────┐
│ AziHsmDdiApi.c           │  High-level API
│ (AziHsmInitBks3, etc.)   │
└────────────┬─────────────┘
             │
             ▼
┌──────────────────────────┐
│ AziHsmDdi.c              │  Request encoding
│ (AzihsmEncodeXxxReq)     │
└────────────┬─────────────┘
             │
             ▼
┌──────────────────────────┐
│ AziHsmMbor.c             │  MBOR serialization
│ (Encoder/Decoder)        │
└────────────┬─────────────┘
             │
             ▼
┌──────────────────────────┐
│ AziHsmCp.c               │  Command submission
│ (Submit to queue)        │
└────────────┬─────────────┘
             │
             ▼
┌──────────────────────────┐
│ AziHsmHci.c              │  Hardware interface
│ (Doorbell operations)    │
└────────────┬─────────────┘
             │
             ▼
        HSM Device
```

**DDI Request Structure:**
```c
Request Message:
├── DdiReqHdr (Header)
│   ├── Revision (optional)
│   ├── DdiOp (operation code: 1111=InitBks3, 1113=SetSealedBks3, etc.)
│   └── SessionId (optional)
├── DdiReqData (Payload)
│   └── Operation-specific data
└── DdiReqExt (Extension - reserved)
```

**DDI Response Structure:**
```c
Response Message:
├── DdiRespHdr (Header)
│   ├── Revision (optional)
│   ├── DdiOp (echoed operation code)
│   ├── SessionId (optional)
│   ├── DdiStatus (0=success, error codes otherwise)
│   └── fips_approved (FIPS compliance flag)
├── DdiRespData (Payload)
│   └── Operation-specific result
└── DdiRespExt (Extension - reserved)
```

### BKS3 Operation Details

#### InitBks3 - Initialize BKS3 Key
- **Input**: 48-byte derived key
- **Process**: HSM wraps/encrypts the key using its internal root key
- **Output**: Wrapped key blob (up to 1024 bytes) + 16-byte GUID

#### SetSealedBks3 - Store Sealed Key
- **Input**: Sealed blob containing:
  - TPM-sealed AES key/IV (variable size)
  - AES-encrypted wrapped key (variable size)
- **Process**: HSM stores the sealed blob for later retrieval
- **Output**: Boolean success/failure

#### GetSealedBks3 - Retrieve Sealed Key
- **Input**: None (retrieves current stored blob)
- **Output**: Previously stored sealed blob

### Memory Management Patterns

The driver follows strict memory management practices:

#### DMA Buffer Allocation
```c
1. Allocate via PciIo->AllocateBuffer()
2. Map for device access via PciIo->Map()
3. Store both host and device addresses
4. Use for hardware command/response buffers
5. Unmap via PciIo->Unmap() before freeing
6. Free via PciIo->FreeBuffer()
```

#### Sensitive Data Handling
```c
Pattern for cryptographic material:
1. Allocate buffer (AllocatePool/AllocateZeroPool)
2. Use for crypto operations
3. Zero memory explicitly (ZeroMem)
4. Free buffer (FreePool)
5. Set pointer to NULL

Example:
  UINT8 *Key = AllocatePool(32);
  // ... use Key ...
  ZeroMem(Key, 32);  // Critical: zero before freeing
  FreePool(Key);
  Key = NULL;
```

### Phase Tracking Mechanism

The driver uses phase bits to handle the single-entry queue model:

```c
// Submission Queue Tail Update
#define AZIHSM_SQ_INC_TAIL(_SQ) { \
  _SQ.u.Tail ^= 1;  // Toggle between 0 and 1
}

// Completion Queue Head Update
#define AZIHSM_CQ_INC_HEAD(_CplQ) { \
  _CplQ.u.Head ^= 1;  // Toggle between 0 and 1
}

// Phase bit tracks expected completion value
QueuePair->Phase ^= 1;  // Toggle after each command cycle
```

**Phase Bit Logic:**
- Initial phase = 1
- Each completion toggles the phase bit in the CQE
- Driver expects CQE.Phase == QueuePair.Phase for valid completion
- Prevents false positives from stale completions

### Error Handling Strategy

The driver implements layered error handling:

```c
Layer 1: Hardware Errors
└─→ HCI detects command timeout or invalid completion
    └─→ Returns EFI_DEVICE_ERROR

Layer 2: Protocol Errors
└─→ DDI decoder detects invalid MBOR structure
    └─→ Returns EFI_PROTOCOL_ERROR

Layer 3: Command Errors
└─→ HSM returns DDI_STATUS_xxx error code
    └─→ Converted to EFI_DEVICE_ERROR or EFI_UNSUPPORTED

Layer 4: Resource Errors
└─→ Memory allocation or DMA mapping fails
    └─→ Returns EFI_OUT_OF_RESOURCES
```

**Error Propagation:**
- All functions return EFI_STATUS
- Errors propagate up the call stack
- Each layer adds debug logging
- Resources cleaned up on error paths

### Global State Variables

```c
// Platform-wide sealed secret (shared across all HSM devices)
STATIC BOOLEAN       mAziHsmSealedPlatormSecretDerived = FALSE;
STATIC AZIHSM_BUFFER mAziHsmSealedPlatformSecret = { 0 };

// Boot event handles
STATIC EFI_EVENT mAziHsmReadyToBootEvent = NULL;
STATIC EFI_EVENT mAziHsmUnableToBootEvent = NULL;

// Cleanup guard
STATIC BOOLEAN mSensitiveDataCleared = FALSE;
```

These globals enable:
- **Secret Sharing**: One TPM derivation serves multiple HSM devices
- **Event Handling**: Cleanup triggers for different boot scenarios
- **Idempotency**: Prevent multiple cleanup attempts

---

## Key Design Principles

### 1. Security-First Design
- **Sensitive Data Protection**: All cryptographic keys are managed with strict security protocols
- **Memory Cleanup**: Automatic zeroing of sensitive data structures after use
- **Multi-Trigger Cleanup**: Multiple mechanisms ensure sensitive data is cleared regardless of boot outcome

### 2. Hardware Abstraction
- **PCI Device Support**: Standardized PCI interface for HSM hardware
- **Queue-Based Communication**: Asynchronous I/O operations through admin and HSM queues
- **DMA Support**: 64-bit DMA addressing for efficient data transfer

### 3. Platform Integration
- **TPM Integration**: Leverages platform TPM for key derivation and sealing operations
- **Boot Event Handling**: Responds to critical boot lifecycle events
- **UEFI Compliance**: Full adherence to UEFI driver model specifications

### 4. AziHsm Key Derivation Flow 
- **Block Diagram**

This block diagram illustrates the key derivation flow from TPM in `AziHsmDriverEntry()` to the Derived key in `AziHsmDriverBindingStart()`.

#### Overview

The key derivation process occurs in two phases:
1. **Driver Entry Phase**: Creates a platform-wide TPM-derived key (executed once)
2. **Driver Binding Phase**: Derives per-device unique keys (executed per HSM device)

---

####  Detailed Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        AziHsmDriverEntry()                              │
│                     (Driver Initialization)                             │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Step 1: AziHsmGetTpmPlatformSecret()                                    │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 1a. Create Platform Hierarchy Primary KeyedHash                     │ │
│ │     - Hierarchy: TPM_RH_PLATFORM                                    │ │
│ │     - Type: KeyedHash with HMAC-SHA256                              │ │
│ │     - User Data: "AZIHSM_VM_BKS3_PRIMARY_KEY"                       │ │
│ │     → Returns: PrimaryHandle                                        │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 1b. TPM2_HMAC to generate PRK (platform secret)                     │ │
│ │     Input: "AZIHSM_VM_BKS3_KDF"                                     │ │
│ │                                                                     │ │
│ │     • TPM2_HMAC(PrimaryHandle, Input, SHA256)                       │ │
│ │     → TpmDerivedSecret (32 bytes - SHA256 output)                   │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│                   TpmDerivedSecret (32 bytes)                           │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Step 2: Seal to TPM Null Hierarchy                                      │
│                                                                         │
│ AziHsmSealToTpmNullHierarchy(TpmDerivedSecret)                          │
│   • Create Null Hierarchy RSA Primary (TPM_RH_NULL)                     │
│   • TPM2_Create: Seal TpmDerivedSecret (32 bytes) as KeyedHash object   │
│   • Returns: SealedSecretBlob (contains TPM2B_PRIVATE + TPM2B_PUBLIC)   │
│                                                                         │
│ Store globally:                                                         │
│   mAziHsmSealedPlatformSecret ← SealedSecretBlob                        │
│   mAziHsmSealedPlatormSecretDerived = TRUE                              │
│                                                                         │
│ Note: Sealed to current boot session (Null hierarchy seed resets on     │
│       reboot, making the sealed blob unusable after reboot)             │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 │ (Global variable persists in memory)
                                 │
┌────────────────────────────────┴────────────────────────────────────────┐
│                    AziHsmDriverBindingStart()                           │
│              (Per-Device Initialization - for each HSM)                 │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Step 3: Get HSM Device Identity                                         │
│                                                                         │
│ AziHsmAdminIdentifyCtrl(State) → HsmIdenData                            │
│   • Returns: HsmIdenData.Sn (Serial Number, unique per device)          │
│   • Validates: Serial number is not all zeros                           │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Step 4: Unseal Platform Secret from TPM                                 │
│                                                                         │
│ AziHsmUnsealUsingTpmNullHierarchy(mAziHsmSealedPlatformSecret)          │
│                                               → TpmPlatformSecret       │
│                                                                         │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 4a. Create Null Hierarchy RSA Primary                               │ │
│ │     - Same hierarchy as used for sealing (TPM_RH_NULL)              │ │
│ │     - Recreates same primary key deterministically                  │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 4b. TPM2_Load(Primary, SealedKeyBlob)                               │ │
│ │     - Loads sealed object into TPM → ObjectHandle                   │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 4c. TPM2_Unseal(ObjectHandle)                                       │ │
│ │     - Extracts plaintext → TpmPlatformSecret (32 bytes)             │ │
│ │     - This is the original TpmDerivedSecret from DriverEntry        │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Step 5: Derive Per-Device BKS3 Key                                      │
│                                                                         │
│ AziHsmDeriveBKS3fromId(TpmPlatformSecret, HsmIdenData.Sn) → BKS3Key     │
│                                                                         │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 5a. HKDF-Expand to Generate BKS3 Key (Software Operation)           │ │
│ │     BKS3Key = ManualHkdfSha256Expand(                               │ │
│ │         PRK = TpmPlatformSecret (32 bytes),                         │ │
│ │         Info = HsmSerialNumber (device-specific),                   │ │
│ │         OutputLength = 48 bytes                                     │ │
│ │     )                                                               │ │
│ │                                                                     │ │
│ │     Note: Uses TpmPlatformSecret directly as PRK                    │ │
│ │     Output: BKS3Key.KeyData (48 bytes)                              │ │
│ │             BKS3Key.KeySize = 48                                    │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Step 6: BKS3 Sealing Workflow to HSM                                    │
│                                                                         │
│ AziHsmPerformBks3SealingWorkflow()                                      │
│                                                                         │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 6a. Initialize BKS3 with HSM                                        │ │
│ │     AziHsmInitBks3(BKS3Key)                                         │ │
│ │     • Sends derived key to  HSM device                              │ │
│ │     • HSM wraps/encrypts the key → WrappedBKS3 (up to 1024 bytes)   │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 6b. Generate Random AES Key and IV                                  │ │
│ │     • AziHsmTpmGetRandom() → Aes256Key (32 bytes)                   │ │
│ │     • AziHsmTpmGetRandom() → Iv (16 bytes)                          │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 6c. Encrypt WrappedBKS3 with AES-256-CBC                            │ │
│ │     • Add PKCS7 padding to WrappedBKS3                              │ │
│ │     • AziHsmAes256CbcEncrypt(WrappedBKS3, Key, IV)                  │ │
│ │       → EncryptedWrappedKey                                         │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 6d. Seal AES Key/IV to TPM Null Hierarchy                           │ │
│ │     • Create KeyIvRecord structure (Key + IV + metadata)            │ │
│ │     • AziHsmSealToTpmNullHierarchy(KeyIvRecord)                     │ │
│ │       → SealedAesSecret                                             │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 6e. Create Final Sealed Blob for HSM                                │ │
│ │     SealedBKS3Buffer contains:                                      │ │
│ │     • Size of SealedAesSecret (UINT16)                              │ │
│ │     • SealedAesSecret data (TPM sealed AES key/IV)                  │ │
│ │     • Size of EncryptedWrappedKey (UINT32)                          │ │
│ │     • EncryptedWrappedKey data (AES-encrypted wrapped BKS3)         │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                 │                                       │
│                                 ▼                                       │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ 6f. Send Sealed Blob to HSM                                         │ │
│ │     AziHsmSetSealedBks3(SealedBKS3Buffer)                           │ │
│ │     • HSM stores the sealed blob for later retrieval                │ │
│ │     • Blob can be unsealed only in current boot session             │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

---

### 5. AziHsm Crypto Operations Call Sequence Diagram

This document provides a detailed call sequence diagram showing crypto operations and interactions between AziHsmDxe.c, AziHsmBKS3.c, TPM 2.0, and HSM.

---

#### Overview

The crypto operations occur in two distinct phases:
1. **Driver Entry Phase**: Platform-wide key derivation and sealing (once per boot)
2. **Driver Binding Phase**: Per-device key derivation and HSM initialization (per HSM device)

---

##### Phase 1: Driver Entry - Platform Secret Derivation

```
┌──────────────┐         ┌──────────────┐         ┌──────────────┐
│ UEFI Driver  │         │ AziHsmBKS3.c │         │   TPM 2.0    │
│(AziHsmDxe.c) │         │ Crypto Module│         │              │
└──────┬───────┘         └──────┬───────┘         └──────┬───────┘
       │                        │                        │
       │ AziHsmDriverEntry()    │                        │
       │════════════════════════│                        │
       │                        │                        │
       │ AziHsmGetTpmPlatformSecret()                    │
       │───────────────────────>│                        │
       │                        │                        │
       │                        │ AziHsmCreatePlatformPrimaryKeyedHash()
       │                        │────┐                   │
       │                        │    │ Prepare:          │
       │                        │<───┘ - InSensitive     │
       │                        │      - InPublic        │
       │                        │      - UserData="AZIHSM_VM_BKS3_PRIMARY_KEY"
       │                        │                        │
       │                        │ InternalTpm2CreatePrimary()
       │                        │───────────────────────>│
       │                        │  TPM_CC_CreatePrimary  │
       │                        │  Hierarchy: TPM_RH_PLATFORM
       │                        │  Type: KeyedHash       │
       │                        │  Scheme: HMAC-SHA256   │
       │                        │                        │
       │                        │                        │──┐
       │                        │                        │  │ Create Primary
       │                        │                        │  │ Object in Platform
       │                        │                        │<─┘ Hierarchy
       │                        │                        │
       │                        │<───────────────────────│
       │                        │  Return: PrimaryHandle │
       │                        │                        │
       │                        │ Prepare HMAC Input     │
       │                        │────┐                   │
       │                        │    │ KdfInput =        │
       │                        │<───┘ "AZIHSM_VM_BKS3_KDF"
       │                        │                        │
       │                        │ InternalTpm2HMAC()     │
       │                        │───────────────────────>│
       │                        │  TPM_CC_HMAC           │
       │                        │  Handle: PrimaryHandle │
       │                        │  Data: KdfInput        │
       │                        │  HashAlg: SHA256       │
       │                        │                        │
       │                        │                        │──┐
       │                        │                        │  │ Compute HMAC
       │                        │                        │  │ using Primary Key
       │                        │                        │<─┘ → PRK (32 bytes)
       │                        │                        │
       │                        │<───────────────────────│
       │                        │  Return: HmacResult    │
       │                        │          (32 bytes)    │
       │                        │                        │
       │                        │ Tpm2FlushContext()     │
       │                        │───────────────────────>│
       │                        │  Flush: PrimaryHandle  │
       │                        │<───────────────────────│
       │                        │                        │
       │<───────────────────────│                        │
       │ Return: TpmDerivedSecret (32 bytes)             │
       │                        │                        │
       │ Copy to AZIHSM_BUFFER  │                        │
       │────┐                   │                        │
       │    │ TpmDerivedSecretBlob.Data ← TpmDerivedSecret.KeyData
       │    │ TpmDerivedSecretBlob.Size ← TpmDerivedSecret.KeySize (32)
       │<───┘                   │                        │
       │                        │                        │
       │ AziHsmSealToTpmNullHierarchy()                  │
       │───────────────────────>│                        │
       │  Input: TpmDerivedSecretBlob (32 bytes)         │
       │                        │                        │
       │                        │ AziHsmCreateNullAesPrimary()
       │                        │───────────────────────>│
       │                        │  TPM_CC_CreatePrimary  │
       │                        │  Hierarchy: TPM_RH_NULL│
       │                        │  Type: RSA Storage     │
       │                        │  KeyBits: 2048         │
       │                        │  Symmetric: AES-128-CFB│
       │                        │                        │
       │                        │                        │──┐
       │                        │                        │  │ Create Null
       │                        │                        │  │ Hierarchy Primary
       │                        │                        │<─┘ (session-bound)
       │                        │                        │
       │                        │<───────────────────────│
       │                        │  Return: NullPrimary   │
       │                        │                        │
       │                        │ AziHsmTpmSealBuffer()  │
       │                        │───────────────────────>│
       │                        │  TPM_CC_Create         │
       │                        │  Parent: NullPrimary   │
       │                        │  Data: TpmDerivedSecretBlob (32 bytes)
       │                        │  Type: KeyedHash (seal)│
       │                        │                        │
       │                        │                        │──┐
       │                        │                        │  │ Create sealed
       │                        │                        │  │ object with
       │                        │                        │<─┘ TPM2B_PRIVATE
       │                        │                        │    + TPM2B_PUBLIC
       │                        │<───────────────────────│
       │                        │  Return: SealedBlob    │
       │                        │  (TPM2B_PRIVATE + TPM2B_PUBLIC)
       │                        │                        │
       │                        │ Tpm2FlushContext()     │
       │                        │───────────────────────>│
       │                        │  Flush: NullPrimary    │
       │                        │<───────────────────────│
       │                        │                        │
       │<───────────────────────│                        │
       │ Return: SealedSecretBlob                        │
       │                        │                        │
       │ Store in global:       │                        │
       │ mAziHsmSealedPlatformSecret ← SealedSecretBlob  │
       │ mAziHsmSealedPlatormSecretDerived = TRUE        │
       │ ZeroMem sensitive data │                        │
       │════════════════════════│                        │
       │                        │                        │
```

---

##### Phase 2: Driver Binding - Per-Device BKS3 Derivation and Sealing

```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ UEFI Driver  │  │ AziHsmBKS3.c │  │   TPM 2.0    │  │     HSM      │ 
│(AziHsmDxe.c) │  │ Crypto Module│  │              │  │    Device    │
└──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘
       │                 │                 │                 │
       │ AziHsmDriverBindingStart()        │                 │
       │═════════════════│                 │                 │
       │                 │                 │                 │
       │ AziHsmAdminIdentifyCtrl()         │                 │
       │────────────────────────────────────────────────────>│
       │                 │                 │  Admin Command  │
       │                 │                 │  Get Device ID  │
       │                 │                 │                 │──┐
       │                 │                 │                 │  │ Return Device
       │                 │                 │                 │<─┘ Serial Number
       │<────────────────────────────────────────────────────│
       │ HsmIdenData.Sn (Serial Number)    │                 │
       │                 │                 │                 │
       │ Validate: Serial Number != all zeros                │
       │────┐            │                 │                 │
       │<───┘            │                 │                 │
       │                 │                 │                 │
       │ AziHsmPerformBks3SealingWorkflow()                  │
       │────────────────>│                 │                 │
       │  Input: State, ApiRevisionMax,    │                 │
       │         HsmSerialData             │                 │
       │                 │                 │                 │
       │                 │ AziHsmUnsealUsingTpmNullHierarchy()
       │                 │────┐            │                 │
       │                 │    │ Input:     │                 │
       │                 │<───┘ mAziHsmSealedPlatformSecret  │
       │                 │                 │                 │
       │                 │ AziHsmCreateNullAesPrimary()      │
       │                 │────────────────>│                 │
       │                 │  TPM_CC_CreatePrimary             │
       │                 │  Hierarchy: TPM_RH_NULL           │
       │                 │  (same as sealing)                │
       │                 │                 │                 │
       │                 │                 │──┐              │
       │                 │                 │  │ Recreate same│
       │                 │                 │<─┘ Null Primary │
       │                 │<────────────────│                 │
       │                 │  Return: NullPrimary              │
       │                 │                 │                 │
       │                 │ AziHsmTpmLoadSealedBuffer()       │
       │                 │────────────────>│                 │
       │                 │  TPM_CC_Load    │                 │
       │                 │  Parent: NullPrimary              │
       │                 │  Private: TPM2B_PRIVATE           │
       │                 │  Public: TPM2B_PUBLIC             │
       │                 │                 │                 │
       │                 │                 │──┐              │
       │                 │                 │  │ Load sealed  │
       │                 │                 │<─┘ object       │
       │                 │<────────────────│                 │
       │                 │  Return: ObjectHandle             │
       │                 │                 │                 │
       │                 │ AziHsmTpmUnsealBuffer()           │
       │                 │────────────────>│                 │
       │                 │  TPM_CC_Unseal  │                 │
       │                 │  Handle: ObjectHandle             │
       │                 │                 │                 │
       │                 │                 │──┐              │
       │                 │                 │  │ Decrypt and  │
       │                 │                 │  │ return       │
       │                 │                 │<─┘ plaintext    │
       │                 │<────────────────│                 │
       │                 │  TpmPlatformSecret (32 bytes)     │
       │                 │                 │                 │
       │                 │ Tpm2FlushContext()                │
       │                 │────────────────>│                 │
       │                 │  Flush: ObjectHandle, NullPrimary │
       │                 │<────────────────│                 │
       │                 │                 │                 │
       │                 │ AziHsmDeriveBKS3fromId()          │
       │                 │────┐            │                 │
       │                 │    │ Input:     │                 │
       │                 │    │ - TpmPlatformSecret (32bytes)|
       │                 │    │ - HsmSerialData              │
       │                 │<───┘            │                 │
       │                 │                 │                 │
       │                 │ ManualHkdfSha256Expand()          │
       │                 │────┐            │                 │
       │                 │    │ Software:  │                 │
       │                 │    │ HKDF-Expand(│                │
       │                 │    │   PRK = TpmPlatformSecret    |
       |                 |               (32 bytes)          |
       │                 │    │   Info = HsmSerialData,      │
       │                 │    │   Length = 48)               │
       │                 │<───┘ → BKS3Key (48 bytes)         │
       │                 │                 │                 │
       │                 │ AziHsmInitBks3()│                 │
       │                 │──────────────────────────────────>│
       │                 │  Input: BKS3Key.KeyData (48 bytes)│
       │                 │                 │   BKS3 Init     │
       │                 │                 │   Command       │
       │                 │                 │                 │──┐
       │                 │                 │                 │  │ Wrap/encrypt
       │                 │                 │                 │  │ BKS3 key with
       │                 │                 │                 │<─┘ internal key
       │                 │<──────────────────────────────────│
       │                 │  Return: WrappedBKS3              │
       │                 │                 │                 │
       │                 │ AziHsmTpmGetRandom()              │
       │                 │────────────────>│                 │
       │                 │  TPM_CC_GetRandom                 │
       │                 │  BytesRequested: 32               │
       │                 │<────────────────│                 │
       │                 │  Return: Aes256Key                │
       │                 │                 │                 │
       │                 │ AziHsmTpmGetRandom()              │
       │                 │────────────────>│                 │
       │                 │  TPM_CC_GetRandom                 │
       │                 │  BytesRequested: 16               │
       │                 │<────────────────│                 │
       │                 │  Return: Iv     │                 │
       │                 │                 │                 │
       │                 │ Apply PKCS7 Padding               │
       │                 │────┐            │                 │
       │                 │    │ Pad WrappedBKS3 to           │
       │                 │<───┘ AES block size (16 bytes)    │
       │                 │                 │                 │
       │                 │ AziHsmAes256CbcEncrypt()          │
       │                 │────┐            │                 │
       │                 │    │ Software:  │                 │
       │                 │    │ AES-256-CBC│                 │
       │                 │    │ (BaseCryptLib)               │
       │                 │<───┘ → EncryptedData              │
       │                 │                 │                 │
       │                 │ Prepare KeyIvRecord               │
       │                 │────┐            │                 │
       │                 │    │ - Copy Aes256Key             │
       │                 │    │ - Copy Iv  │                 │
       │                 │<───┘ - Set KeyVersion             │
       │                 │                 │                 │
       │                 │ AziHsmSealToTpmNullHierarchy()    │
       │                 │────────────────>│                 │
       │                 │  Input: KeyIvRecord               │
       │                 │  (AES key + IV) │                 │
       │                 │                 │                 │
       │                 │ [Same sealing process as Phase 1] │
       │                 │<────────────────│                 │
       │                 │  Return: SealedAesSecret          │
       │                 │                 │                 │
       │                 │ Build SealedBKS3Buffer            │
       │                 │────┐            │                 │
       │                 │    │ -SealedAesSecret.Size(UINT16)|
       │                 │    │ - SealedAesSecret.Data       │
       │                 │    │ - EncryptedData.Size (UINT32)|
       │                 │<───┘ - EncryptedData              │
       │                 │                 │                 │
       │                 │ AziHsmSetSealedBks3()             │
       │                 │──────────────────────────────────>│
       │                 │  Input: SealedBKS3Buffer          │
       │                 │                 │   SetSealedBks3 │
       │                 │                 │   Command       │
       │                 │                 │                 │──┐
       │                 │                 │                 │  │ Store sealed
       │                 │                 │                 │<─┘ blob for later
       │                 │<──────────────────────────────────│
       │                 │  Return: Success                  │
       │                 │                 │                 │
       │                 │ ZeroMem all sensitive data        │
       │                 │────┐            │                 │
       │                 │    │ - BKS3Key  │                 │
       │                 │    │ - TpmPlatformSecret          │
       │                 │    │ - Aes256Key│                 │
       │                 │    │ - Iv       │                 │
       │                 │    │ - WrappedBKS3                │
       │                 │<───┘ - EncryptedData              │
       │                 │                 │                 │
       │<────────────────│                 │                 │
       │ Return: EFI_SUCCESS               │                 │
       │════════════════ │                 │                 │
       │                 │                 │                 │
```

---

####  Key Components

##### Constants Used

| Constant | Value | Purpose |
|----------|-------|---------|
| `AZIHSM_PRIMARY_KEY_USER_DATA` | "AZIHSM_VM_BKS3_PRIMARY_KEY" | Seeds platform hierarchy primary key |
| `AZIHSM_HASH_USER_INPUT` | "AZIHSM_VM_BKS3_KDF" | Input for HKDF PRK generation via TPM2_HMAC |
| `AZIHSM_APPLICATION_INFO` | "AZIHSM_VM_BKS3_HASH_INFO" | Context info for HKDF-Expand |
| `AZIHSM_DERIVED_KEY_SIZE` | 48 bytes | Output key size (384 bits for BKS3) |
| `AZIHSM_AES256_KEY_SIZE` | 32 bytes | AES-256 key size for encrypting wrapped keys |
| `AZIHSM_AES_IV_SIZE` | 16 bytes | AES initialization vector size |

##### TPM Hierarchies Used

1. **TPM_RH_PLATFORM**: Used in DriverEntry for initial key derivation
   - Platform hierarchy provides platform-wide secrets
   - Requires platform authorization (typically available during firmware execution)

2. **TPM_RH_NULL**: Used for sealing/unsealing
   - Null hierarchy seed resets on every reboot
   - Perfect for session-bound secrets that shouldn't persist
   - No authorization required

---
## Detailed Component Analysis

### Driver Initialization Sequence

```
AziHsmDriverEntry()
├── 1. TPM-Based Secret Derivation
│   ├── AziHsmGetTpmPlatformSecret() - Generate platform secret from TPM
│   ├── AziHsmSealToTpmNullHierarchy() - Seal secret to current boot session
│   └── Store sealed secret globally (mAziHsmSealedPlatformSecret)
├── 2. Protocol Installation
│   ├── Install Driver Binding Protocol
│   ├── Install Component Name Protocols
│   └── Install Driver Supported EFI Version Protocol
├── 3. Event Registration
│   ├── Register Ready-to-Boot event (successful boot scenarios)
│   └── Register Unable-to-Boot event (boot failure scenarios)
└── 4. Return success status
```

### Device Discovery and Binding

**AziHsmBindingSupported()** implements the UEFI driver binding discovery mechanism:

```
Device Discovery Process:
1. Verify device path protocol availability
2. Open PCI I/O protocol
3. Read PCI Vendor ID (0x1414) and Device ID (0xC003)
4. Validate against expected HSM identifiers
5. Return EFI_SUCCESS for supported devices
```

### Device Initialization (Start Operation)

**AziHsmDriverBindingStart()** performs comprehensive device initialization:

```
Device Start Sequence:
├── 1. Protocol Acquisition
│   ├── Open Device Path Protocol
│   └── Open PCI I/O Protocol
├── 2. Controller State Setup
│   ├── Allocate AZIHSM_CONTROLLER_STATE structure
│   ├── Initialize controller metadata
│   └── Enable 64-bit DMA support
├── 3. Protocol Installation
│   └── Install AziHsm Protocol on controller handle
├── 4. Hardware Initialization
│   ├── AziHsmHciInitialize() - Initialize Host Controller Interface
│   └── AziHsmInitHsm() - Initialize HSM device
├── 5. HSM Identity and Authentication
│   ├── AziHsmAdminIdentifyCtrl() - Get HSM unique identifier
│   ├── Validate HSM serial number (non-zero check)
│   └── AziHsmGetApiRevision() - Determine API capabilities
├── 6. BKS3 Key Derivation and Sealing Workflow
│   ├── Unseal platform secret (mAziHsmSealedPlatformSecret)
│   ├── AziHsmDeriveBKS3fromId() - Derive device-specific BKS3 key
│   ├── AziHsmPerformBks3SealingWorkflow() - Complete sealing workflow:
│   │   ├── AziHsmInitBks3() - Get wrapped key from HSM
│   │   ├── Generate random AES key/IV from TPM
│   │   ├── Encrypt wrapped key with AES-256-CBC
│   │   ├── Seal AES key/IV to TPM Null hierarchy
│   │   └── AziHsmSetSealedBks3() - Send sealed blob to HSM
└── 7. Cleanup and Return
    └── Zero all temporary key material
```

### Key Management System (BKS3)

The Boot Key Storage System 3 (BKS3) provides hierarchical key management:

#### Key Derivation Hierarchy
```
TPM Platform Hierarchy
├── AziHsmDeriveSecretFromTpm()
│   └── Base Platform Key (unique per boot)
├── AziHsmSealToNullHierarchy()
│   └── Sealed Platform Key (persistent during boot session)
├── Device-Specific Derivation
│   ├── Input: Sealed Platform Key + HSM Serial Number
│   ├── AziHsmDeriveSecretFromBlob()
│   └── Output: Device-Specific BKS3 Key
└── HSM Integration
    └── AziHsmInitBks3() - Provision key to HSM hardware
```

#### Security Properties
- **Boot Session Binding**: Keys are sealed to TPM null hierarchy, ensuring they don't persist across reboots
- **Device Uniqueness**: Each HSM device gets a unique key derived from its serial number
- **Forward Secrecy**: Key material is automatically invalidated at boot transition

### Sensitive Data Cleanup System

The driver implements a multi-layered cleanup strategy to ensure sensitive data protection:

#### Cleanup Triggers

1. **Ready-to-Boot Event** (`AziHsmReadyToBootCallback`)
   - Triggered when UEFI prepares to launch an operating system
   - Indicates successful boot path
   - Clears sensitive data before OS takes control

2. **Unable-to-Boot Event** (`AziHsmUnableToBootCallback`)
   - Triggered when system cannot find bootable devices
   - Handles boot failure scenarios
   - Ensures cleanup even when boot process fails

3. **Driver Binding Stop** (`AziHsmDriverBindingStop`)
   - Triggered when driver is unloaded or device is removed
   - Provides backup cleanup mechanism
   - Handles unexpected driver termination

#### Cleanup Implementation (`AziHsmCleanupSensitiveData`)

```c
Security Measures:
├── Idempotency Protection (mSensitiveDataCleared flag)
├── Platform Hierarchy Key Clearing
│   ├── ZeroMem(&mAziHsmPHSealedKey)
│   └── mAziHsmPHSealedKeyInit = FALSE
└── Debug Logging (for security audit trail)
```

### Hardware Communication Layer

#### PCI Interface Management
- **Vendor/Device ID**: 0x1414/0xC003
- **DMA Support**: 64-bit addressing capability
- **Attribute Management**: PCI configuration and resource management

#### Queue-Based Communication
```
AZIHSM_CONTROLLER_STATE
├── AdminQueue (AZIHSM_IO_QUEUE_PAIR)
│   └── Administrative commands and responses
├── HsmQueue (AZIHSM_IO_QUEUE_PAIR)
│   └── HSM-specific cryptographic operations
└── HsmQueuesCreated (Boolean)
    └── Queue initialization status tracking
```

#### Host Controller Interface (HCI)
- **Initialization**: `AziHsmHciInitialize()`
- **Cleanup**: `AziHsmHciUninitialize()`
- **Asynchronous Operations**: Queue-based command/response handling

### Protocol and Interface Specifications

#### AziHsm Protocol (`gMsvmAziHsmProtocolGuid`)
```c
typedef struct _AZIHSM_PROTOCOL {
  UINT64    _Reserved;  // Currently reserved for future use
} AZIHSM_PROTOCOL;
```

#### Component Name Support
- **English Language Support**: "eng;en"
- **Driver Name**: "Azure Integrated HSM Driver"
- **Controller Name**: "Azure Integrated HSM Controller"

### Error Handling and Diagnostics

#### Debug Infrastructure
- **Comprehensive Logging**: All major operations include debug output
- **Error Propagation**: Proper EFI status code handling throughout
- **Security Events**: Special logging for sensitive data operations

#### Failure Recovery
- **Graceful Degradation**: System continues to boot even if HSM initialization fails
- **Resource Cleanup**: Proper cleanup on all error paths
- **Memory Management**: No memory leaks on failure scenarios

## Boot Integration Points

### UEFI Boot Services Integration
- **Event Notification**: Integration with UEFI event system
- **Protocol Installation**: Standard UEFI protocol management
- **Memory Management**: UEFI-compliant memory allocation/deallocation

### Platform Configuration Dependencies
- **TPM Availability**: Requires functional TPM 2.0 for key operations
- **PCI Infrastructure**: Requires PCI bus support for device discovery
- **Isolation Support**: Integration with platform isolation features

## Security Considerations

### Threat Model
1. **Key Exposure**: Sensitive keys exposed to unauthorized access
2. **Boot Persistence**: Keys persisting beyond intended boot session
3. **Device Impersonation**: Malicious devices masquerading as HSMs
4. **Memory Dumps**: Sensitive data visible in memory dumps

### Mitigation Strategies
1. **Automatic Cleanup**: Multi-trigger cleanup system
2. **TPM Sealing**: Keys bound to current boot session
3. **Device Authentication**: PCI ID validation and HSM identity verification
4. **Memory Zeroing**: Explicit zeroing of all sensitive buffers

### Compliance and Standards
- **UEFI Specification**: Full compliance with UEFI 2.8+ requirements
- **Security Best Practices**: Implementation follows industry security guidelines
- **Microsoft Security Standards**: Adherence to Microsoft internal security policies

## Performance Characteristics

### Boot Impact
- **Minimal Boot Delay**: Efficient initialization procedures
- **Asynchronous Operations**: Non-blocking HSM communication where possible
- **Resource Efficiency**: Minimal memory footprint and CPU usage

### Scalability
- **Multiple Device Support**: Can manage multiple HSM devices simultaneously
- **Queue Management**: Efficient queue-based communication model
- **Memory Management**: Dynamic allocation based on actual device count

## Future Extensibility

### Design Flexibility
- **Protocol Versioning**: Support for API revision evolution
- **Modular Architecture**: Clean separation of concerns for easy modification
- **Configuration Support**: Platform-specific configuration through PCDs

### Enhancement Opportunities
- **Additional HSM Types**: Framework supports multiple HSM implementations
- **Enhanced Logging**: Structured logging for better diagnostics
- **Performance Optimization**: Opportunities for further optimization

## Dependencies and Requirements

### Build Dependencies
```
Required Packages:
├── MdePkg/MdePkg.dec (Core UEFI interfaces)
├── MdeModulePkg/MdeModulePkg.dec (Standard modules)
├── SecurityPkg/SecurityPkg.dec (TPM and security)
├── CryptoPkg/CryptoPkg.dec (Cryptographic functions)
└── MsvmPkg/MsvmPkg.dec (Platform-specific)
```

### Runtime Dependencies
- **TPM 2.0**: Required for key derivation and sealing operations
- **PCI Bus Driver**: Required for device discovery and communication
- **UEFI Boot Services**: Required for event handling and protocol management

### Library Dependencies
- **BaseMemoryLib**: Memory manipulation functions
- **TpmMeasurementLib**: TPM measurement and attestation
- **BaseCryptLib**: Cryptographic primitives
- **IsolationLib**: Platform isolation support

## Conclusion

The Azure Integrated HSM DXE driver represents a comprehensive security solution for UEFI-based systems, providing robust key management capabilities while maintaining strict security boundaries. The multi-layered approach to sensitive data protection, combined with the flexible architecture, ensures both security and maintainability for Azure virtualization platforms.

The driver's design successfully balances security requirements with performance considerations, providing a foundation for secure boot processes in Azure virtual machine environments while adhering to industry standards and best practices.
