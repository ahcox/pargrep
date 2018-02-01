# pargrep

**----WIP----**

The beginnings of a simple parallel grep implementation in modern C++.
Mono thread, dual thread, and many thread variants of the core algorithm
for searching lines of an input stream for a regex have been implemented.
Pending some profiling to motivate further work.

* Reference mono thread version: [grep_stream()](https://github.com/ahcox/pargrep/blob/master/src/pargrep.cpp#L36).
* Splitting off writing into a separate thread (unlikely to benefit performance): [pargrep_stream_par1()](https://github.com/ahcox/pargrep/blob/master/src/pargrep.cpp#L375)
* Spawning the per-line regex evaluations in their own threads: [pargrep_stream_par2()](https://github.com/ahcox/pargrep/blob/master/src/pargrep.cpp#L472).
