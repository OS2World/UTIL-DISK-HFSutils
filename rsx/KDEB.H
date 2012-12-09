#ifndef _RSX_KDEB_H
#define _RSX_KDEB_H

int	KDEB_check_breakpoints(DWORD pc);
void	KDEB_enable_breakpoints(void);
void	KDEB_disable_breakpoints(void);
int	KDEB_ptrace(int request, DWORD addr, DWORD data, DWORD * ret);
void	KDEB_debug_handler();
volatile void KDEB_rsx32_orgjmp();

#endif /* _RSX_KDEB_H */
