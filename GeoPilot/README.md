# GIGA DASH BOT v1.1 — Geometry Dash 2.2081

Target: **Windows x64 · Geometry Dash 2.2081 · Geode 5.11.2**.

## Download

Open [GitHub Actions](https://github.com/locquach700-del/GeoPilot/actions), select the latest **GIGA DASH BOT — Windows Build** run, and download the artifact `GIGA-DASH-BOT-Windows-GD-2.2081` after a successful build.

## What changed in v1.1

- Replaced the old hard-coded ID list with native `GameObjectType::Hazard`, `AnimatedHazard` and `Solid` types.
- Uses `GameObject::getObjectRect()` and `PlayerObject::getObjectRect()` for collision-lane checks.
- Ignores objects clearly above the player's head or below the player's feet for ground-jump decisions.
- Calculates distance from the player's leading hitbox edge to the obstacle and estimates time-to-contact from current horizontal speed.
- Default jump lead reduced to 95 ms and can be adjusted in the Geode mod settings.
- Scans the level's object array on each game update (normally 60 updates/second), so the cheap path check refreshes more often than once every 0.1 seconds.
- Detects Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing and Platformer state from live player fields.
- Platformer mode holds Right automatically. Ship/Wave/Swing use a simple projected-lane steering heuristic.

## Why the old version jumped too early

The old code treated IDs 90–97 and 133–140 as hazards even though several are normal square/decorative objects, an exclamation decoration, a rod, wheels and jump objects. That made the bot react to things that were not dangerous. The new version asks the game's native object-type field whether an object is a hazard or solid, then checks collision rectangles before acting.

## Tuning

Open Geode → GIGA DASH BOT → Settings:

- **Enable Auto Play** — off by default.
- **Jump timing (ms)** — start at 95 ms. Reduce toward 70–80 ms if it still jumps too early; raise it if it hits the obstacle before jumping.
- **Debug Log** — prints mode, target object ID, hitbox gap, time-to-contact and decision.

Use Practice mode while testing.

## Important limitations

This is a lightweight predictive prototype, not a fully trained AI or a verified TAS solver. It is more selective than v1.0, but the Ship/Wave/Swing controls still use simplified steering, and Robot/UFO/Spider/Ball have mode-specific physics that need additional trajectory simulation. It will not reliably beat every custom level, trigger sequence, dual section or complex Platformer map. The 95 ms setting is a starting point, not a universal timing value.
