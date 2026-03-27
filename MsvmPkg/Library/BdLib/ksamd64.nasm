;
;  Bug Check Code Definitions
;

NMI_HARDWARE_FAILURE equ 00080H
UNEXPECTED_KERNEL_MODE_TRAP equ 0007FH
MACHINE_CHECK_EXCEPTION equ 009CH

;
;  Breakpoint type definitions
;

STATUS_ASSERTION_FAILURE equ 0C0000420H

;
;  Exception Record Definitions
;

ErExceptionCode equ 00000H
ErExceptionFlags equ 00004H
ErExceptionRecord equ 00008H
ErExceptionAddress equ 00010H
ErNumberParameters equ 00018H
ErExceptionInformation equ 00020H

;
;  Status Code Definitions
;

STATUS_ACCESS_VIOLATION equ 0C0000005H
STATUS_ARRAY_BOUNDS_EXCEEDED equ 0C000008CH
STATUS_BREAKPOINT equ 080000003H
STATUS_DATATYPE_MISALIGNMENT equ 080000002H
STATUS_FLOAT_INVALID_OPERATION equ 0C0000090H
STATUS_ILLEGAL_INSTRUCTION equ 0C000001DH
STATUS_INTEGER_DIVIDE_BY_ZERO equ 0C0000094H
STATUS_INTEGER_OVERFLOW equ 0C0000095H
STATUS_IN_PAGE_ERROR equ 0C0000006H
STATUS_SINGLE_STEP equ 080000004H
STATUS_STACK_BUFFER_OVERRUN equ 0C0000409H
STATUS_SUCCESS equ 00000H
STATUS_NONCONTINUABLE_EXCEPTION equ 0C0000025H

;
;  Miscellaneous Definitions
;

BREAKPOINT_BREAK equ 00000H

;
;  Equates for exceptions which cause system fatal error
;

EXCEPTION_NPX_NOT_AVAILABLE equ 00007H
EXCEPTION_INVALID_TSS equ 0000AH

;
;  Exception Frame Offset Definitions and Length
;

ExP5 equ 00020H
ExRbp equ 000F8H
ExRbx equ 00100H
ExRdi equ 00108H
ExRsi equ 00110H
ExR12 equ 00118H
ExR13 equ 00120H
ExR14 equ 00128H
ExR15 equ 00130H

KEXCEPTION_FRAME_LENGTH equ 00140H
EXCEPTION_RECORD_LENGTH equ 000A0H

;
;  Trap Frame Offset and EFLAG Definitions and Length
;

EFLAGS_TF_MASK equ 00100H
EFLAGS_TF_SHIFT equ 00008H

TrP5              equ -(0FFh & -(0FFFFFFA0H - 80000000h))
TrFaultIndicator  equ -(0FFh & -(0FFFFFFAAH - 80000000h))
TrExceptionActive equ -(0FFh & -(0FFFFFFABH - 80000000h))
TrRax             equ -(0FFh & -(0FFFFFFB0H - 80000000h))
TrRcx             equ -(0FFh & -(0FFFFFFB8H - 80000000h))
TrRdx             equ -(0FFh & -(0FFFFFFC0H - 80000000h))
TrR8              equ -(0FFh & -(0FFFFFFC8H - 80000000h))
TrR9              equ -(0FFh & -(0FFFFFFD0H - 80000000h))
TrR10             equ -(0FFh & -(0FFFFFFD8H - 80000000h))
TrR11             equ -(0FFh & -(0FFFFFFE0H - 80000000h))
TrFaultAddress equ 00050H
TrRbp equ 000D8H
TrErrorCode equ 000E0H
TrRip equ 000E8H
TrEFlags equ 000F8H
TrEflags equ 000F8H

KTRAP_FRAME_LENGTH equ 00190H

;
;  Context Frame Offset and Flag Definitions
;

CONTEXT_FULL equ 010000BH
CONTEXT_SEGMENTS equ 0100004H

CxP1Home equ 00000H
CxP2Home equ 00008H
CxP3Home equ 00010H
CxP4Home equ 00018H
CxP5Home equ 00020H
CxP6Home equ 00028H
CxContextFlags equ 00030H
CxMxCsr equ 00034H
CxSegCs equ 00038H
CxSegDs equ 0003AH
CxSegEs equ 0003CH
CxSegFs equ 0003EH
CxSegGs equ 00040H
CxSegSs equ 00042H
CxEFlags equ 00044H
CxRax equ 00078H
CxRcx equ 00080H
CxRdx equ 00088H
CxRbx equ 00090H
CxRsp equ 00098H
CxRbp equ 000A0H
CxRsi equ 000A8H
CxRdi equ 000B0H
CxR8 equ 000B8H
CxR9 equ 000C0H
CxR10 equ 000C8H
CxR11 equ 000D0H
CxR12 equ 000D8H
CxR13 equ 000E0H
CxR14 equ 000E8H
CxR15 equ 000F0H
CxRip equ 000F8H
CxFltSave equ 00100H
CxXmm0 equ 001A0H
CxXmm1 equ 001B0H
CxXmm2 equ 001C0H
CxXmm3 equ 001D0H
CxXmm4 equ 001E0H
CxXmm5 equ 001F0H
CxXmm6 equ 00200H
CxXmm7 equ 00210H
CxXmm8 equ 00220H
CxXmm9 equ 00230H
CxXmm10 equ 00240H
CxXmm11 equ 00250H
CxXmm12 equ 00260H
CxXmm13 equ 00270H
CxXmm14 equ 00280H
CxXmm15 equ 00290H

;
;  Legacy Floating Save Area Structure Offset definitions
;

LfControlWord equ 00000H
LfStatusWord equ 00002H
LfTagWord equ 00004H
LfErrorOpcode equ 00006H
LfErrorOffset equ 00008H
LfErrorSelector equ 0000CH
LfDataOffset equ 00010H
LfDataSelector equ 00014H
LfMxCsr equ 00018H
LfMxCsr_Mask equ 0001CH
LfFloatRegisters equ 00020H
LfXmmRegisters equ 000A0H


;
;  Processor State Frame Offset Definitions
;

PsCr0 equ 00000H
PsCr2 equ 00008H
PsCr3 equ 00010H
PsCr4 equ 00018H
PsKernelDr0 equ 00020H
PsKernelDr1 equ 00028H
PsKernelDr2 equ 00030H
PsKernelDr3 equ 00038H
PsKernelDr6 equ 00040H
PsKernelDr7 equ 00048H
PsGdtr equ 00056H
PsIdtr equ 00066H
PsTr equ 00070H
PsLdtr equ 00072H
PsCr8 equ 000A0H
