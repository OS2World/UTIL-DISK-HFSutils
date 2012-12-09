/* Copyright	Rainer Schnitker 92,93 */

#ifndef _DPMI_TYPES
#define _DPMI_TYPES
#include "DPMITYPE.H"
#endif

#define N_MAGIC(exec) ((exec).a_info & 0xffff)
#define OMAGIC 0x107
#define NMAGIC 0x108
#define ZMAGIC 0x10B

/* load different sizes */
#define _N_HDROFF(x)	_n_hdroff
#define N_TXTADDR(x)	n_txtaddr
#define SEGMENT_SIZE	segment_size

#define N_DATADDR(x) (SEGMENT_SIZE + ((N_TXTADDR(x)+(x).a_text-1) & ~(SEGMENT_SIZE-1)))
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data)

#define N_TXTOFF(x) (_N_HDROFF((x)) + sizeof (struct exec))
#define N_DATOFF(x) (N_TXTOFF(x) + (x).a_text)
#define N_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#define N_SYMOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)

typedef struct exec {
    DWORD a_info;     /* Use macros N_MAGIC, etc for access */
    DWORD a_text;     /* length of text, in bytes */
    DWORD a_data;     /* length of data, in bytes */
    DWORD a_bss;      /* length of uninitialized data area for file, in bytes */
    DWORD a_syms;     /* length of symbol table data in file, in bytes */
    DWORD a_entry;    /* start address */
    DWORD a_trsize;   /* length of relocation info for text, in bytes */
    DWORD a_drsize;   /* length of relocation info for data, in bytes */
    } GNUOUT ;

typedef struct {
    WORD  magic;
    WORD  vstamp;
    DWORD tsize;	/* text size in bytes, padded to FW bdry*/
    DWORD dsize;	/* initialized data "  "                */
    DWORD bsize;	/* uninitialized data "   "             */
    DWORD entry;	/* entry pt.				*/
    DWORD text_start;	/* base of text used for this file	*/
    DWORD data_start;	/* base of data used for this file	*/
} AOUTHDR;

typedef struct filehdr {
	WORD	f_magic;
	WORD	f_nscns;
	DWORD	f_timdat;
	DWORD	f_symptr;
	DWORD	f_nsyms;
	WORD	f_opthdr;
	WORD	f_flags;
} FILEHDR;

typedef struct scnhdr {
	char	s_name[8];
	DWORD	s_paddr;
	DWORD	s_vaddr;
	DWORD	s_size;
	DWORD	s_scnptr;
	DWORD	s_relptr;
	DWORD	s_lnnoptr;
	WORD	s_nreloc;
	WORD	s_nlnno;
	DWORD	s_flags;
} SCNHDR;

struct exe_hdr {
    WORD signatur;
    WORD low;
    WORD high;
    WORD reloc;
    WORD hdr_para;
    } ;

struct emx_hdr2 {
    char sig[18];
    char next_hdr[4];
    char option[64];
};

struct nt_hdr {
    DWORD   ImageBase;
    DWORD   SectionAlignment;
    DWORD   FileAlignment;
    WORD    MajorOperatingSystemVersion;
    WORD    MinorOperatingSystemVersion;
    WORD    MajorImageVersion;
    WORD    MinorImageVersion;
    WORD    MajorSubsystemVersion;
    WORD    MinorSubsystemVersion;
    DWORD   Reserved1;
    DWORD   SizeOfImage;
    DWORD   SizeOfHeaders;
    DWORD   CheckSum;
    WORD    Subsystem;
    WORD    DllCharacteristics;
};
