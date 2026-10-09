# GeoPilot Auto Play (Experimental)

Target build: **Geometry Dash 2.2081 + Geode v5.10.1**.

GeoPilot is an early native Geode mod prototype. It adds configurable controls to the mod's Geode settings menu and uses object-ID + position heuristics to react to a limited set of common hazards. It is **not yet a reliable solver for every level**: custom objects, orbs, pads, portals, gravity/speed triggers, dual mode, moving objects, Swing, and complicated Ship/Wave corridors need a more complete physics simulator/pathfinder.

## Settings

Open Geode menu → GeoPilot Auto Play → Settings:

- `Enable Auto Play`: off by default.
- `Game Mode`: `Auto` or a manual mode override.
- `Reaction Distance`: how early the heuristic reacts.
- `Debug Log`: prints occasional decisions to the Geode log.

The current implementation uses a simple tap strategy for Cube/Ball/UFO/Robot/Spider/Swing and a simple hold/release lane heuristic for Ship/Wave. Treat it as an experimental prototype, not a validated completion bot.

## Build a multi-platform .geode

The included GitHub Actions workflow builds Windows x64, macOS, iOS, Android 32-bit and Android 64-bit with Geode v5.10.1, then combines the outputs into a `.geode` artifact.

1. Create an empty GitHub repository.
2. Upload the contents of this project (including `.github/workflows/multi-platform.yml`).
3. Open the repository's **Actions** tab and run `GeoPilot Multi-Platform Build`, or push a commit.
4. When the run finishes, open its run page and download the artifact named `GeoPilot-Geode-v5.10.1-GD-2.2081`.
5. Extract the artifact ZIP. Install the `.geode` file by opening it with Geode or placing it in the platform's Geode mods directory.

The workflow follows Geode's multi-platform build pattern. A universal package can contain separate native builds for different platforms; it is not a single binary that runs unchanged on every OS.

## Local build

Install Geode CLI, then install the matching SDK and platform binaries:

```sh
geode sdk install
geode sdk install-binaries
geode build
```

For Android, install the Android NDK and use the corresponding target, e.g. `geode build -p android64` or `geode build -p android32`.

## Current limitations

- This project bundle contains source code, not a precompiled `.geode` binary.
- The build was not executed in this workspace because the Geode SDK, platform headers, and Android/iOS toolchains are not installed here.
- The auto-player is heuristic-based and has not been tested in-game; it will fail on many levels and mode transitions. Don't treat the selected mode as proof the controller can beat every level.
