/*****************************************************************************
 * FILE: timedos.c                                                           *
 *									     *
 * DESC:								     *
 *      - time from DOS to UNIX                                              *
 *      - time from UNIX to DOS                                              *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include <time.h>
#include "TIMEDOS.H"

#define MINUTE 60L
#define HOUR (60L*MINUTE)
#define DAY (24L*HOUR)
#define YEAR (365L*DAY)

static long month[12] =
{
    0L,
    DAY * (long) (31),
    DAY * (long) (31 + 29),
    DAY * (long) (31 + 29 + 31),
    DAY * (long) (31 + 29 + 31 + 30),
    DAY * (long) (31 + 29 + 31 + 30 + 31),
    DAY * (long) (31 + 29 + 31 + 30 + 31 + 30),
    DAY * (long) (31 + 29 + 31 + 30 + 31 + 30 + 31),
    DAY * (long) (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
    DAY * (long) (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
    DAY * (long) (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
    DAY * (long) (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30)
};

unsigned long dos2gmt(struct dos_date * dd, struct dos_time * dt)
{
    unsigned long res;
    unsigned long year;

    /* calc date */
    year = dd->ddate_year - 1970L;
    res = YEAR * year + DAY * ((year + 1) / 4);
    res += month[dd->ddate_month - 1];
    if (dd->ddate_month > 2 && ((year + 2) % 4))
	res -= DAY;
    res += DAY * (dd->ddate_day - 1);

    /* calc time */
    res += HOUR * (dt->dtime_hour);
    res += MINUTE * (dt->dtime_minutes);
    res += (dt->dtime_seconds);

    return res;
}

#define leap(y) \
  ((y) % 4 != 0 ? 0 : (y) % 100 != 0 ? 1 : (y) % 400 != 0 ? 0 : 1)

void gmt2tm(unsigned long *t, struct tm * result)
{
    static int const mon_len[12] =
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned long t0, t1;
    unsigned long q;

    t0 = *t;
    if (t0 == (unsigned long) (-1))
	return;

    result->tm_sec = (int) (t0 % 60L);
    q = t0 / 60L;
    t0 = q;
    result->tm_min = (int) (t0 % 60L);
    q = t0 / 60L;
    t0 = q;
    result->tm_hour = (int) (t0 % 24L);
    q = t0 / 24L;
    t0 = q;

    result->tm_wday = (int) ((t0 + 4) % 7);
    result->tm_year = 70;
    for (;;) {
	t1 = (leap(result->tm_year + 1900) ? 366 : 365);
	if (t1 > t0)
	    break;
	t0 -= t1;
	++result->tm_year;
    }
    result->tm_mon = 0;
    result->tm_yday = (int) t0;
    for (;;) {
	if (result->tm_mon == 1)
	    t1 = (leap(result->tm_year + 1900) ? 29 : 28);
	else
	    t1 = mon_len[result->tm_mon];
	if (t1 > t0)
	    break;
	t0 -= t1;
	++result->tm_mon;
    }
    result->tm_mday = (int) t0 + 1;
    result->tm_isdst = 0;
}

unsigned long filetime2gmt(struct file_time * ft)
{
    struct dos_date dd;
    struct dos_time dt;

    dd.ddate_year = (unsigned short) ((ft->ft_date >> 9) + 1980);
    dd.ddate_month = (unsigned char) ((ft->ft_date >> 5) & 15);
    dd.ddate_day = (unsigned char) (ft->ft_date & 31);

    dt.dtime_hour = (unsigned char) (ft->ft_time >> 11);
    dt.dtime_minutes = (unsigned char) ((ft->ft_time >> 5) & 63);
    dt.dtime_seconds = (unsigned char) ((ft->ft_time & 31) << 1);

    return dos2gmt(&dd, &dt);
}

void gmt2filetime(unsigned long time, struct file_time * ft)
{
    struct tm tm;

    gmt2tm(&time, &tm);
    ft->ft_date = tm.tm_mday + ((tm.tm_mon + 1) << 5) +
	((tm.tm_year - 80) << 9);
    ft->ft_time = tm.tm_sec / 2 + (tm.tm_min << 5) + (tm.tm_hour << 11);
}
