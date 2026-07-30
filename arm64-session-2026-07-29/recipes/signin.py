#!/usr/bin/env python3
"""Sign noice in, end to end. The in-app link handler no longer works: `bm dump` shows the bundle
registers only EntryAbility, so `aa start ... SignInLinkHandlerActivity` fails. Do it host-side.

  python3 recipes/signin.py            # create a mailbox, sign up, exchange, write credentials.xml

★The credential exchange needs header `X-Refresh-Token`. `Authorization: Bearer` returns 400.
"""
import json, os, random, re, string, subprocess, sys, time, urllib.error, urllib.request

# Paths come from recipes/env.sh (shell vars do NOT expand inside a python string literal).
# Run this as:  . recipes/env.sh && python3 recipes/signin.py
HDC = os.environ.get("HDC") or sys.exit("run `. recipes/env.sh` first (HDC unset)")
WSL_STAGE = os.environ.get("WIN_STAGE") or sys.exit("run `. recipes/env.sh` first (WIN_STAGE unset)")
WIN_STAGE = subprocess.run(["wslpath", "-w", WSL_STAGE],
                           capture_output=True, text=True).stdout.strip()
APPDIR = "/data/app/el2/100/base/com.github.ashutoshgngwr.noice"
UA = "Noice/2.5.1 (Android 14; OpenHarmony) OkHttp/4.10.0"


def req(url, data=None, hdrs=None, method=None):
    h = {"Accept": "application/json", "User-Agent": UA}
    if data is not None:
        h["Content-Type"] = "application/json"
    if hdrs:
        h.update(hdrs)
    body = json.dumps(data).encode() if data is not None else None
    r = urllib.request.Request(url, data=body, headers=h,
                               method=method or ("POST" if data else "GET"))
    try:
        with urllib.request.urlopen(r, timeout=45) as f:
            raw = f.read()
            return f.status, (json.loads(raw) if raw else None)
    except urllib.error.HTTPError as e:
        raw = e.read()
        try:
            return e.code, json.loads(raw)
        except Exception:
            return e.code, raw[:200].decode("utf8", "replace")


def main():
    st, doms = req("https://api.mail.tm/domains")
    lst = doms["hydra:member"] if isinstance(doms, dict) else doms   # NOTE: plain list, not hydra
    dom = lst[0]["domain"]
    addr = "noice" + "".join(random.choice(string.digits) for _ in range(8)) + "@" + dom
    pw = "Noice!Test123"
    print("mailbox:", addr, req("https://api.mail.tm/accounts", {"address": addr, "password": pw})[0])
    jwt = req("https://api.mail.tm/token", {"address": addr, "password": pw})[1]["token"]

    print("signUp:", req("https://api.trynoice.com/v1/accounts/signUp",
                         {"email": addr, "name": "Noice Tester"})[0])

    def mget(u):
        r = urllib.request.Request(u, headers={"Authorization": "Bearer " + jwt,
                                              "Accept": "application/json"})
        with urllib.request.urlopen(r, timeout=45) as f:
            return json.loads(f.read())

    msg = None
    for _ in range(30):
        d = mget("https://api.mail.tm/messages")
        items = d["hydra:member"] if isinstance(d, dict) else d
        if items:
            msg = items[0]
            break
        time.sleep(4)
    if not msg:
        sys.exit("no sign-in email arrived")
    full = mget("https://api.mail.tm/messages/" + msg["id"])
    body = (full.get("text") or "") + " " + " ".join(full.get("html") or [])
    import re
    m = re.search(r"token=([A-Za-z0-9._\-]{40,})", body)
    if not m:
        sys.exit("no token in email")
    token = m.group(1)

    # ★the exchange: X-Refresh-Token, NOT Authorization: Bearer
    st, creds = req("https://api.trynoice.com/v1/accounts/credentials",
                    hdrs={"X-Refresh-Token": token})
    if st != 200:
        sys.exit(f"exchange failed: {st} {creds}")

    xml = ("<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n<map>\n"
           f"    <string name=\"refresh-token\">{creds['refreshToken']}</string>\n"
           f"    <string name=\"access-token\">{creds['accessToken']}</string>\n"
           "</map>\n")
    open("/tmp/credentials.xml", "w").write(xml)
    subprocess.run([HDC, "shell", "pkill -9 -f run406.sh; pkill -9 appspawn-x; "
                    "aa force-stop com.github.ashutoshgngwr.noice"], capture_output=True)
    os.makedirs(WSL_STAGE, exist_ok=True)
    subprocess.run(["cp", "/tmp/credentials.xml", WSL_STAGE + "/credentials.xml"])
    subprocess.run([HDC, "file", "send", WIN_STAGE + r"\credentials.xml",
                    "/data/local/tmp/credentials.xml"], capture_output=True)
    subprocess.run([HDC, "shell",
                    f"mkdir -p {APPDIR}/shared_prefs && "
                    f"cp /data/local/tmp/credentials.xml "
                    f"{APPDIR}/shared_prefs/com.trynoice.api.client.auth.credentials.xml && "
                    f"chown 20010042:20010042 {APPDIR}/shared_prefs/com.trynoice.api.client.auth.credentials.xml && "
                    f"chmod 660 {APPDIR}/shared_prefs/com.trynoice.api.client.auth.credentials.xml"],
                   capture_output=True)
    print("credentials installed — restart the app; /v2/subscriptions should now return 200")


if __name__ == "__main__":
    main()
