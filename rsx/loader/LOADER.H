typedef unsigned char	BYTE;
typedef unsigned short	WORD;
typedef unsigned long	DWORD;

/* descriptor fields */
#define ACCESS_BIT		1
#define READ_BIT		2
#define WRITE_BIT		2
#define CONFIRMING_BIT		4
#define EXPAND_BIT		4
#define CODE_BIT		8
#define SEGMENT_BIT		16
#define PRESENT_BIT		128
#define AVL_BIT 		16
#define DEFAULT_BIT		64
#define BIG_BIT 		64
#define GRANULAR_BIT		128
#define DPL_MASK		96
#define TYPE_MASK		15
#define LIMIT_HI_MASK		15

#define APP_CODE_SEL		250   /* dpl=3 , read */
#define APP_DATA_SEL		242   /* dpl=3 ,write */

#define TYPE_CODE		10
#define TYPE_CONFIRM_CODE	14
#define TYPE_DATA		2
#define TYPE_EXPAND_DATA	6
#define TYPE_286TSS		1
#define TYPE_LDT		2
#define TYPE_BUSY286TSS 	3
#define TYPE_286CALLGATE	4
#define TYPE_TASKGATE		5
#define TYPE_286INTGATE 	6
#define TYPE_286TRAPGATE	7
#define TYPE_386TSS		9
#define TYPE_BUSY386TSS 	11
#define TYPE_386CALLGATE	12
#define TYPE_386INTGATE 	14
#define TYPE_386TRAPGATE	15

/* *************************************************************** */

int	AllocLDT(WORD anzahl,WORD *sel);
int	FreeLDT(WORD);
WORD	SelInc(void);
int	SetBaseAddress(WORD sel,DWORD address);
int	SetLimit(WORD sel,DWORD limit);
int	SetAccess(WORD sel,BYTE access,BYTE extaccess);
WORD	GetCS(void);
WORD	GetDS(void);
int	GetDpmiEntryPoint(DWORD *entry,WORD *para,WORD *flags,WORD *v,BYTE *cpu);
int	GetDpmiHostParagraph(WORD paragr);
int	DpmiEnterProtectedMode(DWORD entry,WORD mode,WORD hostsegm);
int	DosReallocParagraph(int segm, int paragr);
#define DosGetParagraph(x) GetDpmiHostParagraph(x)
int	LockLinRegion(DWORD size,DWORD address);

/* *************************************************************** */

void	dos_puts(char *name);
int	dos_open(char *name, int mode);
int	dos_close(int handle);
int	dos_read_far(int handle, void far *buffer, int bytes);
int	dos_read(int handle, void * buffer, int bytes);
long	dos_lseek(int handle, long pos, int orgin);
void	far_memcpy(void far *, void far *, int);
void	far_bzero(void far *, int);
void	jmp_to_user(void);
int	npx_installed(void);

/* *************************************************************** */

#define N_MAGIC(exec) ((exec).a_info & 0xffff)
#define OMAGIC 0x107
#define NMAGIC 0x108
#define ZMAGIC 0x10B

#define SEGMENT_SIZE 0x10000L
#define N_TXTADDR(x) SEGMENT_SIZE
#define N_DATADDR(x) (SEGMENT_SIZE + ((N_TXTADDR(x)+(x).a_text-1) & ~(SEGMENT_SIZE-1)))
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data)

#define _N_HDROFF(x)	(1024 - sizeof (struct exec))
#define N_TXTOFF(x)	(_N_HDROFF((x)) + sizeof (struct exec))
#define N_DATOFF(x)	(N_TXTOFF(x) + (x).a_text)
#define N_TRELOFF(x)	(N_DATOFF(x) + (x).a_data)
#define N_DRELOFF(x)	(N_TRELOFF(x) + (x).a_trsize)
#define N_SYMOFF(x)	(N_DRELOFF(x) + (x).a_drsize)
#define N_STROFF(x)	(N_SYMOFF(x) + (x).a_syms)

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

struct exe_hdr {
    WORD signatur;
    WORD low;
    WORD high;
    WORD reloc;
    WORD hdr_para;
    };

#ifndef NULL
#define NULL (void *)0
#endif

/* DOSOPEN : AH = 0x3d , AL = mode , DS:DX name */
/* bits 0-2 */
#define DO_RDONLY	0x00
#define DO_WRONLY	0x01
#define DO_RDWR 	0x02
/* bits 4-6 */
#define DO_COMPAT	0x00
#define DO_DENYRW	0x10
#define DO_DENYWR	0x20
#define DO_DENYRD	0x30
#define DO_DENYNO	0x40
/* bit 7 */
#define DO_INHERIT	0x80

#define SEEK_SET 0
