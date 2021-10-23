#ifndef QUEUE_H
#define QUEUE_H

#include <unistd.h>

typedef struct process_s {
  unsigned id;
  pid_t pid;
  char *name;
  struct process_s *next;
} process;

void *safe_malloc(size_t size);
void enqueue(pid_t pid, char *name);
void dequeue(pid_t pid);
void rotate_queue();
#endif
