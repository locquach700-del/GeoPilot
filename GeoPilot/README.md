# GIGA DASH BOT v1.2 — Geometry Dash 2.2081

Target: Windows x64 · Geometry Dash 2.2081 · Geode 5.11.2.

## Fixes in v1.2
- **No jump from the floor:** v1.1 included every `Solid` object, including ground. v1.2 ignores solids whose top only touches the player's feet and accepts raised blocks/walls only.
- **Object filtering:** only Hazard, AnimatedHazard and raised Solid hitboxes are considered.
- **Longer prediction:** default lead is 230 ms (previously 95 ms), adjustable from 100 to 450 ms.
- **Landing detection:** checks both `m_isOnGround` and `m_isOnGround2`.
- **Anti-spam:** remembers the last jumped spike cluster to prevent repeated/random taps.
- **Directional controls:** Ship/Wave/Swing steer away from the obstacle's side based on the predicted vertical lane.
- **Platformer controls:** Right is released when the bot is turned off or the level ends.

## Build / install
Open GitHub Actions → latest **GIGA DASH BOT — Windows Build** → download artifact `GIGA-DASH-BOT-Windows-GD-2.2081`. Extract and install the `.geode` file with Geode.

## Tuning
Start with Jump lead = 230 ms in Geode settings. If it jumps too early, lower toward 170–200 ms. If it hits the obstacle before jumping, raise toward 260–300 ms. Test in Practice mode and adjust in 10–20 ms steps.

## Limitations
This remains an experimental heuristic, not a trained AI or perfect pathfinder. It is not guaranteed to beat every level. UFO/Robot/Ball/Spider and Platformer have mode-specific physics; Ship/Wave/Swing use simplified steering. Complex triggers, dual mode, orb timing, slopes, moving hazards and custom objects can still cause failures.
