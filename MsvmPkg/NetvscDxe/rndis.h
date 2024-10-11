/** @file
    This module defines the Remote NDIS message structures.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

//
//  Basic types
//
typedef UINT32                                  RNDIS_REQUEST_ID;
typedef UINT32                                  RNDIS_HANDLE;
typedef UINT32                                  RNDIS_STATUS;
typedef UINT32                                  RNDIS_REQUEST_TYPE;
typedef UINT32                                  RNDIS_OID;
typedef UINT32                                  RNDIS_CLASS_ID;
typedef UINT32                                  RNDIS_MEDIUM;
typedef UINT32                                  *PRNDIS_REQUEST_ID;
typedef UINT32                                  *PRNDIS_HANDLE;
typedef UINT32                                  *PRNDIS_STATUS;
typedef UINT32                                  *PRNDIS_REQUEST_TYPE;
typedef UINT32                                  *PRNDIS_OID;
typedef UINT32                                  *PRNDIS_CLASS_ID;
typedef UINT32                                  *PRNDIS_MEDIUM;

//
//  Status codes
//

#define RNDIS_STATUS_SUCCESS                    ((RNDIS_STATUS)0x00000000L)

#define RNDIS_STATUS_MEDIA_CONNECT              ((RNDIS_STATUS)0x4001000BL)
#define RNDIS_STATUS_MEDIA_DISCONNECT           ((RNDIS_STATUS)0x4001000CL)
// NDIS Status value for REMOTE_NDIS_INDICATE_STATUS_MSG messages
#define NDIS_STATUS_NETWORK_CHANGE              ((NDIS_STATUS)0x40010018L)

//
// General Objects
//

#define RNDIS_OID_GEN_CURRENT_PACKET_FILTER             0x0001010E

//
// 802.3 Objects (Ethernet)
//

#define RNDIS_OID_802_3_CURRENT_ADDRESS                 0x01010102

//
// Remote NDIS message types
//
#define REMOTE_NDIS_PACKET_MSG                  0x00000001
#define REMOTE_NDIS_INITIALIZE_MSG              0x00000002
#define REMOTE_NDIS_HALT_MSG                    0x00000003
#define REMOTE_NDIS_QUERY_MSG                   0x00000004
#define REMOTE_NDIS_SET_MSG                     0x00000005
#define REMOTE_NDIS_RESET_MSG                   0x00000006
#define REMOTE_NDIS_INDICATE_STATUS_MSG         0x00000007
#define REMOTE_NDIS_KEEPALIVE_MSG               0x00000008
#define REMOTE_NDIS_SET_EX_MSG                  0x00000009

#define REMOTE_CONDIS_MP_CREATE_VC_MSG          0x00008001
#define REMOTE_CONDIS_MP_DELETE_VC_MSG          0x00008002
#define REMOTE_CONDIS_MP_ACTIVATE_VC_MSG        0x00008005
#define REMOTE_CONDIS_MP_DEACTIVATE_VC_MSG      0x00008006
#define REMOTE_CONDIS_INDICATE_STATUS_MSG       0x00008007


// Remote NDIS message completion types
#define REMOTE_NDIS_INITIALIZE_CMPLT            0x80000002
#define REMOTE_NDIS_QUERY_CMPLT                 0x80000004
#define REMOTE_NDIS_SET_CMPLT                   0x80000005

//
// Reserved message type for private communication between lower-layer
// host driver and remote device, if necessary.
//
#define REMOTE_NDIS_BUS_MSG                     0xff000001


//
//  Remote NDIS version numbers
//
#define RNDIS_MAJOR_VERSION                 0x00000001
#define RNDIS_MINOR_VERSION                 0x00000000

//
//  NdisInitialize message
//
typedef struct _RNDIS_INITIALIZE_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    UINT32                                  MajorVersion;
    UINT32                                  MinorVersion;
    UINT32                                  MaxTransferSize;
} RNDIS_INITIALIZE_REQUEST, *PRNDIS_INITIALIZE_REQUEST;


//
//  Response to NdisInitialize
//
typedef struct _RNDIS_INITIALIZE_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
    UINT32                                  MajorVersion;
    UINT32                                  MinorVersion;
    UINT32                                  DeviceFlags;
    RNDIS_MEDIUM                            Medium;
    UINT32                                  MaxPacketsPerMessage;
    UINT32                                  MaxTransferSize;
    UINT32                                  PacketAlignmentFactor;
    UINT32                                  AFListOffset;
    UINT32                                  AFListSize;
} RNDIS_INITIALIZE_COMPLETE, *PRNDIS_INITIALIZE_COMPLETE;



//
//  NdisHalt message
//
typedef struct _RNDIS_HALT_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
} RNDIS_HALT_REQUEST, *PRNDIS_HALT_REQUEST;


//
// NdisQueryRequest message
//
typedef struct _RNDIS_QUERY_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_OID                               Oid;
    UINT32                                  InformationBufferLength;
    UINT32                                  InformationBufferOffset;
    RNDIS_HANDLE                            DeviceVcHandle;
} RNDIS_QUERY_REQUEST, *PRNDIS_QUERY_REQUEST;


//
//  Response to NdisQueryRequest
//
typedef struct _RNDIS_QUERY_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
    UINT32                                  InformationBufferLength;
    UINT32                                  InformationBufferOffset;
} RNDIS_QUERY_COMPLETE, *PRNDIS_QUERY_COMPLETE;


//
//  NdisSetRequest message
//
typedef struct _RNDIS_SET_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_OID                               Oid;
    UINT32                                  InformationBufferLength;
    UINT32                                  InformationBufferOffset;
    RNDIS_HANDLE                            DeviceVcHandle;
} RNDIS_SET_REQUEST, *PRNDIS_SET_REQUEST;


//
//  Response to NdisSetRequest
//
typedef struct _RNDIS_SET_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
} RNDIS_SET_COMPLETE, *PRNDIS_SET_COMPLETE;


//
//  NdisSetExRequest message
//
typedef struct _RNDIS_SET_EX_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_OID                               Oid;
    UINT32                                  InformationBufferLength;
    UINT32                                  InformationBufferOffset;
    RNDIS_HANDLE                            DeviceVcHandle;
} RNDIS_SET_EX_REQUEST, *PRNDIS_SET_EX_REQUEST;


//
//  Response to NdisSetExRequest
//
typedef struct _RNDIS_SET_EX_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
    UINT32                                  InformationBufferLength;
    UINT32                                  InformationBufferOffset;
} RNDIS_SET_EX_COMPLETE, *PRNDIS_SET_EX_COMPLETE;


//
//  NdisReset message
//
typedef struct _RNDIS_RESET_REQUEST
{
    UINT32                                  Reserved;
} RNDIS_RESET_REQUEST, *PRNDIS_RESET_REQUEST;

//
//  Response to NdisReset
//
typedef struct _RNDIS_RESET_COMPLETE
{
    RNDIS_STATUS                            Status;
    UINT32                                  AddressingReset;
} RNDIS_RESET_COMPLETE, *PRNDIS_RESET_COMPLETE;


//
//  NdisMIndicateStatus message
//
typedef struct _RNDIS_INDICATE_STATUS
{
    RNDIS_STATUS                            Status;
    UINT32                                  StatusBufferLength;
    UINT32                                  StatusBufferOffset;
} RNDIS_INDICATE_STATUS, *PRNDIS_INDICATE_STATUS;


//
//  Diagnostic information passed as the status buffer in
//  RNDIS_INDICATE_STATUS messages signifying error conditions.
//
typedef struct _RNDIS_DIAGNOSTIC_INFO
{
    RNDIS_STATUS                            DiagStatus;
    UINT32                                  ErrorOffset;
} RNDIS_DIAGNOSTIC_INFO, *PRNDIS_DIAGNOSTIC_INFO;



//
//  NdisKeepAlive message
//
typedef struct _RNDIS_KEEPALIVE_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
} RNDIS_KEEPALIVE_REQUEST, *PRNDIS_KEEPALIVE_REQUEST;


//
// Response to NdisKeepAlive
//
typedef struct _RNDIS_KEEPALIVE_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
} RNDIS_KEEPALIVE_COMPLETE, *PRNDIS_KEEPALIVE_COMPLETE;


//
//  Data message. All Offset fields contain byte offsets from the beginning
//  of the RNDIS_PACKET structure. All Length fields are in bytes.
//  VcHandle is set to 0 for connectionless data, otherwise it
//  contains the VC handle.
//
typedef struct _RNDIS_PACKET
{
    UINT32                                  DataOffset;
    UINT32                                  DataLength;
    UINT32                                  OOBDataOffset;
    UINT32                                  OOBDataLength;
    UINT32                                  NumOOBDataElements;
    UINT32                                  PerPacketInfoOffset;
    UINT32                                  PerPacketInfoLength;
    RNDIS_HANDLE                            VcHandle;
    UINT32                                  Reserved;
} RNDIS_PACKET, *PRNDIS_PACKET;


//
//  Format of Information buffer passed in a SetRequest for the OID
//  OID_GEN_RNDIS_CONFIG_PARAMETER.
//
typedef struct _RNDIS_CONFIG_PARAMETER_INFO
{
    UINT32                                  ParameterNameOffset;
    UINT32                                  ParameterNameLength;
    UINT32                                  ParameterType;
    UINT32                                  ParameterValueOffset;
    UINT32                                  ParameterValueLength;
} RNDIS_CONFIG_PARAMETER_INFO, *PRNDIS_CONFIG_PARAMETER_INFO;

//
//  CONDIS Miniport messages for connection oriented devices
//  that do not implement a call manager.
//

//
//  CoNdisMiniportCreateVc message
//
typedef struct _RCONDIS_MP_CREATE_VC
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_HANDLE                            NdisVcHandle;
} RCONDIS_MP_CREATE_VC, *PRCONDIS_MP_CREATE_VC;

//
//  Response to CoNdisMiniportCreateVc
//
typedef struct _RCONDIS_MP_CREATE_VC_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_HANDLE                            DeviceVcHandle;
    RNDIS_STATUS                            Status;
} RCONDIS_MP_CREATE_VC_COMPLETE, *PRCONDIS_MP_CREATE_VC_COMPLETE;


//
//  CoNdisMiniportDeleteVc message
//
typedef struct _RCONDIS_MP_DELETE_VC
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_HANDLE                            DeviceVcHandle;
} RCONDIS_MP_DELETE_VC, *PRCONDIS_MP_DELETE_VC;

//
//  Response to CoNdisMiniportDeleteVc
//
typedef struct _RCONDIS_MP_DELETE_VC_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
} RCONDIS_MP_DELETE_VC_COMPLETE, *PRCONDIS_MP_DELETE_VC_COMPLETE;


//
//  CoNdisMiniportQueryRequest message
//
typedef struct _RCONDIS_MP_QUERY_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_REQUEST_TYPE                      RequestType;
    RNDIS_OID                               Oid;
    RNDIS_HANDLE                            DeviceVcHandle;
    UINT32                                  InformationBufferLength;
    UINT32                                  InformationBufferOffset;
} RCONDIS_MP_QUERY_REQUEST, *PRCONDIS_MP_QUERY_REQUEST;


//
//  CoNdisMiniportSetRequest message
//
typedef struct _RCONDIS_MP_SET_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_REQUEST_TYPE                      RequestType;
    RNDIS_OID                               Oid;
    RNDIS_HANDLE                            DeviceVcHandle;
    UINT32                                  InformationBufferLength;
    UINT32                                  InformationBufferOffset;
} RCONDIS_MP_SET_REQUEST, *PRCONDIS_MP_SET_REQUEST;


//
//  CoNdisIndicateStatus message
//
typedef struct _RCONDIS_INDICATE_STATUS
{
    RNDIS_HANDLE                            NdisVcHandle;
    RNDIS_STATUS                            Status;
    UINT32                                  StatusBufferLength;
    UINT32                                  StatusBufferOffset;
} RCONDIS_INDICATE_STATUS, *PRCONDIS_INDICATE_STATUS;


//
//  CONDIS Call/VC parameters
//

typedef struct _RCONDIS_SPECIFIC_PARAMETERS
{
    UINT32                                  ParameterType;
    UINT32                                  ParameterLength;
    UINT32                                  ParameterOffset;
} RCONDIS_SPECIFIC_PARAMETERS, *PRCONDIS_SPECIFIC_PARAMETERS;

typedef struct _RCONDIS_MEDIA_PARAMETERS
{
    UINT32                                  Flags;
    UINT32                                  Reserved1;
    UINT32                                  Reserved2;
    RCONDIS_SPECIFIC_PARAMETERS             MediaSpecific;
} RCONDIS_MEDIA_PARAMETERS, *PRCONDIS_MEDIA_PARAMETERS;


typedef struct _RNDIS_FLOWSPEC
{
    UINT32                                  TokenRate;
    UINT32                                  TokenBucketSize;
    UINT32                                  PeakBandwidth;
    UINT32                                  Latency;
    UINT32                                  DelayVariation;
    UINT32                                  ServiceType;
    UINT32                                  MaxSduSize;
    UINT32                                  MinimumPolicedSize;
} RNDIS_FLOWSPEC, *PRNDIS_FLOWSPEC;

typedef struct _RCONDIS_CALL_MANAGER_PARAMETERS
{
    RNDIS_FLOWSPEC                          Transmit;
    RNDIS_FLOWSPEC                          Receive;
    RCONDIS_SPECIFIC_PARAMETERS             CallMgrSpecific;
} RCONDIS_CALL_MANAGER_PARAMETERS, *PRCONDIS_CALL_MANAGER_PARAMETERS;

//
//  CoNdisMiniportActivateVc message
//
typedef struct _RCONDIS_MP_ACTIVATE_VC_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    UINT32                                  Flags;
    RNDIS_HANDLE                            DeviceVcHandle;
    UINT32                                  MediaParamsOffset;
    UINT32                                  MediaParamsLength;
    UINT32                                  CallMgrParamsOffset;
    UINT32                                  CallMgrParamsLength;
} RCONDIS_MP_ACTIVATE_VC_REQUEST, *PRCONDIS_MP_ACTIVATE_VC_REQUEST;

//
//  Response to CoNdisMiniportActivateVc
//
typedef struct _RCONDIS_MP_ACTIVATE_VC_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
} RCONDIS_MP_ACTIVATE_VC_COMPLETE, *PRCONDIS_MP_ACTIVATE_VC_COMPLETE;


//
//  CoNdisMiniportDeactivateVc message
//
typedef struct _RCONDIS_MP_DEACTIVATE_VC_REQUEST
{
    RNDIS_REQUEST_ID                        RequestId;
    UINT32                                  Flags;
    RNDIS_HANDLE                            DeviceVcHandle;
} RCONDIS_MP_DEACTIVATE_VC_REQUEST, *PRCONDIS_MP_DEACTIVATE_VC_REQUEST;

//
//  Response to CoNdisMiniportDeactivateVc
//
typedef struct _RCONDIS_MP_DEACTIVATE_VC_COMPLETE
{
    RNDIS_REQUEST_ID                        RequestId;
    RNDIS_STATUS                            Status;
} RCONDIS_MP_DEACTIVATE_VC_COMPLETE, *PRCONDIS_MP_DEACTIVATE_VC_COMPLETE;


//
// union with all of the RNDIS messages
//
typedef union _RNDIS_MESSAGE_CONTAINER
{
    RNDIS_PACKET                        Packet;
    RNDIS_INITIALIZE_REQUEST            InitializeRequest;
    RNDIS_HALT_REQUEST                  HaltRequest;
    RNDIS_QUERY_REQUEST                 QueryRequest;
    RNDIS_SET_REQUEST                   SetRequest;
    RNDIS_SET_EX_REQUEST                SetExRequest;
    RNDIS_RESET_REQUEST                 ResetRequest;
    RNDIS_KEEPALIVE_REQUEST             KeepaliveRequest;
    RNDIS_INDICATE_STATUS               IndicateStatus;
    RNDIS_INITIALIZE_COMPLETE           InitializeComplete;
    RNDIS_QUERY_COMPLETE                QueryComplete;
    RNDIS_SET_COMPLETE                  SetComplete;
    RNDIS_SET_EX_COMPLETE               SetExComplete;
    RNDIS_RESET_COMPLETE                ResetComplete;
    RNDIS_KEEPALIVE_COMPLETE            KeepaliveComplete;
    RCONDIS_MP_CREATE_VC                CoMiniportCreateVc;
    RCONDIS_MP_DELETE_VC                CoMiniportDeleteVc;
    RCONDIS_INDICATE_STATUS             CoIndicateStatus;
    RCONDIS_MP_ACTIVATE_VC_REQUEST      CoMiniportActivateVc;
    RCONDIS_MP_DEACTIVATE_VC_REQUEST    CoMiniportDeactivateVc;
    RCONDIS_MP_CREATE_VC_COMPLETE       CoMiniportCreateVcComplete;
    RCONDIS_MP_DELETE_VC_COMPLETE       CoMiniportDeleteVcComplete;
    RCONDIS_MP_ACTIVATE_VC_COMPLETE     CoMiniportActivateVcComplete;
    RCONDIS_MP_DEACTIVATE_VC_COMPLETE   CoMiniportDeactivateVcComplete;
} RNDIS_MESSAGE_CONTAINER, *PRNDIS_MESSAGE_CONTAINER;

//
// Remote NDIS message format
//
typedef struct _RNDIS_MESSAGE
{
    UINT32                                  NdisMessageType;

    //
    // Total length of this message, from the beginning
    // of the RNDIS_MESSAGE struct, in bytes.
    //
    UINT32                                  MessageLength;

    // Actual message
    RNDIS_MESSAGE_CONTAINER                 Message;

} RNDIS_MESSAGE, *PRNDIS_MESSAGE;



//
// Handy macros

// get the size of an RNDIS message. Pass in the message type,
// RNDIS_SET_REQUEST, RNDIS_PACKET for example
#define RNDIS_MESSAGE_SIZE(Message)                             \
    (sizeof(Message) + (sizeof(RNDIS_MESSAGE) - sizeof(RNDIS_MESSAGE_CONTAINER)))

