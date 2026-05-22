# lastbreach


Post Apocalypse Mundanity Game: A post-apocalypse shelter simulation focused on everyday survival work.

## Overview

`lastbreach` is a management/simulation game where the dramatic events happened before the game starts. The player lives with the long tail of collapse: staying warm, staying fed, maintaining tools, and keeping a small settlement functional.

The core fantasy is simple: build a routine that survives pressure.

## Game Premise

The player is responsible for a small shelter community on the edge of a hostile world. Most play happens inside and around the shelter. Outside expeditions are short, risky, and purpose-driven.

The game is not about constant combat. It is about logistics, maintenance, morale, and making tradeoffs under scarcity.

## Design Pillars

- Mundane survival tasks should be meaningful and mechanically deep.
- Player planning should matter more than reflexes.
- Shortages should create decisions, not random punishment.
- The shelter should feel increasingly personal over time.

## Core Gameplay Loop

1. Plan the day: assign tasks based on weather, resource levels, and risk.
2. Perform and resolve tasks: cooking, repairs, scouting, crafting, defense drills, and care work.
3. Consume and degrade: food, fuel, water, and tool durability are updated.
4. Recover and prepare: sleep, treatment, cleanup, and tomorrow planning.

## Major Systems

- Task system: Characters perform jobs with time, skill, tool, and material requirements.
- Item system: Crafting components, equipment, furniture, and consumables drive what tasks are possible.
- Shelter system: Heat, power, water, and structure condition affect safety and efficiency.
- Character state: Hunger, fatigue, morale, and injuries shape output and risk.
- External pressure: Weather, resource scarcity, and occasional threats force reprioritization.

## Content Data

The current prototype content lives in:

- `data/tasks.txt`: available activities to schedule/resolve.
- `data/items.txt`: world objects, tools, resources, and equipment.
- `dsl/world.lbw`, `dsl/catalog.lbc`, `dsl/joel.lbp`, and `dsl/mara.lbp`: the default playable three-day scenario.
- `dsl/visual_catalog.json`: visual station, item, and task metadata used by the iOS client.

These files are intended to seed balancing and simulation rules.

## Early Scope

- Simulate one shelter with a small cast of survivors.
- Resolve task outcomes in discrete time steps.
- Support item dependencies for specialized tasks (for example: fishing needs bait/hooks/fish handling gear; electronics work needs tools like multimeter and soldering iron).
- Add progression via improved routines, tool quality, and shelter upgrades.

## Build and Run

### Mac simulation runner

`lastbreach-mac` contains the standalone C99 simulation runner and test harness.

```sh
cd lastbreach-mac/lastbreach-mac
make
make test
```

Run the bundled DSL scenario from the runner directory:

```sh
./lastbreach ../../dsl/joel.lbp ../../dsl/mara.lbp --world ../../dsl/world.lbw --catalog ../../dsl/catalog.lbc --days 3 --seed 123
```

Emit machine-readable JSON lines for tools and the iOS bridge:

```sh
./lastbreach ../../dsl/joel.lbp ../../dsl/mara.lbp --world ../../dsl/world.lbw --catalog ../../dsl/catalog.lbc --days 3 --seed 123 --json
```

### iOS visual app

`lastbreach-ios` contains the SwiftUI/SceneKit iOS app. It launches directly into the shelter scene.

The app bundles the default DSL scenario and `dsl/visual_catalog.json`. The C simulation is embedded through a small bridge and remains the gameplay authority; SwiftUI/SceneKit displays the JSON event stream and turns task events into visible station actions.

The current scene builds a catalog-backed shelter layout with selectable character avatars, stations, props, inventory markers, featured task markers, weak-link alerts, and a compact inspector panel.

The play panel can run, pause, step one tick, run the current day, reload the deterministic simulation, save/load progress, and export/import debug saves.

Simulation playback animates the core loop in the shelter: avatars move to stations and task-specific effects show gunsmithing, plant watering/fertilizing/harvesting, cooking/eating, water filtering, and defense work. Save/load and key simulation events use light audio and haptic cues.

Open `lastbreach-ios/lastbreach-ios.xcodeproj` in Xcode, select the `lastbreach-ios` scheme, choose an iPhone or iPad simulator, and run the app.

Command-line build check:

```sh
xcodebuild -project lastbreach-ios/lastbreach-ios.xcodeproj -scheme lastbreach-ios -destination 'generic/platform=iOS Simulator' build
```

### Playing the first scenario

The bundled scenario is a short three-day loop:

1. Day 1 teaches shelter upkeep: water filtration, meal prep, plant watering, and sleep.
2. Day 2 adds pressure: worn tools, low raw water, structure maintenance, fatigue, and breach risk.
3. Day 3 tests readiness: plant harvests, low-stock recovery, and another breach path.

During play, watch the weak-link alerts and event log. Low water, food, structure, ammo, tired survivors, sick plants, and worn gear all surface as recoverable problems. The scene should visibly reflect changed inventory, plant health, harvest output, water stores, shelter damage, and active tasks.

Saves are stored as versioned JSON in app support. Debug export writes both the JSON save and a generated `.lbw` snapshot into `LastBreachExports`; those `.lbw` files can be replayed with `lastbreach-mac` for investigation without modifying the authored DSL files.

### Release-candidate QA

Run the automated release check from the repository root:

```sh
scripts/release_candidate_check.sh
```

It builds the Mac runner, runs unit tests, verifies the default three-day JSONL scenario is deterministic and complete, checks key bridge-visible events, and builds the iOS simulator target.

Use the manual checklist in `qa/manual_playthrough.md` for device layout, save/load, haptics/audio, visible action readability, and performance notes.

### Output you’ll see

Per day header (shelter state + breach chance)

Per tick:

EVENT: BREACH level=N! (if it happens)

EVENT: overnight_threat_check (end of day)

Task starts / continues / completes

Station conflicts (if both try the same station)

Updated stats lines per character

## Roadmap

1. Define data schema for tasks/items with explicit dependencies.
2. Implement simulation tick and scheduler.
3. Add character stats and skill checks.
4. Add event layer (weather, failures, external incidents).
5. Build playable UI loop for daily planning and resolution.

## License

MIT (see `LICENSE`).

## Background

There is a class of games that currently only exist in AI generated mobile ads.

[![Watch the video](https://img.youtube.com/vi/g9OBQQ4AIxw/mqdefault.jpg)](https://www.youtube.com/watch?v=g9OBQQ4AIxw)

Given the simulation aspects of these games or at least what they appear to be it interested me to see how easy it would be to write a compelling game version of this. As the image generation engine is commercially available:

For this exact look, the best system is a diffusion workflow in ComfyUI using:

```FLUX.1-dev (or SDXL) as the base model
IP-Adapter with your MP4 frames as style references
ControlNet (depth/canny) to keep composition consistent
img2img + batch generation at 360x640
Why this is easiest for your case:

It can lock the same “cozy shelter + hostile outside” aesthetic across many outputs.
You can reuse your existing frames directly as style anchors.
It’s scriptable and repeatable (good for generating lots of variants).
If you want fastest no-setup (less control), use Midjourney or Leonardo AI.
If you want reliable style consistency and volume, ComfyUI pipeline is the right system.
