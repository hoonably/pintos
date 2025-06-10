#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"

// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ 
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ 입출력 관련 헤더
#include "devices/input.h"
#include "filesys/file.h"
#include "vm/page.h"

// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - file 여러개 접근 방지 lock
struct lock file_lock;

bool is_valid_buffer(const void* buffer, unsigned size, bool writable); // 유효한 버퍼인지 검사

// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️
// 미리 포인터를 검사하고 쓰기
// - 이 주소가 PHYS_BASE보다 작나?
// - 이 주소가 페이지 테이블에 매핑되어 있나?
// pagedir 에서 user address를 uaddr라고 씀
bool is_valid_user_ptr(const void *uaddr);
bool is_valid_user_ptr(const void *uaddr) {
  if (uaddr == NULL) return false;
  if (!is_user_vaddr(uaddr)) return false; // 유저 영역 주소? (vaddr < PHYS_BASE)
  struct thread *cur_thread = thread_current();
  if (pagedir_get_page(cur_thread->pagedir, uaddr) == NULL) return false; // physical address 매핑 안됐다면 NULL
  return true;
}

// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int get_user(const uint8_t *uaddr) {
  if (!is_valid_user_ptr(uaddr))
      return -1;
  return *uaddr;
}

// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
// static bool put_user(uint8_t *udst, uint8_t byte) {
//   if (!is_valid_user_ptr(udst))
//       return false;
//   *udst = byte;
//   return true;
// }


// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️
typedef int pid_t;
static void syscall_handler (struct intr_frame *);
void halt(void);              // 시스템 종료
void exit(int status);        // 프로그램 종료
pid_t exec(const char *cmd_line); // 새 프로그램 실행
int wait(pid_t pid);          // 자식 프로세스 기다리기
bool create(const char *file, unsigned initial_size); // 파일 만들기
bool remove(const char *file); // 파일 삭제하기
int open(const char *file);    // 파일 열기
int filesize(int fd);          // 파일 크기 알아오기
int read(int fd, void *buffer, unsigned size); // 파일이나 키보드에서 읽기
int write(int fd, const void *buffer, unsigned size); // 파일이나 콘솔에 쓰기
void seek(int fd, unsigned position); // 파일 읽기/쓰기 위치 바꾸기
unsigned tell(int fd);         // 파일 현재 위치 알아오기
void close(int fd);            // 파일 닫기
mapid_t syscall_mmap(int fd, void *addr, void *esp);
void syscall_munmap(mapid_t target_idx);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 파일 작업 락 초기화
  lock_init (&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // ❌❌❌❌❌
  // printf ("system call!\n");
  // thread_exit ();

  // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️
  // 여기서 f->eaxsms System Call 리턴 값을 저장하는 레지스터
  if (!is_valid_user_ptr(f->esp)) exit(-1);  // 유저 포인터 이상하면 바로 종료
  
  thread_current()->user_esp = f->esp;
  int syscall_num = get_user((uint8_t *) f->esp);
  
  // 각각 의 syscall에 맞는 인자들을 검사
  switch (syscall_num) {
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT: {
      if (!is_valid_user_ptr(f->esp + 4)) exit(-1);
      int status = *(int *)(f->esp + 4);
      exit(status);
      break;
    }

    case SYS_EXEC: {
      if (!is_valid_user_ptr(f->esp + 4)) exit(-1);

      const char *cmd_line = *(const char **)(f->esp + 4);
      if (!is_valid_user_ptr(cmd_line)) exit(-1);

      // Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 문자열 전체가 유효한 메모리에 있는지 확인 -> NULL까지 접근 가능
      const char *p = cmd_line;
      while (is_user_vaddr(p)) {
        // p가 유저 영역에 있더라고 해도, 실제로 페이지 테이블에 매핑 안되어있으면 NULL이니까 바로 exit(-1)
        if (pagedir_get_page(thread_current()->pagedir, p) == NULL)
          exit(-1);
        if (*p == '\0') break;  // 문자열 끝이면 검사 끝
        p++;
      }

      f->eax = exec(cmd_line);
      break;
    }

    case SYS_WAIT: {
      if (!is_valid_user_ptr(f->esp + 4)) exit(-1);
      pid_t pid = *(pid_t *)(f->esp + 4);
      f->eax = wait(pid);
      break;
    }

    case SYS_CREATE:
    {
        const char *file;

        unsigned initial_size;
        if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8)) exit(-1);
        file = *(char **)(f->esp + 4);
        initial_size = *(unsigned *)(f->esp + 8);

        if (!is_valid_user_ptr(file)) exit(-1);

        f->eax = create(file, initial_size);
        break;
    }

    case SYS_REMOVE:
    {
        const char *file;
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        file = *(char **)(f->esp + 4);
        f->eax = remove(file);
        break;
    }

    case SYS_OPEN:
    {
        const char *file;
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        file = *(char **)(f->esp + 4);

        if (!is_valid_user_ptr(file)) exit(-1);

        f->eax = open(file);
        break;
    }

    case SYS_FILESIZE:
    {
        int fd;
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        fd = *(int *)(f->esp + 4);
        f->eax = filesize(fd);
        break;
    }

    case SYS_READ:  // filesize가 구현이 되어있어야 중간에 사용해서 성공함
    {
        int fd;
        void *buffer;
        unsigned size;
        if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8) || !is_user_vaddr(f->esp + 12)) exit(-1);
        fd = *(int *)(f->esp + 4);
        buffer = *(void **)(f->esp + 8);
        size = *(unsigned *)(f->esp + 12);
        f->eax = read(fd, buffer, size);
        break;
    }

    case SYS_WRITE:
    {
        int fd;
        const void *buffer;
        unsigned size;
        if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8) || !is_user_vaddr(f->esp + 12)) exit(-1);
        fd = *(int *)(f->esp + 4);
        buffer = *(const void **)(f->esp + 8);
        size = *(unsigned *)(f->esp + 12);
        f->eax = write(fd, buffer, size);
        break;
    }

    case SYS_SEEK:
    {
        int fd;
        unsigned position;
        if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8)) exit(-1);
        fd = *(int *)(f->esp + 4);
        position = *(unsigned *)(f->esp + 8);
        seek(fd, position);
        break;
    }

    case SYS_TELL:
    {
        int fd;
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        fd = *(int *)(f->esp + 4);
        f->eax = tell(fd);
        break;
    }

    case SYS_CLOSE:
    {
        int fd;
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        fd = *(int *)(f->esp + 4);
        close(fd);
        break;
    }

    //! MMAP
    case SYS_MMAP: {
        if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8)) exit(-1);
        int fd = *(int *)(f->esp + 4);
        void *addr = *(void **)(f->esp + 8);
        f->eax = syscall_mmap(fd, addr, f->esp);
        break;
    }

    //! MUNMAP
    case SYS_MUNMAP: {
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        mapid_t idx = *(mapid_t *)(f->esp + 4);
        syscall_munmap(idx);
        break;
    }

    default:
      // 이상한 syscall 번호가 들어오면 종료
      printf("System call number error: %d\n", syscall_num);
      exit(-1);
  }
}

// 컴퓨터를 꺼버리는 시스템 콜
// shutdown_power_off() 함수를 호출해서 PintOS를 완전히 종료
// 주의: 이걸 호출하면 deadlock이나 에러 정보가 남지 않기 때문에, 정말 필요할 때만
void halt(void) {
  shutdown_power_off();
}

// 현재 스레드를 종료하고, 종료 상태를 반환
// 만약 부모 프로세스가 자식 프로세스를 wait()하고 있었다면, 이 status 값을 부모가 받아감
// 0 = 성공 / 0이 아닌 값 = 실패
void exit(int status) {
  struct thread *cur_thread = thread_current();
  cur_thread->is_exit = status;
  printf("%s: exit(%d)\n", cur_thread->name, status);
  thread_exit();
}

// 새로운 사용자 프로세스를 실행하고 pid 반환
// 자식 프로세스가 load에 성공할 때까지 부모는 대기
// 실패 시 -1 반환
pid_t exec(const char *cmd_line) {
  tid_t tid = process_execute(cmd_line);  // 새로운 스레드 생성
  if (tid == TID_ERROR) return -1;

  struct thread *child = get_child(tid);
  if (child == NULL) return -1;

  sema_down(&child->s_load);  // 자식의 load() 완료까지 대기
  if (!child->is_load) return -1;

  return tid;
}

// 자식 프로세스 pid의 종료를 기다리고 종료 코드를 반환
// pid가 자식이 아니거나 이미 기다린 경우 -1 반환
// 자식이 exit()을 호출하면 그 값을 반환
int wait(pid_t pid){
  return process_wait(pid);
}


// 성공하면 true, 실패하면 false
bool create(const char *file, unsigned initial_size) {
  if(!file) return 0;
  lock_acquire(&file_lock);
  bool ret = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return ret;
}

bool remove(const char *file) {
  if(!file) return 0;
  lock_acquire(&file_lock);
  bool ret = filesys_remove(file);
  lock_release(&file_lock);
  return ret;
} 


// 성공하면 새로운 파일 디스크립터(fd)를 반환, 실패하면 -1
int open(const char *file) {
  lock_acquire(&file_lock);
  struct file *f = filesys_open(file);
  if (f == NULL) {
    lock_release(&file_lock);  // 이거 안하면 deadlock
    return -1;
  }

  struct thread *cur_thread = thread_current();
  
  // fd = 0(STDIN_FILENO)은 표준 입력, fd = 1(STDOUT_FILENO)은 표준 출력, 실패하면 -1
  // fd_table index 늘리면서 file discripter 리턴해야하니까 기록
  int fd = cur_thread->fd_idx++;

  if (fd >= FD_MAX) {
    printf("File descriptor table is full\n");  // 디버깅용
    file_close(f);
    lock_release(&file_lock);
    return -1;
  }

  cur_thread->fd_table[fd] = f;  // 파일 저장
  
  lock_release(&file_lock);
  return fd;
}

// 열려 있는 파일 디스크립터 fd의 총 바이트 크기를 반환
// 잘못된 fd이거나 닫힌 파일이면 -1 반환
// 내부적으로 file_length() 사용
int filesize(int fd) {
  // file descriptor check & file check
  if (fd < 2 || fd >= FD_MAX) return -1;
  struct file *f = thread_current()->fd_table[fd];
  if (f == NULL) return -1;

  return file_length(f);
}

bool is_valid_buffer(const void* buffer, unsigned size, bool writable) {
  if(!buffer) return 0;

  //! round down해서 페이지 단위로 검사로 변경 -> read_boundary 검사 통과
  char *start = (char *)pg_round_down(buffer);
  char *end = (char *)pg_round_down(buffer + size - 1);
  char *ptr;
  //! round_down했기 때문에 end까지 검사해야 함 -? bad_ptr 검사 통과
  for(ptr = start; ptr <= end; ptr+= PGSIZE) {
    if (!is_user_vaddr(ptr)) return 0;
    struct page *page = find_page_entry(&thread_current()->page_table, ptr);
    if (page == NULL) {
      //! lazyloding 허용 -> page_fault() 에서 결정해줌
      // // return false;
      continue;
    }
    if (writable && !page->writable) return false;
  }
  return 1;
}


// fd로부터 size만큼 데이터를 읽어 buffer에 저장
// fd == 0이면 키보드에서 입력, 그 외는 파일에서 읽음
// 읽은 바이트 수 반환, 실패 시 -1
int read(int fd, void *buffer, unsigned size) {

  // 사용자로부터 buffer에 데이터를 쓰니까 writable == true
  if(!is_valid_buffer(buffer, size, true)) exit(-1);

  // fd == 0: 표준입력
  if (fd == 0) {
    unsigned i;
    for (i = 0; i < size; i++) {
      // device/input.c의 input_getc()를 사용해서 키보드 입력을 받음
      ((char *)buffer)[i] = input_getc();  // 한 글자씩
    }
    return size;
  }

  // file descriptor check & file check
  if (fd < 2 || fd >= FD_MAX) return -1;
  //? DEBUG
  // printf("🚨 READ fd=%d, buffer=%p, size=%u\n", fd, buffer, size);

  struct file *f = thread_current()->fd_table[fd];
  if (f == NULL) return -1;

  // 파일 읽기
  lock_acquire(&file_lock);
  int bytes_read = file_read(f, buffer, size);
  lock_release(&file_lock);

  return bytes_read;
}


// fd에 size만큼 buffer 내용을 씀
// fd == 1이면 콘솔, 그 외에는 열린 파일에 기록
// 실제로 쓴 바이트 수를 반환
int write(int fd, const void *buffer, unsigned size) {

  // 사용자로부터 buffer에 데이터를 읽으니까 writable == false
  if(!is_valid_buffer(buffer, size, false)) exit(-1);

  // 콘솔에 출력
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }

  // file descriptor check & file check
  if (fd < 2 || fd >= FD_MAX) return -1;

  //? DEBUG
  // printf("🚨 WRITE fd=%d, buffer=%p, size=%u\n", fd, buffer, size);
  // hex_dump((uintptr_t)buffer, buffer, 32, true);  // 앞부분만

  struct file *f = thread_current()->fd_table[fd];
  if (f == NULL) return -1;

  // 파일에 출력
  // struct file *cur_file = thread_current()->cur_file;
  lock_acquire(&file_lock);
  int bytes_write = file_write(f, buffer, size);
  lock_release(&file_lock);
  return bytes_write;
}

// fd 파일에서 현재 보고있는 위치를 position으로 변경
void seek(int fd, unsigned position) {
  if(fd < 2 || fd >= FD_MAX) return;
  struct file *f = thread_current()->fd_table[fd];
  if(!f) return;
  file_seek(f, position);
}

// fd 파일에서 현재 보고있는 위치를 반환
unsigned tell(int fd) {
  if(fd < 2 || fd >= FD_MAX) return -1;
  struct file *f = thread_current()->fd_table[fd];
  if(!f) return -1;
  return file_tell(f);
}

// 파일 디스크립터(fd)를 닫기
// fd가 표준 입출력(STDIN, STDOUT)이면 무시
// fd가 범위 밖에 있으면 무시
// 유효한 fd라면 해당 파일을 닫고 fd_table 엔트리를 NULL로 초기화
void close(int fd) {
  if(fd < 2 || fd >= FD_MAX) return;
  struct thread *cur_thread = thread_current();
  file_close (cur_thread->fd_table[fd]);
  cur_thread->fd_table[fd] = NULL;
}

// MMAP
mapid_t syscall_mmap(int fd, void *addr, void *esp) {
    struct thread *t = thread_current();

    if (fd <= 1 || addr == NULL || pg_ofs(addr) != 0)
        return -1;

    // mmap 주소가 현재 스택보다 위 or 스택 확장 가능한 영역과 겹치면 실패
    if (addr >= esp || addr >= (PHYS_BASE - STACK_MAX_SIZE))
        return -1;

    // fd가 가리키는 열린 파일 가져오기
    struct file *file = t->fd_table[fd];
    if (file == NULL) return -1;

    off_t length = file_length(file);
    if (length == 0) return -1;  // 빈 파일 mmap X

    // reopen으로 mmap 전용 file 객체 생성
    file = file_reopen(file);
    if (file == NULL) return -1;

    struct mmap_file *mf = malloc(sizeof(struct mmap_file));
    if (!mf) return -1;

    mf->idx = t->mmap_idx++;
    mf->file = file;
    mf->addr = addr;
    mf->length = length;
    list_push_back(&t->mmap_list, &mf->elem);  // 리스트에 등록

    // SPT에 lazy loading용 page 추가
    off_t offset = 0;
    void *page_addr = addr;

    while (length > 0) {
        size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct page *p = malloc(sizeof(struct page));
        if (!p) return -1;

        // 기본 페이지 정보 설정
        p->vaddr = page_addr;
        p->writable = true;  //! mmap은 원래 쓰기 가능
        p->is_loaded = false;  // lazy loading
        p->type = PAGE_MMAP;

        // lazy loading 할거니까 파일 정보 저장
        p->file = file;
        p->offset = offset;
        p->read_bytes = page_read_bytes;
        p->zero_bytes = page_zero_bytes;

        // SPT에 등록
        if (!insert_page_entry(&t->page_table, p))
            return -1;

        // 다음 페이지로
        page_addr += PGSIZE;
        offset += page_read_bytes;
        length -= page_read_bytes;
    }

    return mf->idx;
}

void syscall_munmap(mapid_t target_idx) {
    struct thread *t = thread_current();
    struct list_elem *e;
    struct mmap_file *mf = NULL;

    // mmap_list에서 해당 idx를 가진 mmap_file 찾기
    for (e = list_begin(&t->mmap_list); e != list_end(&t->mmap_list); e = list_next(e)) {
        struct mmap_file *cur = list_entry(e, struct mmap_file, elem);
        if (cur->idx == target_idx) {
            mf = cur;
            break;
        }
    }

    if (mf == NULL) return;

    void *addr = mf->addr;
    size_t left = mf->length;

    // 매핑된 주소 범위 순회하면서
    while (left > 0) {
        struct page *p = find_page_entry(&t->page_table, addr);

        if (p && p->is_loaded) {
            // dirty면 파일에 저장
            if (pagedir_is_dirty(t->pagedir, p->vaddr) && p->file && p->read_bytes > 0) {
                file_write_at(p->file, p->vaddr, p->read_bytes, p->offset);
            }

            pagedir_clear_page(t->pagedir, p->vaddr);  // 유저 페이지 제거
            void *kpage = pagedir_get_page(t->pagedir, p->vaddr);
            if (kpage != NULL) frame_free(kpage);
        }

        // SPT에서 entry 제거
        if (p) {
            hash_delete(&t->page_table, &p->elem);
        }

        addr += PGSIZE;
        left = (left > PGSIZE) ? left - PGSIZE : 0;
    }

    file_close(mf->file);
    list_remove(&mf->elem);
    free(mf);
}