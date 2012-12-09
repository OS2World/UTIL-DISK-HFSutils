/* Copyright	Rainer Schnitker 92,93 */

#ifndef _RSX_START32_H
#define _RSX_START32_H

/* extender segments,selectors */
extern UINT cs16real,ds16real;		   /* 16-bit segments for extender */
extern UINT code16sel,data16sel;	   /* 16-bit cs,ds for extender */
extern UINT stack16sel; 		   /* 16-bit stack sel */
extern DWORD stackp16;			   /* 16-bit stack offset */
extern UINT sel_incr;			   /* increment to next sel */
extern UINT dosmem_sel; 		   /* dos memory selector */
extern DWORD dosmem_address;		   /* dos memory address */

extern char dpmi10;
extern char dpmi10_map;
extern char dpmi10_page;
extern char dpmi10_zero;

extern POINTER16_16 int21rmv;		/* dos int21h realmode handler */

void	init_real_mode ( void );
void	protected_to_real ( WORD );
int	real_to_protected ( WORD );
int	test_dpmi_capabilities( void );
int	hangin_extender ( void );
int	init_fpu( void );
void	clean_up ( void );

#endif /* _RSX_START32_H */
