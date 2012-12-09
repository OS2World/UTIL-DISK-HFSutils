/* Copyright	Rainer Schnitker 92,93 */

#ifndef _PROCESS_H
#include "PROCESS.H"
#endif

int	argvenv ( int, char **, int, char **, NEWPROCESS * );
void	cpy_exename_to_stack ( NEWPROCESS *, char *);
int	load_protected_program ( char *, NEWPROCESS * );
int	write_core_file ( NEWPROCESS * );
int	write_core ( unsigned, NEWPROCESS * );
