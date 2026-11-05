#!/usr/bin/env python3
"""Reproducible warm noninteractive process benchmarks; never measures interactive startup."""
import argparse, json, pathlib, platform, random, shutil, statistics, subprocess, time
p=argparse.ArgumentParser();p.add_argument('nash');p.add_argument('--reference');p.add_argument('--samples',type=int,default=40);p.add_argument('--output',default='benchmark-results.json');args=p.parse_args()
shells={'nash':[str(pathlib.Path(args.nash).resolve()),'--no-config']}
for name,flags in [('bash',['--noprofile','--norc']),('fish',['--no-config'])]:
    if path:=shutil.which(name):shells[name]=[path]+flags
if args.reference:shells['ang.sh']=[str(pathlib.Path(args.reference).resolve()),'--no-config','--posix']
work={'startup_true':'true','external':'/usr/bin/true','pipeline':"printf hello | cat > /dev/null"}
results={name:{case:[] for case in work} for name in shells}
for name,argv in shells.items():
    for case,source in work.items():
        for _ in range(3):subprocess.run(argv+['-c',source],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
schedule=[(name,case) for name in shells for case in work for _ in range(args.samples)];random.Random(0).shuffle(schedule)
for name,case in schedule:
    start=time.perf_counter_ns();subprocess.run(shells[name]+['-c',work[case]],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
    results[name][case].append((time.perf_counter_ns()-start)/1e6)
report={'platform':platform.platform(),'samples':args.samples,'method':'3 warmups; seeded interleaving; subprocess wall milliseconds; no configs; no prompt rendering','results':{name:{case:{'median_ms':round(statistics.median(values),3),'min_ms':round(min(values),3),'max_ms':round(max(values),3)} for case,values in cases.items()} for name,cases in results.items()}}
pathlib.Path(args.output).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
