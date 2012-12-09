#ifndef __EMXBAD__

#define S_IFMT		0170000 	/* file type mask */
#define S_IFDIR 	0040000 	/* directory */
#define S_IFCHR 	0020000 	/* character special */
#define S_IFREG 	0100000 	/* regular */
#define S_IREAD 	0000400 	/* read permission, owner */
#define S_IWRITE	0000200 	/* write permission, owner */
#define S_IEXEC 	0000100 	/* execute/search permission, owner */

struct stat
{
  long	  st_dev;
  long	  st_ino;
  long	  st_mode;
  long	  st_nlink;
  long	  st_uid;
  long	  st_gid;
  long	  st_rdev;
  long	  st_size;
  long	  st_atime;
  long	  st_mtime;
  long	  st_ctime;
  long	  st_attr;
  long	  st_reserved;
};

#else /* __EMX__ */
#include <sys/types.h>
#include <sys/stat.h>
#endif

int	sys_stat(char *filename, struct stat *statbuf);
int	sys_fstat(int handle, struct stat *statbuf);
