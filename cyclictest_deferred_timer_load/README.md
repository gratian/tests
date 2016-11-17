cyclictest results when using a hrtimer load[1] in combination with hack for deferring non-RT timers to the timer softirq[2].

  * deferred_timer_load.hist - cyclictest histogram output.
  * deferred_timer_load.log - cyclictest console output.
  * deferred_timer_load.run - script used to run the test.

Hardware: Xilinx Zynq 7020 dual-core ARMv7 @ 667MHz.

[1] timer-stress/ttest.c
[2] hack/0001-sysfs-hrtimer-Add-sysfs-knob-to-defer-non-rt-timers-.patch