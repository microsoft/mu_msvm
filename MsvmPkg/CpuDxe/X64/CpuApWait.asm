      TITLE   CpuAsm.asm: 
;++
;
; Copyright (c) 2013  Microsoft Corporation
;
; Module Name:
;
;   CpuApWait.asm
;
; Abstract:
;
;   This module implements functions to support the APs waiting in the mailbox for TDX guests.
;
;--

include macamd64.inc

altentry ApWaitInMailboxEnd

#define COMMAND_OFFSET           0x0
#define APICID_OFFSET            0x4
#define WAKEUP_VECTOR_OFFSET     0x8

#define MpCommandNoop            0x0
#define MpCommandWakeup          0x1

#define HasVcpuEnteredMailboxWaitOffset   0x800   

;
; AP loops and wait in the mailbox.
;
; @param[in]  R8:  Mailbox address
; @param[in]  R9:  VCPU Index
; @param[in]  R10: PageTableBase
;
; @return     None  This routine does not return
;

LEAF_ENTRY ApWaitInMailbox, _TEXT$00

    ; Update the CR3 to point to the updated page table which maps the mailbox page.
    mov        CR3, R10 

    ; Set the HasVcpuEnteredMailboxWait in the mailbox so that CpuDxe can continue processing
    ; the next AP.
    mov   dword ptr[R8 + HasVcpuEnteredMailboxWaitOffset], 1

    ; Build the APIC ID and mailbox command as a single 8-byte quantity
    shl     R9, 32
    or      R9, MpCommandWakeup

MailBoxLoop:

    ; Wait until the command is set
    cmp        [R8 + COMMAND_OFFSET], R9
    jne         MailBoxLoop

    ; Capture the wake vector and clear the mailbox command with a memory barrier
    mov        R10, [R8 + WAKEUP_VECTOR_OFFSET]
    mov        R9d, MpCommandNoop
    lock xchg  [R8 + COMMAND_OFFSET], R9w

    jmp        R10

ALTERNATE_ENTRY ApWaitInMailboxEnd

LEAF_END ApWaitInMailbox, _TEXT$00

END

