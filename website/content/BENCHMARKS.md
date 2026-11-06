# Measured benchmarks

Measured on 2026-11-05 using Release builds of Nash and the unmodified ang.sh reference.
GCC 16.2.1; Linux-7.2.14-zen1-1-zen-x86_64-with-glibc2.44.

Forty samples per case after three warmups, seeded interleaving, Python subprocess wall time.
Configuration loading is disabled for every shell. These measurements include process launch
and measure **noninteractive execution**, not interactive startup or Starship prompt latency.
The host is shared and results are noisy; min/max values are preserved in the JSON.

| Shell | `true` startup | external `/usr/bin/true` | two-stage pipeline |
|---|---:|---:|---:|
| nash | 6.52 ms | 8.57 ms | 11.78 ms |
| bash | 4.331 ms | 6.239 ms | 8.906 ms |
| fish | 7.35 ms | 8.768 ms | 8.626 ms |
| ang.sh | 5.861 ms | 6.156 ms | 9.903 ms |

Nash is slower than Bash and the reference in these runs; optimization is future work.
No interactive-startup, idle-CPU or memory measurements are claimed.

Reproduce (build the reference outside its source tree):

```sh
python3 tests/benchmark.py build-release/nash --reference /tmp/nash-reference-build/angsh --output docs/benchmark-results.json
```

See [raw measurements](benchmark-results.json).
