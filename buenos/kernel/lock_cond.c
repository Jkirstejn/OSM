#include "lock.h"
#include "kernel/thread.h"
#include "kernel/sleepq.h"

int lock_reset(lock_t *lock) {

	int rtn = -1;
	
	lock->state = LOCK_OPEN;
	
	if (lock->state == LOCK_OPEN) {
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
	/* Lock opened */
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


int condition_reset(cond_t *cond);

void condition_wait(cont_t *cond, lock_t *condition_lock);

void condition_signal(cont_t *cond, lock_t *condition_lock);

void condition_broadcast(cond_t *cond, lock_t *condition_lock);
