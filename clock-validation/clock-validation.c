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
#include <assert.h>
#include <sys/mman.h>

#define DEBUG 0

#define SCHED_POLICY	SCHED_FIFO
#define CLOCK_ID	CLOCK_MONOTONIC
#define SLEEP_INTERVAL	513313LL
#define DELAY_INTERVAL	50000000LL
#define NS_PER_SECOND	1000000000LL

#define FILE_PATH	"test.dat"

static int g_cpu = -1;
static int g_priority = -1;
static uint32_t g_loops = 0;

#define ZYNQ_SLCR	0xF8000000
static const off_t	k_slcr_mio_pin_42 = 0x000007A8;

#define ZYNQ_GPIO	0xE000A000
static const off_t	k_gpio_bank1 = 0x00000044;
static const uint32_t	k_gpio_42_mask = (1 << (42 - 32/*bank1*/));

static void *g_zynq_gpio = NULL;
static uint32_t *g_zynq_gpio_bank1 = NULL;
static int g_mem_fd = -1;

enum {
	OPT_HELP = 1,
	OPT_AFFINITY,
	OPT_PRIORITY,
	OPT_LOOPS
};

static struct option opt_long[] = {
	{"help", no_argument, NULL, OPT_HELP},
	{"affinity", optional_argument, NULL, OPT_AFFINITY},
	{"priority", required_argument, NULL, OPT_PRIORITY},
	{"loops", required_argument, NULL, OPT_LOOPS},
	{NULL, 0, NULL, 0}
};

const char *opt_short = "a:p:l:h";

static void usage()
{
	printf("Usage:\n"
	       "clock-validation <options>\n\t"
	       "-a <cpu> --affinity=<cpu>	Run thread on processor <cpu>\n\t"
	       "-h --help			Display this help\n\t"
	       "-p <prio> --priority=<prio>	Run thread with RT priority <prio>\n\t"
	       "-l <n> --loops=<n>		Run test for <n> interations\n"
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
			case 'a':
			case OPT_AFFINITY:
				g_cpu = atoi(optarg);
				break;
			case 'p':
			case OPT_PRIORITY:
				g_priority = atoi(optarg);
				break;
			case 'l':
			case OPT_LOOPS:
				g_loops = atoi(optarg);
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

static int init_io(void)
{
	void *slcr;
	volatile uint32_t *reg;
	int page_sz = getpagesize();
	uint32_t mask = (page_sz - 1);

	if ((g_mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
		perror("Failed to open /dev/mem");
		return errno;
	}

	/* configure GPIO output */
	slcr = mmap(0, page_sz, PROT_READ | PROT_WRITE, MAP_SHARED, g_mem_fd,
		    ZYNQ_SLCR & ~mask);
	if (slcr <= 0) {
		perror("SLCR mmap failed");
		return errno;
	}

	reg = (uint32_t*)(slcr + (k_slcr_mio_pin_42 & mask));
	*reg = (*reg & ~0xFF);	/* zero [7:0] bits select GPIO from pin mux */
	if (munmap(slcr, page_sz) < 0) {
		perror("SLCR unmap failed");
		return errno;
	}

	/* map GPIO */
	g_zynq_gpio = mmap(0, page_sz, PROT_READ | PROT_WRITE, MAP_SHARED,
			   g_mem_fd, ZYNQ_GPIO & ~mask);
	if (g_zynq_gpio <= 0) {
		perror("GPIO mmap failed");
		return errno;
	}
	g_zynq_gpio_bank1 = (uint32_t*)(g_zynq_gpio + (k_gpio_bank1 & mask));
}

static void uninit_io(void)
{
	if (g_zynq_gpio)
		munmap(g_zynq_gpio, getpagesize());

	if (g_mem_fd >= 0)
		close(g_mem_fd);
}

static uint32_t gpio_42_out(uint32_t state)
{
	uint32_t reg;

	reg = *g_zynq_gpio_bank1 & k_gpio_42_mask;
	if (state)
		*g_zynq_gpio_bank1 = reg | k_gpio_42_mask;
	else
		*g_zynq_gpio_bank1 = reg & ~k_gpio_42_mask;

	return reg;
}

static inline uint32_t diff_gettime(struct timespec *start)
{
	uint64_t s, e;
	struct timespec end;

	clock_gettime(CLOCK_ID, &end);
	assert_perror(errno);

	s = (uint64_t)(start->tv_sec) * NS_PER_SECOND + start->tv_nsec;
	e = (uint64_t)(end.tv_sec) * NS_PER_SECOND + end.tv_nsec;

	return (uint32_t)(e - s);
}

void* test_thread(void *param)
{
	int i;
	cpu_set_t mask;
	struct sched_param schedp;
	pthread_t thread = pthread_self();
	uint32_t *data = (uint32_t*)param;
	struct timespec interval = { 0, SLEEP_INTERVAL };
	struct timespec delay = { 0, DELAY_INTERVAL };
	struct timespec ts;

	if (g_cpu >= 0) {
		CPU_ZERO(&mask);
		CPU_SET(g_cpu, &mask);
		if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
			perror("Failed to set the CPU affinity");
			return NULL;
		}
	}

	if (g_priority < 0)
		g_priority = sched_get_priority_min(SCHED_POLICY);
	else if (g_priority > sched_get_priority_max(SCHED_POLICY))
		g_priority = sched_get_priority_max(SCHED_POLICY);

	if (g_priority >= 0) {
		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = g_priority;
		if (sched_setscheduler(0, SCHED_POLICY, &schedp) == -1) {
			perror("Failed to set the test thread priority");
			return NULL;
		}
	}

	gpio_42_out(0);
	for (i=0; i < g_loops; i++) {
		clock_nanosleep(CLOCK_ID, 0, &delay, NULL);

		gpio_42_out(1);
		clock_gettime(CLOCK_ID, &ts);
		assert_perror(errno);

		clock_nanosleep(CLOCK_ID, 0, &interval, NULL);
		assert_perror(errno);

		gpio_42_out(0);
		data[i] = diff_gettime(&ts);
	}
}

#if DEBUG
void print_data(uint32_t *data)
{
	int i;
	for (i = 0; i < g_loops; i++)
		printf("%u\n", data[i]);
}
#endif

void write_data(const char* path, uint32_t *data)
{
	size_t ret;
	FILE *f = fopen(path, "w+");
	
	if (!f) {
		perror("Failed to open file");
		return;
	}
	
	ret = fwrite(data, sizeof(uint32_t), g_loops, f);
	assert(ret == g_loops);

	fflush(f);
	fclose(f);
}

int main(int argc, char *argv[])
{
	int ret;
	pthread_t th;
	cpu_set_t mask;
	uint32_t *data;

	if (parse_options(argc, argv) == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (g_cpu >= 0) {
		CPU_ZERO(&mask);
		CPU_SET(g_cpu, &mask);
		if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
			perror("Failed to set the CPU affinity");
			return EXIT_FAILURE;
		}
	}

	if (g_loops <= 0) {
		usage();
		return EXIT_FAILURE;
	}

	data = (uint32_t*)malloc(g_loops * sizeof(uint32_t));
	if (!data) {
		fprintf(stderr, "Failed to allocate memory for %d loops\n",
			g_loops);
		return EXIT_FAILURE;
	}

	if (init_io() < 0)
		return EXIT_FAILURE;

	ret = pthread_create(&th, NULL, test_thread, data);
	pthread_join(th, NULL);
#if DEBUG
	print_data(data);
#endif
	write_data(FILE_PATH, data);

	uninit_io();

	return EXIT_SUCCESS;
}
