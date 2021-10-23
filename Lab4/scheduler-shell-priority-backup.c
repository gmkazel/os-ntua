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
queue *lp_queue;
queue *hp_queue;
queue *current_queue;
process *current_process;

/* Print a list of all tasks currently being scheduled.  */
static void sched_print_tasks(void) {
  printf("----------------HIGH PRIORITY QUEUE-----------------\n");
  if (is_empty(hp_queue)) {
    printf("THERE ARE NO PROCESSES TO PRINT\n");
  } else {
    printf("Queue has size: %u\n", get_size(hp_queue));
    printf(BLUE "Current Process: ");
    print_queue(hp_queue, false);
  }
  // printf("--------------------------------------------------\n");

  printf("\n----------------LOW PRIORITY QUEUE----------------\n");
  if (is_empty(lp_queue)) {
    printf("THERE ARE NO PROCESSES TO PRINT\n");
  } else {
    printf("Queue has size: %u\n", get_size(lp_queue));
    if (is_empty(hp_queue)) {
      printf(BLUE "Current Process: ");
    }
    print_queue(lp_queue, is_empty(hp_queue));
  }
  printf("-------------------------------------------------\n");
  return;
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int sched_kill_task_by_id(int id) {
  bool is_in_lp_queue = true;

  process *temp = get_process_by_id(lp_queue, id);
  if (temp == NULL) {
    is_in_lp_queue = false;
    temp = get_process_by_id(hp_queue, id);
  }

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

  if (is_in_lp_queue)
    dequeue(lp_queue, r_pid);
  else
    dequeue(hp_queue, r_pid);

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
    show_pstree(getpid());
    enqueue(lp_queue, NULL, pid, new_name);
  }
}

static void sched_set_high(int id) {
  process *temp = NULL;
  if (!is_empty(lp_queue)) {
    temp = get_process_by_id(lp_queue, id);
  }
  if (temp == NULL) {
    temp = get_process_by_id(hp_queue, id);
    if (temp == NULL) {
      printf(RED "Process not in queues" RST "\n");
    } else {
      printf("Process already has " MAGENTA "HIGH " RST "priority\n");
    }
  } else {
    process *new_node = initialize_process(temp->pid, temp->name, temp->id);
    enqueue(hp_queue, new_node, new_node->pid, new_node->name);
    dequeue(lp_queue, temp->pid);
    printf("Process now has " MAGENTA "HIGH " RST "priority\n");
  }
}

static void sched_set_low(int id) {
  process *temp = NULL;
  if (!is_empty(hp_queue)) {
    temp = get_process_by_id(hp_queue, id);
  }
  if (temp == NULL) {
    temp = get_process_by_id(lp_queue, id);
    if (temp == NULL) {
      printf(RED "Process not in queues" RST "\n");
    } else {
      printf("Process already has " MAGENTA "LOW " RST "priority\n");
    }
  } else {
    process *new_node = initialize_process(temp->pid, temp->name, temp->id);
    enqueue(lp_queue, new_node, new_node->pid, new_node->name);
    dequeue(hp_queue, temp->pid);
    printf("Process now has " MAGENTA "LOW " RST "priority\n");
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

    case REQ_HIGH_TASK:
      sched_set_high(rq->task_arg);
      return 0;

    case REQ_LOW_TASK:
      sched_set_low(rq->task_arg);
      return 0;

    default:
      return -ENOSYS;
  }
}

/*
 * SIGALRM handler
 */
static void sigalrm_handler(int signum) {
  // printf(RED "IN SIGALARM" RST "\n");
  if (signum != SIGALRM) {
    fprintf(stderr, "Internal error: Called for signum %d, not SIGALRM\n",
            signum);
    exit(1);
  }

  // kill the proccess
  if (kill(current_process->pid, SIGSTOP) < 0) {
    perror("kill- sigalrm_handler");
    exit(1);
  }
  // printf(RED "OUT SIGALARM" RST "\n");
}

/*
 * SIGCHLD handler
 */
static void sigchld_handler(int signum) {
  // printf(RED "IN sigchld_handler" RST "\n");
  if (signum != SIGCHLD) {
    fprintf(stderr, "Internal error: Called for signum %d, not SIGCHLD\n",
            signum);
    exit(1);
  }

  if (is_empty(hp_queue)) {
    // printf(RED "IN hp empty" RST "\n");
    current_queue = lp_queue;
  } else {
    // printf(RED "IN hp not empty" RST "\n");
    current_queue = hp_queue;
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
      if (!is_empty(current_queue))
        dequeue(current_queue, current_queue->head->pid);
      // get in if elements have high priority
      if (!is_empty(hp_queue)) {
        // printf(RED "11" RST "\n");
        rotate_queue(hp_queue);
        current_process = hp_queue->head;

        fprintf(stderr, "Proccess with pid=%ld is about to begin...\n",
                (long int)current_process->pid);

        if (kill(current_process->pid, SIGCONT) < 0) {
          perror(
              "Continue to process- WIFEXITED-WIFSIGNALED -sigchld_handler1");
          exit(1);
        }
      } else  // get in if there are no high priority elements
      {
        // printf(RED "12" RST "\n");
        if (is_empty(lp_queue)) {  // both queues are empty
          // printf(RED "13" RST "\n");
          printf(GREEN "Job's Done!\n" RST);
          exit(0);
        } else {
          // printf(RED "14" RST "\n");
          if (current_queue == hp_queue) {  // previous queue is high queue
            current_process = lp_queue->head;
            if (kill(current_process->pid, SIGCONT) < 0) {
              perror(
                  "Continue to process-WIFEXITED-WIFSIGNALED-sigchld_handler2");
              exit(1);
            }
          } else {  // continue to low queue
            // printf(RED "15" RST "\n");
            rotate_queue(lp_queue);
            current_process = lp_queue->head;
            if (kill(current_process->pid, SIGCONT) < 0) {
              perror(
                  "Continue to process-WIFEXITED-WIFSIGNALED-sigchld_handler3");
              exit(1);
            }
          }
        }
      }
      /* Setup the alarm again */
      if (alarm(SCHED_TQ_SEC) < 0) {
        perror("alarm- sigchld_handler");
        exit(1);
      }
    }
    if (WIFSTOPPED(status)) {
      // printf(RED "IN WIFSTOPPED" RST "\n");
      /* A child has stopped due to SIGSTOP/SIGTSTP, etc... */
      printf("Parent: Child has been stopped. Moving right along...\n");

      if (is_empty(current_queue)) {  // both queues are empty
        // printf(RED "13" RST "\n");
        printf(GREEN "Job's Done!\n" RST);
        exit(0);
      }

      // rotate queue
      rotate_queue(current_queue);
      current_process = current_queue->head;
      pid_t r_pid = current_queue->head->pid;

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

/* Disable elivery of SIGALRM and SIGCHLD. */
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
  enqueue(lp_queue, NULL, p, new_name);

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

  lp_queue = initialize_queue();  // to initialize the low priority queue
  hp_queue = initialize_queue();  // to initialize the high priority queue

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
      enqueue(lp_queue, NULL, pid, new_name);
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

  if (kill(lp_queue->head->pid, SIGCONT) < 0) {
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
