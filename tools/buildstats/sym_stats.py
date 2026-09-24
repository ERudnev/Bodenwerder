import re, sys, collections
def strip_templates(s):
    out=[];depth=0
    for ch in s:
        if ch=='<': depth+=1; 
        if depth==0: out.append(ch)
        if ch=='>': depth-=1
    return ''.join(out)
def analyze(path, aspects):
    seen=set(); fam=collections.Counter(); ns=collections.Counter(); asp=collections.Counter(); kw=collections.Counter()
    kws=['std::function','_Func_impl','unordered_map','_Hash','shared_ptr','Patch','Future','Delta','Review','Reality','QuantumGate','call_action','Retrospection','pfr','Behavior','reaction','format','Binding','Table','Iterator','iterator','vector','optional','Log','log']
    n=0
    with open(path,'r',errors='ignore') as f:
        for line in f:
            if ' | ' not in line or 'SECT' not in line: continue
            left,_,right=line.partition(' | ')
            if 'External' not in left and 'Static' not in left: continue
            right=right.strip()
            m=re.match(r'(\S+)\s+\((.*)\)$',right)
            dec=m.group(1) if m else right.split()[0]
            if dec in seen: continue
            seen.add(dec); n+=1
            name=m.group(2) if m else right
            for a in aspects:
                if a in name: asp[a]+=1
            for k in kws:
                if k in name: kw[k]+=1
            core=name
            core=re.sub(r'^(public|private|protected): ','',core)
            core=re.sub(r'^(static |virtual )+','',core)
            # find the function/variable qualified name: token before first '(' after stripping templates
            st=strip_templates(core)
            mm=re.search(r'([A-Za-z_][\w:]*)\(',st)
            q=mm.group(1) if mm else st.split()[-1] if st.split() else st
            # family = enclosing type (drop last ::member)
            famname=q.rsplit('::',1)[0] if '::' in q else q
            fam[famname]+=1
            root=q.split('::')[0]
            ns[root]+=1
    print(f"== {path.split('/')[-1]}: unique defined symbols={n}")
    print("-- by root namespace:"); 
    for k,v in ns.most_common(8): print(f"   {v:7d} {k}")
    print("-- top families (template args stripped):")
    for k,v in fam.most_common(35): print(f"   {v:7d} {k}")
    print("-- symbols mentioning keyword:")
    for k in kws: print(f"   {kw[k]:7d} {k}")
    print("-- symbols mentioning aspect (top 25 of %d):"%len(aspects))
    for k,v in asp.most_common(25): print(f"   {v:7d} {k}")
    print(f"   aspects with 0: {sum(1 for a in aspects if asp[a]==0)}")
def aspects_from(src):
    txt=open(src,encoding='utf-8',errors='ignore').read()
    return sorted(set(re.findall(r'aspect<([\w:]+)>',txt)),key=len,reverse=True)
eng=aspects_from('modules/raidenmamare/source/engine.cpp')
gm=aspects_from('stories/Eltanin/private/game.cpp')
print("engine schema aspects:",len(eng)," game schema aspects:",len(gm))
S=sys.argv[1]
analyze(S+'/engine_syms.txt', ['rmmr::'+a if not a.startswith('rmmr') else a for a in eng])
analyze(S+'/game_syms.txt', ['eltanin::'+a for a in gm]+['rmmr::'+a for a in eng])
