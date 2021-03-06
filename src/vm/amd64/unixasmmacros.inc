//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information. 
//

#define INVALIDGCVALUE -0x33333333 // 0CCCCCCCDh - the assembler considers it to be a signed integer constant

.macro NOP_3_BYTE 
        nop dword ptr [rax]
.endm

.macro NOP_2_BYTE 
        xchg ax, ax
.endm

.macro REPRET
        .byte 0xf3
        .byte 0xc3
.endm

.macro TAILJMP_RAX
        .byte 0x48
        .byte 0xFF
        .byte 0xE0
.endm

#if defined(__APPLE__)
#define C_FUNC(name) _##name
#else
#define C_FUNC(name) name
#endif

.macro PATCH_LABEL Name
        .global C_FUNC(\Name)
C_FUNC(\Name):
.endm

.macro LEAF_ENTRY Name, Section
        .global C_FUNC(\Name)
#if defined(__APPLE__)
        .text
#else
        .type \Name, %function
#endif
C_FUNC(\Name):
.endm

.macro LEAF_END_MARKED Name, Section
C_FUNC(\Name\()_End):
        .global C_FUNC(\Name\()_End)
#if !defined(__APPLE__)
        .size \Name, .-\Name
#endif
.endm

.macro LEAF_END Name, Section
        LEAF_END_MARKED \Name, \Section
.endm

.macro PREPARE_EXTERNAL_VAR Name, HelperReg
        mov \HelperReg, [rip + C_FUNC(\Name)@GOTPCREL]
.endm

.macro push_nonvol_reg Register
        push \Register
        //.pushreg \Register
.endm

.macro NESTED_ENTRY Name, Section, Handler
        LEAF_ENTRY \Name, \Section
.endm

.macro NESTED_END Name, Section
        LEAF_END \Name, \Section
.endm

.macro END_PROLOGUE
.endm

.macro alloc_stack Size
.att_syntax
        lea -\Size(%rsp), %rsp
.intel_syntax noprefix
        //.allocstack \Size
.endm

.macro set_frame Reg, Offset
        lea     \Reg, \Offset[rsp]
.endm

.macro save_reg_postrsp Reg, Offset

        .ifdef ___FRAME_REG_SET
        .error "save_reg_postrsp cannot be used after set_frame"
        .endif

        __Offset = \Offset
        mov     qword ptr [rsp + __Offset], \Reg

//
// TODO: find the right directive
//       this one gives an "unknown directive" error
//
//        .savereg \Reg, \Offset

        ___STACK_ADJUSTMENT_FORBIDDEN = 1

.endm

.macro restore_reg Reg, Offset
        __Offset = \Offset
        mov             \Reg, [rsp + __Offset]
.endm

.macro save_xmm128_postrsp Reg, Offset

        .ifdef ___FRAME_REG_SET
        .error "save_reg_postrsp cannot be used after set_frame"
        .endif

        __Offset = \Offset
        movdqa  [rsp + __Offset], \Reg

//
// TODO: find the right directive
//       this one gives an "unknown directive" error
//
//        .savexmm128 \Reg, \Offset
    
        ___STACK_ADJUSTMENT_FORBIDDEN = 1

.endm

.macro restore_xmm128 Reg, ofs
        __Offset = \ofs
        movdqa          \Reg, [rsp + __Offset]
.endm 

.macro POP_CALLEE_SAVED_REGISTERS

        pop             rbx
        pop             rbp
        pop             r12
        pop             r13
        pop             r14
        pop             r15

.endm

.macro PUSH_ARGUMENT_REGISTERS

        push            r9
        push            r8
        push            rdx
        push            rcx
        push            rsi
        push            rdi

.endm

.macro POP_ARGUMENT_REGISTERS

        pop             rdi
        pop             rsi
        pop             rcx
        pop             rdx
        pop             r8
        pop             r9

.endm

.macro SAVE_FLOAT_ARGUMENT_REGISTERS ofs

        save_xmm128_postrsp xmm0, \ofs
        save_xmm128_postrsp xmm1, \ofs + 0x10
        save_xmm128_postrsp xmm2, \ofs + 0x20
        save_xmm128_postrsp xmm3, \ofs + 0x30
        save_xmm128_postrsp xmm4, \ofs + 0x40
        save_xmm128_postrsp xmm5, \ofs + 0x50
        save_xmm128_postrsp xmm6, \ofs + 0x60
        save_xmm128_postrsp xmm7, \ofs + 0x70

.endm

.macro RESTORE_FLOAT_ARGUMENT_REGISTERS ofs

        restore_xmm128  xmm0, \ofs
        restore_xmm128  xmm1, \ofs + 0x10
        restore_xmm128  xmm2, \ofs + 0x20
        restore_xmm128  xmm3, \ofs + 0x30
        restore_xmm128  xmm4, \ofs + 0x40
        restore_xmm128  xmm5, \ofs + 0x50
        restore_xmm128  xmm6, \ofs + 0x60
        restore_xmm128  xmm7, \ofs + 0x70

.endm

// Stack layout:
//
// (stack parameters)
// ...
// return address
// CalleeSavedRegisters::r15
// CalleeSavedRegisters::r14
// CalleeSavedRegisters::r13
// CalleeSavedRegisters::r12
// CalleeSavedRegisters::rbp
// CalleeSavedRegisters::rbx 
// ArgumentRegisters::r9
// ArgumentRegisters::r8
// ArgumentRegisters::rcx
// ArgumentRegisters::rdx
// ArgumentRegisters::rsi
// ArgumentRegisters::rdi    <- __PWTB_StackAlloc, __PWTB_TransitionBlock
// padding to align xmm save area
// xmm7
// xmm6
// xmm5
// xmm4
// xmm3
// xmm2
// xmm1
// xmm0                      <- __PWTB_FloatArgumentRegisters
// extra locals + padding to qword align
.macro PROLOG_WITH_TRANSITION_BLOCK extraLocals = 0, stackAllocOnEntry = 0, stackAllocSpill1, stackAllocSpill2, stackAllocSpill3

        __PWTB_FloatArgumentRegisters = \extraLocals

        .if ((__PWTB_FloatArgumentRegisters % 16) != 0)
        __PWTB_FloatArgumentRegisters = __PWTB_FloatArgumentRegisters + 8
        .endif

        __PWTB_StackAlloc = __PWTB_FloatArgumentRegisters + 8 * 16 + 8 // 8 floating point registers
        __PWTB_TransitionBlock = __PWTB_StackAlloc

        .if \stackAllocOnEntry >= 4*8
        .error "Max supported stackAllocOnEntry is 3*8"
        .endif

        .if \stackAllocOnEntry > 0
        //.allocstack \stackAllocOnEntry
        .endif

        // PUSH_CALLEE_SAVED_REGISTERS expanded here

        .if \stackAllocOnEntry < 8
        push_nonvol_reg r15
        .endif

        .if \stackAllocOnEntry < 2*8
        push_nonvol_reg r14
        .endif

        .if \stackAllocOnEntry < 3*8
        push_nonvol_reg r13
        .endif

        push_nonvol_reg r12
        push_nonvol_reg rbp
        push_nonvol_reg rbx
        
        // ArgumentRegisters
        PUSH_ARGUMENT_REGISTERS

        .if \stackAllocOnEntry >= 3*8
        mov \stackAllocSpill3, [rsp + 0x48]
        save_reg_postrsp    r13, 0x48
        .endif

        .if \stackAllocOnEntry >= 2*8
        mov \stackAllocSpill2, [rsp + 0x50]
        save_reg_postrsp    r14, 0x50
        .endif

        .if \stackAllocOnEntry >= 8
        mov \stackAllocSpill1, [rsp + 0x58]
        save_reg_postrsp    r15, 0x58
        .endif

        alloc_stack     __PWTB_StackAlloc
        SAVE_FLOAT_ARGUMENT_REGISTERS __PWTB_FloatArgumentRegisters
        
        END_PROLOGUE

.endm

.macro EPILOG_WITH_TRANSITION_BLOCK_TAILCALL 

        RESTORE_FLOAT_ARGUMENT_REGISTERS __PWTB_FloatArgumentRegisters
        lea rsp,        [rsp + __PWTB_StackAlloc]
        POP_ARGUMENT_REGISTERS
        POP_CALLEE_SAVED_REGISTERS

.endm

