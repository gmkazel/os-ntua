#ifndef QUEUE_H_
#define QUEUE_H_

#include <sys/types.h>
#include <unistd.h>

#define true 1
#define false 0

/* Define colors for output */
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define RST "\033[0m"

typedef int bool;

typedef struct process_s {
  pid_t pid;
  unsigned id;
  char* name;
  struct process_s* next;
} process;

typedef struct queue_s {
  process* head;
  process* tail;
  unsigned size;
} queue;

void* safe_malloc(size_t size);
queue* initialize_queue(void);
process* initialize_process(pid_t pid, char* name, unsigned id);
bool is_empty(queue* q);
unsigned get_size(queue* q);
void enqueue(queue* q, process* new_proc, pid_t pid, char* name);
void dequeue(queue* q, pid_t pid);
void print_queue(queue* q, bool add_space);
process* get_process_by_id(queue* q, unsigned r_id);
void rotate_queue(queue* q);
void rotate_queue_new(queue* q);
#endif  // QUEUE_H_
