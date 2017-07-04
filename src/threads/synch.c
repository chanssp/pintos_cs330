/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */

/*---------checkp sema down--------------*/
//---------------------------------
// 세마포어의 waiters 리스트에 넣을때 priority가 높은 순으로sorting되게 한다.

void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
  {
    list_push_back (&sema->waiters, &thread_current ()->elem);
    //list_insert_ordered(&sema->waiters, &thread_current()->elem, &compare_priority_func, NULL);
    //이줄만 추가
    thread_block ();
  }

  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */

/*--------check1 sema up--------*/

void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;
  struct thread* temp;
  struct thread* cur;
  cur = thread_current();
  temp = thread_current();

  ASSERT (sema != NULL);

  old_level = intr_disable ();
 
  if (!list_empty (&sema->waiters)) {
    // check donate sema 에서 필요함. waiters 에 집어 넣을 때 순서대로 집어 넣었지만
    // 그 후에 리스트 안에서 priority 가 바뀌는 경우가 있기 때문에 꺼내기 전에 대시 재정렬.
    list_sort(&sema->waiters, &compare_priority_func, NULL);
    temp = list_entry (list_pop_front (&sema->waiters), struct thread, elem);
    thread_unblock (temp);
    //thread_unblock(list_entry (list_pop_front (&sema->waiters), struct thread, elem));
  }
  sema->value++;
  intr_set_level (old_level);

  if(cur->priority < temp->priority) 
    thread_yield();  // 인터럽트가 켜진 후 priority 높은 것에게 CPU양도
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);  // 여기서 세마를 1로 세팅
  lock->max_val=-1;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

/*------check lock acquire------*/

void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  
  struct thread * cur = thread_current(); // 현재 thread
  cur->wait_lock = lock; 

  /* 만약 lock 의 holder 가 없다면 이 부분은 skip 한다. */
  if(lock->holder != NULL) {
    donate_priority(lock);
  }

  sema_down (&lock->semaphore);
  cur->wait_lock = NULL;
  list_push_back(&cur->holding_locks, &lock->list_elem);   // 이 thread 가 own 하고 있는 lock 리스트에 추가.
  lock->holder = cur;
  
}
//static int count = 0;
/*------check donate priority------*/
void 
donate_priority(struct lock * lock)
{
  struct thread *cur = thread_current();
  struct thread *owner = lock->holder;
  
  if(lock->max_val < cur->priority)  // lock 을 요청한 thread 중 priority 가
    lock->max_val = cur->priority;   // 가장 높은 값으로 lock->max_val 을 세팅해줌

  /* 만약 lock->holder->priority 가 현재 lock 을 요청하는 thread->priority 보다 낮은경우 */
  if (owner->priority < cur->priority) {
    if(owner->init_priority==-1){ // 아직 donation을 한번도 안받은 경우
      owner->init_priority = owner->priority;
    }
    owner->priority = cur->priority;  // "priority donation" 이 일어난다 
  }

  if(owner->wait_lock != NULL){   // nest
    donate_priority(owner->wait_lock);
  }
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */

bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));
  

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */

/* ------check lock release------*/
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  list_remove(&lock->list_elem);  // thread 가 own 하고있던 lock 의 리스트에서 제거.
  
  priority_back(lock);
  //lock->holder = NULL;
  sema_up (&lock->semaphore);
  
  lock->holder = NULL;
}

/*------check priority back------*/
//donate가 일어낫을때만 이함수가 실행되도록 설정해준다.
void
priority_back(struct lock * lock)
{
  struct thread * owner = lock->holder;
  //struct list * holding_locks = owner->holding_locks;
  int max_lock;
  //struct list_elem * e;

  if(owner->init_priority == -1)   // donation 이 아예 일어나지 않은 경우
    return;

  else{
    /* 아직 hold하고 있는 다른 lock 이 쌓여 있는 경우 */
    if(!list_empty(&owner->holding_locks)){
      // 현재 owner 의 holding_locks 에서 가장 priority 가 큰 lock 을 찾는 과정
      /*
      max_lock = list_entry(list_begin(&owner->holding_locks), struct lock, list_elem)->max_val;
      for(e = list_begin(&owner->holding_locks); e != list_end(&owner->holding_locks); e = list_next(e)) {
        if (max_lock < list_entry(e, struct lock, list_elem)->max_val)
          max_lock = list_entry(e, struct lock, list_elem)->max_val;
      }
      */
      max_lock = list_entry(list_max(&owner->holding_locks, &compare_priority_func, NULL), 
                                      struct lock, list_elem)->max_val;
      owner->priority = max_lock;
    }
    /* 가지고 있던 모든 lock 을 realease 한 상태 */
    else{
      owner->priority = owner->init_priority;
      owner->init_priority = -1;  // 다시 예전 priority 로 돌아감
    }
  }
}



/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/*-----check-----*/
/*---user defined function---*/
bool
compare_sema_func(struct list_elem *a, struct list_elem *b, void *aux UNUSED)
{
  struct semaphore_elem *c = list_entry(a, struct semaphore_elem, elem);
  struct semaphore_elem *d = list_entry(b, struct semaphore_elem, elem);

  if (list_size(&c->semaphore.waiters) <= 0)
    return false;
  else if (list_size(&d->semaphore.waiters) <= 0)
    return true;

  struct thread *thread_a = list_entry(list_front(&c->semaphore.waiters), struct thread, elem);
  struct thread *thread_b = list_entry(list_front(&d->semaphore.waiters), struct thread, elem);

  return thread_a->priority > thread_b->priority;
}



/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  list_insert_ordered(&cond->waiters, &waiter.elem, &compare_sema_func, NULL);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    //check
    list_sort(&cond->waiters, &compare_sema_func, NULL);
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
