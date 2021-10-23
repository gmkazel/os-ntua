/*
 * mandel.c
 *
 * A program to draw the Mandelbrot Set on a 256-color xterm.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

int NTHREADS = 1;

// .................................
#define perror_pthread(ret, msg) \
	do                           \
	{                            \
		errno = ret;             \
		perror(msg);             \
	} while (0)

typedef struct
{
	pthread_t tid; /* POSIX thread id, as returned by the library */
	int line;
	sem_t sem;
} thread_info_struct;

thread_info_struct *threads;

// .................................

/***************************
 * Compile-time parameters *
 ***************************/

/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
int y_chars = 50;
int x_chars = 90;

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;

/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep;
double ystep;

/*
 * This function computes a line of output
 * as an array of x_char color values.
 */
void compute_mandel_line(int line, int color_val[])
{
	/*
	 * x and y traverse the complex plane.
	 */
	double x, y;

	int n;
	int val;

	/* Find out the y value corresponding to this line */
	y = ymax - ystep * line;

	/* and iterate for all points on this line */
	for (x = xmin, n = 0; n < x_chars; x += xstep, n++)
	{

		/* Compute the point's color value */
		val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
		if (val > 255)
			val = 255;

		/* And store it in the color_val[] array */
		val = xterm_color(val);
		color_val[n] = val;
	}
}

/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */
void output_mandel_line(int fd, int color_val[])
{
	int i;

	char point = '@';
	char newline = '\n';

	for (i = 0; i < x_chars; i++)
	{
		/* Set the current color, then output the point */
		set_xterm_color(fd, color_val[i]);
		if (write(fd, &point, 1) != 1)
		{
			perror("compute_and_output_mandel_line: write point");
			exit(1);
		}
	}

	/* Now that the line is done, output a newline character */
	if (write(fd, &newline, 1) != 1)
	{
		perror("compute_and_output_mandel_line: write newline");
		exit(1);
	}
}

// .................................
void usage(char *argv0)
{
	fprintf(stderr, "Usage: %s NTHREADS\n\n"
					"Exactly one argument required:\n"
					"    NTHREADS: The number of threads.\n",
			argv0);
	exit(1);
}

int safe_atoi(char *s, int *val)
{
	long l;
	char *endp;

	l = strtol(s, &endp, 10);
	if (s != endp && *endp == '\0')
	{
		*val = l;
		return 0;
	}
	else
		return -1;
}

void *safe_malloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
	{
		fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n",
				size);
		exit(1);
	}

	return p;
}
// .................................

/*
 * This function provides user the safety for reseting all character
 * attributes before leaving,to ensure the prompt is
 * not drawn in a fancy color.
 */

void unexpectedSignal(int sign)
{
	signal(sign, SIG_IGN);
	reset_xterm_color(1);
	exit(1);
}

void *compute_and_output_mandel_line(void *argument)
{

	int line = *(int *)argument;
	int i;

	/*
	 * A temporary array, used to hold color values for the line being drawn
	 */
	int color_val[x_chars];

	for (i = line; i < y_chars; i += NTHREADS)
	{
		//compute mandel line
		compute_mandel_line(i, color_val);

		//each thread should wait and so it uses sem_wait func
		//with argument the semaphore referring to this particular thread
		sem_wait(&threads[i % NTHREADS].sem);

		//output mandel line, STDOUT_FILENO for standard output
		output_mandel_line(STDOUT_FILENO, color_val);

		// send signal to the next thread
		sem_post(&threads[((i % NTHREADS) + 1) % NTHREADS].sem);
	}

	return NULL;
}

int main(int argc, char *argv[])
{

	// .................................
	if (argc != 2)
		usage(argv[0]);
	if (safe_atoi(argv[1], &NTHREADS) < 0 || NTHREADS <= 0)
	{
		fprintf(stderr, "`%s' is not valid for `NTHREADS'\n", argv[1]);
		exit(1);
	}

	signal(SIGINT, unexpectedSignal);

	xstep = (xmax - xmin) / x_chars;
	ystep = (ymax - ymin) / y_chars;

	int line;
	// .................................

	/* Create threads */
	threads = safe_malloc(NTHREADS * sizeof(*threads));

	// set the first semaphore as not asleep
	if ((sem_init(&threads[0].sem, 0, 1)) == -1)
	{
		perror("initialization of semaphores");
		exit(1);
	}

	// set the rest of the semaphores to wait
	for (line = 1; line < NTHREADS; ++line)
	{
		if ((sem_init(&threads[line].sem, 0, 0)) == -1)
		{
			perror("initialization of semaphores");
			exit(1);
		}
	}

	/*
	 * Create NTHREADS
	*/
	for (line = 0; line < NTHREADS; ++line)
	{
		threads[line].line = line;
		if ((
				pthread_create(&threads[line].tid, NULL, compute_and_output_mandel_line, &threads[line].line)) != 0)
		{
			perror("creation of threads");
			exit(1);
		}
	}

	/*
	 * Wait for all threads to terminate
	 */
	for (line = 0; line < NTHREADS; ++line)
	{
		int ret = pthread_join(threads[line].tid, NULL);
		if (ret)
		{
			perror_pthread(ret, "pthread_join");
			exit(1);
		}
	}

	// destroy all semaphores
	for (line = 0; line < NTHREADS; ++line)
	{
		sem_destroy(&threads[line].sem);
	}

	// free allocated space
	free(threads);

	reset_xterm_color(1);
	return 0;
}
