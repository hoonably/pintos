#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - semaphores (sema_init, sema_down, sema_up...)

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define FD_MAX 256  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - file descriptor 최대 개수 (palloc으로 하려면 나중에 수정하면 됨.)

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    // ⭐️⭐️⭐️⭐️⭐️ - 스레드의 우선순위 (0~63), 높은 숫자가 높은 우선순위
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 스레드 깨어나야 할 시간
    int64_t wake_up_ticks;

    // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 자식 프로세스의 종료 상태
    int is_exit;  // 0 = 성공, 0이 아닌 값 = 실패

    // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - File Discripter Table
    int fd_idx;  // 0 = stdin, 1 = stdout, 2 = file descriptor 시작
    struct file *fd_table[FD_MAX];  // fd_table[0] = stdin, fd_table[1] = stdout

   // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 새로운 thread 구조체 변수
   struct thread *parent;  // 부모 스레드

   struct list_elem child;  // 자식 스레드 리스트의 요소
   struct list child_list;  // 자식 스레드 리스트

   struct semaphore s_load;  // exec에서 부모가 자식의 load() 완료까지 대기
   bool is_load;  // 자식의 load 성공 여부를 부모에게 전달ㄴ

   struct semaphore s_wait;  // wait()에서 부모가 자식의 종료를 대기
   bool is_wait;  // 중복 wait 호출 방지용 플래그

   struct file *cur_file;  // 실행 중인 파일에 대한 포인터, 쓰기 방지
   // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️

   uint32_t *pagedir;                  /* Page directory. */
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    #endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);
struct thread *get_child (tid_t tid);// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️

void thread_sleep (int64_t ticks);  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️
void thread_wake_up (int64_t ticks);  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️

bool priority_comp (const struct list_elem *a, const struct list_elem *b, void *aux);  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

#endif /* threads/thread.h */
