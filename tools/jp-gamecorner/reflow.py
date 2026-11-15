# Re-wraps the English rows of the HGSS port's lib/port/slots/slot_maps.c to the fewest, most even lines of at most
# 208 px. Usage: reflow.py slot_maps.c GLYPHS [write]. GLYPHS is a dump of the field font's widths, one
# "[glyph] CODE WIDTH" line per character code 0x100-0x1FF, printed from the game with FontID_FlatArray_GetWidth(1,
# {code, EOS}, 0) (not committed: it comes from the ROM's font).
import re, sys
glyph = {}
for l in open(sys.argv[2]):
    p = l.split()
    if p[1] != 'AA': glyph[int(p[1], 16)] = int(p[2])
def code(c):
    if '0' <= c <= '9': return 0x121 + ord(c) - 48
    if 'A' <= c <= 'Z': return 0x12B + ord(c) - 65
    if 'a' <= c <= 'z': return 0x145 + ord(c) - 97
    return {' ':0x1DE,'!':0x1AB,'?':0x1AC,',':0x1AD,'.':0x1AE,'…':0x1AF,'/':0x1B1,"'":0x1B3,'’':0x1B3,'(':0x1B9,')':0x1BA,
            '-':0x1BE,':':0x1C4,'&':0x1C2,'$':0x1A8,'é':0x188}[c]
def width(s): return sum(glyph[code(c)] for c in s)
MAX = 208
def wrap(box):
    words = re.split(r'[ \n\f]+', box.strip(' \n\f'))
    # Fewest lines, then the most even: the least sum of squared slack, so no word is left on a line alone
    n = len(words)
    best = [(0, 0, 0)] + [None] * n  # (lines, cost, previous break) for words[:i]
    for i in range(1, n + 1):
        for j in range(i - 1, -1, -1):
            w = width(' '.join(words[j:i]))
            if w > MAX: break
            if best[j] is None: continue
            cand = (best[j][0] + 1, best[j][1] + (MAX - w) ** 2, j)
            if best[i] is None or cand[:2] < best[i][:2]: best[i] = cand
    lines, i = [], n
    while i > 0:
        j = best[i][2]; lines.insert(0, ' '.join(words[j:i])); i = j
    out = lines[0]
    for i, l in enumerate(lines[1:]):
        out += ('\n' if i == 0 else '\f') + l
    return out
src = open(sys.argv[1]).read()
def esc(s): return s.replace('\n','\\n').replace('\r','\\r').replace('\f','\\f')
def fix(m):
    raw = m.group(2)
    if '{' in raw or 'Coins"' in m.group(0) and '\\n' in raw and raw.count(' ') < 3: return m.group(0)
    text = raw.replace('\\n','\n').replace('\\r','\r').replace('\\f','\f')
    trail = text.endswith('\r')
    boxes = text.rstrip('\r').split('\r')
    new = '\r'.join(wrap(b) for b in boxes) + ('\r' if trail else '')
    if new != text: print('-', raw); print('+', esc(new))
    return m.group(1) + '"' + esc(new) + '",'
out = re.sub(r'(/\* \d\d \*/ )"((?:[^"\\]|\\.)*)",', fix, src)
if len(sys.argv) > 3: open(sys.argv[1], 'w').write(out)
