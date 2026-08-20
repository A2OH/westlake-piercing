#!/usr/bin/env python3
"""§615: widget-level click-all over every walkable catalog demo.
Per demo: enter (hunt/card/landing/demo), enumerate c=1 en=1 widgets, tap each
(sliders get a drag; EditTexts get typed text), health+segv after each, recover from
dialogs/navigation via back->X->re-enter (max 2 re-entries). One log line per demo.
Skips Adaptive (hard crasher §613). Transition walked LAST (its exit wedges the UI)."""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from walkcat_drive import (sh, log, tap, drag, back, dump, rects, titles_visible,
                           alive, relaunch, ensure_toc, snap)

SKIP = {'Adaptive'}
LAST = ['Transition']

def segv():
    out = sh("C=$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1); grep -ac 'CHILDSEGV. #' $C")
    try: return int(out.split()[0])
    except Exception: return -1

def find_and_enter(name):
    """TOC -> card -> landing -> main demo. Returns True when the demo tree is up."""
    if not ensure_toc(): return False
    for _ in range(6): drag(600, 500, 600, 1600, settle=2)
    pos = None
    for _ in range(14):
        d = dump()
        if not d.strip() and not alive():
            if not relaunch(): return False
            continue
        vis = dict(titles_visible(d))
        if name in vis and 260 <= vis[name][1] <= 1870:
            pos = vis[name]; break
        drag(600, 1400, 600, 700, settle=4)
    if pos is None: return False
    ty = min(pos[1] - 150, 1750)
    tap(pos[0], ty, settle=8)
    d = dump()
    landing = rects(d, r'id=cat_demo_landing_main_demo_container')
    if not landing:
        tap(pos[0], pos[1], settle=8)
        d = dump()
        landing = rects(d, r'id=cat_demo_landing_main_demo_container')
    if not landing: return False
    lx, ly, _, _ = landing[0]
    tap(lx, ly, settle=9)
    return True

def clickables(d):
    out = []
    for x, y, txt, line in rects(d, r'c=1 en=1'):
        if x < 170 and y < 170: continue          # toolbar X = exit, never click
        if 'favorite_toggle' in line: continue    # persists state
        cls = line.split(' id=')[0].strip()
        out.append((x, y, cls, txt))
    return out

def widget_key(w): return (w[2], w[0], w[1])

def sweep(name):
    if not find_and_enter(name):
        log(f'{name}: ENTER FAILED'); return
    d0 = dump()
    todo = clickables(d0)
    total, clicked, dialogs, deaths, reenter = len(todo), 0, 0, 0, 0
    s0 = segv()
    seen = set()
    i = 0
    while i < len(todo) and reenter <= 2:
        w = todo[i]; i += 1
        k = widget_key(w)
        if k in seen: continue
        seen.add(k)
        x, y, cls, txt = w
        if 'Slider' in cls:
            drag(max(x - 200, 60), y, min(x + 200, 1140), y, settle=4)
        elif 'EditText' in cls:
            tap(x, y, settle=4)
            sh('echo wl612 > /data/local/tmp/noice_text'); time.sleep(4)
        else:
            tap(x, y, settle=4)
        clicked += 1
        if not alive():
            deaths += 1
            sh('C=$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1); '
               f'cp "$C" /data/local/tmp/shots612/stderr_click_{name.replace(" ","_")}.txt')
            log(f'  !! died clicking {cls} "{txt}" at ({x},{y}) — banked')
            if not relaunch(): break
            reenter += 1
            if not find_and_enter(name): break
            continue
        d = dump()
        if 'cat_demo_landing' in d or 'header_logo' in d or not d.strip():
            # navigated away (or empty dump) — try dialog-dismiss then re-enter
            back(settle=4)
            d = dump()
            if 'cat_demo_landing' in d or 'header_logo' in d or not d.strip():
                reenter += 1
                if not find_and_enter(name): break
                continue
        nroots = d.count('==== root[')
        if nroots > 1:
            dialogs += 1
            back(settle=4)
            d2 = dump()
            if d2.count('==== root[') > 1:
                nav = [r for r in rects(d2, r'MaterialButton|AppCompatImageButton')
                       if 'c=1' in r[3]]
                if nav: tap(nav[-1][0], nav[-1][1], settle=4)
        new = [w2 for w2 in clickables(dump()) if widget_key(w2) not in seen]
        for w2 in new[:6]:
            if len(todo) < total + 24: todo.append(w2)
    ds = segv() - s0
    snap(f'click_{name.replace(" ","_")}')
    log(f'{name}: widgets={total} clicked={clicked} extra={len(todo)-total} '
        f'dialogs={dialogs} deaths={deaths} segvdelta={ds} alive={alive()}')

def toc_pages():
    """TOC-level pages: search + preferences dialog."""
    if not ensure_toc(): return
    d = dump()
    for wid, nm in (('cat_toc_search_button', 'SEARCH-PAGE'), ('cat_toc_preferences_button', 'PREFS-DIALOG')):
        r = rects(d, f'id={wid}')
        if not r:
            log(f'{nm}: button not found'); continue
        tap(r[0][0], r[0][1], settle=7)
        d2 = dump()
        n = len(d2.splitlines()); roots = d2.count('==== root[')
        snap(nm)
        log(f'{nm}: lines={n} roots={roots} alive={alive()} segv={segv()}')
        back(settle=5)
        if nm == 'SEARCH-PAGE' and 'header_logo' not in dump():
            ensure_toc()
        d = dump()

def main():
    sh('mkdir -p /data/local/tmp/shots612')
    toc_pages()
    d = dump()
    if 'cat_toc_title' not in d and not ensure_toc():
        log('FATAL: no TOC'); return
    for _ in range(6): drag(600, 500, 600, 1600, settle=2)   # enumerate from the TOP
    seen, stable = {}, 0
    while stable < 2:
        d = dump()
        new = 0
        for t, _ in titles_visible(d):
            if t not in seen: seen[t] = True; new += 1
        stable = stable + 1 if new == 0 else 0
        drag(600, 1500, 600, 420)
    order = [n for n in seen if n not in SKIP and n not in LAST] + [n for n in LAST if n in seen]
    log(f'CLICK-SWEEP over {len(order)} demos')
    for name in order:
        sweep(name)
    log('CLICK-SWEEP COMPLETE')

if __name__ == '__main__':
    main()
