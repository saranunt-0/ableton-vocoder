# Module: ampero-routing
<!-- Hotone Ampero II Stage + Ableton over USB-C -->

---

## [2026-10-10] "I hear the backing track but can't record it in Ampero via USB-C"

**Type**: `investigation`
**Status**: `handed-off` (needs confirmation on the user's hardware)

### Context
The user plays a backing track from Ableton and hears it on the Ampero II Stage,
but can't record it "in Ampero" over USB-C.

### Work Done
- The hotone.com / manualslib PDFs could not be fetched from this environment
  (egress policy), so the evidence comes from search excerpts of the official
  manuals (Stage 1.4.1/1.5.1, Stomp 1.2.1) and retailer spec sheets.
- Established: 8×8 USB interface; chain input node Source can be USB 3/4…7/8
  ("convenient for reamping"); OUTPUT SOURCE Normal vs USB Only; the Stage manual's
  reamp example uses input node = USB OUT 3/4, DAW track out = USB Audio Output 3/4;
  bypassing the unit bypasses USB audio; Input R is an XLR combo with a 52 dB mic
  preamp and phantom power.
- User report: the noise gate defaults to Input L, so a reamped USB source is silent
  unless the gate source is changed.

### Root Cause / Outcome (ranked)
1. **Most likely**: USB OUT 1/2 is a post-chain monitor path, so the backing track
   never reaches a chain, the looper or the USB returns. Fix: route the track to
   Ext. Out 3/4 and set the chain input Source to USB 3/4 (docs/ampero-ii-stage-usb.md).
2. Ableton Output Config has only 1/2 enabled, so 3/4 isn't selectable. Enable it.
3. Noise gate on Input L mutes the USB source. Retarget or disable it.
4. Windows: non-ASIO driver exposes only 2 channels. Use Hotone ASIO.

### Unverified Items
- [ ] Exact USB IN channel map on the Stage (expected 1/2 chain A, 3/4 chain B,
      5/6 FX return, 7/8 dry; the Stomp manual says this, the Stage manual cites 7 for dry).
- [ ] Whether the looper (Post position) captures a USB-sourced chain.
- [ ] What the user means by "record in Ampero": the looper, recording into Ableton,
      or processing the track through Ampero effects (asked).
