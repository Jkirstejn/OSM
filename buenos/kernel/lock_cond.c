#include "kernel/lock_cond.h"
#include "kernel/thread.h"
#include "kernel/sleepq.h"

/*
 * Reset an already allocated lock structure if possible
 * Returns 0 if successful and -1 otherwise
 */
int lock_reset(lock_t *lock) {
	/* Assume error */
	int rtn = -1;
	
	/* If allocated */
	if (lock != NULL) {
		lock->state = LOCK_OPEN;
		rtn = 0;
	}
	return rtn;
}

/*
 * Lock the given lock
 * uses waiting queues
 */
void lock_acquire(lock_t *lock) {

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
		
		sleepq_wake(lock);
	}
	
}

int condition_reset(cond_t *cond) {
	sleepq_wake_all(cond);
	cond->c = '\0';
	return 0;
}

void condition_wait(cond_t *cond, lock_t *condition_lock) {
	sleepq_add(cond); // Wait for a signal from cond
	lock_release(condition_lock); // Release the condition lock
	thread_switch(); // Sleep thread
}

void condition_signal(cond_t *cond, lock_t *condition_lock) {
	lock_acquire(condition_lock);
	sleepq_wake(cond);
}

void condition_broadcast(cond_t *cond, lock_t *condition_lock) {
	lock_acquire(condition_lock);
	sleepq_wake_all(cond);
}
