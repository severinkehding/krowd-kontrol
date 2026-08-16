# Itch.io Storefront-Readiness Checklist

Tracks the minimum requirements for krowd-kontrol's Itch.io storefront listing, per
the release-readiness PRD's REQ-2. Items 2 and 3 are transcribed verbatim from the
GDD (`/home/severin/krowd-kontrol-prds/og-google-doc/krowd-kontrol-gdd.md`), the same
source `MISSION.md` itself derives from. Item 1 remains blocked with the reason why,
rather than filled with placeholder-but-plausible content. A follow-up issue will
fill in item 1 once the underlying gameplay dependency exists.

- [ ] Footage/GIF of the core gameplay loop at visible crowd density.
  Blocked: the P0 core loop and enemy AI (`MISSION.md` Core Capabilities, P0 tier)
  are not yet implemented/playable in `app/`, so no footage exists and none is
  fabricated here.
- [ ] Short store description.
  Source (verbatim from GDD "Short Description",
  `/home/severin/krowd-kontrol-prds/og-google-doc/krowd-kontrol-gdd.md:76`):
  "Save tiny robot KR-0WD from hostile robots that are after the scarcest resource
  in the universe: his energy! This twist on classic top-down shooters lets you
  control large crowds of enemies in a unique visual style mixing colorful lights
  and shadows to bring an abandoned space station back to life."
- [ ] Control-scheme summary.
  Source (verbatim from GDD "Control Scheme",
  `/home/severin/krowd-kontrol-prds/og-google-doc/krowd-kontrol-gdd.md:386-402`;
  ability names cross-checked against `AbilitySlot.h`'s locked `EAbilitySlot` enum —
  Stun, Sleep, Root, Fear, Snare all match):

  | Keyboard/Mouse | Controller | Action |
  |---|---|---|
  | W | LSB Up | Player moves up |
  | S | LSB Down | Player moves down |
  | A | LSB Left | Player moves left |
  | D | LSB Right | Player moves right |
  | Mouse Pointer | RSB | Change player direction |
  | Space | A | Player jumps |
  | F | B | Player dashes in the direction they're currently facing |
  | Left Mouse Button | RT | Stun ability |
  | Right Mouse Button | LT | Sleep ability |
  | Q | RB | Root ability |
  | E | LB | Snare ability |
  | Middle Mouse Button or Left + Right Mouse Button | LT + RT | Fear ability |
  | Esc | Start | Show/dismiss Pause Menu |
