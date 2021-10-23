#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "proc-common.h"

#define SLEEP_PROC_SEC 10
#define SLEEP_TREE_SEC 3

#define EXIT_A 16
#define EXIT_B 19
#define EXIT_C 17
#define EXIT_D 13

/*
 * Create this process tree:
 * A-+-B---D
 *   `-C
 */
void fork_procs(void) {
  /*
   * initial process is A.
   */
  pid_t pid_b, pid_c, pid_d;
  int status;

  change_pname("A");
  printf("Proccess A has PID = %ld\n", (long)getpid());

  // Code bellow is for child B
  fprintf(stderr, "Parent A, PID = %ld: Creating child B\n", (long)getpid());
  pid_b = fork();
  if (pid_b < 0) {
    /* fork failed */
    perror("B: fork");
    exit(1);
  }

  if (pid_b == 0) {
    change_pname("B");
    printf("Proccess B has PID = %ld\n", (long)getpid());
    fprintf(stderr, "Parent B, PID = %ld: Creating child D\n", (long)getpid());
    pid_d = fork();
    if (pid_d < 0) {
      /* fork failed */
      perror("D: fork");
      exit(1);
    }

    if (pid_d == 0) {
      change_pname("D");
      printf("Proccess D has PID = %ld\n", (long)getpid());
      printf("D: Sleeping...\n");
      sleep(SLEEP_PROC_SEC);
      printf("D: Done Sleeping...\n");
      printf("D with PID = %ld is ready to terminate...\nD: Exiting...\n",
             (long)getpid());
      exit(EXIT_D);
    }

    printf(
        "Parent B, PID = %ld: Created child D with PID = %ld, waiting for it "
        "to terminate...\n",
        (long)getpid(), (long)pid_d);
    pid_d = wait(&status);
    explain_wait_status(pid_d, status);
    printf("B with PID = %ld is ready to terminate...\nB: Exiting...\n",
           (long)getpid());
    exit(EXIT_B);
  }

  // Code bellow is for child C
  fprintf(stderr, "Parent A, PID = %ld: Creating child C\n", (long)getpid());
  pid_c = fork();
  if (pid_c < 0) {
    /* fork failed */
    perror("C: fork");
    exit(1);
  }

  if (pid_c == 0) {
    change_pname("C");
    printf("Proccess C has PID = %ld\n", (long)getpid());
    printf("C: Sleeping...\n");
    sleep(SLEEP_PROC_SEC);
    printf("C: Done Sleeping...\n");
    printf("C with PID = %ld is ready to terminate...\nC: Exiting...\n",
           (long)getpid());
    exit(EXIT_C);
  }

  printf(
      "Parent A, PID = %ld: Created child C with PID = %ld, waiting for it to "
      "terminate...\n",
      (long)getpid(), (long)pid_c);
  pid_c = wait(&status);
  explain_wait_status(pid_c, status);

  printf(
      "Parent A, PID = %ld: Created child B with PID = %ld, waiting for it to "
      "terminate...\n",
      (long)getpid(), (long)pid_b);
  pid_b = wait(&status);
  explain_wait_status(pid_b, status);
  printf("A with PID = %ld is ready to terminate...\nA: Exiting...\n",
         (long)getpid());
  exit(EXIT_A);
}

/*
 * The initial process forks the root of the process tree,
 * waits for the process tree to be completely created,
 * then takes a photo of it using show_pstree().
 *
 * How to wait for the process tree to be ready?
 * In ask2-{fork, tree}:
 *      wait for a few seconds, hope for the best.
 * In ask2-signals:
 *      use wait_for_ready_children() to wait until
 *      the first process raises SIGSTOP.
 */
int main(void) {
  pid_t pid;
  int status;

  /* Fork root of process tree */
  pid = fork();
  if (pid < 0) {
    perror("main: fork");
    exit(1);
  }
  if (pid == 0) {
    /* Child */
    fork_procs();
    exit(1);
  }

  /*
   * Father
   */

  /* for ask2-{fork, tree} */
  sleep(SLEEP_TREE_SEC);

  /* Print the process tree root at pid */
  show_pstree(pid);

  /* Wait for the root of the process tree to terminate */
  pid = wait(&status);
  explain_wait_status(pid, status);

  return 0;
}
