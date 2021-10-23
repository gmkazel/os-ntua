/*                        - Known Problems -
 * Although the program works fine and all the commands to the shell
 * (e,p,k,q) have the requested output, there rarely occurs a segmentation fault
 * while we are running it. The problem doesn't occur after a specific sequence
 * of commands or at a specific point in the program, it just appears (if it
 * appears) out of nowhere. (We can't recreate the bug although we have
 * pinpointed the seg-fault in sigchld_handler function).
 */

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
#include "queue-shell.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 2                /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */

/* Define global variables */
unsigned id = 0;
queue *p_queue;

/* Print a list of all tasks currently being scheduled.  */
static void sched_print_tasks(void) {
  printf("--------------------------------------------------\n");
  if (is_empty(p_queue)) {
    printf("THERE ARE NO PROCESSES TO PRINT\n");
    return;
  }
  printf("Queue has size: %u\n", get_size(p_queue));
  printf(BLUE "Current Process: ");
  print_queue(p_queue, false);
  printf("--------------------------------------------------\n");
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int sched_kill_task_by_id(int id) {
  process *temp = get_process_by_id(p_queue, id);
  if (temp == NULL) {
    printf("Process not in queue\n");
    return -1;
  }

  pid_t r_pid = temp->pid;

  printf(CYAN "ID: %d, PID: %d, NAME: %s is being killed" RST "\n", temp->id,
         temp->pid, temp->name);

  if (kill(r_pid, SIGTERM) < 0) {
    perror("Kill proccess error- sched_kill_task_by_id");
    exit(1);
  }
  dequeue(p_queue, r_pid);
  return 1;
}

/* Create a new task.  */
static void sched_create_task(char *executable) {
  pid_t pid;
  char *new_name;
  new_name = safe_malloc(sizeof(executable));
  strcpy(new_name, executable);

  pid = fork();

  if (pid < 0) {
    perror("forking task- sched_kill_task_by_id");
    exit(1);
  } else if (pid == 0) {
    char *newargv[] = {new_name, NULL};
    char *newenviron[] = {NULL};
    raise(SIGSTOP);
    execve(new_name, newargv, newenviron);
  } else {
    // DEBUG:
    show_pstree(getpid());
    enqueue(p_queue, NULL, pid, new_name);
  }
}

/* Process requests by the shell.  */
static int process_request(struct request_struct *rq) {
  switch (rq->request_no) {
    case REQ_PRINT_TASKS:
      sched_print_tasks();
      return 0;

    case REQ_KILL_TASK:
      return sched_kill_task_by_id(rq->task_arg);

    case REQ_EXEC_TASK:
      sched_create_task(rq->exec_task_arg);
      return 0;

    default:
      return -ENOSYS;
  }
}

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
  if (kill(p_queue->head->pid, SIGSTOP) < 0) {
    perror("kill- sigalrm_handler");
    exit(1);
  }
}

/*
 * SIGCHLD handler
 */
static void sigchld_handler(int signum) {
  if (signum != SIGCHLD) {
    fprintf(stderr, "Internal error: Called for signum %d, not SIGCHLD\n",
            signum);
    exit(1);
  }

  pid_t p;
  int status;

  for (;;) {
    p = waitpid(-1, &status, WUNTRACED | WNOHANG);
    if (p < 0) {
      perror("waitpid- sigchld_handler");
      exit(1);
    }
    if (p == 0) break;

    explain_wait_status(p, status);

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      /* A child has died */
      printf("Parent: Received SIGCHLD, child is dead.\n");
      // process *temp = p_queue->head->next;

      dequeue(p_queue, p_queue->head->pid);

      if (is_empty(p_queue)) {
        printf(GREEN "Job's Done!\n" RST);
        exit(0);
      } else {
        // rotate_queue(p_queue);

        fprintf(stderr, "Proccess with pid=%ld is about to begin...\n",
                (long int)p_queue->head->pid);

        if (kill(p_queue->head->pid, SIGCONT) < 0) {
          perror(
              "Continue to process- WIFEXITED-WIFSIGNALED - sigchld_handler");
          exit(1);
        }

        /* Setup the alarm again */
        if (alarm(SCHED_TQ_SEC) < 0) {
          perror("alarm- sigchld_handler");
          exit(1);
        }
      }
    }

    if (WIFSTOPPED(status)) {
      /* A child has stopped due to SIGSTOP/SIGTSTP, etc... */
      printf("Parent: Child has been stopped. Moving right along...\n");

      // rotate queue
      rotate_queue(p_queue);
      dequeue(p_queue, 0);  // maybe for debugging
      if (is_empty(p_queue)) {
        printf(GREEN "Job's Done!\n" RST);
        exit(0);
      }

      pid_t r_pid = p_queue->head->pid;

      fprintf(stderr, "Proccess with pid=%ld is about to begin...\n",
              (long int)r_pid);
      if (kill(r_pid, SIGCONT) < 0) {
        perror("Continue to process- WIFSTOPPED- sigchld_handler");
        exit(1);
      }

      /* Setup the alarm again */
      if (alarm(SCHED_TQ_SEC) < 0) {
        perror("alarm- WIFSTOPPED- sigchld_handler");
        exit(1);
      }
    }
  }
}

/* Disable delivery of SIGALRM and SIGCHLD. */
static void signals_disable(void) {
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigaddset(&sigset, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
    perror("signals_disable: sigprocmask");
    exit(1);
  }
}

/* Enable delivery of SIGALRM and SIGCHLD.  */
static void signals_enable(void) {
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigaddset(&sigset, SIGCHLD);
  if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
    perror("signals_enable: sigprocmask");
    exit(1);
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

static void do_shell(char *executable, int wfd, int rfd) {
  char arg1[10], arg2[10];
  char *newargv[] = {executable, NULL, NULL, NULL};
  char *newenviron[] = {NULL};

  sprintf(arg1, "%05d", wfd);
  sprintf(arg2, "%05d", rfd);
  newargv[1] = arg1;
  newargv[2] = arg2;

  raise(SIGSTOP);
  execve(executable, newargv, newenviron);

  /* execve() only returns on error */
  perror("scheduler: child: execve");
  exit(1);
}

/* Create a new shell task.
 *
 * The shell gets special treatment:
 * two pipes are created for communication and passed
 * as command-line arguments to the executable.
 */
static void sched_create_shell(char *executable, int *request_fd,
                               int *return_fd) {
  pid_t p;
  int pfds_rq[2], pfds_ret[2];

  if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
    perror("pipe");
    exit(1);
  }

  p = fork();
  if (p < 0) {
    perror("scheduler: fork");
    exit(1);
  }

  if (p == 0) {
    /* Child */
    close(pfds_rq[0]);
    close(pfds_ret[1]);
    do_shell(executable, pfds_rq[1], pfds_ret[0]);
    assert(0);
  }

  // initialize queue with the shell process
  char *new_name;
  new_name = safe_malloc(sizeof(executable));
  strcpy(new_name, executable);
  enqueue(p_queue, NULL, p, new_name);

  /* Parent */
  close(pfds_rq[1]);
  close(pfds_ret[0]);
  *request_fd = pfds_rq[0];
  *return_fd = pfds_ret[1];
}

static void shell_request_loop(int request_fd, int return_fd) {
  int ret;
  struct request_struct rq;

  /*
   * Keep receiving requests from the shell.
   */
  for (;;) {
    if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
      perror("scheduler: read from shell");
      fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
      break;
    }

    signals_disable();
    ret = process_request(&rq);
    signals_enable();

    if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
      perror("scheduler: write to shell");
      fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
      break;
    }
  }
}

int main(int argc, char *argv[]) {
  int nproc;
  pid_t pid;
  char *new_name;
  /* Two file descriptors for communication with the shell */
  static int request_fd, return_fd;

  p_queue = initialize_queue();  // to initialize the queue

  /* Create the shell. */
  sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);

  /*
   * For each of argv[1] to argv[argc - 1],
   * create a new child process, add it to the process list.
   */

  nproc = argc; /* number of proccesses goes here */

  for (int i = 1; i < nproc; i++) {
    new_name = safe_malloc(sizeof(argv[i]));
    strcpy(new_name, argv[i]);

    pid = fork();

    if (pid < 0) {
      perror("forking task- sched_create_shell");
      exit(1);
    } else if (pid == 0) {
      char *newargv[] = {new_name, NULL};
      char *newenviron[] = {NULL};
      raise(SIGSTOP);
      execve(new_name, newargv, newenviron);
    } else {
      enqueue(p_queue, NULL, pid, new_name);
      printf(
          "Parent: Created child with PID = %ld, waiting for it to "
          "terminate...\n",
          (long)pid);
    }
  }

  /* Wait for all children to raise SIGSTOP before exec()ing. */
  wait_for_ready_children(nproc - 1);

  // DEBUG:
  show_pstree(getpid());

  /* Install SIGALRM and SIGCHLD handlers. */
  install_signal_handlers();

  if (nproc == 0) {
    fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
    exit(1);
  }

  if (kill(p_queue->head->pid, SIGCONT) < 0) {
    perror("First child error with continuing - main");
    exit(1);
  }

  if (alarm(SCHED_TQ_SEC) < 0) {
    perror("alarm - main");
    exit(1);
  }

  shell_request_loop(request_fd, return_fd);

  /* Now that the shell is gone, just loop forever
   * until we exit from inside a signal handler.
   */
  while (pause())
    ;

  /* Unreachable */
  fprintf(stderr, "Internal error: Reached unreachable point\n");
  return 1;
}
