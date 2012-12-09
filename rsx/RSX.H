#ifndef _RSX_H
#define _RSX_H

#define OPT_MEMACCESS 1

extern char	copro;
extern char     lfn;

extern char	opt_force_copro;	/* force copro status */
extern char	rsx387_in_dosmem;
extern char	opt_kdeb;		/* Kernel debug mode */
extern char	opt_debugrsx;		/* debug rsx; don't set int3 */
extern char     opt_stderr;

struct options {
  unsigned	opt_memaccess:1;	/* -am */
  unsigned	opt_force_dpmi09:1;	/* -R(1|9) */
  unsigned	opt_nocore:1;		/* -c */
  unsigned	opt_schedule:1;		/* -Rx */
  unsigned	opt_zero:1;		/* -Rz */
  unsigned	opt_os2:1;		/* -RO */
  unsigned	opt_convert_dot:1;	/* -Rp  : convert '.name' -> '!name' */
  int		opt_convert_filename;	/* -t | -Rt(533|803)
					 *      : convert filenames */
  int		opt_stackval;		/* -Rs# */
  int		opt_memalloc;		/* -Rm# */
  unsigned	opt_printall;		/* -RP */
  unsigned	opt_print_syscalls;	/* -RI */
  char		opt_default_disk;	/* -r*	: set default disk-drive */
};
extern struct options main_options;

extern char	npx_present;
extern char kdeb_program[80];

extern int	rsx_stdout;
extern int	kread;
extern int	kready;
extern unsigned bios_selector;
extern char	**org_env;
extern int	org_envc;

int	    hexstr2int(char *s);
char *	    scan_for_option(char *, struct options *);

#endif /* _RSX_H */
