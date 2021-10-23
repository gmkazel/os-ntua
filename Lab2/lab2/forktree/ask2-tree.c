#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "proc-common.h"
#include "tree.h"

#define SLEEP_PROC_SEC 10
#define SLEEP_TREE_SEC 3

void fork_procs(struct tree_node *node)
{
    pid_t child;
    int status;
    int i;

    change_pname(node->name);
    printf("Proccess %s has PID = %ld and %d children\n", node->name, (long)getpid(), node->nr_children);

    for (i = 0; i < node->nr_children; ++i)
    {
        fprintf(stderr, "Parent %s, PID = %ld: Creating child %s\n", node->name, (long)getpid(), node->children[i].name);
        child = fork();
        if (child < 0)
        {
            /* fork failed */
            perror("Error at children");
            exit(1);
        }

        if (child == 0)
        {
            fork_procs(&node->children[i]);
        }
    }

    for (i = 0; i < node->nr_children; ++i)
    {
        child = wait(&status);
        explain_wait_status(child, status);
    }

    if (node->nr_children == 0)
    {
        printf("%s: Sleeping...\n", node->name);
        sleep(SLEEP_PROC_SEC);
        printf("%s: Done Sleeping...\n", node->name);
    }
    printf("%s with PID = %ld is ready to terminate...\n%s: Exiting...\n", node->name, (long)getpid(), node->name);

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
int main(int argc, char *argv[])
{
    pid_t pid;
    int status;
    struct tree_node *root;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <input_tree_file>\n\n", argv[0]);
        exit(1);
    }

    root = get_tree_from_file(argv[1]);
    printf("Constructing the following process tree:\n");
    print_tree(root);

    /* Fork root of process tree */
    pid = fork();
    if (pid < 0)
    {
        perror("main: fork");
        exit(1);
    }
    if (pid == 0)
    {
        /* Child */
        fork_procs(root);
        exit(1);
    }

    /* for ask2-{fork, tree} */
    sleep(SLEEP_TREE_SEC);

    /* Print the process tree root at pid */
    show_pstree(pid);

    /* Wait for the root of the process tree to terminate */
    pid = wait(&status);
    explain_wait_status(pid, status);

    return 0;
}
