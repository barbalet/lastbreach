# LastBreach Release Candidate Manual QA

Use this checklist after `scripts/release_candidate_check.sh` passes.

## Devices

- iPhone SE or equivalent compact simulator.
- iPhone 15/16 size simulator.
- iPad simulator.
- One physical iPhone or iPad when available.

## Launch And Layout

- Launch `lastbreach-ios`; it opens directly to the shelter scene.
- Verify Joel and Mara are visible and selectable.
- Toggle shell/grid controls; the scene remains nonblank.
- Rotate supported device orientations; the planning panel, event log, and inspector do not overlap critically.
- On compact width, the planning panel remains scrollable and readable.

## Three-Day Play Path

- Tap play, pause, next tick, and run day.
- Confirm haptic/audio cues occur for play controls, saves, imports/exports, warnings, breaches, and harvests.
- Run through at least three days with seed 1337.
- Verify the event log shows water filtration, plant watering, meal prep/eating, sleeping, breach contact, and harvest.
- Confirm avatars move to stations and effects are readable:
  - Multi-use setup markers and colored conversion rings appear at active shelter stations.
  - Watering can and droplets at hydroponics.
  - Fertilizer/harvest produce at hydroponics.
  - Kitchen stove/bowl/steam for food tasks.
  - Filter and water flow at wash.
  - Dock-bay water collection, fishing, and water-motion cues remain inside the base.
  - Comms scanning/periscope cues for scouting/telescope-style tasks.
  - Weapon/impact feedback in the gate room during defense.
  - Workshop/rifle/tools when gunsmithing is active.

## Readability And Recovery

- Confirm weak-link alerts appear for low water, food, structure, ammunition, tired/hungry survivors, plants, and worn gear when applicable.
- Let water/raw water run low and verify failed filtration explains why.
- Inspect hydroponics after missed or repeated care; plant state and produce counts remain understandable.
- Inspect workshop/defense when weapon or ammo state is poor; worn/low signals are visible.

## Save Flow

- Advance to a non-initial tick and tap save.
- Quit and relaunch; the app restores the same day/tick, shelter stats, inventory, characters, and plant state.
- Tap export; verify a `.json` save and generated `.lbw` appear in `LastBreachExports`.
- Tap import; the latest debug save restores cleanly.
- Replay the generated `.lbw` in the Mac runner with the command shown inside the exported file comments.

## Performance Notes

- During three-day playback, the scene should stay responsive and avoid blank frames.
- Scene rebuilds from next tick/run day should feel immediate on simulator.
- On physical device, watch for sustained heat, repeated hitching, or audio/haptic spam during normal play.
