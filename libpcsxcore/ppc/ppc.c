/*
 * ix86 core v0.5.1
 *  Authors: linuzappz <linuzappz@pcsx.net>
 *           alexey silinov
 */

#include <stdio.h>
#include <string.h>

#include "ppc.h"

// General Purpose hardware registers
/** 
R12/R13 are reserved by compiler/os
R14 is reserverd for vm base address
**/
int cpuHWRegisters[NUM_HW_REGISTERS] = {
    6, 7, 8,						// 3
	9, 10, 11,						// 6
	/*12, 13, 14,*/					
	15, 16, 17,						// 9
	18, 19, 20,						// 12
    21, 22, 23,						// 15
	24, 25, 26,						// 18
	27, 28, 29,						// 21
	30, 31							// 23
};

u32 *ppcPtr;

void ppcInit() {
}
void ppcSetPtr(u32 *ptr) {
	ppcPtr = ptr;
}
void ppcAlign(int bytes) {
	// forward align
	ppcPtr = (u32*)(((u32)ppcPtr + bytes) & ~(bytes - 1));
}

void ppcShutdown() {
}

