#pragma once

#include "types.h"

namespace hv
{
    struct trap_frame
    {
        union
        {
            u64 rax;
            u32 eax;
            u16 ax;
            u8  al;
        };
        union
        {
            u64 rcx;
            u32 ecx;
            u16 cx;
            u8  cl;
        };
        union
        {
            u64 rdx;
            u32 edx;
            u16 dx;
            u8  dl;
        };
        union
        {
            u64 rbx;
            u32 ebx;
            u16 bx;
            u8  bl;
        };
        union
        {
            u64 rbp;
            u32 ebp;
            u16 bp;
            u8  bpl;
        };
        union
        {
            u64 rsi;
            u32 esi;
            u16 si;
            u8  sil;
        };
        union
        {
            u64 rdi;
            u32 edi;
            u16 di;
            u8  dil;
        };
        union
        {
            u64 r8;
            u32 r8d;
            u16 r8w;
            u8  r8b;
        };
        union
        {
            u64 r9;
            u32 r9d;
            u16 r9w;
            u8  r9b;
        };
        union
        {
            u64 r10;
            u32 r10d;
            u16 r10w;
            u8  r10b;
        };
        union 
        {
            u64 r11;
            u32 r11d;
            u16 r11w;
            u8  r11b;
        };
        union 
        {
            u64 r12;
            u32 r12d;
            u16 r12w;
            u8  r12b;
        };
        union 
        {
            u64 r13;
            u32 r13d;
            u16 r13w;
            u8  r13b;
        };
        union 
        {
            u64 r14;
            u32 r14d;
            u16 r14w;
            u8  r14b;
        };
        union 
        {
            u64 r15;
            u32 r15d;
            u16 r15w;
            u8  r15b;
        };

        u8 vector;

        u64 error;
        u64 rip;
        u64 cs;
        u64 rflags;
        u64 rsp;
        u64 ss;
    };

    //update this value
    static_assert(sizeof(trap_frame) == (0x78 + 0x38));
}

