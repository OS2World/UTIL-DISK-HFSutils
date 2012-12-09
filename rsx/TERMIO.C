/*****************************************************************************
 * FILE: termio.c							     *
 *									     *
 * DESC:								     *
 *	- ioctl() terminal functions					     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 * Thanks to: (bug-fixes, patches, testing, ..)                              *
 *      Friedbert Widmann <widi@informatik.uni-koblenz.de>                   *
 *                                                                           *
 *****************************************************************************/

#include <ctype.h>
#include "DPMI.H"
#include "RMLIB.H"
#include "SIGNALS.H"
#include "PROCESS.H"
#include "START32.H"
#include "COPY32.H"
#include "RSX.H"
#include "EXCEP32.H"
#include "TERMIO.H"
#include "DOSERRNO.H"
#include "PRINTF.H"

/*
 *  Sourcecode based on Linux v1.1
 *
 *  linux/kernel/tty_io.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
*/

#define _O_FLAG(tty,X)	((tty)->termios->c_oflag & (unsigned long)(X))
#define _C_FLAG(tty,X)	((tty)->termios->c_cflag & (unsigned long)(X))
#define _L_FLAG(tty,X)	((tty)->termios->c_lflag & (unsigned long)(X))
#define _I_FLAG(tty,X)	((tty)->termios->c_iflag & (unsigned long)(X))

#define L_ISIG(tty)	_L_FLAG((tty),ISIG)
#define L_ICANON(tty)	_L_FLAG((tty),ICANON)
#define L_XCASE(tty)	_L_FLAG((tty),XCASE)
#define L_ECHO(tty)	_L_FLAG((tty),ECHO)
#define L_ECHOE(tty)	_L_FLAG((tty),ECHOE)
#define L_ECHOK(tty)	_L_FLAG((tty),ECHOK)
#define L_ECHONL(tty)	_L_FLAG((tty),ECHONL)
#define L_NOFLSH(tty)	_L_FLAG((tty),NOFLSH)
#define L_IDEFAULT	_L_FLAG((tty),IDEFAULT)

#define I_IUCLC(tty)	_I_FLAG((tty),IUCLC)
#define I_INLCR(tty)	_I_FLAG((tty),INLCR)
#define I_ICRNL(tty)	_I_FLAG((tty),ICRNL)
#define I_IGNCR(tty)	_I_FLAG((tty),IGNCR)
#define I_IXON(tty)	_I_FLAG((tty),IXON)
#define I_IXANY(tty)	_I_FLAG((tty),IXANY)
#define I_ISTRIP(tty)	_I_FLAG((tty),ISTRIP)
#define I_IDELETE(tty)	_I_FLAG((tty),IDELETE)

#define INTR_CHAR(tty)	((tty)->termios->c_cc[VINTR])
#define QUIT_CHAR(tty)	((tty)->termios->c_cc[VQUIT])
#define ERASE_CHAR(tty) ((tty)->termios->c_cc[VERASE])
#define KILL_CHAR(tty)	((tty)->termios->c_cc[VKILL])
#define EOF_CHAR(tty)	((tty)->termios->c_cc[VEOF])
#define EOL_CHAR(tty)	((tty)->termios->c_cc[VEOL])
#define __DISABLED_CHAR '\0'

#define INC(a) ((a) = ((a)+1) & (TTY_BUF_SIZE-1))
#define DEC(a) ((a) = ((a)-1) & (TTY_BUF_SIZE-1))
#define EMPTY(a) ((a)->head == (a)->tail)
#define LEFT(a) (((a)->tail-(a)->head-1)&(TTY_BUF_SIZE-1))
#define LAST(a) ((a)->buf[(TTY_BUF_SIZE-1)&((a)->head-1)])
#define FULL(a) (!LEFT(a))
#define CHARS(a) (((a)->head-(a)->tail)&(TTY_BUF_SIZE-1))

#define TTY_BUF_SIZE  128
struct tty_queue {
    int head;
    int tail;
    char buf[TTY_BUF_SIZE];
};

struct tty_struct {
    struct termio *termios;
    int canon_data;
    int canon_head;
    unsigned int erasing:1;
    unsigned int lnext:1;
    unsigned long secondary_flags[TTY_BUF_SIZE / (sizeof(long))];
    struct tty_queue secondary;
};

struct termio __termios =
{
    BRKINT | ICRNL | IXON | IXANY,			/* iflag */
    0,							/* oflag */
    B9600 | CS8 | CREAD | HUPCL,			/* cflag */
    ISIG | ICANON | ECHO | ECHOE | ECHOK | IDEFAULT,	/* lflag */
    0,							/* line  */
    { 003, 034, 010, 025, 004, 000, 006, 001 }		/* c_cc  */
    /* INTR, QUIT,ERASE, KILL, EOF , EOL , VMIN, VTIME */
    /* C-C , C-\ , C-H , C-U , C-D , EOL , VMIN, VTIME */
};

struct tty_struct __tty =
{
    &__termios,
    0
};

static char control_c;
static unsigned long timeout;

/* flush keyboard buffer */
static void keyboard_flush(void)
{
    while (rm_bios_read_keybrd(kready))
	rm_bios_read_keybrd(kread);
}

/* read a key ; extended = 0,next call scan-code */
static int keyboard_read(struct tty_struct * tty)
{
    static int next_key = 0;
    int key, scan, ascii;

    if (next_key) {
	ascii = next_key;
	next_key = 0;
    } else {
	if (!rm_bios_read_keybrd(kready)) {
	    if (timeout)
		if (timeout <= time_tic)
		    timeout = 0;
	    return -1;
	}
	key = rm_bios_read_keybrd(kread);
	ascii = key & 0xff;
	scan = key >> 8;

	if (ascii == 0xE0)
	    ascii = 0;
	else if (scan < 20 && I_IDELETE(tty)) {
	    /*			ascii	scan		scan < 20 means:
	     *	BACKSPACE	8	< 20		this key is in the
	     *	CTRL-H		8	> 20		upper row.
	     *	CTRL-BACKSPACE	127	< 20
	     * CTRL-H should not be converted.
	     * So the IDELETE-conversion must be done here, because we
	     * have the scan-code only in this function.
	     */
	    if (ascii == 8)	/* Backspace */
		ascii = 127;
	    else if (ascii == 127)	/* C-Backspace */
		ascii = 8;
	}

	if (ascii == 0)
	    next_key = scan;
    }

    return ascii;
}

int set_bit(int nr, void *vaddr)
{
    long mask;
    int retval;
    unsigned long *addr = vaddr;

    addr += nr >> 5;
    mask = 1L << (nr & 0x1f);
    retval = (mask & *addr) != 0;
    *addr |= mask;
    return retval;
}

int clear_bit(int nr, void *vaddr)
{
    long mask;
    int retval;
    unsigned long *addr = vaddr;

    addr += nr >> 5;
    mask = 1L << (nr & 0x1f);
    retval = (mask & *addr) != 0;
    *addr &= ~mask;
    return retval;
}

static void put_tty_queue(unsigned char c, struct tty_queue * queue)
{
    int head = (queue->head + 1) & (TTY_BUF_SIZE - 1);

    if (head != queue->tail) {
	queue->buf[queue->head] = c;
	queue->head = head;
    }
}

#if 0
static int get_tty_queue(struct tty_queue * queue)
{
    int result = -1;

    if (queue->tail != queue->head) {
	result = queue->buf[queue->tail];
	INC(queue->tail);
    }
    return result;
}
#endif

static void flush_input(struct tty_queue * queue)
{
    keyboard_flush();
    queue->head = queue->tail;
}

#define TTY_READ_FLUSH(tty) copy_to_cooked(tty)

static char next_line[2] = "\r\n";
int output;

static int opost(unsigned char c, struct tty_struct * tty)
{
    if ((c == 10) && (L_ECHO(tty) || (L_ICANON(tty) && L_ECHONL(tty))))
	rm_write(output, next_line, sizeof(next_line));
    else if (L_ECHO(tty))
	rm_write(output, &c, 1);
    return 1;
}

static void echo_char(unsigned char c, struct tty_struct * tty)
{
    if (iscntrl(c) && c != '\t') {
	opost('^', tty);
	opost((unsigned char)(c ^ 0100), tty);
    } else
	opost((unsigned char) c, tty);
}

static void eraser(unsigned char c, struct tty_struct * tty)
{
    enum {
	ERASE, WERASE, KILL
    } kill_type;
    int seen_alnums;

    if (tty->secondary.head == tty->canon_head) {
	/* opost('\a', tty); *//* what do you think? */
	return;
    }
    if (c == ERASE_CHAR(tty))
	kill_type = ERASE;
    else {
	if (!L_ECHO(tty)) {
	    tty->secondary.head = tty->canon_head;
	    return;
	}
	if (!L_ECHOK(tty)) {
	    tty->secondary.head = tty->canon_head;
	    if (tty->erasing) {
		opost('/', tty);
		tty->erasing = 0;
	    }
	    echo_char(KILL_CHAR(tty), tty);
	    /* Add a newline if ECHOK is on */
	    if (L_ECHOK(tty))
		opost('\n', tty);
	    return;
	}
	kill_type = KILL;
    }

    seen_alnums = 0;
    while (tty->secondary.head != tty->canon_head) {
	c = LAST(&tty->secondary);
	DEC(tty->secondary.head);
	if (L_ECHO(tty)) {
	    if (!L_ECHOE(tty)) {
		echo_char(ERASE_CHAR(tty), tty);
	    }
	}
	if (kill_type == ERASE)
	    break;
    }
    if (tty->erasing && tty->secondary.head == tty->canon_head) {
	opost('/', tty);
	tty->erasing = 0;
    }
}

static void copy_to_cooked(struct tty_struct * tty)
{
    char c;

    for (;;) {
	if (! LEFT(&tty->secondary))
	    break;

	c = (char) keyboard_read(tty);
	if (c == -1)
	    break;

	if (I_ISTRIP(tty))
	    c &= 0x7f;

	if (!tty->lnext) {
	    if (c == '\r') {
		if (I_IGNCR(tty))
		    continue;
		if (I_ICRNL(tty))
		    c = '\n';
	    } else if (c == '\n' && I_INLCR(tty))
		c = '\r';
	}
	if (I_IUCLC(tty))
	    c = (char) tolower(c);

	if (c == __DISABLED_CHAR)
	    tty->lnext = 1;
	if (L_ICANON(tty) && !tty->lnext) {
	    if ((unsigned char)c == ERASE_CHAR(tty) ||
		 (unsigned char)c == KILL_CHAR(tty)) {
		eraser(c, tty);
		continue;
	    }
	}

	if (L_ISIG(tty) && !tty->lnext) {
	    if ((unsigned char)c == INTR_CHAR(tty)) {
		control_c = 1;
		send_signal(npz, SIGINT);
		flush_input(&tty->secondary);
		return;
	    }
	    if ((unsigned char) c == QUIT_CHAR(tty)) {
		control_c = 1;
		send_signal(npz, SIGQUIT);
		flush_input(&tty->secondary);
		return;
	    }
	}
	if (tty->erasing) {
	    opost('/', tty);
	    tty->erasing = 0;
	}
	if (c == '\n' && !tty->lnext) {
	    if (L_ECHO(tty) || (L_ICANON(tty) && L_ECHONL(tty)))
		opost('\n', tty);
	} else if (L_ECHO(tty)) {
	    /* Don't echo the EOF char in canonical mode.  Sun
	       handles this differently by echoing the char and
	       then backspacing, but that's a hack. */
	    if ((unsigned char) c != EOF_CHAR(tty) || !L_ICANON(tty) ||
		tty->lnext) {
		echo_char(c, tty);
	    }
	}
	if (c == '\377' && (c != (char)EOF_CHAR(tty) || !L_ICANON(tty) || tty->lnext))
	    put_tty_queue(c, &tty->secondary);

	if (L_ICANON(tty) && !tty->lnext &&
	    (c == '\n' || c == (char)EOF_CHAR(tty) ||
	    c == (char)EOL_CHAR(tty) )) {
	    if (c == (char)EOF_CHAR(tty))
		c = __DISABLED_CHAR;
	    set_bit(tty->secondary.head, &tty->secondary_flags);
	    put_tty_queue(c, &tty->secondary);
	    tty->canon_head = tty->secondary.head;
	    tty->canon_data++;
	} else
	    put_tty_queue(c, &tty->secondary);
	tty->lnext = 0;
    }
}

static int input_available_p(struct tty_struct * tty)
{
    /* Avoid calling TTY_READ_FLUSH unnecessarily. */
    if (L_ICANON(tty) ? tty->canon_data : !EMPTY(&tty->secondary))
	return 1;

    /* Shuffle any pending data down the queues. */
    TTY_READ_FLUSH(tty);

    if (L_ICANON(tty) ? tty->canon_data : !EMPTY(&tty->secondary))
	return 1;
    return 0;
}

static int read_chan(struct tty_struct * tty, WORD ds, long buf, int nr)
{
    int c;
    long b = buf;
    int minimum, time;
    int retval = 0;

    control_c = 0;

    if (L_ICANON(tty)) {
	minimum = time = 0;
	timeout = (unsigned long) -1;
    } else {
	time = tty->termios->c_cc[VTIME] * 10;
	minimum = tty->termios->c_cc[VMIN];
	if (minimum)
	    timeout = 0xffffffff;
	else {
	    if (time)
		timeout = (unsigned long) time / 55L + time_tic;
	    else
		timeout = 0;
	    time = 0;
	    minimum = 1;
	}
    }

    for (;;) {
	if (!input_available_p(tty)) {
	    if (!timeout)
		break;
	    if (npz->filp[0]->f_flags & FCNTL_NDELAY) {
		retval = -EMX_EAGAIN;
		break;
	    }
	    continue;
	}
	for (;;) {
	    int eol;

	    if (EMPTY(&tty->secondary))
		break;

	    eol = clear_bit(tty->secondary.tail, &tty->secondary_flags);
	    c = tty->secondary.buf[tty->secondary.tail];

	    if (!nr) {
		/* Gobble up an immediately following EOF if
		   there is no more room in buf (this can
		   happen if the user "pushes" some characters
		   using ^D).  This prevents the next read()
		   from falsely returning EOF. */
		if (eol) {
		    if (c == __DISABLED_CHAR) {
			tty->canon_data--;
			INC(tty->secondary.tail);
		    } else {
			set_bit(tty->secondary.tail,
				&tty->secondary_flags);
		    }
		}
		break;
	    }
	    INC(tty->secondary.tail);
	    if (eol) {
		if (--tty->canon_data < 0) {
		    printf("read_chan: canon_data < 0!\n");
		    tty->canon_data = 0;
		}
		if (c == __DISABLED_CHAR)
		    break;
		put_user_byte(ds, b, (char)c);
		b++;
		nr--;
		break;
	    }
	    put_user_byte(ds, b++, (char)c);
	    nr--;
	}
	if ((int) (long) (b - buf >= minimum) || !nr)
	    break;
	if (time)
	    timeout = time + time_tic;
    }
    timeout = 0;
    return (int) ((b - buf) ? b - buf : retval);
}

int termio_read(unsigned ds, unsigned long buf, int count)
{
    struct tty_struct *tty = &__tty;
    output = npz->filp[1]->f_doshandle;

    if (!tty)
	return -EMX_EIO;

    return read_chan(tty, ds, buf, count);
}


DWORD do_fionread(void)
{
    struct tty_struct *tty = &__tty;
    TTY_READ_FLUSH(tty);
    return (DWORD) (CHARS(&tty->secondary));
}

int kbd_ioctl(unsigned cmd, unsigned long termio_arg)
{
    struct tty_struct *tty = &__tty;

    switch (cmd) {
	case TCGETA:
	if (verify_illegal_write(npz, termio_arg, sizeof(struct termio)))
	    return -EMX_EINVAL;
	cpy16_32(npz->data32sel, termio_arg,
		 tty->termios, sizeof(struct termio));
	return 0;

    case TCSETAF:
	flush_input(&tty->secondary);
	/* fall through */
    case TCSETAW:
    case TCSETA:
	if (verify_illegal(npz, termio_arg, sizeof(struct termio)))
	    return -EMX_EINVAL;
	cpy32_16(npz->data32sel, termio_arg,
		 tty->termios, sizeof(struct termio));
	flush_input(&tty->secondary);
	if (!L_IDEFAULT)
	    npz->p_flags |= PF_TERMIO;	 /* enable termio */
	else
	    npz->p_flags &= ~PF_TERMIO;
	return 0;

    case TCFLSH:
	/* if (termio_arg == 0) */
	flush_input(&tty->secondary);
	return 0;

    case FIONREAD:
	if (verify_illegal_write(npz, termio_arg, sizeof(long)))
	    return -EMX_EINVAL;
	store32(npz->data32sel, termio_arg, do_fionread());
	return 0;

    case FGETHTYPE:
	if (verify_illegal_write(npz, termio_arg, sizeof(long)))
	    return -EMX_EINVAL;
	store32(npz->data32sel, termio_arg, (DWORD)HT_DEV_CON);
	return 0;

    default:
	return -EMX_EINVAL;
    }
}
