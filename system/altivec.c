
#include <proto/exec.h>
#include <proto/utility.h>
#include <exec/resident.h>
#include <exec/system.h>

#include "altivec.h"

void setupAVExceptions(void)
{
	ULONG a[ 4 ] __attribute((aligned( 16 )));
	vector unsigned short b;
	b = vec_mfvscr();
	vec_st(b, 0, (unsigned short *) a);
	a[ 3 ] |= 1 << 16;
	b = vec_ld(0, (unsigned short *)a);
	vec_mtvscr(b);
}

///	Altivec cache prewarming stuff

/*
 * blockcount: number of blocks (from 0 to 255, 0 = 256 blocks)
 * blocksize: number of 16-byte chunks (from 0 to 31, 0 = 32 * 16-byte chunks)
 * blockstride: 16-bit value from -32768 to 32767 (0 = 32768), denotes the address
 *   increment from the beginning of one block to the beginning of the next.
 */

void cpu_vec_dst(CONST_APTR ptr, ULONG opts, ULONG channel)
{
	switch (channel)
	{
		case 0:
			vec_dst((VECTOR_UBYTE *)ptr, opts, 0);
			break;

		case 1:
			vec_dst((VECTOR_UBYTE *)ptr, opts, 1);
			break;

		case 2:
			vec_dst((VECTOR_UBYTE *)ptr, opts, 2);
			break;

		case 3:
			vec_dst((VECTOR_UBYTE *)ptr, opts, 3);
			break;
	}
}

void cpu_vec_dstt(CONST_APTR ptr, ULONG opts, ULONG channel)
{
	switch (channel)
	{
		case 0:
			vec_dstt((VECTOR_UBYTE *)ptr, opts, 0);
			break;

		case 1:
			vec_dstt((VECTOR_UBYTE *)ptr, opts, 1);
			break;

		case 2:
			vec_dstt((VECTOR_UBYTE *)ptr, opts, 2);
			break;

		case 3:
			vec_dstt((VECTOR_UBYTE *)ptr, opts, 3);
			break;
	}
}


void cpu_vec_dstst(CONST_APTR ptr, ULONG opts, ULONG channel)
{
	switch (channel)
	{
		case 0:
			vec_dstst((VECTOR_UBYTE *)ptr, opts, 0);
			break;

		case 1:
			vec_dstst((VECTOR_UBYTE *)ptr, opts, 1);
			break;

		case 2:
			vec_dstst((VECTOR_UBYTE *)ptr, opts, 2);
			break;

		case 3:
			vec_dstst((VECTOR_UBYTE *)ptr, opts, 3);
			break;
	}
}


void cpu_vec_dststt(CONST_APTR ptr, ULONG opts, ULONG channel)
{
	switch (channel)
	{
		case 0:
			vec_dststt((VECTOR_UBYTE *)ptr, opts, 0);
			break;

		case 1:
			vec_dststt((VECTOR_UBYTE *)ptr, opts, 1);
			break;

		case 2:
			vec_dststt((VECTOR_UBYTE *)ptr, opts, 2);
			break;

		case 3:
			vec_dststt((VECTOR_UBYTE *)ptr, opts, 3);
			break;
	}
}


void cpu_vec_dss(ULONG channel)
{
	switch (channel)
	{
		case 0:
			vec_dss(0);
			break;

		case 1:
			vec_dss(1);
			break;

		case 2:
			vec_dss(2);
			break;

		case 3:
			vec_dss(3);
			break;
	}
}
