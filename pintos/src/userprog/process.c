#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy, *fn_parse, *args;
  tid_t tid;

  fn_copy = palloc_get_page(0);
  fn_parse = palloc_get_page(0);  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 파싱용으로도 따로 만들어줘야함
  if (fn_copy == NULL || fn_parse == NULL)
    return TID_ERROR;

  strlcpy(fn_copy, file_name, PGSIZE);
  strlcpy(fn_parse, file_name, PGSIZE);

  // 파싱용으로 따로 argument 버리고 전달
  char *program = strtok_r(fn_parse, " ", &args);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (program, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 

  palloc_free_page(fn_parse);

  return tid;
}


/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 파싱해서 스택에 삽입하는 로직
  char *program, *args, *token;
  program = strtok_r(file_name, " ", &args);
  success = load (program, &if_.eip, &if_.esp);


  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 자식 프로세스의 load 성공 여부 저장
  thread_current()->is_load = success;

  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 부모 프로세스의 exec() 대기 해제 (load 완료 알림)
  sema_up(&thread_current()->s_load);

  if(success) {
    int argc = 0;
    char *argv[32];

    argv[argc++] = program;
    // ⭐️ pointer여서 수정했음
    for(token = strtok_r(NULL, " ", &args); token != NULL; token = strtok_r(NULL, " ", &args))
      argv[argc++] = token;

    void *esp = if_.esp;
    char *addr[32];
    int i;
    for(i = argc-1; i >= 0; --i) {
      int len = strlen(argv[i])+1;
      esp -= len;
      memcpy(esp, argv[i], len);
      addr[i] = esp;
    }

    uintptr_t sz = (uintptr_t)esp%4;
    if(sz) {
      esp -= sz;
      memset(esp, 0, sz);
    }

    esp -= 4;
    memset(esp, 0, 4);

    for(i = argc-1; i >= 0; --i) {
      esp -= 4;
      memcpy(esp, &addr[i], 4);
    }

    char *argv_addr = esp;
    esp -= 4;
    memcpy(esp, &argv_addr, 4);

    esp -= 4;
    memcpy(esp, &argc, 4);

    esp -= 4;
    memset(esp, 0, 4);

    if_.esp = esp;
  }
  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 파싱해서 스택에 삽입하는 로직

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  struct thread *child = get_child(child_tid);
  if (child == NULL) return -1;       // 자식이 아닌 경우
  if (child->is_wait) return -1;      // 이미 wait()한 경우
  
  child->is_wait = true;              // 중복 방지 플래그 설정
  sema_down(&child->s_wait);          // 자식이 죽을 때까지 대기

  int status = child->is_exit;        // 자식의 종료코드 획득
  list_remove(&child->child);         // child_list에서 제거
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 자식 프로세스 종료를 기다리는 부모를 깨움
  if (cur->parent != NULL) {
    sema_up(&cur->s_wait);  // 부모가 wait() 중이면 깨워줌
  }

  //! mmap 파일 닫기 전에 해제!!!!
  struct list_elem *e, *next;
  for (e = list_begin(&cur->mmap_list); e != list_end(&cur->mmap_list); e = next) {
    next = list_next(e);
    struct mmap_file *mf = list_entry(e, struct mmap_file, elem);
    syscall_munmap(mf->idx);
  }

  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 현재 실행중인 파일에 다시 쓰기가 가능하도록 바꿔줌
  if(cur->cur_file) {
    file_allow_write(cur->cur_file);
    file_close(cur->cur_file);
    cur->cur_file = NULL;
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  
  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - fd_table에 있는 파일들 모두 닫아줌
  int i;
  for (i = 2; i < FD_MAX; i++) {
    if (cur->fd_table[i]) {
      file_close(cur->fd_table[i]);
      cur->fd_table[i] = NULL;
    }
  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  //! page 테이블 초기화
  page_table_init(&t->page_table);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  //! 독립적 file 객체 복제
  // file 객체가 여러 프로세스에서 공유되면 
  // file_deny_write를 file_allow_write보다 많이 할 수 있어서 cnt가 음수가 되면 PANIC 발생함.
  // file 객체를 복사하면 안전하게 파일 공유 가능
  file = file_reopen(file);

  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 현재 실행중인 파일에 쓰기 금지
  t->cur_file = file;
  file_deny_write(t->cur_file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  // ❌❌❌❌❌ - process_exit()에서 파일을 닫아야함
  // file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      //! Page table entry 생성 및 등록
      if (!allocate_page(PAGE_BINARY, upage, writable))
          return false;

      struct page *p = find_page_entry(&thread_current()->page_table, upage);
      if (p == NULL) return false;

      //! 파일 매핑 정보 저장
      p->file = file;
      p->offset = ofs;
      p->read_bytes = page_read_bytes;
      p->zero_bytes = page_zero_bytes;

      /* Get a page of memory. */
      //* kernel page니까 frame으로 alloc
      // uint8_t *kpage = palloc_get_page (PAL_USER);
      // uint8_t *kpage = frame_alloc(PAL_USER, upage);
      // if (kpage == NULL)
      //   return false;

      // /* Load this page. */
      // if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
      //   {
      //     palloc_free_page (kpage);
      //     return false; 
      //   }
      // memset (kpage + page_read_bytes, 0, page_zero_bytes);

      // /* Add the page to the process's address space. */
      // if (!install_page (upage, kpage, writable)) 
      //   {
      //     // kernel page frame으로 만들었으니 frame free
      //     // palloc_free_page (kpage);
      //     frame_free(kpage);
      //     return false; 
      //   }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      //! ofs는 파일 오프셋이니까 읽은 만큼 증가시켜야 함
      ofs += page_read_bytes;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack(void **esp)
{
    void *upage = (uint8_t *) PHYS_BASE - PGSIZE;

    // frame 할당 시도 - lazy
    if (!allocate_page(PAGE_STACK, upage, true)) {
        return false;
    }

    // 실제 페이지 로드 (기존 frame_alloc + install_page의 역할)
    struct page *p = find_page_entry(&thread_current()->page_table, upage);
    if (p == NULL || !load_page(p)) {
        return false;
    }

    *esp = PHYS_BASE;
    return true;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
