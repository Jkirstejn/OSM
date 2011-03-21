/*
 * TLB handling
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
 * $Id: tlb.c,v 1.6 2004/04/16 10:54:29 ttakanen Exp $
 *
 */

#include "kernel/panic.h"
#include "kernel/assert.h"
#include "vm/tlb.h"
#include "vm/pagetable.h"
#include "kernel/exception.h"
#include "kernel/thread.h"
//#include "kernel/thread.c"

static void print_tlb_debug(void)
{
   tlb_exception_state_t tes;
   _tlb_get_exception_state(&tes);

   kprintf("TLB exception. Details:\n"
           "Failed Virtual Address: 0x%8.8x\n"
           "Virtual Page Number:    0x%8.8x\n"
           "ASID (Thread number):   %d\n",
           tes.badvaddr, tes.badvpn2, tes.asid);
}

void tlb_modified_exception(void)
{
	print_tlb_debug();
    KERNEL_PANIC("TLB modified exception: Tried to write to an non-writeable address");
}

/**
 * Perform a lookup of the virtual page, that caused the exception,
 * in the thread's pagetable. If found, the entry is written to the
 * TLB and 0 is return. Otherwise -1 is returned.
 **/
int tlb_lookup_pagetable(void) {
	tlb_exception_state_t tes;
	_tlb_get_exception_state(&tes);
	thread_table_t *thread = thread_get_thread_entry(tes.asid);
	
	int i;
	for(i = 0; i < PAGETABLE_ENTRIES; i++) {
		if (thread->pagetable->entries[i].VPN2 == tes.badvpn2) {
			/* Virtual address page found in the thread's pagetable
			 * Write entry to TLB and return 0
			 */
			_tlb_write_random(&thread->pagetable->entries[i]);
			return 0;
		}
	}
	return -1;
}

/**
 * TLB load exception.
 * Perform a tlb_lookup_pagetable and kernel panic if not successful
 **/
void tlb_load_exception(void)
{
	if (!tlb_lookup_pagetable()) {
		print_tlb_debug();
		KERNEL_PANIC("TLB load exception: Address to non-allocated space");
	}
}

/**
 * TLB load exception.
 * Perform a tlb_lookup_pagetable and kernel panic if not successful
 **/
void tlb_store_exception(void)
{
	if (!tlb_lookup_pagetable()) {
		print_tlb_debug();
		KERNEL_PANIC("Unhandled TLB store exception");
	}
}

/**
 * Fill TLB with given pagetable. This function is used to set memory
 * mappings in CP0's TLB before we have a proper TLB handling system.
 * This approach limits the maximum mapping size to 128kB.
 *
 * @param pagetable Mappings to write to TLB.
 *
 */

void tlb_fill(pagetable_t *pagetable)
{
    if(pagetable == NULL)
	return;

    /* Check that the pagetable can fit into TLB. This is needed until
     we have proper VM system, because the whole pagetable must fit
     into TLB. */
    KERNEL_ASSERT(pagetable->valid_count <= (_tlb_get_maxindex()+1));

    _tlb_write(pagetable->entries, 0, pagetable->valid_count);

    /* Set ASID field in Co-Processor 0 to match thread ID so that
       only entries with the ASID of the current thread will match in
       the TLB hardware. */
    _tlb_set_asid(pagetable->ASID);
}
