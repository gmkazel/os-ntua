pid_t pid_b, pid_c, pid_d;
int status;

change_pname("A");
printf("Proccess A has PID = %ld", (long)getpid());

// Code bellow is for child B
fprintf(stderr, "Parent A, PID = %ld: Creating child B\n", (long)getpid());
pid_b = fork();
if (pib_b < 0)
{
    /* fork failed */
    perror("B: fork");
    exit(1);
}

if (pib_b == 0)
{
    change_pname("B");
    printf("Proccess B has PID = %ld", (long)getpid());
    fprintf(stderr, "Parent B, PID = %ld: Creating child D\n", (long)getpid());
    pid_d = fork();
    if (pib_d < 0)
    {
        /* fork failed */
        perror("D: fork");
        exit(1);
    }

    if (pib_d == 0)
    {
        change_pname("D");
        printf("Proccess D has PID = %ld", (long)getpid());
        printf("D: Sleeping...\n");
        sleep(SLEEP_PROC_SEC);
        printf("D: Done Sleeping...\n");
        exit(EXIT_D);
    }

    printf("Parent B, PID = %ld: Created child D with PID = %ld, waiting for it to terminate...\n",
           (long)getpid(), (long)p);
    pid_d = wait(&status);
    explain_wait_status(pid_d, status);
    exit(EXIT_B);
}

// Code bellow is for child C
fprintf(stderr, "Parent A, PID = %ld: Creating child C\n", (long)getpid());
pid_c = fork();
if (pib_c < 0)
{
    /* fork failed */
    perror("C: fork");
    exit(1);
}

if (pib_c == 0)
{
    change_pname("C");
    printf("Proccess C has PID = %ld", (long)getpid());
    printf("C: Sleeping...\n");
    sleep(SLEEP_PROC_SEC);
    printf("C: Done Sleeping...\n");
    exit(EXIT_C);
}

printf("Parent A, PID = %ld: Created child B with PID = %ld, waiting for it to terminate...\n",
       (long)getpid(), (long)p);
pid_b = wait(&status);
explain_wait_status(pid_b, status);

printf("Parent A, PID = %ld: Created child C with PID = %ld, waiting for it to terminate...\n",
       (long)getpid(), (long)p);
pid_c = wait(&status);
explain_wait_status(pid_c, status);

exit(EXIT_A);