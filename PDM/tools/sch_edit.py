# Small editor for PDM.kicad_sch as generated for this project: every symbol line is followed by one
# (wire, label) pair per pin, in the pin order of its library symbol, so a pin's net is its label text.
import re, uuid

class Sch:
    def __init__(self, path):
        self.path = path
        self.lines = open(path).read().split('\n')

    def save(self):
        open(self.path, 'w').write('\n'.join(self.lines))

    # ---------- library ----------
    def lib_block(self, lib_id):
        s = '\n'.join(self.lines)
        i = s.find(f'(symbol "{lib_id}"')
        assert i >= 0, lib_id
        depth, j = 0, i
        while True:
            c = s[j]
            if c == '(': depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0: return s[i:j + 1]
            j += 1

    def pin_order(self, lib_id):
        return re.findall(r'\(pin \w+ \w+ \(at [-\d. ]+\) \(length [\d.]+\).*?\(number "([^"]+)"', self.lib_block(lib_id))

    def add_lib_symbol(self, text):
        for k, l in enumerate(self.lines):
            if '(lib_symbols' in l:
                self.lines[k] = l.replace('(lib_symbols', '(lib_symbols ' + text, 1); return
        raise RuntimeError('no lib_symbols')

    # ---------- symbols ----------
    def find(self, ref):
        for k, l in enumerate(self.lines):
            if l.startswith('(symbol (lib_id') and f'(property "Reference" "{ref}"' in l:
                lib_id = re.search(r'lib_id "([^"]+)"', l).group(1)
                pins = self.pin_order(lib_id)
                return k, lib_id, pins
        raise KeyError(ref)

    def pin_xy(self, ref, pin):
        k, lib_id, _ = self.find(ref)
        X, Y, rot = map(float, re.search(r'\(symbol \(lib_id "[^"]+"\) \(at ([-\d.]+) ([-\d.]+) ([-\d.]+)\)', self.lines[k]).groups())
        assert rot == 0, (ref, rot)
        for chunk in self.lib_block(lib_id).split('(pin ')[1:]:
            if re.search(rf'\(number "{pin}"', chunk):
                x, y = map(float, re.match(r'\w+ \w+ \(at ([-\d.]+) ([-\d.]+)', chunk).groups()); break
        else:
            raise KeyError((ref, pin))
        return round(X + x, 2), round(Y - y, 2)

    def pin_label_idx(self, ref, pin):
        """Label line of the stub wire that starts at the pin (matched by coordinates, not by order)."""
        k, _, _ = self.find(ref)
        px, py = self.pin_xy(ref, pin)
        j = k + 1
        while j < len(self.lines) and self.lines[j].startswith(('(wire', '(label', '(no_connect')):
            m = re.match(r'\(wire \(pts \(xy ([-\d.]+) ([-\d.]+)\)', self.lines[j])
            if m and abs(float(m.group(1)) - px) < 0.01 and abs(float(m.group(2)) - py) < 0.01:
                assert self.lines[j + 1].startswith('(label '), (ref, pin)
                return j + 1
            j += 1
        raise KeyError((ref, pin, px, py))

    def relabel(self, ref, pin, net):
        idx = self.pin_label_idx(ref, pin)
        self.lines[idx] = re.sub(r'^\(label "[^"]*"', f'(label "{net}"', self.lines[idx])

    def set_prop(self, ref, prop, val):
        k, _, _ = self.find(ref)
        self.lines[k] = re.sub(rf'\(property "{prop}" "[^"]*"', f'(property "{prop}" "{val}"', self.lines[k], count=1)

    def delete(self, ref):
        k, _, _ = self.find(ref)
        j = k + 1
        while j < len(self.lines) and self.lines[j].startswith(('(wire', '(label', '(no_connect')): j += 1
        del self.lines[k:j]

    def add2(self, lib_id, template_ref, ref, value, footprint, x, y, nets, desc=''):
        """Add a vertical 2-pin part (R/C/D style, pins at y +-3.81) cloned from template_ref."""
        k, tl, pins = self.find(template_ref)
        l = self.lines[k]
        l = l.replace(f'(lib_id "{tl}")', f'(lib_id "{lib_id}")')
        ox, oy = map(float, re.search(r'\(symbol \(lib_id "[^"]+"\) \(at ([-\d.]+) ([-\d.]+)', l).groups())
        def mv(m):
            return f'(at {float(m.group(1)) - ox + x:.2f} {float(m.group(2)) - oy + y:.2f}'
        l = re.sub(r'\(at ([-\d.]+) ([-\d.]+)', mv, l)
        l = re.sub(r'\(uuid "[^"]+"\)', lambda m: f'(uuid "{uuid.uuid4()}")', l)
        l = re.sub(r'\(reference "[^"]+"\)', f'(reference "{ref}")', l)
        l = re.sub(r'\(property "Reference" "[^"]*"', f'(property "Reference" "{ref}"', l)
        l = re.sub(r'\(property "Value" "[^"]*"', f'(property "Value" "{value}"', l)
        l = re.sub(r'\(property "Footprint" "[^"]*"', f'(property "Footprint" "{footprint}"', l)
        l = re.sub(r'\(property "Description" "[^"]*"', f'(property "Description" "{desc}"', l)
        l = re.sub(r'\(dnp \w+\)', '(dnp no)', l)
        new = [l]
        for p in self.pin_order(lib_id):
            top = p == self.pin_order(lib_id)[0]
            py, ly = (y - 3.81, y - 6.35) if top else (y + 3.81, y + 6.35)
            ang, just = ('90', 'left bottom') if top else ('270', 'right bottom')
            new.append(f'(wire (pts (xy {x:.2f} {py:.2f}) (xy {x:.2f} {ly:.2f})) (stroke (width 0) (type default)) (uuid "{uuid.uuid4()}"))')
            new.append(f'(label "{nets[p]}" (at {x:.2f} {ly:.2f} {ang}) (fields_autoplaced yes) (effects (font (size 1.27 1.27)) (justify {just})) (uuid "{uuid.uuid4()}"))')
        j = k + 1
        while j < len(self.lines) and self.lines[j].startswith(('(wire', '(label', '(no_connect')): j += 1
        self.lines[j:j] = new

    def pos(self, ref):
        k, _, _ = self.find(ref)
        return tuple(map(float, re.search(r'\(symbol \(lib_id "[^"]+"\) \(at ([-\d.]+) ([-\d.]+)', self.lines[k]).groups()))

    def set_text(self, old, new):
        for k, l in enumerate(self.lines):
            if l.startswith(f'(text "{old}"'):
                self.lines[k] = l.replace(f'(text "{old}"', f'(text "{new}"', 1); return
        raise KeyError(old)

def _add_prop(self, ref, name, value):
    """Add (or replace) a hidden property on a symbol."""
    k, _, _ = self.find(ref)
    l = self.lines[k]
    if f'(property "{name}" ' in l:
        self.lines[k] = re.sub(rf'\(property "{name}" "[^"]*"', f'(property "{name}" "{value}"', l, count=1); return
    x, y = re.search(r'\(symbol \(lib_id "[^"]+"\) \(at ([-\d.]+) ([-\d.]+)', l).groups()
    prop = f'(property "{name}" "{value}" (at {x} {y} 0) (effects (font (size 1.27 1.27)) (hide yes))) '
    i = l.find('(pin ')
    self.lines[k] = l[:i] + prop + l[i:]
Sch.add_prop = _add_prop
