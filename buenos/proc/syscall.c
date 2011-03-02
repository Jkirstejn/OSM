/*
 * System calls.
 *
 * Copyright (C) 2003 Juha Aatrokoski, Timo Lilja,
 *   Leena Salmela, Teemu Takanen, Aleksi Virtanen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: syscall.c,v 1.3 2004/01/13 11:10:05 ttakanen Exp $
 *
 */
#include "kernel/cswitch.h"
#include "proc/syscall.h"
#include "kernel/halt.h"
#include "kernel/panic.h"
#include "lib/libc.h"
#include "kernel/assert.h"
#include "proc/process.h"
#include "drivers/device.h"
#include "drivers/gcd.h"
#include "kernel/lock_cond.h"

/* Write length bytes from buffer to std.out */
int syscall_write(int filehandle, const void *buffer, int length) {
	int retval = -1;
	if (filehandle == FILEHANDLE_STDOUT) {
		
		device_t *dev;
		gcd_t *gcd;
		
		dev = device_get(YAMS_TYPECODE_TTY, 0);
 		KERNEL_ASSERT(dev != NULL);

    	gcd = (gcd_t *)dev->generic_device;
    	KERNEL_ASSERT(gcd != NULL);
        retval = gcd->write(gcd, buffer, length);
	}
	return retval;
}

/* Read length bytes from std.in to buffer */
int syscall_read(int filehandle, void *buffer, int length) {
	int retval = -1;
	if (filehandle == FILEHANDLE_STDIN) {
	
		device_t *dev;
		gcd_t *gcd;
		
		dev = device_get(YAMS_TYPECODE_TTY, 0);
		KERNEL_ASSERT(dev != NULL);
		
		gcd = (gcd_t *)dev->generic_device;
		KERNEL_ASSERT(gcd != NULL);
		
		retval = gcd->read(gcd, buffer, length);
	}
	return retval;
}

int syscall_fork(void (*func)(int), int arg) {
    return process_fork(func, arg);
}


int syscall_lock_create(usr_lock_t *lock) {
	return lock_reset(lock);
}

void syscall_lock_acquire(usr_lock_t *lock) {
	lock_acquire(lock);
}

void syscall_lock_release(usr_lock_t *lock) {
	lock_release(lock);
}

int syscall_condition_create(usr_cond_t *cond) {
	return condition_reset(cond);
}

void syscall_condition_wait(usr_cond_t *cond, usr_lock_t *lock) {
	condition_wait(cond, lock);
}

void syscall_condition_signal(usr_cond_t *cond, usr_lock_t *lock) {
	condition_signal(cond, lock);
}

void syscall_condition_broadcast(usr_cond_t *cond, usr_lock_t *lock) {
	condition_broadcast(cond, lock);
}

/**
 * Handle system calls. Interrupts are enabled when this function is
 * called.
 *
 * @param user_context The userland context (CPU registers as they
 * where when system call instruction was called in userland)
 */
void syscall_handle(context_t *user_context)
{
    /* When a syscall is executed in userland, register a0 contains
     * the number of the syscall. Registers a1, a2 and a3 contain the
     * arguments of the syscall. The userland code expects that after
     * returning from the syscall instruction the return value of the
     * syscall is found in register v0. Before entering this function
     * the userland context has been saved to user_context and after
     * returning from this function the userland context will be
     * restored from user_context.
     */
    switch(user_context->cpu_regs[MIPS_REGISTER_A0]) {
    case SYSCALL_HALT:
        halt_kernel();
        break;
    case SYSCALL_READ:
    	user_context->cpu_regs[MIPS_REGISTER_V0] = syscall_read(user_context->cpu_regs[MIPS_REGISTER_A1], (void *)user_context->cpu_regs[MIPS_REGISTER_A2],user_context->cpu_regs[MIPS_REGISTER_A3]);
    	break;
    case SYSCALL_WRITE:
    	user_context->cpu_regs[MIPS_REGISTER_V0] = syscall_write(user_context->cpu_regs[MIPS_REGISTER_A1], (const void *)user_context->cpu_regs[MIPS_REGISTER_A2],user_context->cpu_regs[MIPS_REGISTER_A3]);
    	break;
	case SYSCALL_EXIT:
		process_finish(user_context->cpu_regs[MIPS_REGISTER_A1]);
    	break;
	case SYSCALL_EXEC:
		user_context->cpu_regs[MIPS_REGISTER_V0] = process_spawn((const char *)user_context->cpu_regs[MIPS_REGISTER_A1]);
    	break;
	case SYSCALL_JOIN:
		user_context->cpu_regs[MIPS_REGISTER_V0] = process_join(user_context->cpu_regs[MIPS_REGISTER_A1]);
    	break;
    case SYSCALL_FORK:
			user_context->cpu_regs[MIPS_REGISTER_V0] =
				syscall_fork((void (*)(int))user_context->cpu_regs[MIPS_REGISTER_A1],
					user_context->cpu_regs[MIPS_REGISTER_A2]);
	    break;
	case SYSCALL_LOCK_CREATE:
			user_context->cpu_regs[MIPS_REGISTER_V0] = syscall_lock_create((usr_lock_t *)user_context->cpu_regs[MIPS_REGISTER_A1]);
		break;
	case SYSCALL_LOCK_ACQUIRE:
			syscall_lock_acquire((usr_lock_t *)user_context->cpu_regs[MIPS_REGISTER_A1]);
		break;
	case SYSCALL_LOCK_RELEASE:
			syscall_lock_release((usr_lock_t *)user_context->cpu_regs[MIPS_REGISTER_A1]);
		break;
	case SYSCALL_CONDITION_CREATE:
		user_context->cpu_regs[MIPS_REGISTER_V0] = syscall_condition_create((usr_cond_t *)user_context->cpu_regs[MIPS_REGISTER_A1]);
		break;
	case SYSCALL_CONDITION_WAIT:
		syscall_condition_wait((usr_cond_t *)user_context->cpu_regs[MIPS_REGISTER_A1], (usr_lock_t *)user_context->cpu_regs[MIPS_REGISTER_A2]);
		break;
	case SYSCALL_CONDITION_SIGNAL:
		syscall_condition_signal((usr_cond_t *)user_context->cpu_regs[MIPS_REGISTER_A1], (usr_lock_t *)user_context->cpu_regs[MIPS_REGISTER_A2]);
		break;
	case SYSCALL_CONDITION_BROADCAST:
		syscall_condition_broadcast((usr_cond_t *)user_context->cpu_regs[MIPS_REGISTER_A1], (usr_lock_t *)user_context->cpu_regs[MIPS_REGISTER_A2]);
		break;
    default: 
        KERNEL_PANIC("Unhandled system call\n");
    }

    /* Move to next instruction after system call */
    user_context->pc += 4;
}
