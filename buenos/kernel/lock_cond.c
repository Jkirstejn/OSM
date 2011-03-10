#include "kernel/lock_cond.h"
#include "kernel/thread.h"
#include "kernel/sleepq.h"
#include "kernel/interrupt.h"
#include "kernel/spinlock.h"

/*
 * Reset an already allocated lock structure if possible
 * Returns 0 if successful and -1 otherwise
 */
int lock_reset(lock_t *lock) {

	interrupt_status_t intr_status;
	intr_status = _interrupt_disable();
	
	/* Assume error */
	int rtn = -1;
	
	/* Reset and acquire spinlock for the lock state */
	spinlock_reset(&lock->spinlock);
	
	/* If allocated */
	if (lock != NULL) {
		lock->state = LOCK_OPEN;
		rtn = 0;
	}
	
	_interrupt_set_state(intr_status);
	return rtn;
}

/*
 * Lock the given lock
 * uses waiting queues
 */
void lock_acquire(lock_t *lock) {

	interrupt_status_t intr_status;
	intr_status = _interrupt_disable();

	spinlock_acquire(&lock->spinlock);		// Acquire spinlock
	
	/* Check if lock is locked already */
	while (lock->state == LOCK_LOCKED) {
	
		/* Add thread to sleep queue and switch thread */
		sleepq_add(lock);
		
		spinlock_release(&lock->spinlock);	// Release spinlock
		
		thread_switch();
		spinlock_acquire(&lock->spinlock);	// Acquire spinlock
	}
	
	/* Lock open. Acquire it! */
	lock->state = LOCK_LOCKED;

	spinlock_release(&lock->spinlock);	// Release spinlock
	
	_interrupt_set_state(intr_status);
}

/*
 * Unlock the given lock
 */
void lock_release(lock_t *lock) {

	interrupt_status_t intr_status;
	intr_status = _interrupt_disable();
	
	spinlock_acquire(&lock->spinlock);	// Acquire spinlock
	
	/* Check if lock was in fact locked */
	if (lock->state == LOCK_LOCKED) {
		/* Open lock and wake up the process waiting for the lock */
		lock->state = LOCK_OPEN;
		
		sleepq_wake(lock);
	}
	spinlock_release(&lock->spinlock);
	_interrupt_set_state(intr_status);
}

/*
 * Reset condition
 * Used when creating a condition.
 * Assumes that space for the structure has been allocated
 */
int condition_reset(cond_t *cond) {

	interrupt_status_t intr_status;
	intr_status = _interrupt_disable();
	/* Assume error */
	int rtn = -1;
	
	/* Check if the condition points to some allocated space */
	if (cond != NULL) {
		cond->c = '\0';
		rtn = 0;
	}
	_interrupt_set_state(intr_status);
	return rtn;
}

/*
 * Wait for a condition
 * Sleeps threads and waits for a signal from the given condition
 */
void condition_wait(cond_t *cond, lock_t *condition_lock) {

	interrupt_status_t intr_status;
	intr_status = _interrupt_disable();
	
	sleepq_add(cond);				// Wait for a signal from cond
	lock_release(condition_lock);	// Release the condition lock
	
	_interrupt_set_state(intr_status);
	thread_switch();				// Sleep thread
	
	lock_acquire(condition_lock);
}

/*
 * Signal next thread waiting for the given condition
 */
void condition_signal(cond_t *cond, lock_t *condition_lock) {
	condition_lock = condition_lock;

	sleepq_wake(cond);				// Wake up next waiting thread

}

/*
 * Signal all threads waiting for the given condition
 */
void condition_broadcast(cond_t *cond, lock_t *condition_lock) {
	condition_lock = condition_lock;

	sleepq_wake_all(cond);			// Wake up all waiting threads
	
}
