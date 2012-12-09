/*****************************************************************************
 * FILE: libc.c 							     *
 *									     *
 * DESC:								     *
 *	- kernel libc functions 					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include <string.h>
#include <malloc.h>
#include <sys\types.h>
#include <ctype.h>

#if !defined (NULL)
#define NULL ((void *)0)
#endif

#if !defined (_SIZE_T)
#define _SIZE_T
typedef unsigned long size_t;
#endif

unsigned errno;
unsigned _doserrno;
unsigned _psp;

size_t strlen(__const__ char *s)
{
    register int __res __asm__("cx");
    __asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %0\n\t"
	"decl %0"
	:"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):"di");
    return __res;
}

char * strcat(char *dest, __const__ char *src)
{
    __asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"decl %1\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	: /* no output */
	:"S" (src),"D" (dest),"a" (0),"c" (0xffffffff):"si","di","ax","cx");
    return dest;
}

int strcmp(__const__ char *cs, __const__ char *ct)
{
    register int __res __asm__("ax");
    __asm__("cld\n"
	"1:\tlodsb\n\t"
	"scasb\n\t"
	"jne 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"jmp 3f\n"
	"2:\tmovl $1,%%eax\n\t"
	"jb 3f\n\t"
	"negl %%eax\n"
	"3:"
	:"=a" (__res):"D" (cs),"S" (ct):"si","di");
    return __res;
}

char *strcpy(char *dest, __const__ char *src)
{
    __asm__("cld\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	: /* no output */
	:"S" (src),"D" (dest):"si","di","ax","memory");
    return dest;
}

char *strstr (const char *string1, const char *string2)
{
  int len1, len2, i;
  char first;

  if (*string2 == 0)
    return ((char *)string1);
  len1 = 0;
  while (string1[len1] != 0) ++len1;
  len2 = 0;
  while (string2[len2] != 0) ++len2;
  if (len2 == 0)
    return ((char *)(string1+len1));
  first = *string2;
  while (len1 >= len2)
    {
      if (*string1 == first)
	{
	  for (i = 1; i < len2; ++i)
	    if (string1[i] != string2[i])
	      break;
	  if (i >= len2)
	    return ((char *)string1);
	}
      ++string1; --len1;
    }
  return (NULL);
}

int memcmp(__const__ void *cs, __const__ void *ct, size_t count)
{
    register int __res __asm__("ax");
    __asm__("cld\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 1f\n\t"
	"movl $1,%%eax\n\t"
	"jb 1f\n\t"
	"negl %%eax\n"
	"1:"
	:"=a" (__res):"0" (0),"D" (cs),"S" (ct),"c" (count)
	:"si","di","cx");
    return __res;
}

extern char **environ;
char *getenv(char *name)
{
    int len;
    char **p, *s;

    if (name == NULL || environ == NULL)
	return (NULL);
    len = strlen(name);
    for (p = environ; *p != NULL; ++p) {
	s = *p;
	if (strlen(s) > len && s[len] == '=' && (memcmp(name, s, len) == 0))
	    return (s + len + 1);
    }
    return (NULL);
}

void *memset(void *s, int c, size_t count)
{
    __asm__(
	"cld\n\t"
	"rep\n\t"
	"stosb"
	: /* no output */
	:"a" (c),"D" (s),"c" (count)
	:"cx","di","memory");
    return s;
}

void *memcpy(void *to, __const__ void *from, size_t n)
{
    __asm__(
	"cld\n\t"
	"movl %%edx, %%ecx\n\t"
	"shrl $2,%%ecx\n\t"
	"rep ; movsl\n\t"
	"testb $1,%%dl\n\t"
	"je 1f\n\t"
	"movsb\n"
	"1:\ttestb $2,%%dl\n\t"
	"je 2f\n\t"
	"movsw\n"
	"2:\n"
	: /* no output */
	:"d" (n),"D" ((long) to),"S" ((long) from)
	: "cx","di","si","memory");
    return (to);
}

static inline void bzero(void *p, unsigned size)
{
    __asm__ __volatile__(
	"cld ; rep ; stosl"
	:
	:"a"(0), "D"((long) p), "c"(size >> 2)
	:"di", "cx");
}

void *sbrk(int bytes)
{
    static brk_value = 0x1000;
    static brk_max = 0xA000;
    int retv;

    if (bytes > 0xFFFF)
	return (void *) -1;
    if (brk_value + bytes > brk_max)
	return (void *) -1;

    retv = brk_value;
    brk_value += bytes;
    return (void *) retv;
}

#define MAGIC (Header *)1111

typedef long Align;
union header {
    struct {
	union header *ptr;
	unsigned size;
    } s;
    Align x;
};
typedef union header Header;

static Header base;
static Header *freep = NULL;

void *malloc(size_t nbytes)
{
    Header *p, *prevp;
    static Header *morecore(unsigned);
    unsigned nunits;

    nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

    if ((prevp = freep) == NULL) {
	base.s.ptr = freep = prevp = &base;
	base.s.size = 0;
    }
    for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr) {
	if (p->s.size >= nunits) {
	    if (p->s.size == nunits)
		prevp->s.ptr = p->s.ptr;
	    else {
		p->s.size -= nunits;
		p += p->s.size;
		p->s.size = nunits;
	    }
	    freep = prevp;

	    p->s.ptr = MAGIC;


	    return (void *) (p + 1);
	}
	if (p == freep)
	    if ((p = morecore(nunits)) == NULL)
		return NULL;
    }
}

#define NALLOC 128		/* 128 * Header = 1 KB */

static Header *morecore(unsigned nu)
{
    void *cp, *sbrk(int);
    Header *up;

    if (nu < NALLOC)
	nu = NALLOC;
    cp = sbrk(nu * sizeof(Header));
    if (cp == (void *) -1)
	return NULL;
    up = (Header *) cp;
    up->s.size = nu;
    up->s.ptr = MAGIC;
    free((void *) (up + 1));
    return freep;
}

void free(void *ap)
{
    Header *bp, *p;

    bp = (Header *) ap - 1;

    if (bp->s.ptr != MAGIC)
	return;
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
	if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
	    break;

    if (bp + bp->s.size == p->s.ptr) {
	bp->s.size += p->s.ptr->s.size;
	bp->s.ptr = p->s.ptr->s.ptr;
    } else
	bp->s.ptr = p->s.ptr;
    if (p + p->s.size == bp) {
	p->s.size += bp->s.size;
	p->s.ptr = bp->s.ptr;
    } else
	p->s.ptr = bp;
    freep = p;
}

void *realloc(void *oldp, size_t newsize)
{
    void *newp;
    Header *h;
    unsigned oldsize;

    if (oldp == NULL)
	return (malloc(newsize));

    h = (Header *) oldp - 1;
    if (h->s.ptr != MAGIC)
	return (NULL);

    oldsize = h->s.size * sizeof(Header) - sizeof(Header);

    free(oldp);
    newp = malloc(newsize);
    if (newp == NULL)
	return NULL;
    if (newp != oldp)
	memcpy(newp, oldp, (newsize < oldsize) ? newsize : oldsize);
    return (newp);
}

void *calloc(size_t nelem, size_t size)
{
    void *p = malloc(nelem * size);

    if (p)
	bzero(p, nelem * size);
    return p;
}

unsigned char _ctype[257] =
	  {
/* -1 */    0,
/* 00 */    _CNTRL,
/* 01 */    _CNTRL,
/* 02 */    _CNTRL,
/* 03 */    _CNTRL,
/* 04 */    _CNTRL,
/* 05 */    _CNTRL,
/* 06 */    _CNTRL,
/* 07 */    _CNTRL,
/* 08 */    _CNTRL,
/* 09 */    _CNTRL|_SPACE,
/* 0A */    _CNTRL|_SPACE,
/* 0B */    _CNTRL|_SPACE,
/* 0C */    _CNTRL|_SPACE,
/* 0D */    _CNTRL|_SPACE,
/* 0E */    _CNTRL,
/* 0F */    _CNTRL,
/* 10 */    _CNTRL,
/* 11 */    _CNTRL,
/* 12 */    _CNTRL,
/* 13 */    _CNTRL,
/* 14 */    _CNTRL,
/* 15 */    _CNTRL,
/* 16 */    _CNTRL,
/* 17 */    _CNTRL,
/* 18 */    _CNTRL,
/* 19 */    _CNTRL,
/* 1A */    _CNTRL,
/* 1B */    _CNTRL,
/* 1C */    _CNTRL,
/* 1D */    _CNTRL,
/* 1E */    _CNTRL,
/* 1F */    _CNTRL,
/* 20 */    _PRINT|_SPACE,
/* 21 */    _PRINT|_PUNCT,
/* 22 */    _PRINT|_PUNCT,
/* 23 */    _PRINT|_PUNCT,
/* 24 */    _PRINT|_PUNCT,
/* 25 */    _PRINT|_PUNCT,
/* 26 */    _PRINT|_PUNCT,
/* 27 */    _PRINT|_PUNCT,
/* 28 */    _PRINT|_PUNCT,
/* 29 */    _PRINT|_PUNCT,
/* 2A */    _PRINT|_PUNCT,
/* 2B */    _PRINT|_PUNCT,
/* 2C */    _PRINT|_PUNCT,
/* 2D */    _PRINT|_PUNCT,
/* 2E */    _PRINT|_PUNCT,
/* 2F */    _PRINT|_PUNCT,
/* 30 */    _PRINT|_DIGIT|_XDIGIT,
/* 31 */    _PRINT|_DIGIT|_XDIGIT,
/* 32 */    _PRINT|_DIGIT|_XDIGIT,
/* 33 */    _PRINT|_DIGIT|_XDIGIT,
/* 34 */    _PRINT|_DIGIT|_XDIGIT,
/* 35 */    _PRINT|_DIGIT|_XDIGIT,
/* 36 */    _PRINT|_DIGIT|_XDIGIT,
/* 37 */    _PRINT|_DIGIT|_XDIGIT,
/* 38 */    _PRINT|_DIGIT|_XDIGIT,
/* 39 */    _PRINT|_DIGIT|_XDIGIT,
/* 3A */    _PRINT|_PUNCT,
/* 3B */    _PRINT|_PUNCT,
/* 3C */    _PRINT|_PUNCT,
/* 3D */    _PRINT|_PUNCT,
/* 3E */    _PRINT|_PUNCT,
/* 3F */    _PRINT|_PUNCT,
/* 40 */    _PRINT|_PUNCT,
/* 41 */    _PRINT|_UPPER|_XDIGIT,
/* 42 */    _PRINT|_UPPER|_XDIGIT,
/* 43 */    _PRINT|_UPPER|_XDIGIT,
/* 44 */    _PRINT|_UPPER|_XDIGIT,
/* 45 */    _PRINT|_UPPER|_XDIGIT,
/* 46 */    _PRINT|_UPPER|_XDIGIT,
/* 47 */    _PRINT|_UPPER,
/* 48 */    _PRINT|_UPPER,
/* 49 */    _PRINT|_UPPER,
/* 4A */    _PRINT|_UPPER,
/* 4B */    _PRINT|_UPPER,
/* 4C */    _PRINT|_UPPER,
/* 4D */    _PRINT|_UPPER,
/* 4E */    _PRINT|_UPPER,
/* 4F */    _PRINT|_UPPER,
/* 50 */    _PRINT|_UPPER,
/* 51 */    _PRINT|_UPPER,
/* 52 */    _PRINT|_UPPER,
/* 53 */    _PRINT|_UPPER,
/* 54 */    _PRINT|_UPPER,
/* 55 */    _PRINT|_UPPER,
/* 56 */    _PRINT|_UPPER,
/* 57 */    _PRINT|_UPPER,
/* 58 */    _PRINT|_UPPER,
/* 59 */    _PRINT|_UPPER,
/* 5A */    _PRINT|_UPPER,
/* 5B */    _PRINT|_PUNCT,
/* 5C */    _PRINT|_PUNCT,
/* 5D */    _PRINT|_PUNCT,
/* 5E */    _PRINT|_PUNCT,
/* 5F */    _PRINT|_PUNCT,
/* 60 */    _PRINT|_PUNCT,
/* 61 */    _PRINT|_LOWER|_XDIGIT,
/* 62 */    _PRINT|_LOWER|_XDIGIT,
/* 63 */    _PRINT|_LOWER|_XDIGIT,
/* 64 */    _PRINT|_LOWER|_XDIGIT,
/* 65 */    _PRINT|_LOWER|_XDIGIT,
/* 66 */    _PRINT|_LOWER|_XDIGIT,
/* 67 */    _PRINT|_LOWER,
/* 68 */    _PRINT|_LOWER,
/* 69 */    _PRINT|_LOWER,
/* 6A */    _PRINT|_LOWER,
/* 6B */    _PRINT|_LOWER,
/* 6C */    _PRINT|_LOWER,
/* 6D */    _PRINT|_LOWER,
/* 6E */    _PRINT|_LOWER,
/* 6F */    _PRINT|_LOWER,
/* 70 */    _PRINT|_LOWER,
/* 71 */    _PRINT|_LOWER,
/* 72 */    _PRINT|_LOWER,
/* 73 */    _PRINT|_LOWER,
/* 74 */    _PRINT|_LOWER,
/* 75 */    _PRINT|_LOWER,
/* 76 */    _PRINT|_LOWER,
/* 77 */    _PRINT|_LOWER,
/* 78 */    _PRINT|_LOWER,
/* 79 */    _PRINT|_LOWER,
/* 7A */    _PRINT|_LOWER,
/* 7B */    _PRINT|_PUNCT,
/* 7C */    _PRINT|_PUNCT,
/* 7D */    _PRINT|_PUNCT,
/* 7E */    _PRINT|_PUNCT,
/* 7F */    _CNTRL,
/* 80 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* A0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* B0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* C0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* D0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* E0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* F0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	  };
