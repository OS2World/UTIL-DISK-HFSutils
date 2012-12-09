/* Copyright	Rainer Schnitker 92,93 */

#ifndef _CDOSX32_H
#define _CDOSX32_H

#define CARRY_OFF   0
#define CARRY_ON    1
#define CARRY_NON   2

#define IOBUF_SIZE 8192
extern char *iobuf;
extern unsigned real_mode_stack;

void	    align_iobuf(void);
int	    int21normal(void);
ARGUSER     cdosx_read(int handle, ARGUSER buf, ARGUSER count);
ARGUSER     cdosx_write(int handle, ARGUSER buf, ARGUSER count);

#endif
