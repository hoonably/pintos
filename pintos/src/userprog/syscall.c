#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

// ✅✅✅✅✅ 
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

// ✅✅✅✅✅
// 미리 포인터를 검사하고 쓰기
// - 이 주소가 PHYS_BASE보다 작나?
// - 이 주소가 페이지 테이블에 매핑되어 있나?
bool is_valid_user_ptr(const void *uaddr);
bool is_valid_user_ptr(const void *uaddr) {
  if (uaddr == NULL) return false;
  if (!is_user_vaddr(uaddr)) return false; // 유저 영역 주소? (vaddr < PHYS_BASE)
  struct thread *cur_thread = thread_current();
  if (pagedir_get_page(cur_thread->pagedir, uaddr) == NULL) return false; // physical address 매핑 안됐다면 NULL
  return true;
}

// ✅✅✅✅✅
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int get_user(const uint8_t *uaddr) {
  if (!is_valid_user_ptr(uaddr))
      return -1;
  return *uaddr;
}

// ✅✅✅✅✅
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool put_user(uint8_t *udst, uint8_t byte) {
  if (!is_valid_user_ptr(udst))
      return false;
  *udst = byte;
  return true;
}


// ✅✅✅✅✅
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

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // ❌❌❌❌❌
  // printf ("system call!\n");
  // thread_exit ();

  // ✅✅✅✅✅
  // 여기서 f->eaxsms System Call 리턴 값을 저장하는 레지스터
  if (!is_valid_user_ptr(f->esp)) exit(-1);  // 유저 포인터 이상하면 바로 종료
  
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

    // case SYS_EXEC: {
    //   if (!is_valid_user_ptr(f->esp + 4)) exit(-1);
    //   const char *cmd_line = *(const char **)(f->esp + 4);
    //   f->eax = exec(cmd_line);
    //   break;
    // }

    // case SYS_WAIT: {
    //   if (!is_valid_user_ptr(f->esp + 4)) exit(-1);
    //   pid_t pid = *(pid_t *)(f->esp + 4);
    //   f->eax = wait(pid);
    //   break;
    // }

    case SYS_CREATE:
    {
        const char *file;
        unsigned initial_size;
        if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8)) exit(-1);
        file = *(char **)(f->esp + 4);
        initial_size = *(unsigned *)(f->esp + 8);
        f->eax = create(file, initial_size);
        break;
    }

    // case SYS_REMOVE:
    // {
    //     const char *file;
    //     if (!is_user_vaddr(f->esp + 4)) exit(-1);
    //     file = *(char **)(f->esp + 4);
    //     f->eax = remove(file);
    //     break;
    // }

    case SYS_OPEN:
    {
        const char *file;
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        file = *(char **)(f->esp + 4);
        f->eax = open(file);
        break;
    }

    // case SYS_FILESIZE:
    // {
    //     int fd;
    //     if (!is_user_vaddr(f->esp + 4)) exit(-1);
    //     fd = *(int *)(f->esp + 4);
    //     f->eax = filesize(fd);
    //     break;
    // }

    // case SYS_READ:
    // {
    //     int fd;
    //     void *buffer;
    //     unsigned size;
    //     if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8) || !is_user_vaddr(f->esp + 12)) exit(-1);
    //     fd = *(int *)(f->esp + 4);
    //     buffer = *(void **)(f->esp + 8);
    //     size = *(unsigned *)(f->esp + 12);
    //     f->eax = read(fd, buffer, size);
    //     break;
    // }

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

    // case SYS_SEEK:
    // {
    //     int fd;
    //     unsigned position;
    //     if (!is_user_vaddr(f->esp + 4) || !is_user_vaddr(f->esp + 8)) exit(-1);
    //     fd = *(int *)(f->esp + 4);
    //     position = *(unsigned *)(f->esp + 8);
    //     seek(fd, position);
    //     break;
    // }

    // case SYS_TELL:
    // {
    //     int fd;
    //     if (!is_user_vaddr(f->esp + 4)) exit(-1);
    //     fd = *(int *)(f->esp + 4);
    //     f->eax = tell(fd);
    //     break;
    // }

    case SYS_CLOSE:
    {
        int fd;
        if (!is_user_vaddr(f->esp + 4)) exit(-1);
        fd = *(int *)(f->esp + 4);
        close(fd);
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

// 성공하면 true, 실패하면 false
bool create(const char *file, unsigned initial_size) {
  if (file == NULL) exit(-1);
  return filesys_create(file, initial_size);
}


// 성공하면 새로운 파일 디스크립터(fd)를 반환, 실패하면 -1
int open(const char *file) {
  
  return -1;
}

// 파일이나 콘솔로 데이터를 쓰기
int write(int fd, const void *buffer, unsigned size) {
  // 지금 Project 2-1에서는 write()를 완전히 다 구현할 필요는 없다.
  // 딱 'fd == 1' (콘솔 출력)만 제대로 하면 된다.
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  // 2-2에서 구현 예정
  else {
    return 0;
  }
}

void close(int fd) {

}