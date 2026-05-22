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

Emit machine-readable JSON lines for tools or the future iOS bridge:

```sh
./lastbreach ../../dsl/joel.lbp ../../dsl/mara.lbp --world ../../dsl/world.lbw --catalog ../../dsl/catalog.lbc --days 3 --seed 123 --json
```

### iOS visual app

`lastbreach-ios` contains the SwiftUI/SceneKit iOS app.

The app bundles `dsl/visual_catalog.json` and decodes it at launch so SceneKit/UI code can map simulation task and item names to visual stations, props, poses, and output effects.

Open `lastbreach-ios/lastbreach-ios.xcodeproj` in Xcode, select the `lastbreach-ios` scheme, choose an iPhone or iPad simulator, and run the app.

Command-line build check:

```sh
xcodebuild -project lastbreach-ios/lastbreach-ios.xcodeproj -scheme lastbreach-ios -destination 'generic/platform=iOS Simulator' build
```

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
