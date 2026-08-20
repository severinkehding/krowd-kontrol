# Issue #20 — Overcrowd screen-distortion visual effect (audio/visual treatment)

Resurrection of closed PR #167, whose code was verified correct but whose
app-source-tracked mirror had gone stale against main (missing PR #164's
FKrowdKontrolRootSurgeBossTest friend grant), producing a real merge conflict.
This branch regenerates every mirror file from the live app/ state on top of
current main — the exact remediation #167's rejection prescribed.

- `UOvercrowdVisualEffectSubsystem`: world subsystem flipping a screen-space
  distortion on/off from `UOvercrowdDetectionComponent`'s panic-overload
  broadcast, in lockstep with the merged audio muffling (PR #142).
- `UCameraModifier_OvercrowdDistortion`: the camera-modifier that applies the
  distortion while the overcrowd state is Active.
- Tests: `KrowdKontrol.Unit.OvercrowdVisualEffectSubsystem` (state flips) and
  `KrowdKontrol.Unit.OvercrowdAudioVisualSync` (audio + visual react to the
  same broadcast in step), plus a distortion-state test listener.
