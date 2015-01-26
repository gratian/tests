#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>

#define CLOCK CLOCK_REALTIME

static int g_nthreads = 1;
static int g_stop = 0;

enum {
	OPT_HELP = 1,
	OPT_NTHREADS
};

static struct option opt_long[] = {
	{"help", no_argument, NULL, OPT_HELP},
	{"threads", required_argument, NULL, OPT_NTHREADS},
	{NULL, 0, NULL, 0}
};

const char *opt_short = "t:h";

static void usage()
{
	printf("Usage:\n"
	       "ttest <options>\n\t"
	       "-t <NUM> --threads=<NUM>	Start <NUM> threads in parallel\n\t"
	       "-h --help			Display this help\n"
		);
}

static int parse_options(int argc, char *argv[])
{
	int c;
	int index = 0;

	for (;;) {
		c = getopt_long(argc, argv, opt_short, opt_long, &index);
		if (c < 0)
			break;
		switch (c) {
			case 't':
			case OPT_NTHREADS:
				g_nthreads = atoi(optarg);
				break;
			case '?':
			case 'h':
			case OPT_HELP:
				usage();
				return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

void* test_thread(void *param)
{
	struct timespec t;
	uint64_t interval;

	while (!g_stop) {
		t.tv_sec = 0;
		interval = rand();
		interval = (interval * 1000000LL) / RAND_MAX;
		t.tv_nsec = interval;
		clock_nanosleep(CLOCK, 0, &t, NULL);
	}

	return NULL;
}

void sighandler(int sig)
{
	g_stop = 1;
}

int main(int argc, char *argv[])
{
	pthread_t *threads;
	int i;
	struct timespec t = {0, 1000000L};	/* 1ms interval */

	mlockall(MCL_CURRENT | MCL_FUTURE);

	if (parse_options(argc, argv) == EXIT_FAILURE)
		return EXIT_FAILURE;

	threads = (pthread_t*)malloc(sizeof(pthread_t) * g_nthreads);
	if (threads == NULL) {
		fprintf(stderr, "Failed to allocate memory for %d threads\n",
			g_nthreads);
		return EXIT_FAILURE;
	}

	signal(SIGINT, sighandler);
	signal(SIGHUP, SIG_IGN);

	for (i = 0; i < g_nthreads; i++) {
		if (pthread_create(&threads[i], NULL, test_thread, NULL) != 0) {
			perror("Failed to create thread");
			return EXIT_FAILURE;
		}
	}

	while (!g_stop)
		clock_nanosleep(CLOCK, 0, &t, NULL);

	for (i = 0; i < g_nthreads; i++)
		pthread_join(threads[i], NULL);

	return EXIT_SUCCESS;
}
