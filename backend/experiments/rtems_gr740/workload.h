#ifndef YARDA_RTEMS_WORKLOAD_H
#define YARDA_RTEMS_WORKLOAD_H

/** @brief Reset inputs outside the analyzed region. @return Nothing. */
void benchmark_initialize(void);

/** @brief Execute one statically bounded memory workload. @return Nothing. */
void benchmark_kernel(void);

/** @brief Check every output against a closed-form reference. @return One on success. */
int benchmark_validate(void);

#endif
