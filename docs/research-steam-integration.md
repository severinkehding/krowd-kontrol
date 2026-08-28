# Research: Steam Integration via SteamCore PRO

**Author**: operator-side research, 2026-08-29, at the operator's request ahead
of a Steam PRD. This is a RESEARCH document, not a PRD — `prd-to-issues` should
ignore it. Sources at the bottom.

## The plugin: SteamCore PRO (eelDev)

- **What it is**: a Steamworks SDK wrapper for Unreal Engine exposing the
  Steam interfaces to both **Blueprints and C++**, with its **own Online
  Subsystem ("SteamCore")** that replaces the engine's built-in Steam
  subsystem — no engine-source changes needed.
- **UE 5.8 supported** since v1.1.5 (2026-06-18); latest release v1.1.7
  (2026-07-23) — actively maintained. Matches this project's engine.
- **Price**: $129.99 on Fab.
- **Coverage** (far more than we need): achievements + stats, remote/cloud
  storage, friends, rich presence, overlay, screenshots, input, inventory,
  Workshop/UGC, multiplayer sessions/VOIP/P2P (irrelevant here — MISSION
  excludes online multiplayer permanently).
- **What we'd actually use for KrowdKontrol** (single-player): init +
  overlay, achievements/stats, and Remote Storage (cloud-syncing the Crowd
  Mastery save — the "sync to Steam later" the mastery PRD anticipated;
  storage already funnels through `UCrowdMasteryTotalSubsystem`, so the sync
  layer wraps one place, as designed).
- **Blueprint note**: the plugin's Blueprint surface is real, but this
  project is C++-first (no Blueprint gameplay assets) — we'd use its C++
  API/subsystem. Either path is supported; "Blueprint-only quick win" is not
  the differentiator for us, active 5.8 maintenance and completeness are.
- **Free alternative considered**: the engine's built-in
  `OnlineSubsystemSteam` covers basic achievements/sessions but is known
  gap-ridden (especially from Blueprints) and would still need all the same
  Steamworks-side setup. Given a $129.99 plugin vs integration time on a
  wrapper the engine half-maintains, the plugin is the pragmatic pick; the
  PRD should still name this alternative.

## Setup mechanics (from the plugin docs)

1. Disable the engine's built-in Steam plugins (Online Subsystem Steam etc.).
2. Install from Fab via the Epic launcher, then **move** (not copy) the
   plugin from the engine's Marketplace directory into the project's
   `Plugins/` folder.
3. C++ use: add `SteamCorePro` to `KrowdKontrol.Build.cs` dependencies.
4. `DefaultEngine.ini`:
   ```ini
   [OnlineSubsystemSteamCore]
   bEnabled=True
   SteamDevAppId=480
   SteamAppId=480

   [OnlineSubsystem]
   DefaultPlatformService=SteamCore
   ```
5. **Development can start with App ID 480** ("Spacewar", Valve's public test
   app) — the Steam client must be running locally. That means plugin
   integration, init, and overlay work can begin BEFORE any Steamworks
   paperwork exists. Achievements/stats against our own definitions need the
   real App ID + partner-portal configuration.
6. Real testing of shipping builds requires uploading to Steam (SteamPipe)
   and launching through the Steam client; development builds test locally.

## Operator prerequisites (the manual, human-only checklist)

These require the operator's identity/money and cannot be automated:

1. **Create a Steamworks partner account** — https://partner.steamgames.com
   → "Join Steamworks". Legal name, address, e-signing the Steamworks
   Distribution Agreement. An individual (no company) is fine.
2. **Identity, bank, and tax verification** — done inside the partner portal
   (bank account for payouts, tax interview e.g. W-8BEN for non-US
   individuals, identity documents). Usually the slowest part; Valve reviews
   it during the 30-day window below.
3. **Pay the $100 Steam Direct fee** (per game; recoupable once the game
   passes $1,000 adjusted gross revenue). Paying it creates the **App ID**
   — the number the plugin config needs.
4. **Mandatory 30-day wait** after the fee before the first release can go
   live. Plan around it; integration work proceeds meanwhile.
5. **Build the store ("Coming Soon") page** — description, capsule images at
   Valve's required sizes, screenshots, trailer optional; submit for Valve
   review (typically a few days), and it must be **publicly visible ≥2
   weeks before launch**.
6. **Configure Steamworks app settings** — achievements/stats definitions,
   cloud-save quotas, depots/branches for builds (this part the factory/agent
   can specify and largely script via the portal's expectations; the operator
   clicks and approves).
7. Realistic end-to-end timeline from zero to a releasable Steam build:
   **4–6 weeks minimum**, dominated by the 30-day wait — not by code.

## Governance gates the PRD must clear (operator decisions)

1. **MISSION.md excludes Steam-specific integration** "before the Itch.io
   release has actually met its own 'overwhelmingly positive feedback' bar"
   — a deliberate Itch-first strategy. A Steam PRD needs the operator to
   either amend that ordering or scope the PRD as *preparation only* (plugin
   integrated behind App ID 480, achievements designed but not shipped,
   account paperwork started early because of the 30-day clock).
2. **Packaging is an unbuilt prerequisite**: the project has never produced
   a packaged build (MISSION's own packaging concerns, `unreal-packaging`
   skill exists). Steam ultimately distributes packaged shipping builds —
   a packaging PRD (or REQ) precedes or accompanies any real Steam upload.
3. **Money**: $129.99 (plugin, operator's Fab account) + $100 (Steam Direct).

## Sources

- https://steamcore-pro.eeldev.com/ (+ /docfiles/getting_started/installing-plugin,
  /docfiles/getting_started/package-project, /docfiles/getting_started/configuring-plugin)
- https://www.fab.com/listings/d101542d-2534-4b2c-be16-e9b3e5cd4d04
- https://eeldev.com/index.php/steamcore-pro-1-1-5/ (UE 5.8 support),
  https://eeldev.com/index.php/steamcore-pro-1-1-7/ (latest)
- https://partner.steamgames.com/doc/gettingstarted/appfee (Steam Direct fee)
- Steam publishing walkthroughs (2026): pixune.com, meshy.ai, summerengine.com,
  thegamemarketer.com — consistent on $100 fee, 30-day wait, 2-week Coming
  Soon minimum, 4–6 week total timeline.
