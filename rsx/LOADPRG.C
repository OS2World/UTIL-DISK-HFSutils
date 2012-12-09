/*****************************************************************************
 * FILE: loadprg.c							     *
 *									     *
 * DESC:								     *
 *	- loader for coff/a.out programs				     *
 *	- set args,environment						     *
 *	- load text,data (if not DPMI 1.0)				     *
 *									     *
 * Copyright (C) 1993-1996						     *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include <string.h>
#include "DPMI.H"
#include "DPMI10.H"
#include "PRINTF.H"
#include "RMLIB.H"
#include "PROCESS.H"
#include "FS.H"
#include "GNUAOUT.H"
#include "LOADPRG.H"
#include "COPY32.H"
#include "START32.H"
#include "CDOSX32.H"
#include "RSX.H"
#include "DOSERRNO.H"

static void relocate (NEWPROCESS *p);
static int reloc_image(SCNHDR *reloc_hdr, DWORD base, unsigned sel, int fhandle);

static int skip_exe_hdr(int filehandle, DWORD * headoff, NEWPROCESS *p)
{
    struct exe_hdr exehdr;
    struct emx_hdr2 emxhdr;

    rm_read(filehandle, &exehdr, sizeof(struct exe_hdr));

    if (exehdr.signatur == 0x5a4d) {	/* falls exe-kopf */
	*headoff = ((DWORD) exehdr.hdr_para) * 16;
	rm_lseek(filehandle, *headoff, SEEK_SET);
	rm_read(filehandle, &emxhdr, sizeof(emxhdr));

	if (memcmp(emxhdr.sig, "emx", 3) == 0) {
	    *headoff = *(DWORD *) emxhdr.next_hdr;
	    if (emxhdr.option[0]) {
		char *s = emxhdr.option;
		for (; *s != '\0'; ++s) {
		    while (*s == ' ')
			++s;
		    if (*s == '-') {
			s = scan_for_option(++s, &p->options);
		    } else
			break;
		}
	    }
	}

	else if (memcmp(emxhdr.sig, "rsx", 3) == 0)
	    goto fail;	    /* prevent loading rsx32.exe as a.out */

	else {	    /* not a emx file */
	    *headoff = (DWORD) exehdr.high * 512L;
	    if (exehdr.low)
		*headoff += (DWORD) exehdr.low - 512L;
	}
    }	/* exe files */

    if (rm_lseek(filehandle, *headoff, SEEK_SET) == -1L)
	goto fail;
    return 0;

  fail:
    *headoff = 0;
    rm_lseek(filehandle, 0, SEEK_SET);
    return -1;
}

/*
** arguments and environment at process start
**
**	environment and argument strings
**	argv[]
**	envp[]
**	pointer to env[0]
**	pointer to argv[0]
**	int argc
**	<- ESP
*/
int argvenv(int argc, char **argv, int envc, char **env, NEWPROCESS * proc)
{
    int i;
    DWORD len, stkp;
    DWORD *vectors;
    UINT count = 3;		/* 0=argc 1=argv 2=env */

    vectors = (DWORD *) iobuf;

    stkp = proc->stackp32;

    /* store env strings in user stack, built vectors */
    for (i = 0; i < envc; i++) {
	len = (DWORD) (UINT) strlen(env[i]) + 1;
	stkp -= len;
	stkp &= ~3L;		/* align4 */
	cpy16_32(proc->data32sel, stkp, env[i], len);
	vectors[count++] = stkp;
    }
    vectors[count++] = 0;	/* last is a NULL pointer */

    /* store arg strings in user stack, built vectors */

    for (i = 0; i < argc; i++) {
	len = (DWORD) (UINT) strlen(argv[i]) + 1;
	stkp -= len;
	stkp &= ~3L;
	cpy16_32(proc->data32sel, stkp, argv[i], len);
	vectors[count] = stkp;
	count++;
	/* for emx: add flag */
	stkp -= 1;
	put_user_byte(proc->data32sel, stkp, 0x80);
    }
    vectors[count++] = 0;	/* last is a NULL pointer */

    stkp &= ~3L;
    len = (DWORD) (count * sizeof(long));
    stkp -= len;
    vectors[0] = argc;
    vectors[1] = stkp + (4L + envc) * sizeof(long);	/* & vectors[3+nenvp+1] */
    vectors[2] = stkp + 3 * sizeof(long);	/* & vectors[3] */
    cpy16_32(proc->data32sel, stkp, vectors, len);

    if (proc->p_flags & PF_EMX_FILE)
	stkp += 3 * 4;

    proc->stackp32 = stkp;
    return 0;
}

void cpy_exename_to_stack(NEWPROCESS *proc, char *name)
{
    int len = strlen(name) + 1;
    long name_pos = proc->stackp32 - (long) (len + 2 * sizeof(DWORD));

    proc->stackp32 -= sizeof(DWORD);
    store32(proc->data32sel, proc->stackp32, name_pos);
    proc->stackp32 -= sizeof(DWORD);
    store32(proc->data32sel, proc->stackp32, (long) len);
    proc->stackp32 -= (long) len;
    cpy16_32(proc->data32sel, proc->stackp32, name, (long) len);
}

int readin_file(short handle, short r_ds, long r_edx, long count)
{
    long bytes;
    int rc;

    while (count > 0) {
	bytes = (count <= IOBUF_SIZE) ? count : (DWORD) IOBUF_SIZE;

	rc = rm_read(handle, iobuf, (short) bytes);
	cpy16_32(r_ds, r_edx, iobuf, rc);
	if (bytes != rc)
	    break;
	count -= bytes;
	r_edx += bytes;
    }
    return 0;
}

/*
**
** RSX program layout:
**
**
**  DPMI 0.9 :	 fixed stack
**		 never ending heap
**
**  emx style
**  |--------------------------------------------------------------
**  |stack|    code    |  data/bss  |  stack,if>64KB |	heap -> ...
**  |--------------------------------------------------------------
**  0	  ^64 KB       ^(n x 64KB)
**
**
**  old djgpp style
**  |--------------------------------------------------------------
**  |	|    code	 | stack    |  data/bss  | heap -> ...
**  |--------------------------------------------------------------
**  0	^4K			    ^0x400000
**
**  djgpp style 1.11
**  |--------------------------------------------------------------
**  |  code	|  data/bss  | stack  | heap -> ...
**  |--------------------------------------------------------------
**  0		^ 4 Kb align ^
**
**
**
**  DPMI 1.0 :	 address room = 64 MegaBytes
**
**  |--------------------------------------------...-----------------------|
**  |	  |  code   |  |  data/bss  |  heap ->	      <- stack	| mappings |
**  |	  |	    |  |	    |				| DOS 1MB  |
**  |--------------------------------------------...-----------------------|
**  0								^60	   64
**
*/

#define MIN_EMX_STACK (0x10000L-0x1000L)	/* 64 KB - one R/O page */
#define BIG_EMX_STACK (0x80000L)		/* 512 KB */
#define TEXT 0
#define DATA 1
#define BSS  2

int load_protected_program(char *filename, NEWPROCESS * proc)
{
    FILEHDR	file_hdr;
    AOUTHDR	aout_hdr;
    SCNHDR	scnd_hdr[3];
    SCNHDR	reloc_hdr;
    DWORD	tdbsegm, stacksegm;
    DWORD	headoff;
    int 	fhandle;
    int 	use_dpmi10;
    DWORD	image_base = 0;
    DWORD       default_stack, file_size, real_bss_size = 0;

    if ((fhandle = rm_open(filename, RM_O_RDONLY | RM_O_DENYWR)) == -1)
	return doserror_to_errno(_doserrno);

    /* compute stack */
    file_size = rm_lseek(fhandle, 0L, SEEK_END);
    rm_lseek(fhandle, 0L, SEEK_SET);
    if (file_size < 300000)
	default_stack = MIN_EMX_STACK;
    else
	default_stack = BIG_EMX_STACK;

    /* skip exe-header, return correct offset in file */
    headoff = 0;
    skip_exe_hdr(fhandle, &headoff, proc);

    /* read file header */
    rm_read(fhandle, &file_hdr, sizeof(file_hdr));

    if (file_hdr.f_magic == 0x10B) {  /* rsxnt dual */
	DWORD pe_sig;
	rm_lseek(fhandle, headoff + sizeof(GNUOUT), 0);
	rm_read(fhandle, &pe_sig, sizeof(DWORD));

	if (pe_sig == 0x00004550) {
            real_bss_size = file_hdr.f_nsyms;
	    rm_read(fhandle, &file_hdr, sizeof(file_hdr));
	    file_hdr.f_magic = 0x3232;
	    headoff += sizeof(GNUOUT) + sizeof(DWORD);
	}
    }

    if (file_hdr.f_magic == 0x14C) {	/* COFF header */
	rm_read(fhandle, &aout_hdr, sizeof(aout_hdr));
	rm_lseek(fhandle, headoff + sizeof(file_hdr) + file_hdr.f_opthdr, 0);
	rm_read(fhandle, scnd_hdr, sizeof(scnd_hdr));

	if ((scnd_hdr[TEXT].s_scnptr & 0xFF) == 0xA8) { /* DJGPP */
	    scnd_hdr[TEXT].s_vaddr &= ~0xFFL;
	    scnd_hdr[TEXT].s_scnptr &= ~0xFFL;	    /* align */
	    scnd_hdr[TEXT].s_size += 0xa8L;
	    proc->p_flags |= PF_DJGPP_FILE;
	}
	else { /* NT, TNT */
	    puts("can't run this win32 file");
	    rm_close(fhandle);
	    return EMX_ENOEXEC;
	}
    }
    else if (file_hdr.f_magic == 0x3232) { /* COFF, dual mode rsx/nt */
        struct nt_hdr nt_hdr;

	rm_read(fhandle, &aout_hdr, sizeof(aout_hdr));
        rm_read(fhandle, &nt_hdr, sizeof(struct nt_hdr));

        if (nt_hdr.Subsystem != 3) {
            puts("RSX: can only run Console Win32 programs");
            rm_close(fhandle);
            return EMX_ENOEXEC;
        }
        image_base = nt_hdr.ImageBase;

	rm_lseek(fhandle, headoff + sizeof(file_hdr) + file_hdr.f_opthdr, 0);
	rm_read(fhandle, scnd_hdr, sizeof(scnd_hdr));

        /* test emx0.9c linker + rsxnt 1.25 */
        if (strcmp(".bss", scnd_hdr[2].s_name) != 0 && real_bss_size != 0)
        {
            long bss_size = scnd_hdr[1].s_paddr - scnd_hdr[1].s_size;
            if (bss_size <= 0) {
                puts("rsx loader confused: s_paddr < s_size");
		rm_close(fhandle);
		return EMX_ENOEXEC;
            }
            scnd_hdr[2].s_vaddr = scnd_hdr[1].s_vaddr + scnd_hdr[1].s_size;
            scnd_hdr[2].s_size = real_bss_size;
	}

        if (image_base) {
	    int i;

	    for (i = 4; i <= file_hdr.f_nscns; i++) {
		rm_read(fhandle, &reloc_hdr, sizeof(SCNHDR));
		if (strcmp(reloc_hdr.s_name, ".reloc") == 0)
		    break;
	    }
	    if (strcmp(reloc_hdr.s_name, ".reloc")) {
		puts("can't run this win32 file");
		rm_close(fhandle);
		return EMX_ENOEXEC;
	    }
	}
	proc->p_flags |= PF_EMX_FILE;
	headoff = 0;	    /* !! stub is counted */
    }
    else if (file_hdr.f_magic == 0x10B) {
	GNUOUT gnu_hdr;
	rm_lseek(fhandle, headoff, 0);
	rm_read(fhandle, &gnu_hdr, sizeof(gnu_hdr));

	aout_hdr.magic = 0x10B;
	aout_hdr.tsize = gnu_hdr.a_text;
	aout_hdr.dsize = gnu_hdr.a_data;
	aout_hdr.bsize = gnu_hdr.a_bss;
	aout_hdr.entry = gnu_hdr.a_entry;

	scnd_hdr[TEXT].s_size = gnu_hdr.a_text;
	scnd_hdr[DATA].s_size = gnu_hdr.a_data;
	scnd_hdr[BSS].s_size = gnu_hdr.a_bss;

	if (gnu_hdr.a_entry == 0x10000L) {   /* EMX STYLE */
	    scnd_hdr[TEXT].s_vaddr = 0x10000L;
	    scnd_hdr[DATA].s_vaddr =
		0x10000L + ((gnu_hdr.a_text + 0xFFFFL) & ~0xFFFFL);
	    scnd_hdr[BSS].s_vaddr =
		scnd_hdr[DATA].s_vaddr + gnu_hdr.a_data;
	    scnd_hdr[TEXT].s_scnptr = 1024;
	    scnd_hdr[DATA].s_scnptr = 1024 + gnu_hdr.a_text;
	    proc->p_flags |= PF_EMX_FILE;
	}
	else { /* OLD DJGPP STYLE */
	    scnd_hdr[TEXT].s_vaddr = 0x400000L;
	    scnd_hdr[DATA].s_vaddr =
		0x400000L + ((gnu_hdr.a_text + 0x3FFFFFL) & ~0x3FFFFFL);
	    scnd_hdr[BSS].s_vaddr =
		scnd_hdr[DATA].s_vaddr + gnu_hdr.a_data;
	    scnd_hdr[TEXT].s_scnptr = 0;
	    scnd_hdr[DATA].s_scnptr = (gnu_hdr.a_text + 0xFFFL) & ~0xFFFL;
	    proc->p_flags |= PF_DJGPP_FILE;
	}
    }
    else {	/* not a valid header */
	rm_close(fhandle);
	return EMX_ENOEXEC;
    }

    proc->filehandle = (long) fhandle;
    proc->text_off = headoff + scnd_hdr[TEXT].s_scnptr;
    proc->data_off = headoff + scnd_hdr[DATA].s_scnptr;

    proc->text_start = scnd_hdr[TEXT].s_vaddr;
    proc->text_end = scnd_hdr[TEXT].s_vaddr + scnd_hdr[TEXT].s_size;
    proc->bss_start = scnd_hdr[BSS].s_vaddr;
    proc->bss_end = scnd_hdr[BSS].s_vaddr + scnd_hdr[BSS].s_size;
    proc->data_start = scnd_hdr[DATA].s_vaddr;
    proc->data_end = (proc->bss_end + 4095L) & ~4095L;
    proc->entry = aout_hdr.entry;

    /* compute size of text-data-bss and stack segment for AllocMem() */
    tdbsegm = proc->data_end;
    if (proc->options.opt_stackval)
	stacksegm = (DWORD) proc->options.opt_stackval *1024;
    else
	stacksegm = default_stack;

    if (proc == &RSX_PROCESS) { /* emu don't need much stack */
	stacksegm = MIN_EMX_STACK;
	use_dpmi10 = 0;
    } else
	use_dpmi10 = (dpmi10) ? 1 : 0;

    if (use_dpmi10)
	proc->p_flags |= PF_USEDPMI10;


    /* look for empty space for stack */
    if (aout_hdr.entry == 0x10000L && stacksegm <= MIN_EMX_STACK) {
	stacksegm = 0;
	/* place stack in the first 64KB segment */
	proc->stack_top = proc->text_start;
	proc->stack_down = 0x1000L;
    } else if (aout_hdr.entry == 0x1020L
	       && stacksegm <= (proc->data_start - proc->text_end)) {
	stacksegm = 0;
	/* place stack between code and data */
	proc->stack_top = proc->data_start;
	proc->stack_down = proc->text_end + 0x1000;
    } else {
	/* put stack after data/bss */
	proc->stack_top = proc->data_end + stacksegm;
	proc->stack_down = proc->data_end;
    }

    if (use_dpmi10) {
	stacksegm = 0;
	proc->stack_top = DPMI_PRG_DATA;
	proc->stack_down = proc->data_end;
    }
    proc->stacksize = proc->stack_top - proc->stack_down;
    proc->stackp32 = proc->stack_top - 4L;

    proc->init_brk = proc->data_end + stacksegm;
    proc->brk_value = proc->init_brk;
    proc->pagefree = 0;

    /* total memory for process */
    if (use_dpmi10) {
	proc->pagefree = DPMI_PRG_DATA - proc->membytes;
	proc->membytes = DPMI_PRG_ROOM;
    } else {
	proc->membytes = tdbsegm + stacksegm;

	/* prealloc more heap */
	if (proc->options.opt_memalloc) {
	    proc->pagefree =  (long) proc->options.opt_memalloc * 1024;
	    proc->membytes += (long) proc->options.opt_memalloc * 1024;
	} else if (proc->options.opt_memaccess) {
	    proc->pagefree =  0x21000;
	    proc->membytes += 0x21000;
	}
    }

    /* since dosmem isn't used by DPMI we put rsx387 in dos memory */
    if (proc == &RSX_PROCESS && rsx387_in_dosmem) {
	UINT segment, selectors;
	if (AllocDosMem((UINT) (proc->membytes >> 4), &segment, &selectors)) {
	    if (AllocMem(proc->membytes, &(proc->memhandle), &(proc->memaddress)))
		return EMX_ENOMEM;
	} else {
	    proc->memaddress = (DWORD) segment << 4;
	    proc->memhandle = (DWORD) selectors;
	}
    }
    /* get memory from DPMI-host */
    else if (!use_dpmi10) {
	if (AllocMem(proc->membytes, &(proc->memhandle), &(proc->memaddress)))
	    return EMX_ENOMEM;
    } else if (AllocLinearMemory(proc->membytes, 0L, 0L, &proc->memhandle, &proc->memaddress))
	return EMX_ENOMEM;

    /* alloc 32bit cs,ds ldt selector */
    if (AllocLDT(2, &(proc->code32sel)))
	return EMX_EIO;
    proc->data32sel = proc->code32sel + sel_incr;

    /* fill descriptors */
    SetBaseAddress(proc->code32sel, proc->memaddress);
    SetBaseAddress(proc->data32sel, proc->memaddress);
    SetAccess(proc->code32sel, APP_CODE_SEL, DEFAULT_BIT);
    SetAccess(proc->data32sel, APP_DATA_SEL, BIG_BIT);

    /* allow execute data & stack  (DJGPP / GDB need this) */
    SetLimit(proc->code32sel, proc->membytes - 1L);

    if (!use_dpmi10) {
	if (proc->options.opt_memaccess)
	    SetLimit(proc->data32sel, 0xFFFFFFFF);
	else
	    SetLimit(proc->data32sel, proc->membytes - 1L);
    } else
	SetLimit(proc->data32sel, DPMI_PRG_ROOM - 1L);

    /* map first DOS MB at the end of linear Addressroom (60MB) */
    if (use_dpmi10 && proc->options.opt_memaccess)
	MapDOSMemInMemoryBlock(proc->memhandle, DPMI_PRG_DATA, 256L, 0L);

    if (!use_dpmi10) {
	/* read in code */
	rm_lseek(fhandle, proc->text_off, SEEK_SET);
	readin_file(fhandle,
	    proc->data32sel, scnd_hdr[TEXT].s_vaddr,
	    scnd_hdr[TEXT].s_size);

	/* read in data,bss */
	rm_lseek(fhandle, proc->data_off, SEEK_SET);
	readin_file(fhandle,
	    proc->data32sel, scnd_hdr[DATA].s_vaddr,
	    scnd_hdr[DATA].s_size);

	if (image_base)
	    if (reloc_image(&reloc_hdr, image_base, proc->data32sel, fhandle) < 0)
		return EMX_ENOEXEC;

	rm_close(fhandle);

	/* zero bss segment */
	if (scnd_hdr[BSS].s_size)
	    bzero32(proc->data32sel, scnd_hdr[BSS].s_vaddr, scnd_hdr[BSS].s_size);
	if (proc->options.opt_os2)
	    relocate(proc);
    } else { /* uncommit first stack page! */
	WORD page = 9;
	ModifyPageAttributes(proc->memhandle, proc->stack_top - 4096, 1, &page);
    }
    return 0;
}

#include <malloc.h>
static int reloc_image(SCNHDR *reloc_hdr, DWORD base, unsigned sel, int fhandle)
{
    struct _IMAGE_BASE_RELOCATION {
	DWORD	VirtualAddress;
	DWORD	SizeOfBlock;
    } baseReloc;
    void *EntryMem;

    EntryMem = malloc(1024*2);
    if (!EntryMem)
	return -1;

    rm_lseek(fhandle, reloc_hdr->s_scnptr, 0);

    for (;;) {
	unsigned i,cEntries;
	WORD relocType;
	WORD *pEntry;

	rm_read(fhandle, &baseReloc, sizeof(baseReloc));

        if (!baseReloc.VirtualAddress || !baseReloc.SizeOfBlock) {
	    free(EntryMem);
	    return 0;
	}

	cEntries = (baseReloc.SizeOfBlock-sizeof(baseReloc))/sizeof(WORD);

	if (cEntries > 1024) {
	    puts("Image relocs to big");
	    free(EntryMem);
	    return -1;
	}

	rm_read(fhandle, EntryMem, cEntries * sizeof(WORD));
	pEntry = (WORD *) EntryMem;

	for ( i=0; i < cEntries; i++ )
	{
	    relocType = (*pEntry & 0xF000) >> 12;

	    if (relocType) {
		DWORD offset = (*pEntry & 0x0FFF) + baseReloc.VirtualAddress;
		DWORD value = read32(sel, offset);

		if (value >= base)
		    store32(sel, offset, value - base);
	    }

	    pEntry++;
	}
    }
}

/*  this structure that starts a core file */
struct user_area {
    WORD    u_magic	;
    WORD    u_reserved1 ;
    DWORD   u_data_base ;
    DWORD   u_data_end	;
    DWORD   u_data_off	;
    DWORD   u_heap_base ;
    DWORD   u_heap_end	;
    DWORD   u_heap_off	;
    DWORD   u_heap_brk	;
    DWORD   u_stack_base;
    DWORD   u_stack_end ;
    DWORD   u_stack_off ;
    DWORD   u_stack_low ;
    DWORD   u_ar0	;
    DWORD   u_fpvalid	;
    DWORD   u_fpstate[27] ;
    DWORD   u_fpstatus;
    DWORD   u_reserved3[23];
    REG386  u_regs;
};
#define U_OFFSET 0x400

int write_section(short handle, short r_ds, long r_edx, long count)
{
    long bytes;
    int rc;

    while (count > 0) {
	bytes = (count <= IOBUF_SIZE) ? count : (DWORD) IOBUF_SIZE;
	cpy32_16(r_ds, r_edx, iobuf, bytes);
	rc = rm_write(handle, iobuf, (short) bytes);
	if (bytes != rc)
	    return -1;
	count -= bytes;
	r_edx += bytes;
    }
    return 0;
}

int write_core(unsigned handle, NEWPROCESS *proc)
{
    struct user_area ua;
    DWORD data_size;
    DWORD heap_size;

    memset(& ua, 0, sizeof(ua));
    ua.u_magic = 0x10F;
    ua.u_ar0 = 0xE0000000L + (DWORD)(UINT)&(((struct user_area *)0)->u_regs);
    memcpy(& ua.u_regs, &(proc->regs), sizeof(REG386));

    ua.u_data_base = proc->data_start;
    ua.u_data_end = proc->bss_end;		    /* bss not rounded */
    data_size = proc->data_end - proc->data_start;  /* data is page-aligned */
    ua.u_data_off = U_OFFSET;

    ua.u_heap_base = proc->init_brk;
    ua.u_heap_end = (proc->brk_value + 0xFFFL) & ~0xFFFL;
    ua.u_heap_brk = proc->brk_value;
    heap_size = ua.u_heap_end - ua.u_heap_base;
    ua.u_heap_off = ua.u_data_off + data_size;

    ua.u_stack_base = proc->stack_down;
    ua.u_stack_end = proc->stack_top;
    ua.u_stack_low = proc->regs.esp;
    ua.u_stack_off = ua.u_heap_off + heap_size;

    rm_write(handle, &ua, sizeof(ua));	/* header */
    memset(iobuf, 0, U_OFFSET);
    rm_write(handle, iobuf, U_OFFSET - sizeof(ua));

    rm_lseek(handle, ua.u_data_off, 0);
    if (write_section(handle, proc->data32sel, ua.u_data_base, data_size))
	goto bad_write;

    rm_lseek(handle, ua.u_heap_off, 0);
    if (write_section(handle, proc->data32sel, ua.u_heap_base, heap_size))
	goto bad_write;

    rm_lseek(handle, ua.u_stack_off, 0);
    if (write_section(handle, proc->data32sel, ua.u_stack_low,
	    ua.u_stack_end - ua.u_stack_low))
	goto bad_write;

    rm_close(handle);
    return 0;

  bad_write:
    return -1;
}

int write_core_file(NEWPROCESS *proc)
{
    int handle, ret;
    static char core_name[]="CORE";

    handle = rm_creat(core_name, _A_NORMAL);
    if (handle == -1)
	return -1;

    puts("writing core file");
    ret = write_core(handle, proc);

    rm_close(handle);

    /* if (ret == -1) unlink(core_name); */

    return ret;
}

/* beim Debugen von EMACS 19.27 unter der OS/2 Option wird eine
 * Funktion an der Addresse 0x00000000 aufgerufen. Deshalb kopiere
 * ich hier zuerst irgendeine Funktion dahin.
 * Diese Funktion ersetzt dann ALLE Aufrufe von Funktionen in
 * OS/2-DLL's.
 */

void relocate (NEWPROCESS *p)
{
#ifdef OLD_RELOCATE
    /*
     *	Code:
     *	    mov eax, 0
     *	    ret
     */
    static char code[] = { 0xb8, 0, 0, 0, 0, 0xc3};
#else
    /*
     *	Code:
     *	    int 3    (to emulate some emacs calls)
     *	    ret
     */
    static char code[] = { 0xCC, 0xc3};

#endif

    if (dpmi10) {
	WORD page = 1 + 8;	    /* commit & read/write */
	ModifyPageAttributes(p->memhandle, 0L, 1, &page);
    }
    cpy16_32 (p->data32sel, 0, code, sizeof(code));
}
