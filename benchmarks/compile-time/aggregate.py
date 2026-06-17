import csv, sys, statistics
from collections import defaultdict

# read all result CSVs, drop warmup (run 1), aggregate
data = defaultdict(list)   # (compiler,example,backend) -> [secs...]
for path in sys.argv[1:]:
    with open(path) as f:
        for row in csv.reader(f):
            if len(row) != 5: continue
            comp, ex, be, run, secs = row
            if secs == "NA": continue
            if int(run) == 1: continue   # warmup
            data[(comp, ex, be)].append(float(secs))

# per-example table per compiler
comps = sorted({k[0] for k in data})
exes = sorted({k[1] for k in data})
backs = ["pfr","p1061","refl"]

def med(k): 
    v=data.get(k); return statistics.median(v) if v else None
def mn(k):
    v=data.get(k); return min(v) if v else None

for comp in comps:
    print(f"\n===== {comp}  (median seconds, -O0 -c; warmup dropped) =====")
    print(f"{'example':<16} {'pfr':>9} {'p1061':>9} {'refl':>9}   {'refl/pfr':>9} {'refl/p1061':>11}")
    sums={b:[] for b in backs}
    for ex in exes:
        cells=[]
        for b in backs:
            m=med((comp,ex,b))
            cells.append(m)
            if m: sums[b].append(m)
        rp = (cells[2]/cells[0]) if cells[0] and cells[2] else None
        r1 = (cells[2]/cells[1]) if cells[1] and cells[2] else None
        def fmt(x): return f"{x:.3f}" if x else "  -  "
        print(f"{ex:<16} {fmt(cells[0]):>9} {fmt(cells[1]):>9} {fmt(cells[2]):>9}   {(f'{rp:.2f}x' if rp else '-'):>9} {(f'{r1:.2f}x' if r1 else '-'):>11}")
    # averages across examples
    avg={b:(statistics.mean(sums[b]) if sums[b] else None) for b in backs}
    print("-"*70)
    print(f"{'AVG (per obj)':<16} {avg['pfr']:>9.3f} {avg['p1061']:>9.3f} {avg['refl']:>9.3f}   {avg['refl']/avg['pfr']:>8.2f}x {avg['refl']/avg['p1061']:>10.2f}x")
    # min across runs (best-case) average too
    minsum={b:[] for b in backs}
    for ex in exes:
        for b in backs:
            m=mn((comp,ex,b))
            if m: minsum[b].append(m)
    print(f"{'AVG min (best)':<16} {statistics.mean(minsum['pfr']):>9.3f} {statistics.mean(minsum['p1061']):>9.3f} {statistics.mean(minsum['refl']):>9.3f}")
