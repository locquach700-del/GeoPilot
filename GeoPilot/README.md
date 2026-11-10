# GIGA DASH BOT — Geometry Dash 2.2081

**Native Geode mod for Windows, targeting Geometry Dash 2.2081 and Geode 5.11.2.**

## Download the built mod

1. Open the repository **Actions** tab.
2. Select **GIGA DASH BOT — Windows Build**.
3. Open the latest run with a green **Success** status.
4. Download the artifact **GIGA-DASH-BOT-Windows-GD-2.2081** at the bottom of the run page.
5. Extract the ZIP and install the `.geode` file using Geode (or copy it to `%LOCALAPPDATA%\GeometryDash\geode\mods\`).

Every push to the `GeoPilot/` source folder automatically starts a new Windows build.

## In-game settings

Open the Geode menu, select **GIGA DASH BOT**, then open its settings:

- **Enable Auto Play** — off by default.
- **Game Mode** — Auto or a manual player-mode override.
- **Reaction Distance** — how early the heuristic reacts to recognized obstacles.
- **Debug Log** — writes controller decisions to the Geode log.

Practice mode is recommended when testing.

## What it does

The mod scans the current PlayLayer's object list, checks a limited list of common spike/saw hazard IDs and reacts based on the nearest recognized object's position. It has direct access to game object data through Geode; it does not guess hazards from screen pixels.

## Limitations

This is an **experimental heuristic**, not a trained AI or a guaranteed level solver. Custom objects, moving triggers, hitboxes, orb timing, dual mode, complex speed/gravity changes and many Ship/Wave corridors need a more complete physics simulator and pathfinder. The current logic may fail on many levels and modes. It is not advertised as capable of beating every level.

The mod is intended for personal Practice/offline testing. It does not attack Geometry Dash servers, edit other players' levels, or modify account data.
