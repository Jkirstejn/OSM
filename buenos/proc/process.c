/*
 * Process startup.
 *
 * Copyright (C) 2003-2005 Juha Aatrokoski, Timo Lilja,
 *       Leena Salmela, Teemu Takanen, Aleksi Virtanen.
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
 * $Id: process.c,v 1.11 2007/03/07 18:12:00 ttakanen Exp $
 *
 */
 
#include "proc/process.h"
#include "proc/elf.h"
#include "kernel/thread.h"
#include "kernel/assert.h"
#include "kernel/interrupt.h"
#include "kernel/spinlock.h"
#include "kernel/config.h"
#include "kernel/sleepq.h"
#include "fs/vfs.h"
#include "drivers/yams.h"
#include "vm/vm.h"
#include "vm/pagepool.h"
#include "lib/libc.h"

/* Spinlock which must be held when manipulating the process table */
spinlock_t pspinlock;

/* Process table containing all processes in the system */
process_table_t ptable[CONFIG_MAX_PROCESSES];

/** @name Process startup
 *
 * This module contains a function to start a userland process.
 */

/**
 * Starts one userland process. The thread calling this function will
 * be used to run the process and will therefore never return from
 * this function. This function asserts that no errors occur in
 * process startup (the executable file exists and is a valid ecoff
 * file, enough memory is available, file operations succeed...).
 * Therefore this function is not suitable to allow startup of
 * arbitrary processes.
 *
 * @executable The name of the executable to be run in the userland
 * process
 */
void process_start(const char *executable)
{
    thread_table_t *my_entry;
    pagetable_t *pagetable;
    uint32_t phys_page;
    context_t user_context;
    uint32_t stack_bottom;
    elf_info_t elf;
    openfile_t file;

    int i;

    interrupt_status_t intr_status;

    my_entry = thread_get_current_thread_entry();

    /* If the pagetable of this thread is not NULL, we are trying to
       run a userland process for a second time in the same thread.
       This is not possible. */
    KERNEL_ASSERT(my_entry->pagetable == NULL);

    pagetable = vm_create_pagetable(thread_get_current_thread());
    KERNEL_ASSERT(pagetable != NULL);

    intr_status = _interrupt_disable();
    my_entry->pagetable = pagetable;
    _interrupt_set_state(intr_status);

    file = vfs_open((char *)executable);
    /* Make sure the file existed and was a valid ELF file */
    KERNEL_ASSERT(file >= 0);
    KERNEL_ASSERT(elf_parse_header(&elf, file));

    /* Trivial and naive sanity check for entry point: */
    KERNEL_ASSERT(elf.entry_point >= PAGE_SIZE);

    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    KERNEL_ASSERT(elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE
		  <= _tlb_get_maxindex() + 1);

    /* Allocate and map stack */
    for(i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE, 1);
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.ro_vaddr + i*PAGE_SIZE, 1);
    }

    for(i = 0; i < (int)elf.rw_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.rw_vaddr + i*PAGE_SIZE, 1);
    }

    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);
    
    /* Now we may use the virtual addresses of the segments. */

    /* Zero the pages. */
    memoryset((void *)elf.ro_vaddr, 0, elf.ro_pages*PAGE_SIZE);
    memoryset((void *)elf.rw_vaddr, 0, elf.rw_pages*PAGE_SIZE);

    stack_bottom = (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - 
        (CONFIG_USERLAND_STACK_SIZE-1)*PAGE_SIZE;
    memoryset((void *)stack_bottom, 0, CONFIG_USERLAND_STACK_SIZE*PAGE_SIZE);

    /* Copy segments */

    if (elf.ro_size > 0) {
	/* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.ro_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.ro_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.ro_vaddr, elf.ro_size)
		      == (int)elf.ro_size);
    }

    if (elf.rw_size > 0) {
	/* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.rw_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.rw_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.rw_vaddr, elf.rw_size)
		      == (int)elf.rw_size);
    }


    /* Set the dirty bit to zero (read-only) on read-only pages. */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        vm_set_dirty(my_entry->pagetable, elf.ro_vaddr + i*PAGE_SIZE, 0);
    }

    /* Insert page mappings again to TLB to take read-only bits into use */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof(user_context));
    user_context.cpu_regs[MIPS_REGISTER_SP] = USERLAND_STACK_TOP;
    user_context.pc = elf.entry_point;

    thread_goto_userland(&user_context);

    KERNEL_PANIC("thread_goto_userland failed.");
}
/* Run process in new thread, returns PID of new process. */
process_id_t process_spawn(const char *executable) {
	int retval = -1;	// Assume that something goes wrong

	/* Find first free process slot */
	int pid = 0;
	while(ptable[pid].state != PROCESS_FREE && pid < CONFIG_MAX_PROCESSES) {
		++pid;
	}

	/* Check if there was a free process slot and if succeeded in creating a new thread */
	if (pid < CONFIG_MAX_PROCESSES) {
	
		/* Disable interrupts and acquire spinlock */
		interrupt_status_t intr_status;
		intr_status = _interrupt_disable();
		
		spinlock_acquire(&pspinlock);
		
		/* Create new thread
		 * if successful the process is linked to the thread
		 * and the thread is run */
		TID_t tid = thread_create((void *)process_start, (int)executable);
		if (tid != -1) {
			(thread_get_thread_entry(tid))->process_id = pid;
			ptable[pid].name = executable;
			ptable[pid].state = PROCESS_ALIVE;
			retval = pid;
			thread_run(tid);
		}
		
		/* Release spinlock and restore interrupt mask */
		spinlock_release(&pspinlock);
		_interrupt_set_state(intr_status);
	}
	return retval; // Return pid if successful or -1 otherwise
}

/* Run process in this thread, only returns if there is an error. */
int process_run(const char *executable) {

	/* Find first free process slot */
	int pid = 0;
	while(ptable[pid].state != PROCESS_FREE && pid < CONFIG_MAX_PROCESSES) {
		++pid;
	}
	
	/* Check if there is room for a new process */
	if (pid < CONFIG_MAX_PROCESSES) {
	
		/* Disable interrupts and acquire spinlock */
		interrupt_status_t intr_status;
		intr_status = _interrupt_disable();
		
		spinlock_acquire(&pspinlock);
		
		/* link process with current thread and insert process in table */
		(thread_get_current_thread_entry())->process_id = pid;
		ptable[pid].name = executable;
		ptable[pid].state = PROCESS_ALIVE;
		
		/* Release spinlock and restore interrupt mask */
		spinlock_release(&pspinlock);
		_interrupt_set_state(intr_status);
		
		/* Run process */
		process_start(executable);
	}
	return -1; // Fail if this is reached
}

/* Get the process id for current process */
process_id_t process_get_current_process(void) {
	return (*thread_get_current_thread_entry()).process_id;
}

/* Set a process table entry to free
 * Caller is responsible for handling interrupts etc. correctly */
void process_free_entity(int pid) {
	ptable[pid].name = NULL;
	ptable[pid].state = PROCESS_FREE;
	ptable[pid].return_value = -1;
}

/* Stop the current process and the kernel thread in which it runs. */
void process_finish(int retval) {
	process_id_t pid;
	pid = process_get_current_process();

	/* Disable interrupts and acquire spinlock */
	interrupt_status_t intr_status;
	intr_status = _interrupt_disable();
	
	spinlock_acquire(&pspinlock);
	
	/* Mark process as dead and save the return value */
	ptable[pid].state = PROCESS_DEAD;
	ptable[pid].return_value = retval;
	
	/* Ask the sleep queue to wake up a process if there is one who waits */
	sleepq_wake(&ptable[pid]);
	
	/* Release spinlock and restore interrupt mask */
	spinlock_release(&pspinlock);
	_interrupt_set_state(intr_status);
	
	/* Stop the current kernel thread in which the processs runs */
	thread_table_t* current_thread = thread_get_current_thread_entry();
	vm_destroy_pagetable(current_thread->pagetable);
	current_thread->pagetable = NULL;
	thread_finish();
}

/* Wait for the given process to terminate, returning its return value, and making the process table entry as free. */
uint32_t process_join(process_id_t pid) {
	int retval = -1;
	if (ptable[pid].state != PROCESS_FREE) {
		// Add this process to the waiting list waiting for the pid process
		/* Disable interrupts and acquire spinlock */
		interrupt_status_t intr_status;
		intr_status = _interrupt_disable();
		
		spinlock_acquire(&pspinlock);
	
		/* Sleep while the given process is alive */
		while(ptable[pid].state == PROCESS_ALIVE) {
		
			/* Add this process to the sleep queue
			 * release spinlock, sleep the thread and acquire spinlock again when awoken */
			sleepq_add(&ptable[pid]);	
			spinlock_release(&pspinlock);
			
			thread_switch();
			
			spinlock_acquire(&pspinlock);
		
		}
		
		/* Process awoken; return values, release spinlock and restore interrupts */
		retval = ptable[pid].return_value;
		process_free_entity(pid);
		
		spinlock_release(&pspinlock);
		
		_interrupt_set_state(intr_status);
	}
	return retval;
}


/* Initialize process table. Should be called before any orher process related calls. */
void process_init(void) {
	/* Disable interrupts and acquire spinlock */
	interrupt_status_t intr_status;
	intr_status = _interrupt_disable();
		
	spinlock_acquire(&pspinlock);
	
	int x;
	for(x = 0; x < CONFIG_MAX_PROCESSES; x++) {
		process_free_entity(x);
	}
	/* Release spinlock and restore interrupt mask */
	spinlock_release(&pspinlock);
		
	_interrupt_set_state(intr_status);
}
/** @} */
