# Operator hotfix: all game input dead (WASD, abilities) — consuming AnyKey binding

No issue number: diagnosed and fixed interactively during the 2026-08-26 operator
playtest (a prior interactive session applied the `app/` fix and rebuilt, but died
before mirroring or reporting; this PR completes the paper trail).

PR #272 added a briefing-card dismiss binding on `EKeys::AnyKey` in
`AKrowdKontrolPlayerController::SetupInputComponent()`. `FInputKeyBinding` consumes
its key by default, and the controller's InputComponent is processed before the
possessed pawn's in `APlayerController::BuildInputStack`'s ordering — so every key
press (WASD axes included; a consumed key contributes nothing to its axes that
frame) was eaten before `AFlatCamera3DPrototypePawn` ever saw it. Result: total
input death in every PIE session since #272 merged, hit in both the 2026-08-24 and
2026-08-26 operator playtests.

Fix: keep the binding but set `bConsumeInput = false` on it — briefing dismiss
still fires on any key, and the press passes through to the pawn's own bindings.
Confirmed in the 2026-08-26 playtest: movement and abilities work, briefing still
dismisses, and the operator cleared Level 1 end-to-end.

The change is already live in `app/` (compiled 07:14, editor running it since
07:20); this PR mirrors it into `app-source-tracked/` per D-009.

## Code-review follow-up (PR #309 review, 2026-08-26)

The review caught that `bConsumeInput = false` alone was incomplete:
`UBriefingCardWidget::ShowBriefing()` pauses the world, and UE skips any key
binding without `bExecuteWhenPaused` while paused — so press-any-key-to-dismiss
never actually fired; the card only closed via its 8s auto-dismiss timer. Fixed
by also setting `bExecuteWhenPaused = true` (safe: `bGamePaused` is snapshotted
per frame, so the unpausing keypress cannot leak into that frame's pawn axes).
Also added `KrowdKontrol.Unit.PlayerControllerInputBinding`, which inspects the
registered `FInputKeyBinding` directly and pins both flags (plus the adjacent
#308 F1 binding's presence) — the prior briefing test called the handler
directly and so passed the whole time input was dead. Verified: clean UBT build
+ test passes 1/1 headless.
