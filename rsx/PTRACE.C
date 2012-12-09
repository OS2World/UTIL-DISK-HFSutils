/*****************************************************************************
 * FILE: ptrace.c							     *
 *									     *
 * DESC:								     *
 *	- ptrace handler						     *
 *									     *
 * Copyright (C) 1993-1996                                                   *
 *	Rainer Schnitker, Heeper Str. 283, 33607 Bielefeld		     *
 *	email: rainer@mathematik.uni-bielefeld.de			     *
 *									     *
 *****************************************************************************/

#include "DPMI.H"
#include "DPMI10.H"
#include "PROCESS.H"
#include "CDOSX32.H"
#include "START32.H"
#include "ADOSX32.H"
#include "COPY32.H"
#include "PTRACE.H"
#include "DOSERRNO.H"
#include "KDEB.H"

#define FLAG_MASK 0x00000dd9L
#define SINGLE_STEP 0x100

int do_ptrace(int request, int child_id, DWORD addr, DWORD data, DWORD * ret)
{
    NEWPROCESS *child;

#ifdef CONFIG_KDEB
    if (child_id == 1)
	return KDEB_ptrace(request, addr, data, ret);
#endif

    if (!(child = find_process(child_id)))
	return EMX_ESRCH;

    if (child->pptr != npz || !(child->p_flags & PF_DEBUG))
	return EMX_ESRCH;

    if (child->p_status != PS_STOP)
	return EMX_ESRCH;

    *ret = 0;

    switch (request) {
    case PTRACE_TRACEME:
    case PTRACE_SESSION:
	return 0;

    case PTRACE_PEEKTEXT:
    case PTRACE_PEEKDATA:
	if (verify_illegal(child, addr, 4))
	    return EMX_EIO;
	*ret = read32(child->data32sel, addr);
	return 0;

    case PTRACE_POKETEXT:
    case PTRACE_POKEDATA:
	if (verify_illegal(child, addr, 4))
	    return EMX_EIO;
	if (dpmi10) {
	    WORD page, pageorg;

	    read32(child->data32sel, addr);	/* page in */
	    if (GetPageAttributes(child->memhandle, addr & ~0xFFFL, 1, &pageorg))
		return EMX_EIO;
	    pageorg &= ~16;	/* don't modify access/dirty */
	    page = pageorg | 8; /* read/write access */
	    if (ModifyPageAttributes(child->memhandle, addr & ~0xFFFL, 1, &page))
		return EMX_EIO;
	    store32(child->data32sel, addr, data);
	    ModifyPageAttributes(child->memhandle, addr & ~0xFFFL, 1, &pageorg);
	} else
	    store32(child->data32sel, addr, data);
	*ret = data;
	return 0;

    case PTRACE_EXIT:
	/* to do : switch to child -> do_signal(); */
	child->p_flags |= PF_WAIT_WAIT;
	return 0;

    case PTRACE_PEEKUSER:
	if (addr == 0x30) {	/* u_ar0  */
	    *ret = 0xE0000000 + ((DWORD) (UINT) & (child->regs));
	    return 0;
	} else {		/* peek regs */
	    DWORD *peekat;
	    peekat = (DWORD *) (UINT) (addr);

	    if (peekat < &(child->regs.gs) || peekat > &(child->regs.ss))
		return EMX_EIO;

	    *ret = *peekat;
	    return 0;
	}

    case PTRACE_POKEUSER:
	{
	    /* poke regs */
	    DWORD *pokeat;

	    pokeat = (DWORD *) (UINT) addr;

	    if (pokeat < &(child->regs.gs) || pokeat > &(child->regs.ss))
		return EMX_EIO;

	    /* change data for critical regs */
	    if (pokeat == &(child->regs.eflags)) {
		data &= FLAG_MASK;
		data |= *pokeat & ~FLAG_MASK;
	    } else if (pokeat <= &(child->regs.ds)
		       || pokeat == &(child->regs.cs))
		data = *pokeat;
	    else if (pokeat == &(child->regs.esp)) {
		if (verify_illegal(child, data, 4))
		    return EMX_EIO;
		child->regs.esp = data;
		child->regs.esporg = data + 12L;
	    } else if (pokeat == &(child->regs.esporg)) {
		if (verify_illegal(child, data, 4))
		    return EMX_EIO;
		child->regs.esporg = data;
		child->regs.esp = data - 12L;
	    } else if (pokeat == &(child->regs.eip))
		if (verify_illegal(child, data, 4))
		    return EMX_EIO;

	    *pokeat = data;
	    *ret = data;

	    return 0;
	}

    case PTRACE_STEP:
	if ((int)data > 0 && (int)data <= MAX_SIGNALS)
	    send_signal(child, (WORD) data);
	child->regs.eflags |= SINGLE_STEP;
	if (child->regs.esp == child->regs.esporg)
	    child->regs.esp -= 12;

	npz->p_status = PS_STOP;
	switch_context(child);
	npz->p_status = PS_RUN;
	return 0;

    case PTRACE_RESUME:
	if ((int)data > 0 && (int)data <= MAX_SIGNALS)
	    send_signal(child, (int) data);
	if (child->regs.esp == child->regs.esporg)
	    child->regs.esp -= 12;

	npz->p_status = PS_STOP;
	switch_context(child);
	npz->p_status = PS_RUN;
	return 0;

    default:
	return EMX_EIO;
    }
}
