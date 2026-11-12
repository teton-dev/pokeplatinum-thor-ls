#!/usr/bin/env python3
# The player's back sprites in Gen 5 battles (teton-dev fork): the owner's own art, Lucas and Dawn, drawn in place of
# BW2's player back sprite (gen5/src/player_back.c). Plain Python, no image library.
#
#   player_back.py convert   the sheets in gen5/art/sheets -> gen5/art/<name>_back.png and <name>_back.json
#   player_back.py header OUT   the converted art -> a C header for gen5/src/player_back.c (a build step)
#   player_back.py preview DIR   each converted frame as a PNG, as player_back.c draws it, to check the layout
#
# convert keys out each sheet's backing colour, reduces the colours to the best 15 (plus transparency) in the DS's
# 15-bit colour, cuts the poses and arm pieces out, packs them into one 256x128 4-bit texture (the size BW2 gives
# each sprite) and writes, for each of BW2's five throw poses (the multi-cells of its back sprites: standing, ball
# up, ball higher, winding up, thrown), the quads that draw it. BW2's own animation picks the pose and its timing.

import json
import pathlib
import struct
import sys
import zlib

ART = pathlib.Path(__file__).resolve().parent.parent / 'art'

TEX_WIDTH = 256
TEX_HEIGHT = 128

# Each sprite: its sheet, the parts cut from it (x0, y0, x1, y1 on the sheet, trimmed to what isn't backing colour),
# the bodies' anchors (the x on the sheet under the figure's feet; its bottom edge is the bottom of the sprite), and
# BW2's five poses, each a list of parts front to back: (part, x, y) puts an arm piece's top-left at x, y on the
# sheet relative to that pose's body, as if it were drawn on the sheet next to it.
SPRITES = {
    'lucas': {
        'sheet': 'trainer_lucas_fullspritesheet.png',
        # The bottom row, Platinum's outfit. The top row (Diamond and Pearl's) isn't used
        'parts': {
            'stand': (15, 124, 63, 242),
            'ball': (72, 124, 135, 242),
            'windup': (148, 124, 207, 242),
            'throw': (214, 124, 293, 242),
        },
        'anchors': {'stand': 39, 'ball': 111, 'windup': 169, 'throw': 237},
        'poses': [
            ['stand'],
            ['ball'],
            ['ball'],
            ['windup'],
            ['throw'],
        ],
    },
    'dawn': {
        'sheet': 'trainer_dawn_fullspritesheet.png',
        # Three poses, armless where an arm piece goes, and five arm pieces
        'parts': {
            'stand': (9, 20, 54, 128),
            'overhead': (62, 15, 120, 128),
            'lean': (121, 30, 192, 128),
            'arm_back': (194, 3, 240, 28),     # the far arm swung back as she throws
            'arm_spare': (199, 42, 213, 72),   # an arm reaching behind her (not used)
            'arm_fist': (218, 40, 234, 71),    # the near arm at her side, the ball in her fist
            'arm_ball': (193, 84, 217, 124),   # the near arm bringing the ball up
            'arm_ball_high': (224, 84, 256, 123),  # and up to her head
        },
        'anchors': {'stand': 31, 'overhead': 86, 'lean': 138},
        'poses': [
            [('arm_fist', 19, 73), 'stand'],
            [('arm_ball', 7, 35), 'stand'],
            [('arm_ball_high', -1, 36), 'stand'],
            ['overhead'],
            ['lean', ('arm_back', 113, 70)],
        ],
    },
}


def png_read(path):
    data = pathlib.Path(path).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    pos = 8
    idat = b''
    palette = None
    trns = b''
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            width, height, depth, colour, _, _, interlace = struct.unpack('>IIBBBBB', chunk)
        elif kind == b'PLTE':
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif kind == b'tRNS':
            trns = chunk
        elif kind == b'IDAT':
            idat += chunk
    if interlace != 0 or colour not in (2, 3, 6) or (colour != 3 and depth != 8) or depth not in (4, 8):
        sys.exit(f'{path}: only non-interlaced 8-bit RGB/RGBA or 4/8-bit indexed PNGs are read')
    channels = {2: 3, 3: 1, 6: 4}[colour]
    stride = (width * channels * depth + 7) // 8
    step = max(1, channels * depth // 8)
    raw = zlib.decompress(idat)
    rows = []
    prev = bytearray(stride)
    i = 0
    for _ in range(height):
        kind = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - step] if x >= step else 0
            b = prev[x]
            c = prev[x - step] if x >= step else 0
            if kind == 1:
                line[x] = (line[x] + a) & 255
            elif kind == 2:
                line[x] = (line[x] + b) & 255
            elif kind == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    pixels = []
    for line in rows:
        if colour == 3:
            if depth == 4:
                indices = [(line[x // 2] >> (4 if x % 2 == 0 else 0)) & 15 for x in range(width)]
            else:
                indices = list(line[:width])
            pixels.append([palette[k] + ((trns[k] if k < len(trns) else 255),) for k in indices])
        elif colour == 2:
            pixels.append([tuple(line[3 * x:3 * x + 3]) + (255,) for x in range(width)])
        else:
            pixels.append([tuple(line[4 * x:4 * x + 4]) for x in range(width)])
    return width, height, pixels


def png_chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def png_write_indexed(path, width, height, indices, palette):
    # 4-bit indexed, index 0 transparent
    raw = b''.join(b'\0' + bytes((row[x] << 4) | row[x + 1] for x in range(0, width, 2)) for row in indices)
    data = (b'\x89PNG\r\n\x1a\n'
            + png_chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 4, 3, 0, 0, 0))
            + png_chunk(b'PLTE', b''.join(bytes(c) for c in palette))
            + png_chunk(b'tRNS', b'\0')
            + png_chunk(b'IDAT', zlib.compress(raw, 9))
            + png_chunk(b'IEND', b''))
    pathlib.Path(path).write_bytes(data)


def png_write_rgba(path, width, height, pixels):
    raw = b''.join(b'\0' + bytes(v for p in row for v in p) for row in pixels)
    pathlib.Path(path).write_bytes(b'\x89PNG\r\n\x1a\n'
                                   + png_chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                                   + png_chunk(b'IDAT', zlib.compress(raw, 9))
                                   + png_chunk(b'IEND', b''))


def to_ds(colour):
    # 8-bit RGB to the DS's 5 bits a channel, and back to 8 bits as the DS shows it
    return tuple((v >> 3) for v in colour[:3])


def reduce_colours(counts, count):
    # The best `count` colours for the pixels (a dict of 5-bit colours to pixel counts): a weighted median cut,
    # then k-means from there. Deterministic
    colours = sorted(counts)
    if len(colours) <= count:
        return colours
    boxes = [colours]
    while len(boxes) < count:
        def spread(box):
            return max(max(c[i] for c in box) - min(c[i] for c in box) for i in range(3)) * sum(counts[c] for c in box)
        boxes.sort(key=spread, reverse=True)
        box = boxes.pop(0)
        if len(box) == 1:
            boxes.append(box)
            break
        axis = max(range(3), key=lambda i: max(c[i] for c in box) - min(c[i] for c in box))
        box.sort(key=lambda c: (c[axis], c))
        total = sum(counts[c] for c in box)
        run = 0
        cut = 1
        for k, c in enumerate(box[:-1]):
            run += counts[c]
            cut = k + 1
            if run * 2 >= total:
                break
        boxes += [box[:cut], box[cut:]]

    def mean(group):
        total = sum(counts[c] for c in group)
        return tuple(sum(c[i] * counts[c] for c in group) / total for i in range(3))

    centres = [mean(box) for box in boxes]
    for _ in range(20):
        groups = [[] for _ in centres]
        for c in colours:
            groups[min(range(len(centres)), key=lambda k: sum((c[i] - centres[k][i]) ** 2 for i in range(3)))].append(c)
        new = [mean(g) if g else centres[k] for k, g in enumerate(groups)]
        if new == centres:
            break
        centres = new
    result = sorted({tuple(min(31, max(0, round(v))) for v in c) for c in centres})
    return result


def nearest(colour, palette):
    return min(range(len(palette)), key=lambda k: sum((colour[i] - palette[k][i]) ** 2 for i in range(3)))


def trim(pixels, backing, rect):
    x0, y0, x1, y1 = rect
    points = [(x, y) for y in range(y0, y1) for x in range(x0, x1) if pixels[y][x][:3] != backing]
    if not points:
        sys.exit(f'part {rect} is empty')
    return (min(p[0] for p in points), min(p[1] for p in points),
            max(p[0] for p in points) + 1, max(p[1] for p in points) + 1)


def pack(sizes):
    # Skyline packing into the texture: each part at the lowest place it fits, then the leftmost. Tallest first
    skyline = [0] * TEX_WIDTH
    places = {}
    for name, (w, h) in sorted(sizes.items(), key=lambda item: (-item[1][1], -item[1][0], item[0])):
        best = None
        for x in range(TEX_WIDTH - w + 1):
            y = max(skyline[x:x + w])
            if y + h <= TEX_HEIGHT and (best is None or y < best[1]):
                best = (x, y)
        if best is None:
            sys.exit(f'the parts don\'t fit in {TEX_WIDTH}x{TEX_HEIGHT}')
        places[name] = best
        for x in range(best[0], best[0] + w):
            skyline[x] = best[1] + h
    return places


def convert(name, spec):
    width, height, pixels = png_read(ART / 'sheets' / spec['sheet'])
    backing = pixels[0][0][:3]
    rects = {part: trim(pixels, backing, rect) for part, rect in spec['parts'].items()}
    used = {part for pose in spec['poses'] for part in [p if isinstance(p, str) else p[0] for p in pose]}

    counts = {}
    for part in used:
        x0, y0, x1, y1 = rects[part]
        for y in range(y0, y1):
            for x in range(x0, x1):
                if pixels[y][x][:3] != backing:
                    c = to_ds(pixels[y][x])
                    counts[c] = counts.get(c, 0) + 1
    colours = reduce_colours(counts, 15)
    palette5 = [(0, 0, 0)] + colours + [(0, 0, 0)] * (15 - len(colours))
    lookup = {c: 1 + nearest(c, colours) for c in counts}

    places = pack({part: (rects[part][2] - rects[part][0], rects[part][3] - rects[part][1]) for part in used})
    texture = [[0] * TEX_WIDTH for _ in range(TEX_HEIGHT)]
    for part in used:
        x0, y0, x1, y1 = rects[part]
        s, t = places[part]
        for y in range(y0, y1):
            for x in range(x0, x1):
                if pixels[y][x][:3] != backing:
                    texture[t + y - y0][s + x - x0] = lookup[to_ds(pixels[y][x])]

    # Each pose's quads, front to back: x and y of the quad's top-left from the sprite's origin (the anchor under
    # the feet, y up), its size and its place in the texture
    poses = []
    for pose in spec['poses']:
        body = next(p for p in pose if isinstance(p, str))
        bx0, by0, bx1, by1 = spec['parts'][body]
        anchor_x = spec['anchors'][body]
        bottom = by1
        quads = []
        for p in pose:
            if isinstance(p, str):
                part, x, y = p, rects[p][0], rects[p][1]
            else:
                part, x, y = p
            x0, y0, x1, y1 = rects[part]
            s, t = places[part]
            quads.append({'part': part, 'x': x - anchor_x, 'y': bottom - y, 'width': x1 - x0, 'height': y1 - y0,
                          's': s, 't': t})
        poses.append(quads)

    palette8 = [tuple((v << 3) | (v >> 2) for v in c) for c in palette5]
    png_write_indexed(ART / f'{name}_back.png', TEX_WIDTH, TEX_HEIGHT, texture, palette8)
    (ART / f'{name}_back.json').write_text(json.dumps({
        'comment': f'Generated by gen5/tools/player_back.py from sheets/{spec["sheet"]}: the poses of {name}_back.png',
        'poses': poses,
    }, indent=1) + '\n')
    print(f'{name}: {len(counts)} colours to {len(colours)}, parts at {places}')


def load(name):
    width, height, pixels = png_read(ART / f'{name}_back.png')
    assert (width, height) == (TEX_WIDTH, TEX_HEIGHT)
    data = pathlib.Path(ART / f'{name}_back.png').read_bytes()
    # The palette, read from the PNG itself so that the indices stay as written
    pos = 8
    palette = None
    idat = b''
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'PLTE':
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif kind == b'IHDR':
            depth, colour = chunk[8], chunk[9]
        elif kind == b'IDAT':
            idat += chunk
    assert colour == 3 and depth == 4 and len(palette) == 16
    raw = zlib.decompress(idat)
    stride = TEX_WIDTH // 2
    indices = []
    prev = bytearray(stride)
    for y in range(TEX_HEIGHT):
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        kind = raw[y * (stride + 1)]
        for x in range(stride):
            a = line[x - 1] if x else 0
            b = prev[x]
            c = prev[x - 1] if x else 0
            if kind == 1:
                line[x] = (line[x] + a) & 255
            elif kind == 2:
                line[x] = (line[x] + b) & 255
            elif kind == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        indices.append([(line[x // 2] >> (4 if x % 2 == 0 else 0)) & 15 for x in range(TEX_WIDTH)])
    poses = json.loads((ART / f'{name}_back.json').read_text())['poses']
    return indices, palette, poses


def header(out):
    lines = ['// Generated by gen5/tools/player_back.py from gen5/art: the player\'s back sprites (player_back.c)', '']
    for name in SPRITES:
        indices, palette, poses = load(name)
        ident = name.capitalize()
        # The DS's 4-bit texture: two pixels a byte, the left one in the low half
        tex = [indices[y][x] | (indices[y][x + 1] << 4) for y in range(TEX_HEIGHT) for x in range(0, TEX_WIDTH, 2)]
        lines.append(f'static const u8 s{ident}BackTexture[{len(tex)}] = {{')
        for i in range(0, len(tex), 32):
            lines.append('    ' + ', '.join(f'0x{v:02x}' for v in tex[i:i + 32]) + ',')
        lines.append('};')
        pltt = [(c[0] >> 3) | ((c[1] >> 3) << 5) | ((c[2] >> 3) << 10) for c in palette]
        lines.append(f'static const u16 s{ident}BackPalette[16] = {{ ' + ', '.join(f'0x{v:04x}' for v in pltt) + ' };')
        lines.append(f'static const Gen5BackQuad s{ident}BackQuads[] = {{')
        frames = []
        first = 0
        for quads in poses:
            for q in quads:
                lines.append(f'    {{ {q["x"]}, {q["y"]}, {q["width"]}, {q["height"]}, {q["s"]}, {q["t"]} }}, // {q["part"]}')
            frames.append((first, len(quads)))
            first += len(quads)
        lines.append('};')
        lines.append(f'static const Gen5BackPose s{ident}BackPoses[] = {{ '
                     + ', '.join(f'{{ {a}, {b} }}' for a, b in frames) + ' };')
        lines.append(f'static const Gen5BackSprite s{ident}Back = {{ s{ident}BackTexture, s{ident}BackPalette, '
                     f's{ident}BackQuads, s{ident}BackPoses, {len(frames)} }};')
        lines.append('')
    text = '\n'.join(lines)
    path = pathlib.Path(out)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def preview(directory):
    directory = pathlib.Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    for name in SPRITES:
        indices, palette, poses = load(name)
        size = 160
        for k, quads in enumerate(poses):
            canvas = [[(96, 160, 96, 255)] * size for _ in range(size)]
            for q in reversed(quads):
                for y in range(q['height']):
                    for x in range(q['width']):
                        index = indices[q['t'] + y][q['s'] + x]
                        if index:
                            canvas[size - 8 - q['y'] + y][size // 2 + q['x'] + x] = palette[index] + (255,)
            # The origin
            canvas[size - 8][size // 2] = (255, 0, 0, 255)
            png_write_rgba(directory / f'{name}_pose{k}.png', size, size, canvas)


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == 'convert':
        for name, spec in SPRITES.items():
            convert(name, spec)
    elif len(sys.argv) == 3 and sys.argv[1] == 'header':
        header(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'preview':
        preview(sys.argv[2])
    else:
        sys.exit('usage: player_back.py convert | header OUT | preview DIR')
