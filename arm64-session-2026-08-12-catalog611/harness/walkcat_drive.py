#!/usr/bin/env python3
"""§612: full Material Catalog demo walkthrough driver.
Enumerates the TOC grid, then for each category: tap card -> landing -> tap main Demo row
-> dump/health -> screenshot (device-side, always) -> back out. One report line per demo.
Never guesses coordinates: every tap comes from a fresh dump (§608 lesson: rows shift)."""
import os, subprocess, re, sys, time

HDC = os.environ.get('HDC', 'hdc')  # hdc client binary (full path when driving from WSL)
LOG = open(os.environ.get('WALK_LOG', 'walk612.log'), 'a', buffering=1)

def log(msg):
    print(msg); LOG.write(msg + '\n')

def sh(cmd, timeout=60):
    r = subprocess.run([HDC, 'shell', cmd], capture_output=True, text=True, timeout=timeout)
    return r.stdout

def tap(x, y, settle=6):
    sh(f'echo "{x} {y}" > /data/local/tmp/noice_tap'); time.sleep(settle)

def drag(x1, y1, x2, y2, settle=5):
    sh(f'echo "{x1} {y1} {x2} {y2}" > /data/local/tmp/noice_tap'); time.sleep(settle)

def back(settle=5):
    sh('echo back > /data/local/tmp/noice_tap'); time.sleep(settle)

def dump():
    return sh('cd /data/local/tmp/asx && sh vdump.sh', timeout=90)

def health():
    c = "C=$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1); P=$(cat /data/local/tmp/asx/walkpid)"
    out = sh(c + '; [ -d /proc/$P ] && echo ALIVE || echo DEAD; grep -ac "CHILDSEGV. #" $C')
    lines = out.split()
    return (lines[0] if lines else '?'), (lines[1] if len(lines) > 1 else '?')

def rects(d, pattern):
    out = []
    for line in d.splitlines():
        if re.search(pattern, line):
            m = re.search(r'rect=\[(-?\d+),(-?\d+) (\d+)x(\d+)\]', line)
            t = re.search(r'"(.*)"\s*$', line)
            if m:
                x, y, w, h = map(int, m.groups())
                out.append((x + w // 2, y + h // 2, t.group(1) if t else '', line.strip()))
    return out

def titles_visible(d):
    return [(t, (x, y)) for x, y, t, _ in rects(d, r'id=cat_toc_title')]

def snap(name):
    sh(f'snapshot_display -f /data/local/tmp/shots612/{name}.jpeg >/dev/null 2>&1')

def alive():
    out = sh('P=$(cat /data/local/tmp/asx/walkpid); [ -d /proc/$P ] && echo OK')
    return 'OK' in out

def relaunch():
    for i in range(3):
        sh('sh /data/local/tmp/asx/walkcat5.sh', timeout=900)
        if alive(): return True
        log(f'relaunch attempt {i+1} failed (child dead after launch)')
    return False

def ensure_toc():
    """Exit to the TOC. ⚠️`back` does NOT pop this app's fragment nav (verified live:
    6 backs from a landing page = no-op) — the toolbar X/up button is the exit path."""
    for _ in range(8):
        d = dump()
        if 'header_logo' in d or 'cat_toc_title' in d: return True
        nav = [r for r in rects(d, r'AppCompatImageButton') if r[0] < 170 and r[1] < 170]
        if nav:
            tap(nav[0][0], nav[0][1], settle=6)
        else:
            back(settle=5)   # last resort (dialogs)
    log('ensure_toc: stuck off-TOC — relaunching')
    relaunch()
    d = dump()
    return 'header_logo' in d or 'cat_toc_title' in d

def main():
    sh('mkdir -p /data/local/tmp/shots612')
    if not ensure_toc():
        log('FATAL: cannot reach TOC'); return
    # -------- Phase 1: enumerate the TOC by scrolling --------
    seen = {}
    stable = 0
    while stable < 2:
        d = dump()
        new = 0
        for t, pos in titles_visible(d):
            if t not in seen:
                seen[t] = True; new += 1
        if new == 0: stable += 1
        else: stable = 0
        drag(600, 1500, 600, 420)
    order = list(seen.keys())
    log(f'TOC categories found: {len(order)}: {order}')
    skip = set(sys.argv[1:])
    if skip:
        order = [n for n in order if n not in skip]
        log(f'skipping {sorted(skip)} -> walking {len(order)}')
    # scroll back to top
    for _ in range(max(3, len(order) // 2)):
        drag(600, 500, 600, 1600, settle=3)

    # -------- Phase 2: walk each demo --------
    done = []
    for idx, name in enumerate(order):
        # find the card: jump to the TOP first (hunt only scrolls down; without the
        # reset, any card above the current viewport is unreachable — bug found live
        # at demo 2), then scroll down until the title is comfortably tappable.
        for _ in range(6):
            drag(600, 500, 600, 1600, settle=2)
        pos = None
        for attempt in range(14):
            d = dump()
            if not d.strip() and not alive():
                log(f'  hunt: child DEAD — relaunching')
                if not relaunch(): break
                for _ in range(6): drag(600, 500, 600, 1600, settle=2)
                continue
            vis = dict(titles_visible(d))
            if name in vis and 260 <= vis[name][1] <= 1650:
                pos = vis[name]; break
            drag(600, 1400, 600, 700, settle=4)
        if pos is None:
            log(f'{idx+1}/{len(order)} {name}: CARD NOT FOUND (scroll hunt failed)'); continue
        tap(pos[0], pos[1] - 150, settle=9)   # card centre is ~150px above the title
        d = dump()
        landing = rects(d, r'id=cat_demo_landing_main_demo_container')
        if not landing:
            # maybe tapped wrong thing; try the title itself
            tap(pos[0], pos[1], settle=9)
            d = dump()
            landing = rects(d, r'id=cat_demo_landing_main_demo_container')
        if not landing:
            top = rects(d, r'header_logo|cat_toc_title')
            log(f'{idx+1}/{len(order)} {name}: NO LANDING (still_toc={bool(top)})')
            snap(f'{idx:02d}_{name.replace(" ","_")}_nolanding')
            continue
        lx, ly, _, _ = landing[0]
        tap(lx, ly, settle=10)                 # enter the main demo fragment
        d = dump()
        n_widgets = len(d.splitlines())
        classes = set(re.findall(r'^\s*(\w+) id=', d, re.M))
        a, segv = health()
        snap(f'{idx:02d}_{name.replace(" ","_")}')
        interesting = [c for c in classes if c not in
                       ('DecorView','LinearLayout','FrameLayout','ContentFrameLayout',
                        'FitWindowsFrameLayout','CoordinatorLayout','ConstraintLayout','View',
                        'AppCompatImageView','MaterialTextView','NestedScrollView','RecyclerView',
                        'AppBarLayout','CollapsingToolbarLayout','Toolbar','AppCompatImageButton')]
        log(f'{idx+1}/{len(order)} {name}: {a} segv={segv} widgets={n_widgets} distinct={sorted(interesting)[:8]}')
        if a == 'DEAD':
            tag = f'{idx:02d}_{name.replace(" ", "_")}'
            sh('C=$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1); '
               f'cp "$C" /data/local/tmp/shots612/stderr_{tag}.txt')
            log(f'  !! child died in {name} — stderr banked, relaunching')
            relaunch()
            continue
        done.append(name)
        ensure_toc()                           # exits via toolbar X (back is a no-op here)
    log(f'WALK COMPLETE: {len(done)}/{len(order)} demos entered clean: {done}')

if __name__ == '__main__':
    main()
