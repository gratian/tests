# RT debug data and tests

Various tests and data captures used in debugging the real-time regressions
observed in 3.14-rt versus 3.2-rt on ARM.

  * clock-validation - Clock configuration/validation using an external
  acquisition system.

  * hrtimer_interrupt-profiling - Low level profiling of hrtimer_interrupt()
  under stress.

  * register-dumps - Various peripheral register dumps used to confirm the
  hardware configuration in 3.2 vs. 3.14.

  * timer-stress - clock_nanosleep()/clock_gettime() stress test.
