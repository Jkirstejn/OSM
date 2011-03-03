#include "kernel/lock_cond.h"
#include "kernel/thread.h"
#include "kernel/sleepq.h"
#include "kernel/interrupt.h"

interrupt_status_t intr_status;
interrupt_status_t intr_status2;
/*
 * Reset an already allocated lock structure if possible
 * Returns 0 if successful and -1 otherwise
 */
int lock_reset(lock_t *lock) {

	intr_status = _interrupt_disable();
	
	/* Assume error */
	int rtn = -1;
	
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
	intr_status = _interrupt_disable();
	/* Check if lock is locked already */
	while (lock->state == LOCK_LOCKED) {
	
		/* Add thread to sleep queue and switch thread */
		sleepq_add(lock);
		thread_switch();
	}
	
	/* Lock open. Acquire it! */
	lock->state = LOCK_LOCKED;
}


/*
 * Unlock the given lock
 */
void lock_release(lock_t *lock) {
	/* Check if lock was in fact locked */
	if (lock->state == LOCK_LOCKED) {
		/* Open lock and wake up the process waiting for the lock */
		lock->state = LOCK_OPEN;
		_interrupt_set_state(intr_status);
		sleepq_wake(lock);
	}
}

int condition_reset(cond_t *cond) {
	intr_status2 = _interrupt_disable();
	/* Assume error */
	int rtn = -1;
	
	if (cond != NULL) {	// Pointer to some allocated space
		cond->c = '\0';
		rtn = 0;
	}
	_interrupt_set_state(intr_status2);
	return rtn;
}

void condition_wait(cond_t *cond, lock_t *condition_lock) {
	intr_status2 = _interrupt_disable();
	sleepq_add(cond); // Wait for a signal from cond
	lock_release(condition_lock); // Release the condition lock
	
	_interrupt_set_state(intr_status2);
	thread_switch(); // Sleep thread
}

void condition_signal(cond_t *cond, lock_t *condition_lock) {
	intr_status2 = _interrupt_disable();
	lock_acquire(condition_lock);
	sleepq_wake(cond);
}

void condition_broadcast(cond_t *cond, lock_t *condition_lock) {
	intr_status2 = _interrupt_disable();
	lock_acquire(condition_lock);
	sleepq_wake_all(cond);
}
