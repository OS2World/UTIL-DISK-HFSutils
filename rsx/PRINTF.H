#ifdef __EMX__

int	puts(char *s);
int	printf(const char *fmt,...);
int	sprintf(char * buf, const char *fmt, ...);

#else

#include  <stdio.h>

#endif
