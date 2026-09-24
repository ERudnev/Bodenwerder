"""Per-output compile durations from a ninja log. Usage: python tools/buildstats/ninja_times.py <build-dir> [--top N]"""
import sys, os, collections
build = sys.argv[1]; top = int(sys.argv[sys.argv.index('--top')+1]) if '--top' in sys.argv else 15
latest = {}
for line in open(os.path.join(build, '.ninja_log'), errors='ignore'):
    if line.startswith('#'): continue
    parts = line.rstrip('\n').split('\t')
    if len(parts) < 4: continue
    start, end, _, out = int(parts[0]), int(parts[1]), parts[2], parts[3]
    if not out.endswith('.obj') or '3party' in out: continue
    latest[out] = (end - start) / 1000.0
rows = sorted(latest.items(), key=lambda kv: -kv[1])
total = sum(v for _, v in rows)
print(f"project objects={len(rows)} total compile CPU={total:.0f} s  mean={total/max(len(rows),1):.1f} s  max={rows[0][1]:.1f} s")
buckets = collections.Counter()
for _, d in rows: buckets['<2 s' if d < 2 else '2-4 s' if d < 4 else '4-8 s' if d < 8 else '8-16 s' if d < 16 else '>16 s'] += 1
print("  distribution:", dict(buckets))
for out, d in rows[:top]: print(f"  {d:6.1f} s  {out.split('.dir/')[-1] if '.dir/' in out else out}")
