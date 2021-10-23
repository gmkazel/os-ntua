#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "proc-common.h"
#include "tree.h"

#define SLEEP_PROC_SEC 10
#define SLEEP_TREE_SEC 3

void fork_procs(struct tree_node *node, int pfd[]) {
  pid_t child;
  int status;
  int i;

  change_pname(node->name);
  printf("Proccess %s has PID = %ld\n", node->name, (long)getpid());

  if (node->nr_children == 0) {
    printf("%s: Sleeping...\n", node->name);
    sleep(SLEEP_PROC_SEC);
    printf("%s: Done Sleeping...\n", node->name);

    if (close(pfd[0]) < 0) {
      perror("closing pipe");
      exit(1);
    }

    int val = atoi(node->name);

    if (write(pfd[1], &val, sizeof(val)) != sizeof(int)) {
      perror("writing in pipe");
      exit(1);
    }
  } else {
    int operators[2] = {0, 0};
    int newpipe[2];

    if (pipe(newpipe) < 0) {
      perror("pipe");
      exit(1);
    }

    for (i = 0; i < 2; ++i) {
      fprintf(stderr, "Parent %s, PID = %ld: Creating child %s\n", node->name,
              (long)getpid(), node->children[i].name);
      child = fork();
      if (child < 0) {
        /* fork failed */
        perror("Error at children");
        exit(1);
      }

      if (child == 0) {
        fork_procs(&node->children[i], newpipe);
      }
    }

    if (close(newpipe[1]) < 0) {
      perror("closing pipe");
      exit(1);
    }

    for (i = 0; i < 2; i++) {
      if (read(newpipe[0], &operators[i], sizeof(operators[i])) !=
          sizeof(operators[i])) {
        perror("Reading pipe");
        exit(1);
      }
    }

    int res;
    if (!strcmp(node->name, "+")) {
      res = operators[0] + operators[1];
      printf("Current operation is: %d + %d = %d\n", operators[0], operators[1],
             res);
    } else {
      res = operators[0] * operators[1];
      printf("Current operation is: %d * %d = %d\n", operators[0], operators[1],
             res);
    }

    if (close(pfd[0]) < 0) {
      perror("closing pipe");
      exit(1);
    }

    if (write(pfd[1], &res, sizeof(res)) != sizeof(res)) {
      perror("writing in pipe");
      exit(1);
    }

    for (i = 0; i < 2; ++i) {
      child = wait(&status);
      explain_wait_status(child, status);
    }
  }
  printf("%s with PID = %ld is ready to terminate...\n%s: Exiting...\n",
         node->name, (long)getpid(), node->name);
  exit(0);
}

/* The initial process forks the root of the process tree,
 * waits for the process tree to be completely created,
 * then takes a photo of it using show_pstree().
 *`
 * How to wait for the process tree to be ready?
 * In ask2-{fork, tree}:
 *      wait for a few seconds, hope for the best.
 * In ask2-signals:
 *      use wait_for_ready_children() to wait until
 *      the first process raises SIGSTOP.
 */
int main(int argc, char *argv[]) {
  pid_t pid;
  int status;
  int pfd[2];
  struct tree_node *root;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <input_tree_file>\n\n", argv[0]);
    exit(1);
  }

  root = get_tree_from_file(argv[1]);
  printf("Constructing the following process tree:\n");
  print_tree(root);

  printf("Parent: Creating pipe...\n");
  if (pipe(pfd) < 0) {
    perror("pipe");
    exit(1);
  }

  printf("Parent: Creating child...\n");
  pid = fork();
  if (pid < 0) {
    /* fork failed */
    perror("main: fork");
    exit(1);
  }
  if (pid == 0) {
    fork_procs(root, pfd);
    exit(1);
  }

  /*
   * In parent process.
   */
  sleep(SLEEP_TREE_SEC);
  show_pstree(pid);

  int res;
  if (read(pfd[0], &res, sizeof(res)) != sizeof(res)) {
    perror("read from pipe");
    exit(1);
  }

  /* Print the process tree root at pid */

  pid = wait(&status);
  explain_wait_status(pid, status);

  printf("The final result is %d\n", res);

  return 0;
}
