/* Modified for hfsutils by Marcus Better, July 1997 */

#ifndef _FS_H
#define _FS_H

#define FMODE_READ  1
#define FMODE_WRITE 2

#if !defined (FD_SETSIZE)

#define FD_SETSIZE 256
typedef struct _fd_set
{
  unsigned long bits[(FD_SETSIZE+31) / 32];
} fd_set;

#define fd_index(c) ((c) >> 5)
#define fd_mask(c) (1 << ((c) & 31))
#define FD_SET(fd, map)     ((map)->bits[fd_index(fd)] |= fd_mask(fd))
#define FD_CLR(fd, map)     ((map)->bits[fd_index(fd)] &= ~(fd_mask(fd)))
#define FD_ISSET(fd, map)   ((map)->bits[fd_index(fd)] & fd_mask(fd))
#define FD_ZERO(s)	    (void)memset(s, 0, sizeof (*(s)))

#endif

typedef struct
{
    unsigned long bits[N_FD];
} p_fdset;


struct pipe_info {
    unsigned long   memhandle;
    unsigned long   memaddress;
    long	    pos;
    long	    len;
    long	    size;
    unsigned int    rd_openers;
    unsigned int    wr_openers;
    unsigned int    readers;
    unsigned int    writers;
    unsigned int    sel;
} ;

struct rm_pipe_info {
    unsigned int    readers;
    unsigned int    writers;
    /* this must be the last struct-component */
    char	  * temp_file_name;
} ;

struct dasd_info {
    int drive;
    unsigned long bytes;
    unsigned short cyls;
    unsigned short heads;
    unsigned short secs;
    int os2;
};

union file_info {
    struct pipe_info	*pipe_i;
    struct rm_pipe_info *rm_pipe_i;
    struct dasd_info    *dasd_i;
    /* others */
};

struct file {
    unsigned short	    f_mode;	    /* 1=RD, 2=WR */
    unsigned short	    f_flags;	    /* NDELAY etc */
    unsigned short	    f_count;	    /* no processes */
    short		    f_doshandle;    /* real dos handle or -1 */
    unsigned long	    f_offset;	    /* seek pos */
    struct file_operations *f_op;	    /* file ops */
    union file_info	    f_info;	    /* extra information */
};

typedef long ARGUSER;

struct file_operations {
    long (*lseek) (struct file *, long, int);
    ARGUSER (*read) (struct file *, ARGUSER, ARGUSER);
    ARGUSER (*write) (struct file *, ARGUSER, ARGUSER);
    int (*select) (struct file *);
    int (*ioctl) (struct file *, ARGUSER, ARGUSER);
    int (*open) (struct file *, ARGUSER);
    void (*release) (struct file *);
};

extern struct file rsx_filetab[RSX_NFILES];

#define NIL_FP ((struct file *)0)

struct _select {
    DWORD nfds;
    DWORD readfds;
    DWORD writefds;
    DWORD excepfds;
    DWORD time;
};

void	    init_rsx_filetab(void);
int	    get_empty_proc_filp(void);
int	    get_dos_handle(int);
int	    sys_close(int);
int	    sys_dup2(unsigned int, unsigned int);
int	    sys_dup(unsigned int);
ARGUSER     sys_read(int fd, ARGUSER buf, ARGUSER bytes);
ARGUSER     sys_write(int fd, ARGUSER buf, ARGUSER bytes);
long	    sys_lseek(int fd, long off, int orgin);
int	    sys_select(ARGUSER select);
int	    sys_ioctl(int fd, ARGUSER request, ARGUSER arg);
int	    sys_pipe(ARGUSER size, ARGUSER arg);

int	    make_rm_pipe(struct file *);
char	  * convert_filename (char *name);

extern struct file_operations msdos_fop;
extern struct file_operations pipe_fop;
extern struct file_operations rm_pipe_fop;
extern struct file_operations tty_fop;
extern struct file_operations dasd_fop; /*MB*/
#endif /* _FS_H */
