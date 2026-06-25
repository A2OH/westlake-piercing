# Device-loss footguns (read before any device work)

These cost a hardware recovery (reflash / physical USB toggle). They survive
reboot. If you delegate device work, put the relevant prohibition as the **literal
first line** of the brief.

## 1. NEVER `param set persist.sys.usb.config none` (or to anything but `hdc_debug`)

Setting `persist.sys.usb.config` to `none` (or any non-`hdc_debug` value) disables
the USB gadget → **hdc-over-USB dies on the next reboot**. Because the key is
`persist.`, the reboot does **not** undo it → the only recovery is to physically
toggle USB debugging on the device. This broke the link 2–3 times in the project,
including a subagent that did it **despite an all-caps warning**.

- Safe value: `param set persist.sys.usb.config hdc_debug`.
- Dismiss the USB connection-mode popup (does NOT disable the gadget):
  `param set persist.usb.setting.gadget_conn_prompt false`.
- Dismiss the hdc-auth prompt: authorize once (key caches in
  `/data/misc/hdc/hdc_keys`) or `param set const.hdc.secure 0`.

## 2. NEVER add `critical` to a fail-prone init service (e.g. `appspawn_x.cfg`)

`"critical":[…]` in an init service config makes init **reboot the device** if the
service fails. With `"start-mode":"boot"` that reboot happens before recovery →
**bootloop brick** → the USB endpoint disappears → reflash + full wipe. **This
happened** (a `critical` appspawn-x bootlooped a DAYU200 → full reflash + wipe).

- Ship appspawn-x **non-critical**: `"critical":[]`, `"start-mode":"ondemand"`.
- Reboot **manually** after staging (no `--reboot` from a deploy script) so a bad
  artifact doesn't bootloop before you can roll back.
- The brick is also partly environmental (the vendor's own procedure bricks this
  exact board). Bisect single artifacts; get serial/UART logs through the reboot;
  don't assume "I deployed it wrong."

## 3. The board is DC-powered with a MOCK battery — reboot freely

`/sys/class/power_supply/` is **empty**; any "low battery / 11%" warning is
**fake** — the board cannot shut down from battery. **Rebooting is the recovery
move, not a risk.** Set a simulated level for screenshots / to suppress the
low-battery lockscreen:
```bash
hidumper -s 3302 -a "--capacity 95"
power-shell wakeup; power-shell timeout -o 600000
```

## 4. Other operational rules

- **Never `kill <pid>`; never touch `com.ohos.launcher`.** Stop services with
  `begetctl stop_service <name>` / `killall <name>`. A bare `kill <launcher-pid>`
  got the pid recycled by hdcd → hdc lost → reflash.
- **Never `hdc file send <src> /system/...` directly.** Stage:
  `send → /data/local/tmp/stage/<basename>` → `ls -la` to confirm it is a `-rw-`
  *file* (not a `drwx` dir from the hdc auto-mkdir quirk) → `cp` into `/system`.
  Size-verify + md5-verify after every send; the big `boot-framework.*`
  (~51/37/23 MB) are the likeliest to be silently truncated.
- **libart is recoverable, not a hard brick** — it only affects Android-app
  (`adapter_child`) processes; OHOS + hdc boot regardless. Back up before
  deploying. (A bad **boot image** or a `critical` service is the brick risk, not
  libart.)
