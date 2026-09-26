import json, sys, collections, re
def strip_t(s):
    out=[];d=0
    for ch in s:
        if ch=='<': d+=1
        if d==0: out.append(ch)
        if ch=='>': d-=1
    return ''.join(out)
for name in sys.argv[1:]:
    ev=json.load(open(name,encoding='utf-8'))['traceEvents']
    tot=collections.Counter(); cnt=collections.Counter(); fam=collections.Counter(); famcnt=collections.Counter()
    top={}
    for e in ev:
        if e.get('ph')!='X': continue
        n=e['name']; dur=e.get('dur',0)
        if n.startswith('Total '): top[n]=dur
        if n in ('InstantiateClass','InstantiateFunction'):
            d=e['args']['detail']; cnt[n]+=1
            f=strip_t(d); f=re.sub(r'\(.*$','',f)
            fam[f]+=dur; famcnt[f]+=1
    print(f"== {name.split('/')[-1]}")
    for k in ['Total ExecuteCompiler','Total Frontend','Total Backend','Total CodeGen Function','Total InstantiateClass','Total InstantiateFunction','Total ParseClass','Total PerformPendingInstantiations','Total OptModule','Total OptFunction']:
        if k in top: print(f"   {top[k]/1e6:7.1f} s  {k}")
    print(f"   instantiation events: class={cnt['InstantiateClass']} function={cnt['InstantiateFunction']}")
    print("   top families by inclusive instantiation time (template args stripped):")
    for k,v in fam.most_common(18): print(f"   {v/1e6:7.1f} s {famcnt[k]:6d}x  {k[:110]}")
