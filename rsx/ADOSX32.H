/* Copyright	Rainer Schnitker 92,93,94 */

long	int10voff ( void );	/* label to fnct that calls old int10h */
long	int10vsel ( void );	/* label to fnct that calls old int10h */
long	int21voff ( void );	/* label to fnct that calls old int21h */
long	int21vsel ( void );	/* label to fnct that calls old int21h */
void	bioscall ( void );
void	doscall ( void );
void    mousecall ( void );
volatile void back_from_syscall ( void );
void	debug_entry (void);
int	execute_dpmi_function (void);
