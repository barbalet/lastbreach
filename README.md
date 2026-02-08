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

These files are intended to seed balancing and simulation rules.

## Early Scope

- Simulate one shelter with a small cast of survivors.
- Resolve task outcomes in discrete time steps.
- Support item dependencies for specialized tasks (for example: fishing needs bait/hooks/fish handling gear; electronics work needs tools like multimeter and soldering iron).
- Add progression via improved routines, tool quality, and shelter upgrades.

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
