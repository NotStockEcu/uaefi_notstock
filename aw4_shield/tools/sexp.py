import re
TOK=re.compile(r'\s*(\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+)')
def parse(s):
    pos=0; stack=[[]]
    for m in TOK.finditer(s):
        t=m.group(1)
        if t=='(': stack.append([])
        elif t==')':
            x=stack.pop(); stack[-1].append(x)
        else: stack[-1].append(t)
    assert len(stack)==1
    return stack[0][0]
def q(t): return t[1:-1].replace('\\"','"') if isinstance(t,str) and t.startswith('"') else t
def dump(x):
    if isinstance(x,list): return "("+" ".join(dump(i) for i in x)+")"
    return x
def find(node,key): return [c for c in node if isinstance(c,list) and c and c[0]==key]
