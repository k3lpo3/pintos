Project 1 Threads Design Doc
Name: Meng Chi Lai
Date: 2026-03-02

Alarm Clock

DATA STRUCTURES
**A1: Copy here the declaration of each new or changed struct or struct member, global or
static variable, typedef, or enumeration. Identify the purpose of each in 25 words or less.**
#In [/home/meng_chi/CSE 150/Project 1/pintos/src/threads/thread.h]

/* Alarm clock. */
int64_t wakeup_tick; /* Tick when this thread should wake up. */
struct list_elem sleep_elem; /* For putting this thread in the
sleep_list. */

Purpose: Each sleeping thread stores its wake time and can be linked into a global sleep list.
----------------------------------
#In [/home/meng_chi/CSE 150/Project 1/pintos/src/devices/timer.c]
static struct list sleep_list; /* Threads blocked in timer_sleep(). */

Purpose: Tracks sleeping threads so the timer interrupt can wake them up.
----------------------------------
#Optional helper comparator (likely in timer.c):
static bool wakeup_less (const struct list_elem *a,
 const struct list_elem *b,
 void *aux);

Purpose: Keeps sleep_list sorted by wakeup_tick so the interrupt handler doesn’t have to
scan everything.
-----------------------------------
ALGORITHMS
**A2: Briefly describe what happens in a call to `timer_sleep()`, including the effects of the
timer interrupt handler.**

Right now, timer_sleep() in timer.c just loops and calls thread_yield() until enough ticks
pass. I’m changing it to actually block.
In timer_sleep(ticks):
1. If ticks <= 0, return right away.
2. Compute wakeup = timer_ticks() + ticks.
3. Turn interrupts off (`intr_disable()`).
4. Save `wakeup` into the current thread, insert it into `sleep_list` (sorted by wakeup time).
5. Call `thread_block()` to stop running.
6. Restore the old interrupt level.
In the timer interrupt (`timer_interrupt()` in `timer.c`), after `ticks++`:
1. Check the front of `sleep_list`.
2. While the earliest thread’s `wakeup_tick <= ticks`, pop it and call `thread_unblock()` on it.
3. Stop as soon as the front thread is not ready to wake yet.

**A3: What steps are taken to minimize the amount of time spent in the timer interrupt
handler?**
I keep `sleep_list` sorted by wake time. Then the interrupt only wakes a few threads from
the front and stops early.

/////////////////////////////////////////////////////
SYNCHRONIZATION
**A4: How are race conditions avoided when multiple threads call `timer_sleep()`
simultaneously?**
All `sleep_list` operations happen with interrupts off, so two threads can’t mess up the list at
the same time.
----------------------------------
**A5: How are race conditions avoided when a timer interrupt occurs during a call to
`timer_sleep()`?**
The timer interrupt can’t run while interrupts are disabled, so it can’t see a half-updated
sleep list or half-blocked thread.
RATIONALE
----------------------------------
**A6: Why did you choose this design? In what ways is it superior to another design you
considered?**
This is better than the current “yield in a loop” because:
1. It doesn’t waste CPU time spinning.
2. Wakeups are handled quickly by the interrupt using an ordered list.
3. It uses the normal Pintos approach (interrupts off) since interrupt code can’t use locks.

///////////////////////////////////////////////////////
Priority Scheduling + Priority Donation
DATA STRUCTURES
**B1: Copy here the declaration of each new or changed struct or struct member, global or
static variable, typedef, or enumeration. Identify the purpose of each in 25 words or less.**
In [/home/meng_chi/CSE 150/Project 1/pintos/src/threads/thread.h]

/* Priority + donation. */
int base_priority; /* The thread's "real" priority (what user set). */
struct list donations; /* Threads donating to me right now. */
struct list_elem donation_elem; /* So I can be inside someone else's
donations list. */
struct lock *waiting_lock; /* Lock I'm waiting on (for nested
donation). */

Purpose: Store original priority, track who is donating to a thread, and support donation
----------------------------------
In [`/home/meng_chi/CSE 150/Project 1/pintos/src/threads/synch.c`]

(small change to help condition variables be priority-aware):
struct semaphore_elem {
 struct list_elem elem;
 struct semaphore semaphore;
 int priority; /* Priority of the waiter when it
called cond_wait(). */
};

Purpose: Lets `cond_signal()` pick the highest-priority waiter.
------------------------------------
**B2: Explain the data structure used to track priority donation. Use ASCII art to diagram a
nested donation.**

Each thread keeps a donations list. If a thread is blocked on a lock I hold, it gets added to my
donations.
Nested donation example:
H(50) wants L2, but M holds L2
M (20) wants L1, but L holds L1
L (10) holds L1
H donates to M, then M donates to L:
H -> M -> L
----------------------------------
ALGORITHMS
**B3: How do you ensure that the highest priority thread waiting for a lock, semaphore, or
condition variable wakes up first?**
I make sure every waiting list is sorted by priority, with the highest priority thread at the
front
1. In sema_down(), when a thread blocks, I insert it into the waiters list in
order(highest first)
2. In sema_down(), I always unblock the thread at the front of the list
3. Locks use semaphores internally, so they automatically follow the same rule
4. For condition variables, I store each thread’s priority and keep the waiters list
stored so it wakes the highest priority thread first.
-----------------------------------
**B4: Describe the sequence of events when a call to `lock_acquire()` causes a priority
donation. How is nested donation handled?**
If I try to get a lock and someone already has it, this is how donation handles
1. I record which lock I’m waiting for
2. If my priority is higher than the thread holding the lock, I donate my priority to it,.
3. The lock holder’s priority is updated to the higher value
4. If that thread is also waiting on another lock, the donation continues
5. I then block and wait until the lock is released.
6. Once I finally get the lock, I clear the waiting lock field
-----------------------------------
**B5: Describe the sequence of events when `lock_release()` is called on a lock that a higherpriority thread is waiting for.**
When a thread releases a lock:
1. I remove any priority donations that happened because of that lock
2. I recalculate my priority. It becomes the highest value between my original priority
and any remaining donations.
3. I wake up the highest priority thread waiting for the lock
4. If my priority is now lower than another ready thread, yield so something can run
without any more work
------------------------------------
SYNCHRONIZATION
**B6: Describe a potential race in `thread_set_priority()` and explain how your
implementation avoids it. Can you use a lock to avoid this race?**

A possible race happens if a thread changes its priority while the read list or wait list are
being used. The lists could become out of order, or the thread might keep running even
though it is no longer the highest priority and end up accessing the same data.

To avoid this, I disable interrupts while updating the priority. After updating it, I check if
another thread has a higher priority. If so yield. Removing any concern for things overriding
or acting at the same time.

Using a lock is not a good solution because interrupt code cannot sleep and frankly im not
sure what would happen after that.
------------------------------------
RATIONALE
**B7: Why did you choose this design? In what ways is it superior to another design you
considered?**

I chose this design since its simple to manage and easier to understand. Keeping the ready
list and wait list sorted makes it easy to always run the highest priority thread. Disabling
the interrupts is a simple and safe way to protect shared data between threads and
interrupt handelers. 