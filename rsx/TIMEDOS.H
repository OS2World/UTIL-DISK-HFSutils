#include <time.h>

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

unsigned long dos2gmt(struct dos_date *dd, struct dos_time *dt);
void gmt2tm(unsigned long *t, struct tm *result);
unsigned long filetime2gmt(struct file_time *ft);
void gmt2filetime(unsigned long time, struct file_time *ft);

