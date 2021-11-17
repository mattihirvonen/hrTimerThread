
This hrTimer test bench is ispired from "rt-tests" tool set how to make (soft) real time
thread system into Linux application. Hearth of timing scheduler thread function is quite
similar as in "cyclictest" application.

- sudo apt install rt-tests
- sudo cyclictest -l100000 -m -S -p90 -i1000 -h1000 -q > output.txt

NOTE(S):
- application must run with root privileges.
- application write data to named shared memory /dev/shm/RT_METRICS
- writing non zero value to first shared memory integer reset metrics

Results:
- BeagleBone AI average latency < 20 us (worst case typically < 50 us), dual core 1.5GHz
- Raspberry  PI average latency < 60 us (worst case typically < 1 ms), single core 700 MHz

https://kernel.googlesource.com/pub/scm/linux/kernel/git/clrkwllms/rt-tests/+/refs/heads/master/src/cyclictest/cyclictest.c

https://source.denx.de/Xenomai/xenomai/tree/b8311643558097efc014803841ae20cdf4e7b001/demo/posix/cyclictest
