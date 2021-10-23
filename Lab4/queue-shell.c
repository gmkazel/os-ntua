#include "queue-shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

unsigned id;

void *safe_malloc(size_t size) {
  void *p;

  if ((p = malloc(size)) == NULL) {
    fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n", size);
    exit(1);
  }
  return p;
}

queue *initialize_queue(void) {
  queue *q = safe_malloc(sizeof(queue));
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
  return q;
}

process *initialize_process(pid_t pid, char *name, unsigned r_id) {
  process *p = safe_malloc(sizeof(process));
  p->pid = pid;
  p->id = r_id;
  p->name = name;
  p->next = NULL;
  return p;
}

bool is_empty(queue *q) { return (q->size == 0); }

unsigned get_size(queue *q) { return q->size; }

void enqueue(queue *q, process *new_proc, pid_t pid, char *name) {
  if (q == NULL) {
    printf("Queue not initialized\n");
    exit(1);
  }

  if (new_proc == NULL) {
    new_proc = initialize_process(pid, name, id);
    id++;
  }

  if (is_empty(q)) {
    q->head = new_proc;
    q->head->next = q->head;
    q->tail = q->head;
  } else if (get_size(q) == 1) {
    q->tail = new_proc;
    q->head->next = q->tail;
    q->tail->next = q->head;
  } else {
    q->tail->next = new_proc;
    q->tail = new_proc;
    q->tail->next = q->head;
  }

  q->size++;
}

void dequeue(queue *q, pid_t pid) {
  if (q == NULL) {
    printf("Queue not initialized\n");
    exit(1);
  }

  if (q->head == NULL) {
    printf("Cannot delete from an empty queue\n");
    return;
  }

  if (get_size(q) == 1) {  // if queue has only 1 process
    if (q->head->pid == pid) {
      free(q->head);
      q->head = NULL;
      q->tail = NULL;
      q->size = 0;
      return;
    } else {
      printf("Process not in queue\n");
      return;
    }
  }

  q->tail->next = NULL;

  process *curr = q->head;  // check if the process to be removed is in the head

  if (curr->pid == pid) {
    q->head = q->head->next;
    q->tail->next = q->head;
    free(curr);
    q->size--;
    return;
  }

  process *prev = NULL;

  while (curr != NULL && curr->pid != pid) {
    prev = curr;
    curr = curr->next;
  }

  if (curr == NULL) {
    printf("Process not in queue\n");
    q->tail->next = q->head;
    return;
  }

  prev->next = curr->next;
  free(curr);
  q->tail->next = q->head;
  q->size--;
}

void print_queue(queue *q, bool print_head_with_space) {
  if (q == NULL) {
    printf("Queue not initialized\n");
    exit(1);
  }

  if (is_empty(q)) {
    printf("Queue is empty\n");
    return;
  }

  process *p = q->head;
  for (int i = 0; i < q->size; i++) {
    if (p == NULL) continue;
    if (p != q->head || print_head_with_space) printf("                 ");
    printf("ID: %d, PID: %ld, NAME: %s\n" RST, p->id, (long)p->pid, p->name);
    p = p->next;
  }
}

process *get_process_by_id(queue *q, unsigned r_id) {
  if (q == NULL) {
    printf("Queue not initialized\n");
    exit(1);
  }

  if (is_empty(q)) {
    printf("Queue is empty\n");
    return NULL;
  }

  process *p = q->head;
  for (int i = 0; i < q->size; i++) {
    if (p == NULL) continue;
    if (p->id == r_id) return p;
    p = p->next;
  }
  return NULL;
}

void rotate_queue(queue *q) {
  // if (q->size > 1) {
  //   process *curr = q->head;
  //   q->head = q->head->next;
  //   q->tail->next = curr;
  //   q->tail = curr;
  //   q->tail->next = q->head;
  // }
  if (q == NULL) {
    printf("Queue not initialized\n");
    exit(1);
  }

  if (is_empty(q)) {
    printf("Queue is empty\n");
    return;
  }

  process *temp = initialize_process(q->head->pid, q->head->name, q->head->id);
  dequeue(q, temp->pid);
  enqueue(q, temp, temp->pid, temp->name);
}

void rotate_queue_new(queue *q) {
  if (q->size > 1) {
    process *curr = q->head;
    q->head = q->head->next;
    q->tail->next = curr;
    q->tail = curr;
    q->tail->next = q->head;
  }
}
