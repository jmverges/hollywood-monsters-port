/* Runtime support for Ghidra decompiler output (hand-written + generated macros). */
#pragma once
/* All code is compiled by one compiler with one convention, so the original conventions are dropped
 * (MinGW predefines some of these as attributes). */
#undef __stdcall
#undef __cdecl
#undef __fastcall
#undef __thiscall
#undef __pascal
#undef __unaligned
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall
#define __pascal
#define __unaligned
typedef void *pointer32;
typedef void *pointer;
typedef void *ImageBaseOffset32_;
typedef unsigned char undefined;
typedef unsigned char undefined1;
typedef unsigned short undefined2;
typedef unsigned int undefined3;
typedef unsigned int undefined4;
typedef unsigned long long undefined5;
typedef unsigned long long undefined6;
typedef unsigned long long undefined7;
typedef unsigned long long undefined8;
typedef unsigned int uint3;
typedef int int3;
typedef unsigned long long uint5, uint6, uint7;
typedef long long int5, int6, int7;
typedef long double float10;
typedef int code();
typedef unsigned long long hm_u64;
typedef long long hm_s64;

/* CONCATxy yields x+y bytes: 32-bit when x+y <= 4 (a 64-bit value would take two argument slots
 * in calls to unprototyped functions and shift every following argument). */
#define CONCAT11(a,b) ((unsigned int)(((hm_u64)(a) & 0xffULL) << 8 | ((hm_u64)(b) & 0xffULL)))
#define CONCAT12(a,b) ((unsigned int)(((hm_u64)(a) & 0xffULL) << 16 | ((hm_u64)(b) & 0xffffULL)))
#define CONCAT13(a,b) ((unsigned int)(((hm_u64)(a) & 0xffULL) << 24 | ((hm_u64)(b) & 0xffffffULL)))
#define CONCAT14(a,b) ((hm_u64)(((hm_u64)(a) & 0xffULL) << 32 | ((hm_u64)(b) & 0xffffffffULL)))
#define CONCAT15(a,b) ((hm_u64)(((hm_u64)(a) & 0xffULL) << 40 | ((hm_u64)(b) & 0xffffffffffULL)))
#define CONCAT16(a,b) ((hm_u64)(((hm_u64)(a) & 0xffULL) << 48 | ((hm_u64)(b) & 0xffffffffffffULL)))
#define CONCAT17(a,b) ((hm_u64)(((hm_u64)(a) & 0xffULL) << 56 | ((hm_u64)(b) & 0xffffffffffffffULL)))
#define CONCAT21(a,b) ((unsigned int)(((hm_u64)(a) & 0xffffULL) << 8 | ((hm_u64)(b) & 0xffULL)))
#define CONCAT22(a,b) ((unsigned int)(((hm_u64)(a) & 0xffffULL) << 16 | ((hm_u64)(b) & 0xffffULL)))
#define CONCAT23(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffULL) << 24 | ((hm_u64)(b) & 0xffffffULL)))
#define CONCAT24(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffULL) << 32 | ((hm_u64)(b) & 0xffffffffULL)))
#define CONCAT25(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffULL) << 40 | ((hm_u64)(b) & 0xffffffffffULL)))
#define CONCAT26(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffULL) << 48 | ((hm_u64)(b) & 0xffffffffffffULL)))
#define CONCAT31(a,b) ((unsigned int)(((hm_u64)(a) & 0xffffffULL) << 8 | ((hm_u64)(b) & 0xffULL)))
#define CONCAT32(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffULL) << 16 | ((hm_u64)(b) & 0xffffULL)))
#define CONCAT33(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffULL) << 24 | ((hm_u64)(b) & 0xffffffULL)))
#define CONCAT34(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffULL) << 32 | ((hm_u64)(b) & 0xffffffffULL)))
#define CONCAT35(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffULL) << 40 | ((hm_u64)(b) & 0xffffffffffULL)))
#define CONCAT41(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffULL) << 8 | ((hm_u64)(b) & 0xffULL)))
#define CONCAT42(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffULL) << 16 | ((hm_u64)(b) & 0xffffULL)))
#define CONCAT43(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffULL) << 24 | ((hm_u64)(b) & 0xffffffULL)))
#define CONCAT44(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffULL) << 32 | ((hm_u64)(b) & 0xffffffffULL)))
#define CONCAT51(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffffULL) << 8 | ((hm_u64)(b) & 0xffULL)))
#define CONCAT52(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffffULL) << 16 | ((hm_u64)(b) & 0xffffULL)))
#define CONCAT53(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffffULL) << 24 | ((hm_u64)(b) & 0xffffffULL)))
#define CONCAT61(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffffffULL) << 8 | ((hm_u64)(b) & 0xffULL)))
#define CONCAT62(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffffffULL) << 16 | ((hm_u64)(b) & 0xffffULL)))
#define CONCAT71(a,b) ((hm_u64)(((hm_u64)(a) & 0xffffffffffffffULL) << 8 | ((hm_u64)(b) & 0xffULL)))
#define SUB21(v,off) ((unsigned char)(((hm_u64)(v)) >> ((off)*8)))
#define SUB31(v,off) ((unsigned char)(((hm_u64)(v)) >> ((off)*8)))
#define SUB32(v,off) ((unsigned short)(((hm_u64)(v)) >> ((off)*8)))
#define SUB41(v,off) ((unsigned char)(((hm_u64)(v)) >> ((off)*8)))
#define SUB42(v,off) ((unsigned short)(((hm_u64)(v)) >> ((off)*8)))
#define SUB43(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB51(v,off) ((unsigned char)(((hm_u64)(v)) >> ((off)*8)))
#define SUB52(v,off) ((unsigned short)(((hm_u64)(v)) >> ((off)*8)))
#define SUB53(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB54(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB61(v,off) ((unsigned char)(((hm_u64)(v)) >> ((off)*8)))
#define SUB62(v,off) ((unsigned short)(((hm_u64)(v)) >> ((off)*8)))
#define SUB63(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB64(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB65(v,off) ((hm_u64)(((hm_u64)(v)) >> ((off)*8)))
#define SUB71(v,off) ((unsigned char)(((hm_u64)(v)) >> ((off)*8)))
#define SUB72(v,off) ((unsigned short)(((hm_u64)(v)) >> ((off)*8)))
#define SUB73(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB74(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB75(v,off) ((hm_u64)(((hm_u64)(v)) >> ((off)*8)))
#define SUB76(v,off) ((hm_u64)(((hm_u64)(v)) >> ((off)*8)))
#define SUB81(v,off) ((unsigned char)(((hm_u64)(v)) >> ((off)*8)))
#define SUB82(v,off) ((unsigned short)(((hm_u64)(v)) >> ((off)*8)))
#define SUB83(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB84(v,off) ((unsigned int)(((hm_u64)(v)) >> ((off)*8)))
#define SUB85(v,off) ((hm_u64)(((hm_u64)(v)) >> ((off)*8)))
#define SUB86(v,off) ((hm_u64)(((hm_u64)(v)) >> ((off)*8)))
#define SUB87(v,off) ((hm_u64)(((hm_u64)(v)) >> ((off)*8)))
#define ZEXT12(v) ((unsigned short)(unsigned char)(v))
#define SEXT12(v) ((short)(signed char)(v))
#define ZEXT13(v) ((unsigned int)(unsigned char)(v))
#define SEXT13(v) ((int)(signed char)(v))
#define ZEXT14(v) ((unsigned int)(unsigned char)(v))
#define SEXT14(v) ((int)(signed char)(v))
#define ZEXT15(v) ((hm_u64)(unsigned char)(v))
#define SEXT15(v) ((hm_s64)(signed char)(v))
#define ZEXT16(v) ((hm_u64)(unsigned char)(v))
#define SEXT16(v) ((hm_s64)(signed char)(v))
#define ZEXT17(v) ((hm_u64)(unsigned char)(v))
#define SEXT17(v) ((hm_s64)(signed char)(v))
#define ZEXT18(v) ((hm_u64)(unsigned char)(v))
#define SEXT18(v) ((hm_s64)(signed char)(v))
#define ZEXT23(v) ((unsigned int)(unsigned short)(v))
#define SEXT23(v) ((int)(short)(v))
#define ZEXT24(v) ((unsigned int)(unsigned short)(v))
#define SEXT24(v) ((int)(short)(v))
#define ZEXT25(v) ((hm_u64)(unsigned short)(v))
#define SEXT25(v) ((hm_s64)(short)(v))
#define ZEXT26(v) ((hm_u64)(unsigned short)(v))
#define SEXT26(v) ((hm_s64)(short)(v))
#define ZEXT27(v) ((hm_u64)(unsigned short)(v))
#define SEXT27(v) ((hm_s64)(short)(v))
#define ZEXT28(v) ((hm_u64)(unsigned short)(v))
#define SEXT28(v) ((hm_s64)(short)(v))
#define ZEXT34(v) ((unsigned int)(unsigned int)(v))
#define SEXT34(v) ((int)(int)(v))
#define ZEXT35(v) ((hm_u64)(unsigned int)(v))
#define SEXT35(v) ((hm_s64)(int)(v))
#define ZEXT36(v) ((hm_u64)(unsigned int)(v))
#define SEXT36(v) ((hm_s64)(int)(v))
#define ZEXT37(v) ((hm_u64)(unsigned int)(v))
#define SEXT37(v) ((hm_s64)(int)(v))
#define ZEXT38(v) ((hm_u64)(unsigned int)(v))
#define SEXT38(v) ((hm_s64)(int)(v))
#define ZEXT45(v) ((hm_u64)(unsigned int)(v))
#define SEXT45(v) ((hm_s64)(int)(v))
#define ZEXT46(v) ((hm_u64)(unsigned int)(v))
#define SEXT46(v) ((hm_s64)(int)(v))
#define ZEXT47(v) ((hm_u64)(unsigned int)(v))
#define SEXT47(v) ((hm_s64)(int)(v))
#define ZEXT48(v) ((hm_u64)(unsigned int)(v))
#define SEXT48(v) ((hm_s64)(int)(v))
#define ZEXT56(v) ((hm_u64)(hm_u64)(v))
#define SEXT56(v) ((hm_s64)(hm_s64)(v))
#define ZEXT57(v) ((hm_u64)(hm_u64)(v))
#define SEXT57(v) ((hm_s64)(hm_s64)(v))
#define ZEXT58(v) ((hm_u64)(hm_u64)(v))
#define SEXT58(v) ((hm_s64)(hm_s64)(v))
#define ZEXT67(v) ((hm_u64)(hm_u64)(v))
#define SEXT67(v) ((hm_s64)(hm_s64)(v))
#define ZEXT68(v) ((hm_u64)(hm_u64)(v))
#define SEXT68(v) ((hm_s64)(hm_s64)(v))
#define ZEXT78(v) ((hm_u64)(hm_u64)(v))
#define SEXT78(v) ((hm_s64)(hm_s64)(v))
#define CARRY1(a,b) ((unsigned char)((unsigned char)(a) + (unsigned char)(b)) < (unsigned char)(a))
#define SCARRY1(a,b) ((((signed char)(a) ^ (signed char)((unsigned char)(a) + (unsigned char)(b))) & ((signed char)(b) ^ (signed char)((unsigned char)(a) + (unsigned char)(b)))) < 0)
#define SBORROW1(a,b) ((((signed char)(a) ^ (signed char)(b)) & ((signed char)(a) ^ (signed char)((unsigned char)(a) - (unsigned char)(b)))) < 0)
#define CARRY2(a,b) ((unsigned short)((unsigned short)(a) + (unsigned short)(b)) < (unsigned short)(a))
#define SCARRY2(a,b) ((((short)(a) ^ (short)((unsigned short)(a) + (unsigned short)(b))) & ((short)(b) ^ (short)((unsigned short)(a) + (unsigned short)(b)))) < 0)
#define SBORROW2(a,b) ((((short)(a) ^ (short)(b)) & ((short)(a) ^ (short)((unsigned short)(a) - (unsigned short)(b)))) < 0)
#define CARRY4(a,b) ((unsigned int)((unsigned int)(a) + (unsigned int)(b)) < (unsigned int)(a))
#define SCARRY4(a,b) ((((int)(a) ^ (int)((unsigned int)(a) + (unsigned int)(b))) & ((int)(b) ^ (int)((unsigned int)(a) + (unsigned int)(b)))) < 0)
#define SBORROW4(a,b) ((((int)(a) ^ (int)(b)) & ((int)(a) ^ (int)((unsigned int)(a) - (unsigned int)(b)))) < 0)
#define CARRY8(a,b) ((hm_u64)((hm_u64)(a) + (hm_u64)(b)) < (hm_u64)(a))
#define SCARRY8(a,b) ((((hm_s64)(a) ^ (hm_s64)((hm_u64)(a) + (hm_u64)(b))) & ((hm_s64)(b) ^ (hm_s64)((hm_u64)(a) + (hm_u64)(b)))) < 0)
#define SBORROW8(a,b) ((((hm_s64)(a) ^ (hm_s64)(b)) & ((hm_s64)(a) ^ (hm_s64)((hm_u64)(a) - (hm_u64)(b)))) < 0)
#define POPCOUNT(x) __builtin_popcountll((hm_u64)(x))
#define ROUND(x) ((long long)__builtin_llrintl((long double)(x)))
#define TRUNC(x) ((long long)(x))
#define NAN_CHECK(x) ((x) != (x))
#define true 1
#define false 0
/* 3-byte partial access helpers (little endian) */
#define HM_PART3(p) ((unsigned int)((const unsigned char *)(p))[0] | ((unsigned int)((const unsigned char *)(p))[1] << 8) | ((unsigned int)((const unsigned char *)(p))[2] << 16))
#define HM_SET3(p, v) do { unsigned int hm__v = (unsigned int)(v); unsigned char *hm__p = (unsigned char *)(p); \
    hm__p[0] = (unsigned char)hm__v; hm__p[1] = (unsigned char)(hm__v >> 8); hm__p[2] = (unsigned char)(hm__v >> 16); } while (0)
/* Ghidra prints inlined string stores as builtin_strncpy(dst, "literal", n) */
#define builtin_strncpy(d, s, n) __builtin_strncpy((char *)(d), (const char *)(s), (n))
#define builtin_memcpy(d, s, n) __builtin_memcpy((void *)(d), (const void *)(s), (n))
/* NAN(x): x87 unordered-compare flag, i.e. isnan */
#define NAN(x) __builtin_isnan(x)
