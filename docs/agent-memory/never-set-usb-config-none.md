---
name: never-set-usb-config-none
description: HARD RULE — NEVER set persist.sys.usb.config (to none or anything) to dismiss USB dialogs; it kills hdc-over-USB persistently and needs physical on-device recovery. Has bricked the host link 2-3× incl. subagents ignoring the warning.
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

**NEVER run `param set persist.sys.usb.config none`** (or set `persist.sys.usb.config` to anything) — on the DAYU200/RK3568 it disables the whole USB gadget function, so **hdc-over-USB dies on the next reboot**. The value is `persist.` so a reboot does NOT undo it; recovery requires the USER to physically re-enable USB debugging on-device (Settings → System → Developer options toggle resets it to `hdc_debug`). The keep-alive value is `hdc_debug` (or `hdc`).

**Why this is a standing rule:** it has now broken the host link **2-3 times**, including a spawned subagent that did it **despite an explicit ALL-CAPS warning in its prompt**. The user is (rightly) frustrated: "usb debugger turned off by you again." Treat this as load-bearing.

**How to apply:**
- Never change `persist.sys.usb.config`. Don't set it to dismiss a dialog, ever. There is no legitimate reason.
- Subagents violate this even when warned in prose → for any device-deploy delegation, either keep USB-adjacent steps in the MAIN agent, or put this prohibition as the literal FIRST line of the prompt and state that the dialog is dismissed via `gadget_conn_prompt`, NEVER via the usb config.
- **To actually disable the recurring USB dialog SAFELY (source-confirmed, none touch persist.sys.usb.config):**
  - "USB connection mode / file transfer / charging" popup → `param set persist.usb.setting.gadget_conn_prompt false` (owner bundle `com.usb.right`, ability `UsbFunctionSwitchExtAbility`; default already false — only shows if set true). Or `bm disable -n com.usb.right -a com.usb.right.UsbFunctionSwitchExtAbility`.
  - "Allow USB debugging from this computer?" hdc-auth prompt → authorize once so the host key caches in `/data/misc/hdc/hdc_keys` (survives reboot), or `param set const.hdc.secure 0` on this dev board.
- If the link is already broken: tell the user to re-enable USB debugging on-device; do NOT promise a remote fix (hdc is dead until they do).

Related: [[adapter-app-launch-bringup]], [[adapter-bootloop-wipe-recovery]] (the other persistent-state footgun is the `critical` init flag → bootloop brick).
