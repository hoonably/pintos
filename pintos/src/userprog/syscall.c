#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

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
  printf ("system call!\n");
  thread_exit ();
}
