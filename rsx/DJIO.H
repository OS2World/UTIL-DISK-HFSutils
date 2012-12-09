void	djio_init(void);
void	djio_dup_fileattr(int from, int to);
int	dj_creat(char *name);
int	dj_open(char *name, int oflag);
int	dj_write (int handle, void *buf, unsigned nbyte);
int	dj_read (int handle, void *buf, unsigned nbyte);
int	dj_setmode (int handle, int mode);
int	dj_chmod(char *name, int pmode);
