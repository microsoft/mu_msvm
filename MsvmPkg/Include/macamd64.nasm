;++
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;
; Module:
;
;   macamd64.nasm
;
; Astract:
;
;   Contains AMD64 public architecture constants and assembly macros.
;   Aids portability from masm to nasm.
;
; Author:
;--

;++
;
; push_reg <reg>
;
; Macro Description:
;
;   This macro emits a single-byte push <reg> instruction in a
;   nested prologue, as well as the associated unwind code.
;
; Arguments:
;
;   reg - supplies the integer register to push
;
;--

%imacro push_reg 1
        push    %1
%endmacro

;++
;
; rex_push_reg <reg>
;
; Macro Description:
;
;   This macro emits a single-byte push <reg> instruction in a
;   nested prologue, as well as the associated unwind code.
;
;   This differs from push_reg only in that a redundant rex prefix
;   is added.  rex_push_reg must be used in lieu of push_reg when it
;   appears as the first instruction in a function, as the calling
;   standard dictates that functions must not begin with a single
;   byte instruction.
;
; Arguments:
;
;   reg - supplies the integer register to push
;
;--

%imacro rex_push_reg 1
        db      048h
        push    Reg
%endmacro

;++
;
; push_eflags
;
; Macro Description:
;
;   This macro emits a single-byte pushfq instruction in a
;   nested prologue, as well as the associated unwind code.
;
; Arguments:
;
;   none
;
;--

%imacro push_eflags 0
        pushfq
%endmacro

;++
;
; rex_push_eflags
;
; Macro Description:
;
;   This macro emits a single-byte pushfq instruction in a
;   nested prologue, as well as the associated unwind code.
;
;   This differs from push_eflags only in that a redundant rex prefix
;   is added.  rex_push_eflags must be used in lieu of push_eflags when it
;   appears as the first instruction in a function, as the calling
;   standard dictates that functions must not begin with a single
;   byte instruction.
;
; Arguments:
;
;   none
;
;--

%imacro rex_push_eflags 0
        db      048h
        pushfq
%endmacro

;++
;
; alloc_stack <Size>
;
; Macro Description:
;
;   This macro emits an opcode to subtract <Size> from rsp, as well
;   as the associated unwind code.
;
; Arguments:
;
;   Size - The number of bytes to subtract from rsp.
;
;--
%imacro alloc_stack 1
        sub     rsp, %1
%endmacro

;++
;
; save_reg   <Reg>, <Offset>
;
; Macro Description:
;
;   This macro emits an opcode to save the non-volatile 64-bit general purpose
;   register indicated by <Reg> at offset <Offset> relative to the current
;   position of the stack pointer.  It also generates the associated unwind
;   code.
;
; Arguments:
;
;   Reg - Supplies the integer register to save
;
;   Offset - Supplies the offset relative to the current position of the stack
;            pointer.
;
;--
%imacro save_reg 2
        mov     %2[rsp], %1
%endmacro

;++
;
; save_xmm128   <Reg>, <Offset>
;
; Macro Description:
;
;   This macro emits an opcode to save the 128-bit non-volatile xmm register
;   indicated by <Reg> at offset <Offset> relative to the current position
;   of the stack pointer.  It also generates the associated unwind code.
;
; Arguments:
;
;   Reg - Supplies the xmm register register to save
;
;   Offset - Supplies the offset relative to the current position of the stack
;            pointer.
;
;--

%imacro save_xmm128 2
        movaps  %2[rsp], %1
%endmacro

;++
;
; push_frame
;
; Macro Description:
;
;   This macro emits unwind data indicating that a machine frame has been
;   pushed on the stack (usually by the CPU in response to a trap or fault).
;
; Arguments:
;
;   None.
;
;--

%imacro push_frame 1
%endmacro

;++
;
; set_frame <Reg>, <Offset>
;
; Macro Description:
;
;   This macro emits an opcode and unwind data establishing the use of <Reg>
;   as the current stack frame pointer.
;
; Arguments:
;
;   Reg - Supplies the integer register to use as the current stack frame
;         pointer.
;
;   Offset - Supplies the optional offset of the frame pointer relative to
;            the stack frame.  In stack frames greater than 080h bytes,
;            a non-zero offset can help reduce the size of subsequent opcodes
;            that access portions of the stack frame by facilitating the use of
;            positive and negative single-byte displacements.
;
;            If not supplied, no offset is assumed.
;
;--

%imacro set_frame 2

%if %2

        lea     %1, %2[rsp]

%else

        mov     %1, rsp

%endif

%endmacro

;++
;
; END_PROLOGUE
;
; Macro Description:
;
;   This macro marks the end of the prologue.  This must appear after all
;   of the prologue directives in a nested function.
;
; Arguments:
;
;   None.
;
;--

%imacro END_PROLOGUE 0
%endmacro

;++
;
; Macro Description:
;
;   This macro marks the beginning of a function epilogue. It may appear
;   one or more times within a function body. The epilogue ends at the
;   next control transfer instruction.
;
; Arguments:
;
;   None.
;
;--

%imacro BEGIN_EPILOGUE 0
%endmacro

;++
;
; LEAF_ENTRY <Name>, <Section>, <NoPad>
;
; Macro Description:
;
;   This macro indicates the beginning of a leaf function.
;
;   A leaf function is one that DOES NOT:
;
;   - manipulate non-volatile registers
;   - manipulate the stack pointer
;   - call other functions
;   - reference an exception handler
;   - contain a prologue
;   - have any unwind data associated with it
;
; Arguments:
;
;   Name - Supplies the name of the function
;
;   Section - Supplies the name of the section within which the function
;             is to appear
;
;   NoPad - If present, indicates that the function should not be prefixed
;           with 6 bytes of padding.  This is for internal use only - the
;           calling standard dictates that functions (nested and leaf) must
;           be prefixed with padding.
;
;--

%imacro LEAF_ENTRY 2

global %1
%1:

%endmacro

;++
;
; LEAF_END <Name>, <Section>
;
; Macro Description:
;
;   This macro indicates the end of a leaf function.  It must be paired
;   with a LEAF_ENTRY macro that includes matching Name and Section
;   parameters.
;
; Arguments:
;
;   Name - Supplies the name of the function.  Must match that supplied to
;          the corresponding LEAF_ENTRY macro.
;
;   Section - Supplies the name of the section within which the function
;             is to appear.  Must match that supplied to the corresponding
;             LEAF_ENTRY macro.
;
;--

%imacro LEAF_END 2
%endmacro

;++
;
; NESTED_ENTRY <Name>, <Section>, <Handler>, <NoPad>
;
; Macro Description:
;
;   This macro indicates the beginning of a nested function.
;
;   A nested function is one that does any of the following:
;
;   - manipulates non-volatile registers
;   - manipulates the stack pointer
;   - references an exception handler
;   - calls other functions
;
;   A nested function must include a prologue with unwind data.
;
; Arguments:
;
;   Name - Supplies the name of the function.
;
;   Section - Supplies the name of the section within which the function
;             is to appear.
;
;   Handler - Supplies the name of the handler for exceptions raised
;             within the scope of this function.
;
;   NoPad - If present, indicates that the function should not be prefixed
;           with 6 bytes of padding.  This is for internal use only - the
;           calling standard dictates that functions (nested and leaf) must
;           be prefixed with padding.
;
;--

%imacro NESTED_ENTRY 2

global %1
%1:

%endmacro

;++
;
; NESTED_END <Name>, <Section>
;
; Macro Description:
;
;   This macro indicates the end of a nested function.  It must be paired
;   with a NESTED_ENTRY macro that includes matching Name and Section
;   parameters.
;
; Arguments:
;
;   Name - Supplies the name of the function.  Must match that supplied to
;          the corresponding NESTED_ENTRY macro.
;
;   Section - Supplies the name of the section within which the function
;             is to appear.  Must match that supplied to the corresponding
;             NESTED_ENTRY macro.
;
;--

%imacro NESTED_END 2
%endmacro

;++
;
; ALTERNATE_ENTRY <Name>
;
; Macro Description:
;
;   This macro indicates an alternate entry point in a function, or
;   a synonymous name for an existing function.
;
; Arguments:
;
;   Name - Supplies the name of the alternate entry point.
;
;--

%imacro ALTERNATE_ENTRY 1
%1:
%endmacro

%imacro end 0
%endmacro

%imacro subttl 1
%endmacro

%imacro title 1
%endmacro

%imacro .savereg 2
%endmacro

%imacro altentry 1
global %1
%endmacro

%define ptr
%define fword
;%define or |
;%define not ~

%imacro include 1
%endmacro

default rel
section .text
