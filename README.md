# RT debug data and tests

Various tests and data captures used in debugging real-time latency regressions.

  * config - kernel configs used.
  * cyclictest_deferred_timer_load - cyclictest results with hrtimer load when deferring non-rt timers to softirq.
  * cyclictest_hackb_iperf_load - cyclictest results with hackbench + iperf load.
  * cyclictest_timer_load - cyclictest results with hrtimer load.
  * hack - quick hack patch to allow deferring non-rt hrtimers to softirq.
  * timer-stress - hrtimer load using a clock_nanosleep()/clock_gettime() stress test.
  * traces - kernel traces showing latency regressions when timers get stacked.
