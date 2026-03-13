# Project 1 Threads Design Doc

Name: _Your Name_

Email: _your.email_

Date: 2026-03-02

What I’m doing: Tasks 1-4 only. Alarm clock sleep/wakeup, priority scheduling (with preemption), priority donation, and implementing `thread_set_priority()` / `thread_get_priority()`.

Where I’ll work: mostly `pintos/src/threads/` and a bit in `pintos/src/devices/timer.c`.

---

## Alarm Clock

### DATA STRUCTURES

**A1: Copy here the declaration of each new or changed struct or struct member, global or static variable, typedef, or enumeration. Identify the purpose of each in 25 words or less.**

In [`/home/meng_chi/CSE 150/Project 1/pintos/src/threads/thread.h`](/home/meng_chi/CSE%20150/Project%201/pintos/src/threads/thread.h):

```c
/* Alarm clock. */
int64_t wakeup_tick;          /* Tick when this thread should wake up. */
struct list_elem sleep_elem;  /* For putting this thread in the sleep_list. */
```

Purpose: Each sleeping thread stores its wake time and can be linked into a global sleep list.

In [`/home/meng_chi/CSE 150/Project 1/pintos/src/devices/timer.c`](/home/meng_chi/CSE%20150/Project%201/pintos/src/devices/timer.c):

```c
static struct list sleep_list;  /* Threads blocked in timer_sleep(). */
```

Purpose: Tracks sleeping threads so the timer interrupt can wake them up.

Optional helper comparator (likely in `timer.c`):

```c
static bool wakeup_less (const struct list_elem *a,
                         const struct list_elem *b,
                         void *aux);
```

Purpose: Keeps `sleep_list` sorted by `wakeup_tick` so the interrupt handler doesn’t have to scan everything.

### ALGORITHMS

**A2: Briefly describe what happens in a call to `timer_sleep()`, including the effects of the timer interrupt handler.**

Right now, `timer_sleep()` in `timer.c` just loops and calls `thread_yield()` until enough ticks pass. I’m changing it to actually block.

In `timer_sleep(ticks)`:

1. If `ticks <= 0`, return right away.
2. Compute `wakeup = timer_ticks() + ticks`.
3. Turn interrupts off (`intr_disable()`).
4. Save `wakeup` into the current thread, insert it into `sleep_list` (sorted by wakeup time).
5. Call `thread_block()` to stop running.
6. Restore the old interrupt level.

In the timer interrupt (`timer_interrupt()` in `timer.c`), after `ticks++`:

1. Check the front of `sleep_list`.
2. While the earliest thread’s `wakeup_tick <= ticks`, pop it and call `thread_unblock()` on it.
3. Stop as soon as the front thread is not ready to wake yet.

**A3: What steps are taken to minimize the amount of time spent in the timer interrupt handler?**

I keep `sleep_list` sorted by wake time. Then the interrupt only wakes a few threads from the front and stops early.

### SYNCHRONIZATION

**A4: How are race conditions avoided when multiple threads call `timer_sleep()` simultaneously?**

All `sleep_list` operations happen with interrupts off, so two threads can’t mess up the list at the same time.

**A5: How are race conditions avoided when a timer interrupt occurs during a call to `timer_sleep()`?**

The timer interrupt can’t run while interrupts are disabled, so it can’t see a half-updated sleep list or half-blocked thread.

### RATIONALE

**A6: Why did you choose this design? In what ways is it superior to another design you considered?**

This is better than the current “yield in a loop” because:

1. It doesn’t waste CPU time spinning.
2. Wakeups are handled quickly by the interrupt using an ordered list.
3. It uses the normal Pintos approach (interrupts off) since interrupt code can’t use locks.

---

## Priority Scheduling + Priority Donation

### DATA STRUCTURES

**B1: Copy here the declaration of each new or changed struct or struct member, global or static variable, typedef, or enumeration. Identify the purpose of each in 25 words or less.**

In [`/home/meng_chi/CSE 150/Project 1/pintos/src/threads/thread.h`](/home/meng_chi/CSE%20150/Project%201/pintos/src/threads/thread.h):

```c
/* Priority + donation. */
int base_priority;              /* The thread's "real" priority (what user set). */
struct list donations;          /* Threads donating to me right now. */
struct list_elem donation_elem; /* So I can be inside someone else's donations list. */
struct lock *waiting_lock;      /* Lock I'm waiting on (for nested donation). */
```

Purpose: Store original priority, track who is donating to a thread, and support donation chains.

In [`/home/meng_chi/CSE 150/Project 1/pintos/src/threads/synch.c`](/home/meng_chi/CSE%20150/Project%201/pintos/src/threads/synch.c) (small change to help condition variables be priority-aware):

```c
struct semaphore_elem {
  struct list_elem elem;
  struct semaphore semaphore;
  int priority;                 /* Priority of the waiter when it called cond_wait(). */
};
```

Purpose: Lets `cond_signal()` pick the highest-priority waiter.

**B2: Explain the data structure used to track priority donation. Use ASCII art to diagram a nested donation.**

Each thread keeps a `donations` list. If a thread is blocked on a lock I hold, it gets added to my `donations`.

Nested donation example:

```text
H (50) wants L2, but M holds L2
M (20) wants L1, but L holds L1
L (10) holds L1

H donates to M, then M donates to L:
H -> M -> L
```

I’ll keep `thread->priority` as the effective priority (max of `base_priority` and the donation list).

### ALGORITHMS

**B3: How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?**

I will sort wait lists by effective priority.

1. `sema_down()`: insert the thread into `sema->waiters` using `list_insert_ordered()` (highest priority first).
2. `sema_up()`: pop the front waiter and unblock it.
3. Locks use semaphores internally, so `lock_acquire()`/`lock_release()` automatically follow the same rule.
4. Condition variables: store a priority in each `semaphore_elem`, keep `cond->waiters` ordered by that priority, and signal the front.

**B4: Describe the sequence of events when a call to `lock_acquire()` causes a priority donation. How is nested donation handled?**

When I try to acquire a lock that is already held:

1. Set `current->waiting_lock = lock`.
2. If my priority is higher than the holder, add me to the holder’s `donations` list.
3. Update the holder’s effective priority.
4. If the holder is itself waiting on another lock, repeat up the chain (up to depth 8, based on the handout).
5. Then I block in `sema_down()` until the lock is released.
6. When I finally get the lock, set `waiting_lock = NULL`.

**B5: Describe the sequence of events when `lock_release()` is called on a lock that a higher-priority thread is waiting for.**

When a thread releases a lock:

1. Remove donations that were caused by this lock.
2. Recompute effective priority = max(`base_priority`, remaining donations).
3. `sema_up()` wakes the highest-priority waiting thread.
4. If my priority dropped and someone higher is ready, yield so they run.

### SYNCHRONIZATION

**B6: Describe a potential race in `thread_set_priority()` and explain how your implementation avoids it. Can you use a lock to avoid this race?**

Race: while changing priority, the ready list / wait lists could be temporarily out of order, or a thread could keep running even though it’s no longer the highest priority.

Fix: do the priority update with interrupts disabled, then check if we should yield.

A lock is not a good solution because interrupt-related code can’t block on locks, and Pintos usually protects scheduler structures with interrupts-off sections.

### RATIONALE

**B7: Why did you choose this design? In what ways is it superior to another design you considered?**

1. Sorting the ready list and wait lists makes it straightforward to always pick the highest priority thread.
2. Donation lists make it easier to “undo” donations when a lock is released.
3. Interrupt disabling is the simplest safe way to protect data shared with interrupt context.

---

## Task 4: `thread_set_priority()` and `thread_get_priority()`

`thread_get_priority()` returns the current thread’s effective priority (including donation).

`thread_set_priority(new_priority)` updates `base_priority`, recomputes effective priority, and yields if the current thread is no longer the highest priority ready thread.

---

## Notes / Assumptions

1. `thread->priority` will mean effective priority, and `base_priority` is what the user set.
2. Donation is for locks (priority inversion issue).
3. Nested donation chain depth is limited to 8 (from the PDF handout).
4. Not doing MLFQS for this project design doc.

