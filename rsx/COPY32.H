/* Copyright	Rainer Schnitker 92,93 */

#ifndef _DPMI_TYPES
#define _DPMI_TYPES
#include "DPMITYPE.H"
#endif

/* CopyFunctions */
DWORD	read32 (WORD,DWORD);			/* read DWORD from 32 */
void	store32 (WORD,DWORD,DWORD);		/* store DWORD in 32 */
void	bzero32 (WORD,DWORD,DWORD);		/* bzero 16:32 */
void	cpy16_32 (WORD,DWORD,void *,DWORD);	/* mem 16:32 <- ds:16 */
void	cpy32_16 (WORD,DWORD,void *,DWORD);	/* mem 16:32 -> ds:16 */
void	strcpy16_32 (WORD,DWORD,void *);	/* str 16:32 <- ds:16 */
void	strcpy32_16 (WORD,DWORD,void *);	/* str 16:32 -> ds:16 */
void	cpy32_32 (WORD,DWORD,WORD,DWORD,DWORD); /* mem 16:32 -> ds:16 */
void	getstr32_16 (WORD,DWORD,void *,char);

void	put_user_byte(WORD,DWORD,BYTE); 	/* store BYTE in 32 */
