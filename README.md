
This hrTimer test bench is ispired from "rt-tests" tool set how to make (soft) real time
thread system into Linux application. Hearth of timing scheduler thread function is quite
similar as in "cyclictest" application.

- sudo apt install rt-tests
- sudo cyclictest -l100000 -m -S -p90 -i1000 -h1000 -q > output.txt

There are some odd things

- cyclictest    average latency is < 60 us (with original RasPI (model 1) and BeagleBone AI)
- hrTimer  measure average latency < 150 us
- cyclictest worst case latency is < 1 ms
- hrTimer    worst case latency is randomly even 10 ... 15 ms (not often but still some)
- hrTimer worst case measurements are a bit better when run CODESYS runtime or cyclictest
  applications same time parallel with hrTimer application

https://kernel.googlesource.com/pub/scm/linux/kernel/git/clrkwllms/rt-tests/+/refs/heads/master/src/cyclictest/cyclictest.c

https://source.denx.de/Xenomai/xenomai/tree/b8311643558097efc014803841ae20cdf4e7b001/demo/posix/cyclictest