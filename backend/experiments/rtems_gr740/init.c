#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <rtems.h>
#include "workload.h"

/**
 * @brief Run one warm-up and ten checked jobs on the single active CPU.
 * @param argument Unused RTEMS startup argument.
 * @return Does not return; exit status reflects numerical validation.
 */
rtems_task Init(rtems_task_argument argument)
{
  (void)argument;
  for (int job = -1; job < 10; ++job)
  {
    benchmark_initialize();
    const uint64_t start = rtems_clock_get_uptime_nanoseconds();
    benchmark_kernel();
    const uint64_t elapsed = rtems_clock_get_uptime_nanoseconds() - start;
    const int valid = benchmark_validate();
    printf("YARDA_RTEMS,job=%d,elapsed_ns=%" PRIu64 ",valid=%d\n",
           job, elapsed, valid);
    if (!valid) exit(1);
  }
  puts("YARDA_RTEMS_COMPLETE");
  exit(0);
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_SIMPLE_CONSOLE_DRIVER
#define CONFIGURE_MAXIMUM_TASKS 1
#define CONFIGURE_MAXIMUM_PROCESSORS 1
#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT
#define CONFIGURE_ENABLE_FLOATING_POINT
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT_TASK_STACK_SIZE (32 * 1024)
#define CONFIGURE_MICROSECONDS_PER_TICK 1000
#define CONFIGURE_INIT
#include <rtems/confdefs.h>
