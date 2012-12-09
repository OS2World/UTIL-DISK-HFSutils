/*****************************************************************************
 * FILE: printf.c							     *
 *									     *
 * DESC:								     *
 *	- printf/puts library function					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include <string.h>
#include <stdarg.h>
#include <sys\types.h>
#include "DPMI.H"
#include "PROCESS.H"
#include "RMLIB.H"
#include "RSX.H"
#include "PRINTF.H"

static int vsprintf(char *, const char *, va_list);
static int skip_atoi(const char **);
static char *number(char *, long, int, int, int, int);

static int output(char *buf)
{
    char buf2[5 * 80];
    char *s;
    int i, j;

    if (rm_isatty(rsx_stdout)) {	/* console ? */
	for (i = 0, j = 0; buf[i]; i++, j++) {
	    buf2[j] = buf[i];
	    if (buf2[j] == '\n')
		buf2[++j] = '\r';
	}
	buf2[j] = '0';
	s = buf2;
    }
    else {
	s = buf;
	j = i = strlen(buf);
    }

    rm_write(rsx_stdout, s, j);
    return i;
}

int puts(char *s)
{
    int i;
    i = output(s);
    output("\n");
    return i;
}

int printf(const char *fmt,...)
{
    char buf[5 * 80];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    return output(buf);
}

int sprintf(char *buf, const char *fmt,...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}

/* ------------------------------------------------------------------------
** This code is modified from the Linux kernel code  (ver 0.99)
**
**  linux/kernel/vsprintf.c
**
**  Copyright (C) 1991, 1992  Linus Torvalds
*/

/* we use this so that we can do without the ctype library */
#define is_digit(c)	((c) >= '0' && (c) <= '9')

static int skip_atoi(const char **s)
{
    int i = 0;

    while (is_digit(**s))
	i = i * 10 + *((*s)++) - '0';
    return i;
}

#define ZEROPAD 1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL 32		/* 0x */
#define SMALL	64		/* use 'abcdef' instead of 'ABCDEF' */

#ifdef __EMX__
#define do_div(n,base) ({	  \
int __res; \			  \
__asm__("divl %4"                 \
    :"=a" (n), "=d" (__res)       \
    :"0" (n),"1" (0),"r" (base)); \
__res; })
#else
#define do_div(n,base) ({ \
    unsigned long _res = (unsigned long) n % (unsigned long) base; \
    n = (unsigned long) n / (unsigned long) base; \
    (unsigned) _res; })
#endif

static char *
 number(char *str, long num, int base, int size, int precision, int type)
{
    char c, sign, tmp[36];
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;

    if (type & SMALL)
	digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (type & LEFT)
	type &= ~ZEROPAD;
    if (base < 2 || base > 36)
	return 0;
    c = (type & ZEROPAD) ? '0' : ' ';
    if (type & SIGN && num < 0) {
	sign = '-';
	num = -num;
    } else
	sign = (type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);
    if (sign)
	size--;
    if (type & SPECIAL)
	if (base == 16)
	    size -= 2;
	else if (base == 8)
	    size--;
    i = 0;
    if (num == 0)
	tmp[i++] = '0';
    else
	while (num != 0) {
	    unsigned long _res = (unsigned long) num % (unsigned long) base;
	    num = (unsigned long) num / (unsigned long) base;
	    tmp[i++] = digits[_res];
	}
    if (i > precision)
	precision = i;
    size -= precision;
    if (!(type & (ZEROPAD + LEFT)))
	while (size-- > 0)
	    *str++ = ' ';
    if (sign)
	*str++ = sign;
    if (type & SPECIAL)
	if (base == 8)
	    *str++ = '0';
	else if (base == 16) {
	    *str++ = '0';
	    *str++ = digits[33];
	}
    if (!(type & LEFT))
	while (size-- > 0)
	    *str++ = c;
    while (i < precision--)
	*str++ = '0';
    while (i-- > 0)
	*str++ = tmp[i];
    while (size-- > 0)
	*str++ = ' ';
    return str;
}

static int vsprintf(char *buf, const char *fmt, va_list args)
{
    int len;
    int i;
    char *str;
    char *s;
    int *ip;

    int flags;			/* flags to number() */

    int field_width;		/* width of output field */
    int precision;		/* min. # of digits for integers; max number
				 * of chars for from string */
    int qualifier;		/* 'h', 'l', or 'L' for integer fields */
    int use_long;

    for (str = buf; *fmt; ++fmt) {
	if (*fmt != '%') {
	    *str++ = *fmt;
	    continue;
	}
	/* process flags */
	flags = 0;
	use_long = 0;
      repeat:
	++fmt;			/* this also skips first '%' */
	switch (*fmt) {
	case '-':
	    flags |= LEFT;
	    goto repeat;
	case '+':
	    flags |= PLUS;
	    goto repeat;
	case ' ':
	    flags |= SPACE;
	    goto repeat;
	case '#':
	    flags |= SPECIAL;
	    goto repeat;
	case '0':
	    flags |= ZEROPAD;
	    goto repeat;
	}

	/* get field width */
	field_width = -1;
	if (is_digit(*fmt))
	    field_width = skip_atoi(&fmt);
	else if (*fmt == '*') {
	    /* it's the next argument */
	    field_width = va_arg(args, int);
	    if (field_width < 0) {
		field_width = -field_width;
		flags |= LEFT;
	    }
	}
	/* get the precision */
	precision = -1;
	if (*fmt == '.') {
	    ++fmt;
	    if (is_digit(*fmt))
		precision = skip_atoi(&fmt);
	    else if (*fmt == '*') {
		/* it's the next argument */
		precision = va_arg(args, int);
	    }
	    if (precision < 0)
		precision = 0;
	}
	/* get the conversion qualifier */
	qualifier = -1;
	if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
	    if (*fmt != 'h')
		use_long = 1;
	    qualifier = *fmt;
	    ++fmt;
	}
	switch (*fmt) {
	case 'c':
	    if (!(flags & LEFT))
		while (--field_width > 0)
		    *str++ = ' ';
	    *str++ = (unsigned char) va_arg(args, int);
	    while (--field_width > 0)
		*str++ = ' ';
	    break;

	case 's':
	    s = va_arg(args, char *);
	    len = strlen(s);
	    if (precision < 0)
		precision = len;
	    else if (len > precision)
		len = precision;

	    if (!(flags & LEFT))
		while (len < field_width--)
		    *str++ = ' ';
	    for (i = 0; i < len; ++i)
		*str++ = *s++;
	    while (len < field_width--)
		*str++ = ' ';
	    break;

	case 'o':
	    if (use_long)
		str = number(str, (long) va_arg(args, unsigned long), 8,
			     field_width, precision, flags);
	    else
		str = number(str, (long) va_arg(args, unsigned), 8,
			     field_width, precision, flags);
	    break;

	case 'p':
	    if (field_width == -1) {
		field_width = 8;
		flags |= ZEROPAD;
	    }
	    str = number(str, (long) va_arg(args, void *), 16,
			 field_width, precision, flags);
	    break;

	case 'x':
	    flags |= SMALL;
	case 'X':
	    if (use_long)
		str = number(str, (long) va_arg(args, unsigned long), 16,
			     field_width, precision, flags);
	    else
		str = number(str, (long) va_arg(args, unsigned), 16,
			     field_width, precision, flags);
	    break;

	case 'd':
	case 'i':
	    flags |= SIGN;
	case 'u':
	    if (use_long)
		str = number(str, (long) va_arg(args, unsigned long), 10,
			     field_width, precision, flags);
	    else
		str = number(str, (long) va_arg(args, unsigned), 10,
			     field_width, precision, flags);
	    break;

	case 'n':
	    ip = va_arg(args, int *);
	    *ip = (str - buf);
	    break;

	default:
	    if (*fmt != '%')
		*str++ = '%';
	    if (*fmt)
		*str++ = *fmt;
	    else
		--fmt;
	    break;
	}
    }
    *str = '\0';
    return str - buf;
}
