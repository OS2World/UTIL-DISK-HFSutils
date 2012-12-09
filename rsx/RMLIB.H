/*
**  (c) Rainer Schnitker 1994
*/

#ifndef _RMLIB_H
#define _RMLIB_H

/* Real Mode DOS Calls */

#if !defined (NULL)
#define NULL ((void *) 0)
#endif

#if !defined (SEEK_SET)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#if !defined (RM_O_RDONLY)
#define RM_O_RDONLY	    0x0000
#define RM_O_WRONLY	    0x0001
#define RM_O_RDWR	    0x0002

#define RM_O_DENYRW	    0x0010
#define RM_O_DENYWR	    0x0020
#define RM_O_DENYRD	    0x0020
#define RM_O_DENYNO	    0x0030

#define RM_O_NOINHERIT	    0x0080
#endif

#if !defined (_A_NORMAL)
#define _A_NORMAL	0x00
#define _A_RDONLY	0x01
#define _A_HIDDEN	0x02
#define _A_SYSTEM	0x04
#define _A_VOLID	0x08
#define _A_SUBDIR	0x10
#define _A_ARCH 	0x20
#endif

#ifndef _FIND_T_DEFINED
#define _FIND_T_DEFINED
struct find_t {

#ifdef USE_DOS
	char		reserved[21];
#else
        unsigned long   lfn_handle;
        unsigned char   lfn_attr;
        unsigned char   magic[3];           /* = "LFN" */
        char            shortname[13];
#endif
	unsigned char	attrib;
	unsigned short	wr_time;
	unsigned short	wr_date;
	unsigned short	size_lo;
	unsigned short	size_hi;
        char            name[257];
	};
#endif

#ifndef _DATETIME_T_DEFINED
#define _DATETIME_T_DEFINED
struct dos_date {
    unsigned short ddate_year;	    /* 1980 - 2099 */
    unsigned char  ddate_month;     /* 1 - 12 */
    unsigned char  ddate_day;	    /* 1 - 31 */
    unsigned char  ddate_dayofweek; /* 0 = sonday */
};

struct dos_time {
    unsigned char dtime_hour;	   /* 0 - 23 */
    unsigned char dtime_minutes;   /* 0 - 59 */
    unsigned char dtime_seconds;   /* 0 - 59 */
    unsigned char dtime_hsec;	   /* 0 - 99 */
};

struct file_time {
    unsigned short ft_date;
    unsigned short ft_time;
};
#endif

#ifndef _COUNTRY_T_DEFINED
#define _COUNTRY_T_DEFINED
#pragma pack(1)
struct country_info {
    unsigned short cd_date;
    unsigned char  cd_currency[5];
    unsigned char  cd_1000_sep[2];
    unsigned char  cd_decimal_sep[2];
    unsigned char  cd_date_sep[2];
    unsigned char  cd_time_sep[2];
    unsigned char  cd_flags;
    unsigned char  cd_ncurrency;
    unsigned char  cd_timeformat;
    unsigned long  cd_casemap_func;
    unsigned char  cd_list_sep[2];
    unsigned char  cd_reserved[10];
};
#pragma pack()
#endif

#ifndef _EXEC_T_DEFINED
#define _EXEC_T_DEFINED
struct execb {
    unsigned short psp_2c;
    unsigned short psp_80_off;
    unsigned short psp_80_seg;
    unsigned short fcb1_off;
    unsigned short fcb1_seg;
    unsigned short fcb2_off;
    unsigned short fcb2_seg;
};
#endif

/*
CREAT_NEW           creat, but fail if exits
CREAT_ALWAYS        creat, override
OPEN_ALWAYS         creat, if file not exist, open if exists
OPEN_EXISTING       open, fail if not exists
TRUCATE_EXISTING    open and zero
*/

/*                      not exist |  exist */
#define CREAT_NEW           (0x10 | 0x00)
#define CREAT_ALWAYS        (0x10 | 0x02)
#define OPEN_ALWAYS         (0x10 | 0x01)
#define OPEN_EXISTING       (0x00 | 0x01)
#define TRUNCATE_EXISTING   (0x00 | 0x02)

unsigned rm_bios_read_keybrd(unsigned mode);

int	rm_setdrive(WORD drive);
int	rm_getdrive(void);
void	rm_setdta(void *buf);

void	rm_getdate(struct dos_date *dd);
int	rm_setdate(struct dos_date *dd);
void	rm_gettime(struct dos_time *dt);
int	rm_settime(struct dos_time *dt);

int	rm_chdir(char *name);
int	rm_unlink(char *name);
int	rm_getfattr(char *name, WORD *attr);
int	rm_setfattr(char *name, WORD attr);
int	rm_access(char *name, WORD mode);
int	rm_getcwd(WORD drive, char *name);
int	rm_findfirst(char *name, WORD attr, struct find_t *ft);
int     rm_findnext(struct find_t *ft);

int	rm_creat(char *name, WORD attr);
int	rm_open(char *name, WORD modes);
int	rm_close(WORD handle);
int     rm_extopen(char *, WORD, WORD, WORD);
int	rm_read(WORD handle, void *buf, WORD bytes);
int	rm_write(WORD handle, void * buf, WORD bytes);
long	rm_lseek(WORD handle, DWORD offset, WORD orgin);
int	rm_dup(WORD handle);
int	rm_dup2(WORD handle, WORD newhandle);
int	rm_creatnew(char *name, WORD attr);
int	rm_getftime(WORD handle, WORD *date, WORD *time);
int	rm_setftime(WORD handle, WORD date, WORD time);
int	rm_sethandles(WORD handles);

int	rm_ioctl_getattr(WORD handle);
int	rm_isatty(WORD handle);
int	rm_ioctl_setattr(WORD handle, WORD mode);
int	rm_ioctl_select_in(WORD handle);
int	rm_ioctl_remotedrive(BYTE drive);

int     rm_get_country_info(struct country_info *);
unsigned char rm_country_casemap(unsigned char, DWORD);
int	rm_exec(char *, struct execb *);
int	rm_get_exit_status(void);
int	rm_get_exterror_code(void);

int     rm_test_lfn(void);

extern unsigned emx_errno;

#endif /* _RMLIB_H */
