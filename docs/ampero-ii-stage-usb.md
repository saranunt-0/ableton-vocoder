# Ableton ↔ Hotone Ampero II Stage over USB-C

## TL;DR

**Why you hear the backing track but can't record it:** Ableton's normal
output (Master → USB **1/2**) is a *monitor* path on the Ampero. It is mixed
straight into the Ampero's outputs and headphones, *after* the effect chains.
It never enters a chain, the looper, or the USB channels going back to the
computer. So it's audible but there is nothing to record.

**To send an Ableton track *into* the Ampero**, route that track to USB
**3/4** (or 5/6, 7/8) and set an Ampero chain's **input node Source** to
**USB 3/4**. This is the Ampero's built-in *reamp* routing. The track then
passes through that chain like a guitar would, so it reaches the outputs and
the USB returns to the computer (and, depending on looper placement, the looper).

```
            USB OUT 1/2  ──────────────────────────────►  Ampero outputs / phones   (monitor only)
 Ableton ─┤
            USB OUT 3/4  ──►  Chain input node (Source = USB 3/4) ──► blocks ──► outputs
                                                                       └──► USB IN 1/2 (or 3/4) ──► Ableton (recordable)
```

> Confidence: the reamp Source option, the USB Only output mode and the
> noise-gate gotcha are documented in Hotone's Ampero II manuals or reported
> by users (sources at the end). The exact USB channel numbers below come
> partly from the **Ampero II Stomp** manual, so verify them on your Stage by
> watching Ableton's input meters. Firmware updates have moved menus before.

---

## What the USB channels carry

The Ampero II Stage is an 8-in / 8-out class-compliant USB audio interface
(macOS needs no driver; Windows needs the **Hotone ASIO driver**).

**Ableton → Ampero (Ableton "Ext. Out")**

| Ableton output | On the Ampero |
|---|---|
| 1/2 | Monitor playback, mixed after the chains (this is what you hear now) |
| 3/4, 5/6, 7/8 | Selectable as a chain's **input Source** (reamp) |

**Ampero → Ableton (Ableton "Ext. In")**, from the Stomp manual plus Stage examples (verify):

| Ableton input | Signal |
|---|---|
| 1/2 | Chain A output (processed) |
| 3/4 | Chain B output (processed) |
| 5/6 | FX Loop Return |
| 7/8 | Dry Input L / R, independent of the patch |

---

## Recipe A: send a backing track into the Ampero (and record it)

**Ableton**

1. Preferences → Audio → **Output Config**: enable **3/4** (Live only shows
   enabled pairs). **Input Config**: enable 1/2 and 3/4.
2. On the backing-track track: *Audio To* → **Ext. Out** → **3/4**.
   It now goes to the Ampero instead of the Master.

**Ampero II Stage**

3. Use a dual-chain patch: **Chain A** = guitar (input Source = Input),
   **Chain B** = backing track with input **Source = USB 3/4**, and no amp/cab
   blocks (or only the effects you want on it).
4. **Noise gate**: the global/input noise gate defaults to *Input L*. Point it
   at the USB source (or turn it off for chain B), or the backing track will be
   gated to silence.
5. Global → USB Audio / I/O: keep **OUTPUT SOURCE = Normal** (effects + USB).
   *USB Only* would drop your guitar from the outputs.

**Recording**

- *Into Ableton*: arm an audio track with *Audio From* = Ext. In **3/4**
  (chain B, the backing track) or **1/2** (chain A, guitar). Set its *Monitor* to
  **Off**, and its *Audio To* must **not** be 3/4, otherwise you build a feedback loop.
- *Into the Ampero looper*: set the looper position to **Post** so it records
  after the chains, and check that the chain carrying the backing track is
  inside what the looper captures. This depends on your patch/output routing
  and I could not confirm it from the manual. Test it once with the
  backing track alone.

> Don't need the Ampero to *process* the backing track? Then you don't need
> any of this: the backing track is already in Ableton. Record only your guitar
> (Ext. In 1/2) on its own track, and if you want one combined file, record a new
> track with *Audio From* = **Resampling**.

## Recipe B: hear only the DAW while recording a DI guitar

Global → **OUTPUT SOURCE = USB Only**. The Ampero outputs only what Ableton
plays (useful when you monitor through amp-sim plugins in Ableton and record
the dry guitar from Ext. In 7/8).

## Mic for the vocoder

The Stage's **Input R** is an XLR / ¼" combo jack with a mic preamp (up to
52 dB, phantom power for condensers). To use it as the vocoder's voice:

1. Plug the mic into Input R and set its input mode to Mic. Turn on phantom
   power only for condensers.
2. In Ableton, create the **Vocal** track with *Audio From* = Ext. In and
   pick the *dry* channel for Input R (expected **8**, from the 7/8 = dry L/R
   pair). Talk and pick the channel whose meter moves.
3. Keep the mic out of any amp/cab chain: the vocoder wants a clean voice.
4. Follow [ableton-setup.md](ableton-setup.md). To play the vocoder *through*
   Ampero effects, set the vocoder track's *Audio To* to **Ext. Out 3/4** and use
   Recipe A, steps 3–5.

## Checklist when something is silent

- [ ] Ableton Preferences → Output Config has 3/4 enabled
- [ ] Track *Audio To* = Ext. Out 3/4 (not Master)
- [ ] Ampero chain input **Source = USB 3/4** (the Stage manual's reamp example also sets *INPUT CH* = L for a mono source)
- [ ] Noise gate source matches the USB input (or gate off)
- [ ] The patch isn't bypassed: bypassing the unit also bypasses USB audio
- [ ] Windows: Ableton uses the **Hotone ASIO** driver, not MME/DirectX
- [ ] Sample rate in Ableton matches the Ampero's USB sample rate setting

## Sources

- Hotone, *Ampero II Stage User Manual* (firmware 1.4.1 / 1.5.1): Global → USB Audio, I/O, reamp example — https://www.hotone.com/support/1 (mirror: https://www.manualslib.com/manual/3373569/Hotone-Ampero-Ii-Stage.html)
- Hotone, *Ampero II Stomp User Manual* (firmware 1.2.1): USB channel assignments, input node Source "USB 3/4~7/8 … convenient for reamping", OUTPUT SOURCE "USB Only"
- The Gear Forum, Ampero II Stomp review thread: noise gate defaults to Input L when reamping — https://thegearforum.com/threads/hotone-ampero-ii-stomp-review.7011/page-2
- Retailer spec sheets (Input R XLR combo, 52 dB mic preamp, phantom power), e.g. https://www.zzounds.com/item--HOTAMPEROIISTAGE
