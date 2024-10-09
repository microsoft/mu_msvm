/** @file
    Implementation of collecting the statistics on a network interface.

Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Resets or collects the statistics on a network interface.

  This function resets or collects the statistics on a network interface. If the
  size of the statistics table specified by StatisticsSize is not big enough for
  all the statistics that are collected by the network interface, then a partial
  buffer of statistics is returned in StatisticsTable, StatisticsSize is set to
  the size required to collect all the available statistics, and
  EFI_BUFFER_TOO_SMALL is returned.
  If StatisticsSize is big enough for all the statistics, then StatisticsTable
  will be filled, StatisticsSize will be set to the size of the returned
  StatisticsTable structure, and EFI_SUCCESS is returned.
  If the driver has not been initialized, EFI_DEVICE_ERROR will be returned.
  If Reset is FALSE, and both StatisticsSize and StatisticsTable are NULL, then
  no operations will be performed, and EFI_SUCCESS will be returned.
  If Reset is TRUE, then all of the supported statistics counters on this network
  interface will be reset to zero.

  @param This            A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.
  @param Reset           Set to TRUE to reset the statistics for the network interface.
  @param StatisticsSize  On input the size, in bytes, of StatisticsTable. On output
                         the size, in bytes, of the resulting table of statistics.
  @param StatisticsTable A pointer to the EFI_NETWORK_STATISTICS structure that
                         contains the statistics. Type EFI_NETWORK_STATISTICS is
                         defined in "Related Definitions" below.

  @retval EFI_SUCCESS           The requested operation succeeded.
  @retval EFI_NOT_STARTED       The Simple Network Protocol interface has not been
                                started by calling Start().
  @retval EFI_BUFFER_TOO_SMALL  StatisticsSize is not NULL and StatisticsTable is
                                NULL. The current buffer size that is needed to
                                hold all the statistics is returned in StatisticsSize.
  @retval EFI_BUFFER_TOO_SMALL  StatisticsSize is not NULL and StatisticsTable is
                                not NULL. The current buffer size that is needed
                                to hold all the statistics is returned in
                                StatisticsSize. A partial set of statistics is
                                returned in StatisticsTable.
  @retval EFI_INVALID_PARAMETER StatisticsSize is NULL and StatisticsTable is not
                                NULL.
  @retval EFI_DEVICE_ERROR      The Simple Network Protocol interface has not
                                been initialized by calling Initialize().
  @retval EFI_DEVICE_ERROR      An error was encountered collecting statistics
                                from the NIC.
  @retval EFI_UNSUPPORTED       The NIC does not support collecting statistics
                                from the network interface.

**/
EFI_STATUS
EFIAPI
SnpStatistics(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      Reset,
  IN OUT UINTN                    *StatisticsSize  OPTIONAL,
  IN OUT EFI_NETWORK_STATISTICS   *StatisticsTable OPTIONAL
  )
{
  SNP_DRIVER         *Snp;
  UINT64             *outStp, *inStp;
  UINTN              Size;
  UINTN              Index;
  EFI_TPL            OldTpl;
  EFI_STATUS         Status;
  NIC_DATA_INSTANCE *adapterInfo; // MS_HYP_CHANGE

  //
  // Get pointer to SNP driver instance for *This.
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Snp = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (This);
  adapterInfo = &Snp->AdapterContext->NicInfo;  // MS_HYP_CHANGE

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Return error if the SNP is not initialized.
  //
  switch (Snp->Mode.State) {
    case EfiSimpleNetworkInitialized:
      break;

    case EfiSimpleNetworkStopped:
      Status = EFI_NOT_STARTED;
      goto ON_EXIT;

    default:
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
  }

  //
  // if we are not resetting the counters, we have to have a valid stat table
  // with >0 size. if no reset, no table and no size, return success.
  //
  if (!Reset && (StatisticsSize == NULL)) {
    Status = (StatisticsTable != NULL) ? EFI_INVALID_PARAMETER : EFI_SUCCESS;
    goto ON_EXIT;
  }

  // MS_HYP_CHANGE BEGIN
  if (Reset) {
    NetvscResetStatistics(adapterInfo);
    Status = EFI_SUCCESS;
    goto ON_EXIT;
  }

  if (StatisticsTable == NULL) {
    *StatisticsSize = sizeof (EFI_NETWORK_STATISTICS);
    Status          = EFI_BUFFER_TOO_SMALL;
    goto ON_EXIT;
  }

  //
  // Convert the NetVsc statistics information to SNP statistics
  // information.
  //
  // MS_HYP_CHANGE BEGIN
  ZeroMem (StatisticsTable, *StatisticsSize);
  outStp  = (UINT64 *)StatisticsTable;
  inStp   = (UINT64 *)&adapterInfo->Statistics;

  for (Index = 0; Index < 64; Index++, outStp++, inStp++) {
    //
    // There must be room for a full UINT64.  Partial
    // numbers will not be stored.
    //
    if ((Index + 1) * sizeof (UINT64) > *StatisticsSize) {
      break;
    }

    *outStp  = *inStp;
  }

  Size = Snp->AdapterContext->NicInfo.SupportedStatisticsSize;
  // MS_HYP_CHANGE END

  if (*StatisticsSize >= Size) {
    *StatisticsSize = Size;
    Status          = EFI_SUCCESS;
  } else {
    *StatisticsSize = Size;
    Status          = EFI_BUFFER_TOO_SMALL;
  }

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
