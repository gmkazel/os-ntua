#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "proc-common.h"
#include "queue.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 2  /* time quantum */
#define TASK_NAME_SZ 60 /* maximum size for a task's name */

process *head, *tail;
unsigned queue_length;

/*
 * SIGALRM handler
 */
static void sigalrm_handler(int signum) {
  if (signum != SIGALRM) {
    fprintf(stderr, "Internal error: Called for signum %d, not SIGALRM\n",
            signum);
    exit(1);
  }

  // kill the proccess
  if (kill(head->pid, SIGSTOP) < 0) {
    perror("kill");
    exit(1);
  }
}

/*
 * SIGCHLD handler
 */
static void sigchld_handler(int signum) {
  pid_t p;
  int status;

  if (signum != SIGCHLD) {
    fprintf(stderr, "Internal error: Called for signum %d, not SIGCHLD\n",
            signum);
    exit(1);
  }

  for (;;) {
    p = waitpid(-1, &status, WUNTRACED | WNOHANG);
    if (p < 0) {
      perror("waitpid");
      exit(1);
    }
    if (p == 0) break;

    explain_wait_status(p, status);

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      /* A child has died */
      printf("Parent: Received SIGCHLD, child is dead.\n");

      dequeue(head->pid);

      rotate_queue();

      fprintf(stderr, "Proccess with pid=%ld is about to begin...\n",
              (long int)head->pid);

      if (kill(head->pid, SIGCONT) < 0) {
        perror("Continue to process");
        exit(1);
      }
      /* Setup the alarm again */
      if (alarm(SCHED_TQ_SEC) < 0) {
        perror("alarm");
        exit(1);
      }
    }
    if (WIFSTOPPED(status)) {
      /* A child has stopped due to SIGSTOP/SIGTSTP, etc... */

      // rotate queue
      rotate_queue();

      fprintf(stderr, "Proccess with pid=%ld is about to begin...\n",
              (long int)head->pid);
      if (kill(head->pid, SIGCONT) < 0) {
        perror("Continue to process");
        exit(1);
      }

      /* Setup the alarm again */
      if (alarm(SCHED_TQ_SEC) < 0) {
        perror("alarm");
        exit(1);
      }
    }
  }
}

/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void install_signal_handlers(void) {
  sigset_t sigset;
  struct sigaction sa;

  sa.sa_handler = sigchld_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGCHLD);
  sigaddset(&sigset, SIGALRM);
  sa.sa_mask = sigset;
  if (sigaction(SIGCHLD, &sa, NULL) < 0) {
    perror("sigaction: sigchld");
    exit(1);
  }

  sa.sa_handler = sigalrm_handler;
  if (sigaction(SIGALRM, &sa, NULL) < 0) {
    perror("sigaction: sigalrm");
    exit(1);
  }

  /*
   * Ignore SIGPIPE, so that write()s to pipes
   * with no reader do not result in us being killed,
   * and write() returns EPIPE instead.
   */
  if (signal(SIGPIPE, SIG_IGN) < 0) {
    perror("signal: sigpipe");
    exit(1);
  }
}

void child(char *name) {
  char *newargv[] = {name, NULL, NULL, NULL};
  char *newenviron[] = {NULL};

  printf("I am %s, PID = %ld\n", name, (long)getpid());
  printf("About to replace myself with the executable %s...\n", name);
  sleep(2);
  raise(SIGSTOP);
  execve(name, newargv, newenviron);

  /* execve() only returns on error */
  perror("execve");
  exit(1);
}

int main(int argc, char *argv[]) {
  int nproc;
  pid_t pid;
  queue_length = 0;
  /*
   * For each of argv[1] to argv[argc - 1],
   * create a new child process, add it to the process list.
   */

  nproc = argc; /* number of proccesses goes here */

  head = safe_malloc(sizeof(process));
  head->next = NULL;

  for (int i = 1; i < nproc; i++) {
    printf("Parent: Creating child...\n");
    pid = fork();

    if (pid < 0) {
      perror("fork");
      exit(1);
    } else if (pid == 0) {
      fprintf(stderr, "A new proccess is created with pid=%ld \n",
              (long int)getpid());

      child(argv[i]);
      assert(0);
    } else {
      enqueue(pid, argv[i]);
      printf(
          "Parent: Created child with PID = %ld, waiting for it to "
          "terminate...\n",
          (long)pid);
    }
  }

  // make the queue circular
  head = head->next;
  free(tail->next);
  tail->next = head;

  /* Wait for all children to raise SIGSTOP before exec()ing. */
  wait_for_ready_children(nproc - 1);

  /* Install SIGALRM and SIGCHLD handlers. */
  install_signal_handlers();

  if (nproc == 0) {
    fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
    exit(1);
  }

  fprintf(stderr, "Proccess with pid=%ld is about to begin...\n",
          (long int)head->pid);

  if (kill(head->pid, SIGCONT) < 0) {
    perror("First child error with continuing");
    exit(1);
  }

  if (alarm(SCHED_TQ_SEC) < 0) {
    perror("alarm");
    exit(1);
  }

  /* loop forever until we exit from inside a signal handler. */
  while (pause())
    ;

  /* Unreachable */
  fprintf(stderr, "Internal error: Reached unreachable point\n");
  return 1;
}
