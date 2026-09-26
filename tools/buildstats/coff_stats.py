import struct, sys, os, glob
def stats(path):
    with open(path,'rb') as f:
        h=f.read(64)
    sig1,sig2=struct.unpack_from('<HH',h,0)
    if sig1==0 and sig2==0xFFFF:  # ANON_OBJECT_HEADER_BIGOBJ
        version,machine=struct.unpack_from('<HH',h,4)
        nsec,=struct.unpack_from('<I',h,44)
        nsym,=struct.unpack_from('<I',h,52)
        return nsec,nsym,'bigobj'
    nsec,=struct.unpack_from('<H',h,2)
    nsym,=struct.unpack_from('<I',h,12)
    return nsec,nsym,'coff'
rows=[]
for root in sys.argv[1:]:
    for p in glob.glob(root+'/**/*.obj',recursive=True):
        try:
            ns,nsy,k=stats(p); rows.append((ns,nsy,k,os.path.getsize(p),p))
        except Exception as e: print('ERR',p,e)
rows.sort(reverse=True)
tot=sum(r[0] for r in rows)
print(f"objects={len(rows)} total_sections={tot} median_sections={sorted(r[0] for r in rows)[len(rows)//2]}")
print(f"{'sections':>9} {'symbols':>8} {'KB':>7}  file")
for ns,nsy,k,sz,p in rows[:30]:
    print(f"{ns:9d} {nsy:8d} {sz//1024:7d}  {p.replace('build/msvc/','')}")
