/*
 * ppc definitions v0.5.1
 *  Authors: linuzappz <linuzappz@pcsx.net>
 *           alexey silinov
 */

#ifndef __PPC_H__
#define __PPC_H__

// include basic types
#include "../PsxCommon.h"
#include "ppc_mnemonics.h"

#define NUM_HW_REGISTERS	28

/* general defines */
#define write8(val)  *(u8 *)ppcPtr = val; ppcPtr++;
#define write16(val) *(u16*)ppcPtr = val; ppcPtr+=2;
#define write32(val) *(u32*)ppcPtr = val; ppcPtr+=4;
#define write64(val) *(u64*)ppcPtr = val; ppcPtr+=8;

#if 0

#define CALLFunc(FUNC) \
{ \
    u32 _func = (FUNC); \
    if ((_func & 0x1fffffc) == _func) { \
        BLA(_func); \
    } else { \
        LIW(0, _func); \
        MTCTR(0); \
        BCTRL(); \
    } \
}

#else

#define CALLFunc(FUNC) \
{ \
    u32 * _func = (u32*)(FUNC); \
    u32 * _cur = ppcPtr; \
	s32 _off = _func-_cur-1; \
	if (abs(_off)<0x7fffff) { \
		BL(_off); \
	} else { \
        LIW(0, (u32)_func); \
        MTCTR(0); \
        BCTRL(); \
		printf("!!!!!!!! bctrl\n"); \
	} \
}

#endif

extern int cpuHWRegisters[NUM_HW_REGISTERS];

extern u32 *ppcPtr;
extern u8  *j8Ptr[32];
extern u32 *j32Ptr[32];

void ppcInit();
void ppcSetPtr(u32 *ptr);
void ppcShutdown();

void ppcAlign(int bytes);
void returnPC();
void recRun(void (*func)(), u32 hw1, u32 hw2);
u8 dynMemRead8(u32 mem);
u16 dynMemRead16(u32 mem);
u32 dynMemRead32(u32 mem);
void dynMemWrite32(u32 mem, u32 val);

#endif /* __PPC_H__ */





















