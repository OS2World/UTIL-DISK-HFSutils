/*****************************************************************************
 * FILE: loader.c							     *
 *									     *
 * DESC: loader for 32bit RSX compiled with EMX/GCC			     *
 *									     *
 * Copyright (C) 1994 - 1996                                                 *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 * See the files "README" and "COPYING" for further copyright and warranty   *
 * information. 							     *
 *									     *
 *									     *
 *  Program memory:  (32-bit data overrides this code)                       *
 *									     *
 * +--+------+-------------------------------------------------+             *
 * |  |16-bit|     env|                                        |  LOADER.EXE *
 * |P |loader|    copy|                                        |             *
 * |S +---------------+---------------+---------------+--------+             *
 * |P | heap    stack |  32-bit       |   32-bit      | 32-bit |             *
 * |  |  --->   <---  |     code      |      data     |   bss  |  RSX.EXE    *
 * +--+---------------+---------------+---------------+--------+             *
 *    0               64             128             192       222           *
 *									     *
 *****************************************************************************/

#include <dos.h>
#include "LOADER.H"

#define PSP_SIZE 256
extern unsigned _psp;

/*
   Globals (needed by jmp_to_uesr()
*/
WORD    code32sel;                      /* RSX CS selector */
DWORD   entry;                          /* RSX EIP pointer */
WORD    data32sel;                      /* RSX DS selector */
DWORD   stackp32;                       /* RSX ESP pointer */

static  unsigned long   membytes;
static  unsigned long   memaddress;
static  unsigned long   text_start;
static  unsigned long   data_start;
static  unsigned long   bss_start;
static  void far *      extdata;        /* DOS memory for RSX */
static  unsigned        segm;           /* segment address */
static  char far *      env_str;        /* env far pointer */
static  char            cline[260];     /* arguments */

static int n_strlen(char *str)
{
    char *s;
    for (s = str; *s; ++s);
    return (s - str);
}

static int f_strlen(char far *str)
{
    char far *s;
    for (s = str; *s; ++s);
    return (FP_OFF(s) - FP_OFF(str));
}

static void my_strcpy(char *dst, const char *src)
{
    for (; (*dst=*src); ++src, ++dst);
}

static void error(char *s)
{
    dos_puts("loader v1.0 : (c) Rainer Schnitker\n\r$");
    dos_puts("rsx_32 loader error: $");
    dos_puts(s);
}

static int skip_exe_hdr(int filehandle, DWORD * headoff)
{
    struct exe_hdr exehdr;

    exehdr.signatur = 0;
    dos_read(filehandle, &exehdr, sizeof(WORD) * 3);

    if (exehdr.signatur == 0x5a4d) {	/* falls exe-kopf */
	*headoff += (DWORD) exehdr.high * 512L;
	if (exehdr.low)
	    *headoff += (DWORD) exehdr.low - 512L;
    }
    if (dos_lseek(filehandle, *headoff, SEEK_SET) != -1L)
	return 0;
    else {
	*headoff = 0;
	dos_lseek(filehandle, 0, SEEK_SET);
	return -1;
    }
}

static int argvenv(int argc, char **argv)
{
    DWORD far * vectors;	    /* building argv, env    */
    WORD len;			    /* current len of string */
    WORD stkp;			    /* current stack pos     */
    WORD count = 3;		    /* 0=argc 1=argv 2=env   */
    int i, envc = 0;
    static char npx_str[] = "RSX_x87=?";

    /* instead allocate memory, take some EMX space at DS:0x8000 */
    vectors = (DWORD far *) (((DWORD)segm << 16) | 0x8000);

    /* EMX program stack pointer */
    /* stack grows down from DS:0xFFFC */
    stkp = (WORD) stackp32;
    FP_SEG(extdata) = segm;
    FP_OFF(extdata) = stkp;

    /* store env strings in user stack, built vectors */
    for (; *env_str; ) {
	len = f_strlen(env_str) + 1;
	stkp -= len;
	stkp &= ~3;
	FP_OFF(extdata) = stkp;
	far_memcpy(extdata, env_str, len);
	vectors[count++] = (DWORD) stkp;
	env_str += len;
	envc ++;
    }
    /* put 387 status on env */
    i = npx_installed();
    npx_str[8] = (char) (i + '0');
    len = sizeof(npx_str) + 1;
    stkp -= len;
    stkp &= ~3;
    FP_OFF(extdata) = stkp;
    far_memcpy(extdata, (char far *)npx_str, len);
    vectors[count++] = (DWORD) stkp;
    env_str += len;
    envc ++;

    vectors[count++] = 0L;	 /* last is a NULL pointer */

    /* store arg strings in user stack, built vectors */
    for (i = 0; i < argc; i++) {
	len = f_strlen(argv[i]) + 1;
	stkp -= len;
	stkp &= ~3;
	FP_OFF(extdata) = stkp;
	far_memcpy(extdata, (void far *) argv[i], len);
	vectors[count] = (DWORD) stkp;
	count++;
    }
    vectors[count++] = 0L;	 /* last is a NULL pointer */

    len = count * sizeof(long);
    stkp -= len;
    vectors[0] = argc;
    vectors[1] = stkp + (4 + envc) * sizeof(long);  /* & vectors[3+nenvp+1] */
    vectors[2] = stkp + 3 * sizeof(long);	    /* & vectors[3] */
    FP_OFF(extdata) = stkp;
    far_memcpy(extdata, vectors, len);

    stackp32 = stkp;		    /* save current pos! (for entry) */
    stackp32 += 3 * sizeof(long);
    return 0;
}


static int read_in(int fhandle, DWORD address, long size)
{
#define READ_MAX 0xFF00

    void far *dosmemp;
    WORD word_size;

    FP_OFF(dosmemp) = 0;

    while (size != 0L) {
	FP_SEG(dosmemp) = (WORD) (address >> 4);
	word_size = (size >> 16) ? READ_MAX : (WORD) size;
	dos_read_far(fhandle, dosmemp, word_size);
        address += (DWORD) word_size;
	size -= (DWORD) word_size;
    }

    return 0;
}

static int load_protected_program(char *filename)
{
    GNUOUT aout_hdr;
    DWORD sizetext, sizedata, sizestack;
    DWORD headoff;
    int fhandle;

    if ((fhandle = dos_open(filename, DO_RDONLY | DO_DENYWR)) == -1) {
	error("open rsx32 error\r\n$");
	return 1;
    }

    headoff = 0;
    skip_exe_hdr(fhandle, &headoff);

    /* read gnu aout header */
    dos_read(fhandle, &aout_hdr, sizeof(aout_hdr));

    /* test header */
    if ((WORD) aout_hdr.a_info != 0x10b) {
	error("illegal a.out header\r\n$");
	dos_close(fhandle);
	return 2;
    }

    sizetext = (aout_hdr.a_text + SEGMENT_SIZE - 1L) & ~(SEGMENT_SIZE - 1L);
    sizedata = aout_hdr.a_data + ((aout_hdr.a_bss + 4095L) & ~4095L);
    sizestack = 64 * 1024L;

    text_start = N_TXTADDR(x);
    data_start = text_start + sizetext;
    bss_start = data_start + aout_hdr.a_data;
    membytes = sizestack + sizetext + sizedata;

    entry = aout_hdr.a_entry;
    stackp32 = text_start - 4L;

    /* MEMORY per DOS besorgen */
    if (!DosReallocParagraph(_psp, (WORD)((membytes+256)>>4))) {
	error("realloc memory error\r\n$");
	return 3;
    }
    segm = _psp + (PSP_SIZE >> 4);

    memaddress = (DWORD)segm << 4;

    /* read in code */
    dos_lseek(fhandle, headoff + N_TXTOFF(aout_hdr), SEEK_SET);
    if (read_in(fhandle, memaddress + text_start, aout_hdr.a_text))
	return 1;

    /* read in data */
    dos_lseek(fhandle, headoff + N_DATOFF(aout_hdr), SEEK_SET);
    if (read_in(fhandle, memaddress + data_start, aout_hdr.a_data))
	return 1;

    dos_close(fhandle);

    /* zero bss segment */
    if (aout_hdr.a_bss) {
        FP_SEG(extdata) = (WORD) ((memaddress + bss_start) >> 4);
        FP_OFF(extdata) = (WORD) ((memaddress + bss_start) & 0xFL);
	far_bzero(extdata, (WORD) aout_hdr.a_bss);
    }
    return 0;
}

static int real_to_protected(WORD mode)
{
    static WORD DPMIdata_para_needed;
    static WORD DPMIdata_segm_address;
    WORD DPMIflags, DPMIversion;
    BYTE processor;
    DWORD PM_jump;		/* switch to protmode jump */

    if (GetDpmiEntryPoint(&PM_jump, &DPMIdata_para_needed,
			  &DPMIflags, &DPMIversion, &processor)) {
	error("No DPMI-host found!\r\n$");
	return -1;
    }
    if (mode == 1 && !(DPMIflags & 1)) {
	error("32bit programs not supported by Host\r\n$");
	return -1;
    }
    if (DPMIdata_para_needed) { /* get DPMI ring 0 stack */
	DPMIdata_segm_address = GetDpmiHostParagraph(DPMIdata_para_needed);
	if (!DPMIdata_segm_address) {
	    error("Can't alloc memory for the DPMI-host-stack\r\n$");
	    return -1;
	}
    }
    if (DpmiEnterProtectedMode(PM_jump, mode, DPMIdata_segm_address)) {
	error("can't switch to Protected Mode\r\n$");
	return -1;
    }

    /* Now we are in Protected Mode */

    if (DPMIdata_para_needed)
	LockLinRegion((DWORD) DPMIdata_segm_address << 4,
		      (DWORD) DPMIdata_para_needed << 4);

    return 0;
}

static void init_descriptors()
{
    AllocLDT(2, &(code32sel));
    data32sel = code32sel + SelInc();

    SetBaseAddress(code32sel, memaddress);
    SetBaseAddress(data32sel, memaddress);
    SetAccess(code32sel, APP_CODE_SEL, DEFAULT_BIT | GRANULAR_BIT);
    SetAccess(data32sel, APP_DATA_SEL, DEFAULT_BIT | GRANULAR_BIT);
    SetLimit(code32sel, membytes - 1);
    SetLimit(data32sel, membytes - 1);
}

static void get_cmdline_from_psp(unsigned psp_segm)
{
    int z, env_seg;
    char far *cmdl;

    /* get envoronment segment from PSP */
    env_seg = *(int far *) (((DWORD) psp_segm << 16) | 0x2c);

    /* build far pointer to environment */
    cmdl = (char far *) ((DWORD) env_seg << 16);

    /* save environment pointer */
    env_str = (char far *) ((DWORD) env_seg << 16);

    /* skip env-strings ; last has two 0-bytes */
    for (;;) {
	if (*cmdl == '\0' && *(cmdl+1) == '\0')
	    break;
	cmdl++;
    }

    cmdl += 4;
    /* copy arg0 */
    for (z = 0; z < sizeof(cline); z++) {
	cline[z] = *cmdl;
	if (cline[z] == '\0')
	    break;
	cmdl++;
    }
    cline[z++] = ' ';

    /* build cmdline far pointer from PSP */
    cmdl = (char far *) ((DWORD) psp_segm << 16 | 0x81);

    /* copy arg1,arg2,... */
    for ( ; z < sizeof(cline) ; z++) {
	cline[z] = *cmdl;
	if (cline[z] == 13)
	    break;
	cmdl++;
    }
    cline[z] = '\0';
}

static void build_args(int *argn, char ***argvp, int stub_opt)
{
    static char extra[] = "!RSX";
    static char *argvec[20];

    int argc, src, dst, bs, quote;
    char *q;

    argc = 0;
    dst = src = 0;

    if (stub_opt)
	argvec[argc++] = extra;

    while (cline[src] == ' ' || cline[src] == '\t' || cline[src] == '\n')
	++src;
    do {
	if (cline[src] == 0)
	    q = NULL;
	else {
	    q = cline + dst;
	    bs = 0;
	    quote = 0;
	    for (;;) {
		if (cline[src] == '"') {
		    while (bs >= 2) {
			cline[dst++] = '\\';
			bs -= 2;
		    }
		    if (bs & 1)
			cline[dst++] = '"';
		    else
			quote = !quote;
		    bs = 0;
		} else if (cline[src] == '\\')
		    ++bs;
		else {
		    while (bs != 0) {
			cline[dst++] = '\\';
			--bs;
		    }
		    if (cline[src] == 0 ||
			((cline[src] == ' ' || cline[src] == '\t') && !quote))
			break;
		    cline[dst++] = cline[src];
		}
		++src;
	    }
	    while (cline[src] == ' ' || cline[src] == '\t'
		   || cline[src] == '\n')
		++src;
	    cline[dst++] = 0;
	}
	argvec[argc++] = q;
    } while (q != NULL);

    *argn = argc - 1;
    *argvp = (char **) &(argvec[0]);
}

int main(void)
{
    int argc;
    char **argv;
    char exe_name[130];

    get_cmdline_from_psp(_psp);
    build_args(&argc, &argv, 0);
    my_strcpy(exe_name, argv[0]);

    if (argc == 2 && argv[1][0]=='-' && argv[1][1] == '/' && argv[1][6] == '/') {
	unsigned newpsp;
	int i;
	char s;
	newpsp = 0;
	for ( i=2 ; i<=5 ; i++) {
	    s = argv[1][i];
	    if (s >= 'A')
		s -= ('A' - 10);
	    else
		s -= '0';
	    newpsp <<= 4;
	    newpsp |= s;
	}
	get_cmdline_from_psp(newpsp);
	build_args(&argc, &argv, 1);
    }

    if (argc > 7 && argv[1][0]=='!' && argv[1][1] == 'p' && argv[1][5] == 'y') {
	unsigned newpsp;
	int i;
	char s;
	newpsp = 0;
	for ( i=0 ; i<=3 ; i++) {
	    s = argv[7][i];
	    if (s >= 'a')
		s -= ('a' - 10);
	    else if (s >= 'A')
		s -= ('A' - 10);
	    else
		s -= '0';
	    newpsp <<= 4;
	    newpsp |= s;
	}
	get_cmdline_from_psp(newpsp);
	build_args(&argc, &argv, 1);
    }

    if (load_protected_program(exe_name))
	return(1);

    if (argvenv(argc, argv))
	return (1);

    if (real_to_protected(1))
	return (1);

    init_descriptors();
    jmp_to_user();
    return 0;
}
