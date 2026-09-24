import re
def parse(s):
    tok=re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()]+',s); st=[[]]
    for t in tok:
        if t=='(': st.append([])
        elif t==')': x=st.pop(); st[-1].append(x)
        else: st[-1].append(t.strip('"') if t.startswith('"') else t)
    return st[0][0]
def f(n,k): return [x for x in n if isinstance(x,list) and x and x[0]==k]
d=parse(open('/prj/pdm.net').read())
comps={}
for c in f(f(d,'components')[0],'comp'):
    g=lambda k:(f(c,k)[0][1] if f(c,k) else '')
    comps[g('ref')]=dict(value=g('value'),fp=g('footprint'),uuid=f(c,'tstamps')[0][1] if f(c,'tstamps') else '',
        fields={fl[1][1]:(fl[2] if len(fl)>2 else '') for fl in f(f(c,'fields')[0],'field')} if f(c,'fields') else {},
        dnp=any(p[1][1]=='dnp' for p in f(c,'property')))
nets={}
for n in f(f(d,'nets')[0],'net'):
    name=f(n,'name')[0][1]; nets[name]=[(f(x,'ref')[0][1],f(x,'pin')[0][1]) for x in f(n,'node')]
