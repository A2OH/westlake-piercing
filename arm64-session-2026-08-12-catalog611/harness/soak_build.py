#!/usr/bin/env python3
"""§616: calibrate-then-soak for the Material Catalog.
Phase C (this script): visit every demo once, record real widget rects -> manifest.json,
and EMIT a pure device-side soak script (soak_run.sh): deterministic nav + one action per
widget (tap / slider-drag / type) + screenshot after every action. No dumps, no health
checks at soak time — validation happens offline afterwards (§616v).
Nav determinism: every demo entry starts from the TOC top (6 up-drags), then N recorded
down-drags, then the recorded card/demo-row taps. Exit = toolbar X x2 (or relaunch for
known roach motels)."""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from walkcat_drive import sh, log, tap, drag, dump, rects, titles_visible, alive, relaunch, ensure_toc

SKIP = {'Adaptive'}                      # §613 hard crasher
ROACH = {'Menus', 'Transition'}          # no X / wedges on exit -> relaunch after
OUT = os.environ.get('WALK_OUT', '.').rstrip('/') + '/'

def toc_positions():
    """name -> (downDrags, x, y) with the list reset to top before counting."""
    ensure_toc()
    for _ in range(6): drag(600, 500, 600, 1600, settle=2)
    posmap, drags = {}, 0
    while True:
        d = dump()
        new = 0
        for t, (x, y) in titles_visible(d):
            if t not in posmap and 260 <= y <= 1870:
                posmap[t] = (drags, x, min(y - 150, 1750)); new += 1
        if new == 0 and drags > 2: break
        drag(600, 1400, 600, 700, settle=4); drags += 1
    return posmap

def calibrate():
    man = {'toc': {}, 'demos': {}}
    posmap = toc_positions()
    log(f'CAL: {len(posmap)} cards mapped')
    man['toc'] = {k: list(v) for k, v in posmap.items()}
    for name, (n, x, y) in posmap.items():
        if name in SKIP: continue
        ensure_toc()
        for _ in range(6): drag(600, 500, 600, 1600, settle=2)
        for _ in range(n): drag(600, 1400, 600, 700, settle=4)
        tap(x, y, settle=8)
        d = dump()
        landing = rects(d, r'id=cat_demo_landing_main_demo_container')
        if not landing:
            log(f'CAL {name}: no landing'); continue
        lx, ly = landing[0][0], landing[0][1]
        tap(lx, ly, settle=9)
        d = dump()
        if not d.strip():
            if not alive(): relaunch()
            log(f'CAL {name}: demo dump empty'); continue
        widgets = []
        for wx, wy, txt, line in rects(d, r'c=1 en=1'):
            if wx < 170 and wy < 170: continue
            cls = line.split(' id=')[0].strip()
            act = 'slide' if 'Slider' in cls else ('type' if 'EditText' in cls else 'tap')
            widgets.append({'cls': cls, 'x': wx, 'y': wy, 'act': act, 'txt': txt[:24]})
        man['demos'][name] = {'nav': [n, x, y], 'row': [lx, ly], 'widgets': widgets}
        log(f'CAL {name}: {len(widgets)} widgets')
        if name in ROACH:
            relaunch()
        else:
            tap(56, 64, settle=5); tap(56, 64, settle=5)
            if 'cat_toc_title' not in dump(): relaunch()
    json.dump(man, open(OUT + 'manifest.json', 'w'), indent=1)
    return man

def emit(man):
    L = ['#!/system/bin/sh', 'cd /data/local/tmp/asx', 'mkdir -p /data/local/tmp/soak616',
         'T=/data/local/tmp/noice_tap', 'X=/data/local/tmp/noice_text',
         'S() { snapshot_display -f /data/local/tmp/soak616/$1.jpeg >/dev/null 2>&1; }',
         'echo SOAK-START $(date) > /data/local/tmp/soak616/soak.log']
    shot = 0
    for name, m in man['demos'].items():
        tag = name.replace(' ', '_').replace('(', '').replace(')', '')
        n, cx, cy = m['nav']; rx, ry = m['row']
        L.append(f'echo "== {tag}" >> /data/local/tmp/soak616/soak.log')
        for _ in range(6): L.append('echo "600 500 600 1600" > $T; sleep 2')
        for _ in range(n): L.append('echo "600 1400 600 700" > $T; sleep 3')
        L.append(f'echo "{cx} {cy}" > $T; sleep 7')
        L.append(f'echo "{rx} {ry}" > $T; sleep 8')
        shot += 1; L.append(f'S {shot:03d}_{tag}_enter')
        for i, w in enumerate(m['widgets'][:20]):
            x, y = w['x'], w['y']
            if w['act'] == 'slide':
                L.append(f'echo "{max(x-200,60)} {y} {min(x+200,1140)} {y}" > $T; sleep 3')
            elif w['act'] == 'type':
                L.append(f'echo "{x} {y}" > $T; sleep 3'); L.append('echo wl616 > $X; sleep 3')
            else:
                L.append(f'echo "{x} {y}" > $T; sleep 3')
            shot += 1; L.append(f'S {shot:03d}_{tag}_w{i:02d}_{w["cls"]}')
            L.append('echo back > $T; sleep 2')   # dismiss whatever opened; no-op otherwise
        if name in ROACH:
            L.append('sh /data/local/tmp/asx/walkcat5.sh >/dev/null 2>&1')
        else:
            L.append('echo "56 64" > $T; sleep 4; echo "56 64" > $T; sleep 4')
        L.append(f'echo "done {tag} $(date +%H:%M:%S)" >> /data/local/tmp/soak616/soak.log')
    L.append('echo SOAK-END $(date) >> /data/local/tmp/soak616/soak.log')
    open(OUT + 'soak_run.sh', 'w').write('\n'.join(L) + '\n')
    log(f'EMIT: soak_run.sh with {shot} screenshots planned')

if __name__ == '__main__':
    man = calibrate()
    emit(man)
