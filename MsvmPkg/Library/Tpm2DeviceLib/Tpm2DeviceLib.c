/** @file
  This is an implementation of Tpm2DeviceLib that is specific to the Hyper-V
  guest firmware.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/Tpm2DeviceLib.h>
// MS_HYP_CHANGE BEGIN
#include <Library/TimerLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
// MS_HYP_CHANGE END
#include <IndustryStandard/Tpm20.h>
// MS_HYP_CHANGE BEGIN
#include <IndustryStandard/Tpm2Acpi.h>

#include <Library/Tpm2DebugLib.h>
#include <TpmInterface.h>               // Definitions specific to Hyper-V VDev.

#pragma pack(push,1)
typedef struct _FTPM_CONTROL_AREA
{
    //
    // This used to a Reserved field. This is the Miscellaneous field for the Command/Response interface.
    //
    volatile UINT32  Miscellaneous;

    //
    // The Status field of the Control area.
    //
    volatile UINT32  Status;

    //
    // The Cancel field of the Control area. TPM does not modify this field, hence it is not declared volatile.
    //
    UINT32  Cancel;

    //
    // The Start field of the Control area.
    //
    volatile UINT32  Start;

    //
    // The control area is in device memory. Device memory
    // often only supports word sized access. Split all
    // 64bit fields into High and Low parts.
    //
    volatile UINT32 InterruptEnable;
    volatile UINT32 InterruptStatus;

    //
    // Command buffer size.
    //
    UINT32 CommandBufferSize;

    //
    // The control area is in device memory. Device memory
    // often only supports word sized access. Split all
    // 64bit fields into High and Low parts.
    //
    // Command buffer physical address.
    //
    UINT32 CommandPALow;
    UINT32 CommandPAHigh;

    //
    // Response Buffer size.
    //
    UINT32 ResponseBufferSize;

    //
    // The control area is in device memory. Device memory
    // often only supports word sized access. Split all
    // 64bit fields into High and Low parts.
    //
    // Response buffer physical address.
    //
    UINT32 ResponsePALow;
    UINT32 ResponsePAHigh;

} FTPM_CONTROL_AREA;
#pragma pack(pop)

static_assert(sizeof(EFI_TPM2_ACPI_CONTROL_AREA) == sizeof(FTPM_CONTROL_AREA), "Invalid structure!");

typedef union _LARGE_INTEGER {
    struct {
        UINT32 LowPart;
        INT32 HighPart;
    } u;
    UINT64 QuadPart;
} LARGE_INTEGER;

FTPM_CONTROL_AREA *mTpm2ControlArea = NULL;
UINT8*   mCommandBuffer = NULL;
UINT8*   mResponseBuffer = NULL;
UINT32   mResponseSize = 0;


VOID
WriteTpmPort(
    IN UINT32 AddressRegisterValue,
    IN UINT32 DataRegisterValue
)
{
#if defined(MDE_CPU_AARCH64)
    UINT64 Port = FixedPcdGet64(PcdTpmBaseAddress) + 0x80;
    MmioWrite32(Port, AddressRegisterValue);
    MmioWrite32(Port + 4, DataRegisterValue);
#elif defined(MDE_CPU_X64)
    IoWrite32(TpmControlPort, AddressRegisterValue);
    IoWrite32(TpmDataPort, DataRegisterValue);
#endif
}

UINT32
ReadTpmPort(
    IN UINT32 AddressRegisterValue
)
{
#if defined(MDE_CPU_AARCH64)
    UINT64 Port = FixedPcdGet64(PcdTpmBaseAddress) + 0x80;
    MmioWrite32(Port, AddressRegisterValue);
    return MmioRead32(Port + 4);
#elif defined(MDE_CPU_X64)
    IoWrite32(TpmControlPort, AddressRegisterValue);
    return IoRead32(TpmDataPort);
#endif
}

EFI_STATUS
EFIAPI
CRSubmitCommand (
    IN  UINT32   InputParameterBlockSize,
    IN  UINT8    *InputParameterBlock,
    IN  UINT32   OutputParameterBlockSize,
    IN  UINT8    *OutputParameterBlock
    )
/*++

Routine Description:

    This routine submits TPM command to vTPM engine.

Arguments:

    InputParameterBlockSize - Command size

    InputParameterBlock - Command Buffer

    OutputParameterBlockSize - Response buffer size

    OutputParameterBlock - Response buffer

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status = EFI_SUCCESS;
    UINT32  outputParameterSize = OutputParameterBlockSize;
    UINT32  waitTime;

    if (mTpm2ControlArea == NULL)
    {
        DEBUG((DEBUG_ERROR, "%a: Tpm2ControlArea is NULL!\n", __FUNCTION__));
        status = EFI_NOT_READY;
        goto Cleanup;
    }

    if (mTpm2ControlArea->Start != 0)
    {
        // pending command.
        DEBUG((DEBUG_ERROR, "%a: Previous command still pending!\n", __FUNCTION__));
        status = EFI_NOT_READY;
        goto Cleanup;
    }

    if (mTpm2ControlArea->Status != 0)
    {
        // device in error state.
        DEBUG((DEBUG_ERROR, "%a: Device in error state!\n", __FUNCTION__));
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    // Check if command fits into command buffer.
    if (mTpm2ControlArea->CommandBufferSize < InputParameterBlockSize)
    {
        DEBUG((DEBUG_ERROR, "%a: Command buffer too small!\n", __FUNCTION__));
        status = EFI_INVALID_PARAMETER;
        goto Cleanup;
    }

    DEBUG_CODE (
      DumpTpmInputBlock( InputParameterBlockSize, InputParameterBlock );
    );

    // Copy command to command buffer.
    CopyMem(mCommandBuffer, InputParameterBlock, InputParameterBlockSize);

    // Set Start to kick off command execution.
    mTpm2ControlArea->Start = 1;

    //
    // Wait/Poll for 90 secs timeout.
    //
    for (waitTime = 0; waitTime < 90000 * 1000; waitTime += 30)
    {
        if (mTpm2ControlArea->Start != 0)
        {
            MicroSecondDelay (30);
            continue;
        }

        if (mTpm2ControlArea->Status != 0)
        {
            status = EFI_DEVICE_ERROR;
            goto Cleanup;
        }

        //
        // Engine finished executing command, get the result back.
        //
        if (outputParameterSize > mResponseSize)
        {
            outputParameterSize = mResponseSize;
        }

        CopyMem(OutputParameterBlock, mResponseBuffer, outputParameterSize);

        DEBUG_CODE (
          DumpTpmOutputBlock( OutputParameterBlockSize, OutputParameterBlock );
        );

        goto Cleanup;
    }

    status = EFI_TIMEOUT;
    DEBUG ((EFI_D_ERROR, "SubmitCommand TIMEOUT - %r\n", status));

Cleanup:

    return status;
}
// MS_HYP_CHANGE END

/**
  This service enables the sending of commands to the TPM2.

  @param[in]      InputParameterBlockSize  Size of the TPM2 input parameter block.
  @param[in]      InputParameterBlock      Pointer to the TPM2 input parameter block.
  @param[in,out]  OutputParameterBlockSize Size of the TPM2 output parameter block.
  @param[in]      OutputParameterBlock     Pointer to the TPM2 output parameter block.

  @retval EFI_SUCCESS            The command byte stream was successfully sent to the device and a response was successfully received.
  @retval EFI_DEVICE_ERROR       The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_BUFFER_TOO_SMALL   The output parameter block is too small.
**/
EFI_STATUS
EFIAPI
Tpm2SubmitCommand (
  IN UINT32            InputParameterBlockSize,
  IN UINT8             *InputParameterBlock,
  IN OUT UINT32        *OutputParameterBlockSize,
  IN UINT8             *OutputParameterBlock
  )
{
    // MS_HYP_CHANGE BEGIN
    EFI_STATUS                status = EFI_SUCCESS;
    UINT32                    outputParameterBlockSize = (*OutputParameterBlockSize);
    TPM2_RESPONSE_HEADER      *header;

    if (InputParameterBlockSize < sizeof(TPM2_COMMAND_HEADER) || InputParameterBlock == NULL ||
        outputParameterBlockSize < sizeof(TPM2_RESPONSE_HEADER) || OutputParameterBlock == NULL)
    {
        status = EFI_INVALID_PARAMETER;
        goto Cleanup;
    }

    status = CRSubmitCommand(InputParameterBlockSize,
                                InputParameterBlock,
                                outputParameterBlockSize,
                                OutputParameterBlock
                                );
    if (EFI_ERROR (status))
    {
        goto Cleanup;
    }

    header = (TPM2_RESPONSE_HEADER *)OutputParameterBlock;
    *OutputParameterBlockSize = SwapBytes32 (header->paramSize);
    if (outputParameterBlockSize < (*OutputParameterBlockSize))
    {
        status = EFI_BUFFER_TOO_SMALL;
    }

Cleanup:

    return status;
    // MS_HYP_CHANGE END
}

/**
  This service requests use TPM2.

  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval EFI_NOT_FOUND    TPM2 not found.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
EFI_STATUS
EFIAPI
Tpm2RequestUseTpm (
  VOID
  )
{
    // MS_HYP_CHANGE BEGIN
    return EFI_SUCCESS;
    // MS_HYP_CHANGE END
}

/**
  This service register TPM2 device.

  @param Tpm2Device  TPM2 device

  @retval EFI_SUCCESS          This TPM2 device is registered successfully.
  @retval EFI_UNSUPPORTED      System does not support register this TPM2 device.
  @retval EFI_ALREADY_STARTED  System already register this TPM2 device.
**/
EFI_STATUS
EFIAPI
Tpm2RegisterTpm2DeviceLib (
  IN TPM2_DEVICE_INTERFACE   *Tpm2Device
  )
{
    // MS_HYP_CHANGE BEGIN
    mTpm2ControlArea = (FTPM_CONTROL_AREA*)Tpm2Device;

    DEBUG((DEBUG_VERBOSE, __FUNCTION__" - TpmBaseAddress == 0x%016lX\n", mTpm2ControlArea));

    // If any of these values are bad, we've failed to register this library.
    if ((mTpm2ControlArea->CommandPALow == (UINT32)-1) ||
        (mTpm2ControlArea->ResponsePALow == (UINT32)-1) ||
        (mTpm2ControlArea->ResponseBufferSize == (UINT32)-1)) {
      DEBUG(( DEBUG_ERROR, __FUNCTION__" - TPM MMIO Space at 0x%08X is not decoding!\tCannot register interface!\n", mTpm2ControlArea ));
      return EFI_DEVICE_ERROR;
    }

    LARGE_INTEGER commandBufferPA;
    commandBufferPA.u.LowPart = mTpm2ControlArea->CommandPALow;
    commandBufferPA.u.HighPart = mTpm2ControlArea->CommandPAHigh;

    LARGE_INTEGER responseBufferPA;
    responseBufferPA.u.LowPart = mTpm2ControlArea->ResponsePALow;
    responseBufferPA.u.HighPart = mTpm2ControlArea->ResponsePAHigh;

    mCommandBuffer = (UINT8*)(UINTN)commandBufferPA.QuadPart;
    mResponseBuffer = (UINT8*)(UINTN)responseBufferPA.QuadPart;
    mResponseSize = (UINTN)mTpm2ControlArea->ResponseBufferSize;

    DEBUG((DEBUG_VERBOSE, __FUNCTION__" - TPM MMIO Space at 0x%016lX, Command=0x%016lX, Response=0x%016lX, Size=0x%08X\n", mTpm2ControlArea, mCommandBuffer, mResponseBuffer, mResponseSize));

    return EFI_SUCCESS;
    // MS_HYP_CHANGE END
}
