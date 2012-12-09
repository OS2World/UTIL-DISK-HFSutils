/*****************************************************************************
 * FILE: rsx.c								     *
 *									     *
 * DESC:								     *
 *	- get rsx options						     *
 *	- switch protected mode (16bit) 				     *
 *	- init protected mode						     *
 *	- check copro, install emu					     *
 *	- load first a.out prg						     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *                                                                           *
 * Thanks to: (bug-fixes, patches, testing, ..)                              *
 *      Friedbert Widmann <widi@informatik.uni-koblenz.de>                   *
 *                                                                           *
 *****************************************************************************/

/* Modified for hfsutils by Marcus Better, July 1997 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "DPMI.H"
#include "PRINTF.H"
#include "PROCESS.H"
#include "START32.H"
#include "LOADPRG.H"
#include "RSX.H"
#include "DOSERRNO.H"
#include "RMLIB.H"
#include "COPY32.H"
#include "SYSDEP.H"
#include "DJIO.H"
#include "VERSION.H"

void    _defext (char *dst, const char *ext);
int     _path (char *dst, const char *name);
void    _searchenv (const char *file, const char *var, char *path);

/* command-line options */
char copro = 1;                 /* prg need 387 : 0=no 1=yes 3=emulate */
char lfn = 0;                   /* LFN support (long file names) */
char opt_stderr = 0;

char opt_force_copro = 0;	/* force copro status */
char rsx387_in_dosmem = 1;	/* rsx387 in dos memory (not rsx32) */
char opt_version = 0;		/* print rsx version */
int  opt_max_dos_handles;	/* DOS handles for RSX */

/* special test options */
char opt_kdeb;			/* Kernel debug mode */
char opt_redir; 		/* redirect handle 1 (testing RSX) */
char opt_debugrsx;		/* debug rsx; don't set int3 */

struct options main_options = {
			0,	/* allows memaccess */
			1,	/* force only DPMI 0.9 calls */
			0,	/* don't write core file */
			0,	/* enable schedule() */
			0,	/* zero heap */
			0,	/* set os/2 bit */
			0,	/* convert '.name' -> '!name' */
			0,	/* convert filename */
			0,	/* stack size in KB */
			0,	/* pre-alloc heap */
			0,	/* show all information (testing RSX) */
			0,	/* show every sys_call (testing RSX) */
			'\0'    /* default disk-drive */
};

/* other globals */
char npx_present;		/* npx present 1/0 */
char kdeb_program[80];		/* program for kernel debug mode */

int rsx_stdout = 1;
int kread;			/* keyboard read */
int kready;			/* keyboard check */
unsigned bios_selector; 	/* selector bios area */

char **org_env; 		/* org. environment to rsx */
int org_envc;			/* org. env items */

static int emxl_psp = 0;	/* exe-stub emxl.exe psp */

int hexstr2int(char *s)
{
    int i, res=0;

    for (i = 0; i < 4; i++) {
	char c = s[i];
	if (c >= 'a')
	    c -= ('a' - 10);
	else if (c >= 'A')
	    c -= ('A' - 10);
	else
	    c -= '0';
	res <<= 4;
	res |= c;
    }
    return res;
}

/* return first non-digit */
static char * asc2int(char *s, int *retv)
{
    char *str = s;
    *retv = 0;

    while (*str != 0) {
	if ((*str < '0') || (*str > '9'))
	    break;
	*retv *= 10;
	*retv += *str - '0';
	str++;
    }
    return (str);
}

/*
** get one options for rsx
** return last char pos, if successful
*/
char *scan_for_option(char *s, struct options *p)
{
    int temp;
    char *t;

    switch (*s) {

    /*
    **	emx options with emxbind:
    **	-a*, -c, -f#, -h#, -p, -r*, -s#, -t, -C#, -L
    */

    case 'a':                   /* DOS features */
	for (++s; *s > ' '; ++s) {
	    if (*s == 'm')
		p->opt_memaccess = 1;
	    else if (*s == 'c' || *s == 'i' || *s == 'w')
		continue;
	    else
		return NULL;
	}
	if (*(--s) == 'a')
	    return NULL;
	break;

    case 'c':                   /* core */
	p->opt_nocore = 1;
	break;

    case 'e':   /* changed !! */
        /* puts("\nError: -e option for copro has changed to -Re"); */
        /* return NULL; */
        opt_stderr = 1;
        break;

    case 'f':                   /* frame size (ignore) */
    case 's':                   /* stack size (ignore) */
    case 'C':                   /* commit */
	while (isdigit(*++s))
	    ;
	--s;
	break;

    case 'h':                   /* max handles */
	++s;
	if (!isdigit(*s))
	    return NULL;
	t = asc2int(s, &temp);
	if (p && temp > opt_max_dos_handles)
	    rm_sethandles(temp);
	opt_max_dos_handles = temp;
	return t;

    case 'p':                   /* don't use lower DOS mem */
	rsx387_in_dosmem = 0;
	break;

    case 'r':                   /* default disk-drive */
	if (!isalpha(*++s))
	    return NULL;
	p->opt_default_disk = tolower(*s);
	break;

    case 't':                   /* strip filenames 8.3 */
	p->opt_convert_filename = 803;
	break;

    case 'R':       /* special RSX options */
	for (++s; *s > ' '; ++s) {
	    if (*s == ',')
		++s;
	    switch (*s) {
                case 'a':                   /* ss = ds (removed) */
		    break;
		case 'e':                   /* copro */
		    copro = 0;
		    if (isdigit(*(s+1)))
			opt_force_copro = *(++s) - '0' + 1;
		    break;
		case 'm':                   /* malloc for memaccess */
		    if (!isdigit(*(s+1)))
			break;
		    s = asc2int(++s, &p->opt_memalloc) - 1;
		    break;
		case 'p':                   /* convert '.name' -> '!name' */
		    p->opt_convert_dot = 1;
		    break;
		case 's':                   /* stack frame */
		    ++s;
		    if (!isdigit(*s))
			return NULL;
		    s = asc2int(s, &p->opt_stackval) - 1;
		    break;
		case 't':                   /* convert filename */
		    ++s;
		    if (!isdigit(*s))
			return NULL;
		    s = asc2int(s, &temp) - 1;
		    if (temp == 533 || temp == 803)
			p->opt_convert_filename = temp;
		    else
			return NULL;
		    break;
		case 'x':                   /* enable schedule */
		    p->opt_schedule = 1;
		    break;
		case 'z':                   /* zero heap */
		    p->opt_zero = 1;
		    break;

		case '1':                   /* no dpmi10 calls */
		    p->opt_force_dpmi09 = 0;
		    break;
		case '9':                   /* no dpmi10 calls */
		    p->opt_force_dpmi09 = 1;
		    break;

		case 'D':                   /* don't touch int3 */
		    opt_debugrsx = 1;
		    break;
		case 'F':                   /* redirect output */
		    opt_redir = 1;
		    break;
		case 'I':                   /* print syscalls */
		    p->opt_print_syscalls = 1;
		    break;
		case 'K':                   /* kernel debug */
		    opt_kdeb = 1;
		    break;
		case 'O':                   /* set OS/2 bit */
		    p->opt_os2 = 1;
		    break;
		case 'P':                   /* print syscalls */
		    p->opt_printall = 1;
		    break;
		default:
                    puts("\nError: unknown -Rxx option");
		    return NULL;
	    }
	}
	if (*(--s) == 'R')
	    return NULL;
	break;

    case 'V':                   /* version print */
	opt_version = 1;
	break;

    default:
	return NULL;

    } /* switch (*s) */

    return s;
}

static void init_bios_keyboard(void)
{
    /* get DPMI selector for BIOS area, on error try 0x40 */
    if (SegToSel(0x40, &bios_selector))
	bios_selector = 0x40;

    /* services for enhanced keyboards, otherwise older types */
    if ((BYTE) read32(bios_selector, 0x96) & 0x10) {
	kread = 0x10;
	kready = 0x11;
    } else {
	kread = 0;
	kready = 1;
    }

    /* flush input */
    while (rm_bios_read_keybrd(kready))
	rm_bios_read_keybrd(kread);
}

static int setup_environment(char **env)
{
    char *s;

    /* save environment, env-size */
    for (org_envc = 0; env[org_envc] != NULL; org_envc++)
	;
    org_env = env;

    /* get enviroment options */
    s = getenv("RSXOPT");
    if (s != NULL) {
	for (; *s != '\0'; ++s) {
	    while (*s == ' ')
		++s;
	    if (*s == '-') {
		s = scan_for_option(++s, &main_options);
		if (s == NULL) {
                    print_version();
                    puts("\nError in RSXOPT");
		    return (1);
		}
	    } else
		break;
	}
    }
    return 0;
}

static int get_rsx_options(int start, char **argv)
{
    int i;
    char *s;

    for (i = start; argv[i]; i++) {
	if (argv[i][0] == '-') {
	    s = scan_for_option(& argv[i][1], &main_options);
	    if (s == NULL) {
                print_version();
                printf("\nbad option: %s\n", argv[i]);
		return -1;
	    }
	} else
	    break;
    }
    return i;
}

#ifdef __EMX__
#define exit(x) dos_exit(x)
#endif

/*
** MAIN():
** real mode for rsx16
** protected mode rsx32
*/

void main(int argc, char **argv, char **env)
{
    static char exefile[128];
    static char cpyfile[128];
    int file_arg, err;

#ifndef __EMX__
    set_stdout();
#endif
    init_real_mode();

    if (setup_environment(environ))
	exit(1);

    /* check bound exe-file */
    if (argc == 2 && argv[1][0] == '-' && argv[1][1] == '/'
	&& argv[1][6] == '/') {
	emxl_psp = hexstr2int(&argv[1][2]);
	get_emxl_psp(emxl_psp);
	build_emx_args(&argc, &argv);
	file_arg = 0;
    } else if (argc > 1 && strcmp(argv[1], "!proxy") == 0) {
	build_dj_args(&argc, &argv);
	emxl_psp = 1;
	file_arg = 0;
    } else if (strcmp(argv[0], "!RSX") == 0) {
	emxl_psp = 1;
	file_arg = 1;
    } else {
	file_arg = get_rsx_options(1, argv);
	if (file_arg == -1)
	    exit(1);
	opt_version = 1;
    }

    /* check filename */
    if (argc <= file_arg) {
        if (argc > 1)
            puts("\nError: no filename defined");
#ifdef __EMX__
        else {
            DPMIVERSION ver;
            FREEMEMINFO fm;

            GetDPMIVersion(&ver);
            GetFreeMemInfo(&fm);
            printf("\nRunning under DPMI version %d.%d\n"
                    "Dpmi virtual memory %lu KB\n"
                    "Dpmi physical memory %lu KB\n",
                    ver.major, ver.minor,
                    fm.MaxUnlockedPages * 4,
                    fm.PhysicalPages * 4);
        }
#endif
	exit(1);
    }

    /* copro required, 387 there ? */
    /*    npx_present = (char) npx_installed();*/
    npx_present = 0;
    copro = 0;

    /*
    if (opt_force_copro)
	copro = opt_force_copro - (char) 1;
    else if (copro == 1 && !npx_present)
	copro = 3;
    */

    if (real_to_protected(1))
	exit(1);

    /* - - - now protected mode! - - - rsx/rsx32 */

    if (test_dpmi_capabilities())
	protected_to_real(1);

    if (hangin_extender()) {
        puts("Error: can't hang in extensions");
	protected_to_real(1);
    }

    if (opt_kdeb && argv[file_arg + 2])
	strcpy(kdeb_program, argv[file_arg + 2]);

    /* init process-tables */
    init_this_process();

    /* init bios_selector & get keyboard */
    init_bios_keyboard();

    lfn = rm_test_lfn();
    if (lfn) {
        char *s = getenv("LFN");
        if (s && (*s == 'N' || *s == 'n'))
            lfn = 0;
    }

    strcpy(cpyfile, argv[file_arg]);
    if (rm_access(cpyfile, 0) == -1) {
        _defext(cpyfile, "exe");
        _path(exefile, cpyfile);
    } else
        strcpy(exefile, cpyfile);

    if (rm_access(exefile, 0) == -1) {
        printf("\nError: file not found: %s\n", cpyfile /*argv[file_arg]*/);
        shut_down(1);
    }

    if (!opt_max_dos_handles)
	opt_max_dos_handles = N_FILES;
    else if (opt_max_dos_handles > RSX_NFILES)
	opt_max_dos_handles = RSX_NFILES;
    rm_sethandles(opt_max_dos_handles);

    /* hang in emulation */
    /*
    if (copro == 3)
	if (install_rsx387())
	    shut_down(2);

    init_fpu();
    */

    /* rsx output */
    if (opt_redir)
	if ((rsx_stdout = rm_creat("rsx.log", _A_NORMAL)) < 0)
	    rsx_stdout = 1;

    err = exec32(P_WAIT, exefile, argc - file_arg, argv + file_arg, org_envc, org_env);

    printf("\nError %s: ", exefile);
    switch (err) {
	case EMX_ENOEXEC:
	    puts("Not a valid a.out format");
	    break;
	case EMX_ENOMEM:
	    puts("Not enough DPMI memory");
	    break;
	default:
	    printf("Can't load file, emx errno = %d\n", err);
	    break;
    }

    shut_down(1);
    /* never reached */
}

void _searchenv (const char *file, const char *var, char *path)
{
  char *list, *end;
  int i;
  
  strcpy (path, file);
  if (rm_access (path, 4) == 0)
    return;
  list = getenv (var);
  if (list != NULL)
    for (;;)
      {
        while (*list == ' ' || *list == '\t') ++list;
        if (*list == 0) break;
        end = list;
        while (*end != 0 && *end != ';') ++end;
        i = end - list;
        while (i>0 && (list[i-1] == ' ' || list[i-1] == '\t')) --i;
        if (i != 0)
          {
            memcpy (path, list, i);
            if (list[i-1] != '/' && list[i-1] != '\\' && list[i-1] != ':')
              path[i++] = '\\';
            strcpy (path+i, file);
            if (rm_access (path, 4) == 0)
              return;
          }
        if (*end == 0) break;
        list = end + 1;
      }
  path[0] = 0;
}

int _path (char *dst, const char *name)
{
  if (strpbrk (name, "/\\:") != NULL)
    {
      if (rm_access (name, 4) == 0)
        strcpy (dst, name);
      else
        dst[0] = 0;
    }
  else
    {
      _searchenv (name, "EMXPATH", dst);
      if (dst[0] == 0)
        _searchenv (name, "PATH", dst);
    }
  if (dst[0] == 0)
    {
      return (-1);
    }
  return (0);
}

#define TRUE 1
#define FALSE 0

void _defext (char *dst, const char *ext)
{
  int dot, sep;

  dot = FALSE; sep = TRUE;

  while (*dst != 0)
    switch (*dst++)
      {
      case '.':
        dot = TRUE;
        sep = FALSE;
        break;
      case ':':
      case '/':
      case '\\':
        dot = FALSE;
        sep = TRUE;
        break;
      default:
        sep = FALSE;
        break;
      }
  if (!dot && !sep)
    {
      *dst++ = '.';
      strcpy (dst, ext);
    }
}

char *strpbrk (const char *string1, const char *string2)
{
  char table[256];

  memset (table, 1, 256);
  while (*string2 != 0)
    table[(unsigned char)*string2++] = 0;
  table[0] = 0;
  while (table[(unsigned char)*string1])
    ++string1;
  return (*string1 == 0 ? NULL : (char *)string1);
}
