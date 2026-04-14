#include "userprog/syscall.h"
#include <stdint.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "lib/kernel/stdio.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

static void syscall_exit (int status) NO_RETURN;
static void validate_user_ptr (const void *uaddr);
static void validate_user_range (const void *uaddr, unsigned size);
static uint32_t copy_in_u32 (const void *uaddr);
static void *copy_in_ptr (const void *uaddr);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_exit (int status)
{
  struct thread *t = thread_current ();
  t->exit_status = status;
  thread_exit ();
}

static void
validate_user_ptr (const void *uaddr)
{
  struct thread *t = thread_current ();

  if (uaddr == NULL)
    syscall_exit (-1);
  if (!is_user_vaddr (uaddr))
    syscall_exit (-1);
  if (pagedir_get_page (t->pagedir, uaddr) == NULL)
    syscall_exit (-1);
}

static void
validate_user_range (const void *uaddr, unsigned size)
{
  const uint8_t *p = uaddr;

  for (unsigned i = 0; i < size; i++)
    validate_user_ptr (p + i);
}

static uint32_t
copy_in_u32 (const void *uaddr)
{
  uint32_t value;

  validate_user_range (uaddr, sizeof value);
  memcpy (&value, uaddr, sizeof value);
  return value;
}

static void *
copy_in_ptr (const void *uaddr)
{
  return (void *) (uintptr_t) copy_in_u32 (uaddr);
}

static int
syscall_write (int fd, const void *buffer, unsigned size)
{
  if (fd != 1)
    return -1;

  if (size == 0)
    return 0;

  validate_user_range (buffer, size);
  putbuf (buffer, size);
  return (int) size;
}

static void
syscall_handler (struct intr_frame *f)
{
  uint32_t syscall_no = copy_in_u32 (f->esp);

  switch (syscall_no)
    {
    case SYS_HALT:
      shutdown_power_off ();
      NOT_REACHED ();

    case SYS_EXIT:
      syscall_exit ((int) copy_in_u32 ((uint8_t *) f->esp + 4));
      NOT_REACHED ();

    case SYS_WRITE:
      {
        int fd = (int) copy_in_u32 ((uint8_t *) f->esp + 4);
        const void *buffer = copy_in_ptr ((uint8_t *) f->esp + 8);
        unsigned size = (unsigned) copy_in_u32 ((uint8_t *) f->esp + 12);
        f->eax = syscall_write (fd, buffer, size);
      }
      break;

    default:
      syscall_exit (-1);
      NOT_REACHED ();
    }
}
