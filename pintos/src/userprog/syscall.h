#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

// Ⓜ️Ⓜ️Ⓜ️Ⓜ️Ⓜ️ - 모범답안대로라면 여기 한줄이 추가 -> exception.c에서 exit() 호출하기 위해서 정의하는듯
void exit(int status);

#endif