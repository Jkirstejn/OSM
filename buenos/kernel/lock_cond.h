/*
 * Condition Locking system
 *
 * Copyright (C) 2011 Malte Stær Nissen, Jacob Daniel Kistejn Hansen
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
 */
 
#ifndef BUENOS_KERNEL_CONDITION_LOCK_H
#define BUENOS_KERNEL_CONDITION_LOCK_H

#include "kernel/spinlock.h"

typedef enum {
	LOCK_OPEN,
	LOCK_LOCKED
} lock_state_t;

typedef struct {
	lock_state_t state;
	spinlock_t spinlock;
} lock_t;

typedef struct {
	char c;
} cond_t;

typedef cond_t usr_cond_t;
typedef lock_t usr_lock_t;

int lock_reset(lock_t *);

void lock_acquire(lock_t *);

void lock_release(lock_t *);

int condition_reset(cond_t *);

void condition_wait(cond_t *, lock_t *);

void condition_signal(cond_t *, lock_t *);

void condition_broadcast(cond_t *, lock_t *);
#endif

