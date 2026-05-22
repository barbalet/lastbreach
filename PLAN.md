# LastBreach Playable Graphics Development Plan

## Total Cycles

Total planned development cycles: 10.

The conclusion of these cycles is a playable iOS shelter simulation where the player can see character avatars, inspect the shelter, assign actions, and watch those actions resolve against visible environment objects. The C simulation remains the source of gameplay truth, the DSL remains the source of editable scenario data, and the iOS app becomes the playable visual client.

## Current Project Shape

- `lastbreach-mac` contains the C99 simulation runner and DSL parser. It currently reads `.lbp`, `.lbw`, and `.lbc` files, runs a tick-based shelter simulation, and prints task/event output.
- `lastbreach-ios` contains the current iOS/SceneKit visual prototype and is the target iOS game app.
- `dsl` contains the game DSL files:
  - `dsl/world.lbw`: shelter state, inventory, weather, and events.
  - `dsl/catalog.lbc`: item and task definitions.
  - `dsl/joel.lbp` and `dsl/mara.lbp`: survivor behavior plans.
  - `dsl/dls_definition.md`: DSL specification.
- `data` contains back history, item lists, task lists, rules, and setting material that should guide the playable version.

## Current Starting Point

The simulation already supports the major verbs needed for the first playable version:

- Scheduling character tasks by priority and day block.
- Shelter state: temperature, power, water, structure, contamination, signature.
- Character state: hunger, hydration, fatigue, morale, injury, illness.
- Hydroponics state, watering, maintenance, fertilizer use, plant growth, and harvest.
- Produce outputs currently include `Tomato`, `Green bean`, `Chili`, and `Garlic`.
- Food preparation, cooking, preservation, water filtration, fishing, heating, power management, breach defense, and gunsmithing.
- Station conflicts and end-of-run diagnostics.

The iOS prototype currently has:

- A SceneKit voxel environment.
- Two simple character avatars.
- Toggle controls for grid/environment display.
- No playable task assignment, simulation bridge, action animation, item inspection, or persistent game loop yet.

## Playable Target

The playable target is a shelter-management game, not a tech demo. The first screen should be the actual shelter scene with active controls. The player should be able to:

- See Joel and Mara as character avatars in the shelter.
- Inspect shelter stations and important objects.
- Assign or confirm actions for the current day.
- Run time forward by ticks or by day.
- Watch avatars move to action objects and perform task animations.
- See visible item/state changes after actions complete.
- Save and load the world through the DSL-backed state pipeline.

Required visible action examples:

- Guns are gunsmithed at a workshop bench using a gun cleaning kit or gunsmith tools.
- Plants are watered with a watering can.
- Plants are fertilized during hydroponics maintenance.
- Produce is harvested and becomes visible inventory or food, including tomatoes, carrots, chilis, and basil.
- Cooking and eating consume visible food resources.
- Water filtration and water collection change visible water stores.
- Breach defense uses visible weapon/defense positions and damages or repairs shelter elements.

## Architecture Direction

Keep gameplay truth in the simulation layer. The iOS app should display and control the simulation, not duplicate rules independently.

Recommended structure:

- `lastbreach-mac`: continue as the C simulation runner and test harness.
- `lastbreach-ios`: renamed iOS app, SwiftUI plus SceneKit visual client.
- Shared gameplay contract:
  - A deterministic game state snapshot format.
  - A task/event stream format.
  - A visual metadata layer connecting DSL task/item names to iOS visuals.
- DSL remains editable content:
  - Character plans in `.lbp`.
  - World state in `.lbw`.
  - Task/item catalog in `.lbc`.
  - Visual metadata can be added to catalog definitions or stored in a companion file if that keeps the parser simpler.

## Cycle 1: Project Identity and Baseline

Status: complete.

Goal: make the workspace names and build targets line up with the intended product.

Work:

- Rename the iOS visual prototype to `lastbreach-ios`.
- Rename the Xcode project, target, app struct, display name, bundle identifier, bridging header path, and source folder references as needed.
- Preserve current SceneKit behavior after the rename.
- Document how to build and run both apps from a fresh checkout.
- Keep `lastbreach-mac` building with `make` and `make test`.

Done when:

- `lastbreach-ios` exists as the iOS app folder.
- No user-facing app name still uses the old prototype name.
- The iOS app launches and shows the existing voxel scene with the two avatars.
- The Mac runner and tests still build.

## Cycle 2: Shared State and Event Contract

Status: complete.

Goal: give the iOS app reliable data from the simulation.

Work:

- Add a machine-readable simulation output mode to `lastbreach-mac`, preferably JSON lines.
- Emit initial world state, per-tick state snapshots, task start events, task completion events, inventory changes, breach events, and harvest events.
- Include stable identifiers for characters, tasks, stations, items, quantities, conditions, and world stats.
- Add deterministic snapshot tests using fixed seeds.
- Decide whether the iOS app embeds the C simulation directly or shells out only during development. The production direction should be direct C integration through a thin Swift bridge.

Done when:

- A fixed command such as `lastbreach joel.lbp mara.lbp --world world.lbw --catalog catalog.lbc --days 3 --seed 123 --json` produces stable machine-readable output.
- Tests prove that key events can be parsed: watering, hydroponics maintenance, harvest, gunsmithing, breach defense, eating, and sleeping.
- The event contract is documented in this file or a linked schema file.

## JSONL Event Contract

The Mac runner supports a client stream with `--json`. Each line is one complete JSON object. Human-readable preamble lines are suppressed in this mode, so consumers can parse stdout line by line.

Command:

```sh
./lastbreach ../../dsl/joel.lbp ../../dsl/mara.lbp --world ../../dsl/world.lbw --catalog ../../dsl/catalog.lbc --days 3 --seed 123 --json
```

Common fields:

- `type`: event type string.
- `day`: zero-based day index when applicable.
- `tick`: zero-based tick within the day when applicable.
- Stable identifiers use lower snake case in `*_id` fields, with display names preserved in adjacent fields.

State payloads:

- `world`: shelter stats plus hydroponics and cooked-food state.
- `characters`: character id/name, needs, injury/illness, defense posture, active task, station, remaining ticks, and priority.
- `inventory`: item id/name, quantity, and best known condition.

Event types:

- `run_start`: schema version, day count, seed, and participating characters.
- `initial_state`: state before the first simulated tick.
- `day_start`: day-level world summary and breach chance.
- `task_started`: character, task, station, duration ticks, and priority.
- `task_completed`: character, task, station, and priority at completion.
- `inventory_changed`: item quantity/condition before and after a task or overnight world update.
- `breach`: breach tick and level.
- `breach_impact`: whether the breach was defended and structure before/after.
- `overnight_threat_check`: overnight roll, chance, and contact result.
- `harvest`: hydroponics harvest source plus produced item quantities.
- `tick_snapshot`: complete state after each tick resolves.
- `final_state`: complete state after the requested run.
- `simulation_complete`: final sentinel for stream consumers.

## Cycle 3: Visual Metadata for DSL Content

Status: complete.

Goal: connect simulation nouns to visible game objects.

Work:

- Extend `catalog.lbc` or add a companion metadata file that maps items/tasks to visual ids.
- Add visual metadata for stations:
  - `workshop`: gunsmith bench, vise, tool tray, rifle rack.
  - `hydroponics`: planter, plant slots, water reservoir, fertilizer bag, grow light.
  - `kitchen`: prep surface, camp stove, storage containers.
  - `wash`: water filter, bucket, water barrel.
  - `cot`: sleep/rest area.
  - `power`: battery, solar controller, multimeter, wiring panel.
  - `defense`: door, window, firing position, barricade.
  - `outside`: scouting/fishing edge.
- Add missing produce items for the requested playable fantasy:
  - `Carrot`
  - `Basil`
- Keep existing produce unless deliberately cut:
  - `Tomato`
  - `Green bean`
  - `Chili`
  - `Garlic`
- Add task metadata for action pose, station, hand prop, visible output, and sound/effect hint.

Done when:

- Every first-playable task has a station and visual mapping.
- Tomatoes, carrots, chilis, and basil exist as harvestable/cookable items.
- The iOS app can load or compile a visual catalog without hardcoding every task name in view code.

Implemented catalog:

- `dsl/visual_catalog.json` is the companion visual metadata file used by the iOS app.
- `stations` maps station ids to display names, scene roles, anchors, palette hints, and prop ids.
- `items` maps inventory/content ids to display names, visual ids, categories, scale hints, and station affinities.
- `tasks` maps every first-playable task name from `data/tasks.txt` to a station id, action pose, hand prop, target prop, output ids, and sound/effect hints.
- The iOS project bundles this JSON resource and decodes it through `VisualCatalog.swift`.
- Produce now includes harvestable and cookable `Tomato`, `Carrot`, `Green bean`, `Chili`, `Garlic`, and `Basil`.

## Cycle 4: iOS Scene Entity System

Status: complete.

Goal: replace the one-off visual scene with a small game scene model.

Work:

- Introduce Swift model types for visual entities: character, station, item, prop, task marker, and floating outcome label.
- Build a deterministic shelter layout from world/catalog metadata.
- Keep the existing voxel environment as the shelter shell, but add authored station anchors inside it.
- Add object selection and highlighting.
- Add labels or compact info panels only where they help play.
- Add a scene reset/rebuild path when a new world snapshot loads.

Done when:

- Joel and Mara are named selectable characters.
- At least six stations are visible and selectable: workshop, hydroponics, kitchen, wash, cot, defense.
- Important inventory objects appear at their stations.
- The scene can rebuild from a loaded state without duplicating nodes.

Implemented scene layer:

- `VisualSceneModel.swift` defines scene entity kinds for characters, stations, items, props, task markers, and outcome labels.
- `VisualSceneState.firstPlayable` provides the initial playable shelter state with Joel, Mara, key tools, water, weapons, fertilizer, food, and produce markers.
- `VisualSceneLayout` builds deterministic entity positions from `VisualCatalog` station anchors and item station affinities.
- `VoxelSceneFactory` now owns a named entity container and `rebuildEntities(...)`, which removes and replaces the entity layer for future loaded snapshots.
- The iOS scene renders all catalog stations, station props, first-playable inventory markers, featured task markers, and floating visible-output labels inside the existing voxel shell.
- `LastBreachSceneView` wraps `SCNView` with tap hit testing; selected entities get a SceneKit highlight and a compact SwiftUI inspector.

## Cycle 5: Character Control and Day Planning UI

Status: complete.

Goal: let the player make meaningful assignments before time runs.

Work:

- Add a planning panel for the current day.
- Show character cards with state bars: hunger, hydration, fatigue, morale, injury, illness.
- Show valid tasks based on DSL/catalog requirements and current inventory.
- Allow task assignment or priority override for each character.
- Show warnings for missing tools, low stock, or dangerous character state.
- Keep the UI compact and practical; the shelter scene remains the main screen.

Done when:

- The player can assign Joel to gunsmithing and Mara to plant care from the iOS UI.
- Invalid assignments explain the missing requirement.
- Starting the day produces a clear schedule or task queue.
- The UI can handle both automatic DSL-driven plans and player overrides.

Implemented planning layer:

- `DayPlanningModel.swift` defines character needs, automatic/player-overridden assignments, task validation, and scheduled queue rows.
- The current-day planning panel is layered over the SceneKit shelter without replacing the main scene view.
- Joel and Mara have compact character cards with hunger, hydration, fatigue, morale, injury, and illness meters.
- Task menus are built from `VisualCatalog.tasks`, so the UI follows the DSL-backed catalog instead of a hardcoded task list.
- Validation checks task props against current inventory and station-provided props, then reports missing requirements or low-stock warnings.
- Priority controls support player overrides, while auto-plan reset restores the DSL-style automatic defaults.
- Starting the day builds a visible task queue sorted by priority and rebuilds the scene task markers from the queued assignments.

## Cycle 6: Core Action Animation Pass

Status: complete.

Goal: make the requested actions visibly happen in the world.

Work:

- Add movement from character position to station anchor.
- Add simple loop animations for:
  - Gunsmithing: character at workbench, gun/tool prop visible, condition/readiness feedback.
  - Watering plants: watering can prop, water arc or particle cue, plant health feedback.
  - Fertilizing plants: fertilizer bag or scoop prop, soil/plant feedback.
  - Harvesting: produce appears on plant, then transfers to inventory or crate.
  - Cooking/eating: food moves through kitchen/prep/eat states.
  - Water filtration: raw water container to filter to safe water store.
  - Defensive shooting/combat: character at defense anchor, weapon pose, shelter damage feedback.
- Add task completion effects that are readable but not noisy.

Done when:

- A player can identify what action is being performed without reading the event log.
- Gunsmithing, watering, fertilizing, and harvesting all have distinct animations.
- Tomatoes, carrots, chilis, and basil have distinct visible produce forms.
- Station and prop visibility matches the simulation state.

Implemented animation layer:

- `SceneActionAnimator.swift` plays the queued day schedule against the existing SceneKit entity layer.
- Starting the day moves each valid assigned character from their current scene position to the assigned station anchor.
- The scene now has a resettable action-effects container, so repeated starts or scene rebuilds do not stack old effects.
- Gunsmithing shows a bench, rifle, tool motion, and spark/readiness feedback.
- Watering plants shows a watering can, looping water droplets, and plant feedback.
- Hydroponics maintenance shows fertilizer grains, plant growth, visible produce, and a harvest transfer cue.
- Tomatoes, carrots, chilis, and basil have distinct produce forms/colors in the harvest effect.
- Cooking/eating-style tasks show stove/bowl/steam cues; water filtration shows raw water, filter, clean water, and flow droplets.
- Defensive shooting/combat show barricade motion, weapon pose, and muzzle-flash or impact feedback.
- Blocked scheduled tasks produce a visible warning label at the relevant station.

## Cycle 7: Simulation Bridge in the iOS App

Goal: run the real game loop from the iOS app.

Work:

- Integrate the C simulation into the iOS target through a stable bridge.
- Load DSL files into the app bundle for the default scenario.
- Support a development mode that reloads local DSL files quickly.
- Convert simulation events into scene actions and UI state updates.
- Add pause, play, next tick, and run day controls.
- Preserve deterministic seed behavior for debugging.

Done when:

- The iOS app can run at least three in-game days using `world.lbw`, `catalog.lbc`, `joel.lbp`, and `mara.lbp`.
- The visible event sequence matches the simulation event stream.
- Pausing and stepping ticks does not desynchronize avatars, inventory, or character state.

Implemented simulation bridge:

- The iOS target now bundles `world.lbw`, `catalog.lbc`, `joel.lbp`, and `mara.lbp` alongside the visual catalog.
- `LastBreachSimulationBridge` runs the shared C simulator in JSONL mode with a deterministic seed and returns the trace to Swift.
- The iOS app loads bundled scenario files by default and reloads local `dsl/` files in Debug builds for fast scenario iteration.
- `SimulationBridge.swift` decodes three-day simulation traces into tick snapshots, timeline events, inventory stacks, active character tasks, and world state.
- The main panel now has play, pause, next-tick, run-day, and reload controls for the simulation trace.
- Character needs, active tasks, inventory props, visible stations, and event log rows are rebuilt from the current simulation tick.
- Scene action animation is triggered from simulator `task_started` events, keeping visual action playback tied to the C event stream.

## Cycle 8: Gameplay Balance and Failure Readability

Goal: make the playable loop understandable and strategically meaningful.

Work:

- Tune initial world inventory so the first three days exercise plant care, water, food, gunsmithing, rest, and at least one threat path.
- Add player-readable alerts for weak links: low water, hungry survivor, tired survivor, low structure, low ammunition, sick plants.
- Add visible consequences:
  - Plants wilt when neglected.
  - Shelter damage appears after breach impact.
  - Guns/tools show worn state.
  - Food/water stores visibly shrink or grow.
- Make failures instructive: failed task, missing tool, poor condition, no water, no fertilizer, or no ammo should be visible.

Done when:

- A new player can understand why a task succeeded or failed.
- Ignoring plants causes visible decline and lower harvests.
- Maintaining plants produces visible food progress.
- Ignoring gunsmithing/repair creates visible defense risk.

Implemented in Cycle 8:

- Rebalanced the default world to start with tight water, food, ammunition, structure, fertilizer, and worn gun/water tools so the first three simulated days surface plant care, filtration, food pressure, rest pressure, repair/gunsmith risk, and guaranteed breaches.
- Added simulation `task_failed` and `task_warning` JSON notes for missing food, water, fertilizer, fish, ammunition, weapon/tool requirements, worn tools, and missing raw water.
- Added iOS weak-link alerts for water, food, hungry/tired survivors, structure, ammunition, plants, and worn gear.
- Propagated world status and item condition into the SceneKit layout so stations/items show low, empty, worn, dry, wilted, and damaged states.
- Updated the iOS event log to render warnings/failures with distinct icons and colors.

## Cycle 9: Persistence, Editing, and Save Flow

Goal: preserve progress and keep the DSL pipeline useful.

Work:

- Add save/load for current world state.
- Decide exact write-back behavior:
  - Save snapshots as generated `.lbw` files, or
  - Maintain a separate save file that references original DSL content.
- Preserve original authored DSL files unless the player explicitly exports a modified scenario.
- Add export/import for debug saves.
- Add minimal migration handling for future DSL/schema changes.

Done when:

- The player can quit and resume the same shelter state.
- A saved game contains inventory, shelter stats, character stats, plant state, current day, and random seed/progression state.
- Developer exports can be replayed in `lastbreach-mac` for debugging.

## Cycle 10: Release Candidate and Concluding Conditions

Goal: finish the first playable graphical version.

Work:

- Complete visual polish for the core loop.
- Add sound and haptic cues where useful.
- Add performance checks on simulator and target devices.
- Add automated tests for the simulation bridge and deterministic runs.
- Add manual QA scripts for the main playable path.
- Update `README.md` with build, run, and gameplay instructions.

Done when all concluding conditions below are met.

## Concluding Conditions

Development is complete for this plan when:

- The iOS app folder and product are named `lastbreach-ios`.
- The app launches directly into a playable shelter scene.
- Joel and Mara appear as controllable or assignable avatars.
- The player can assign actions and run time forward.
- The Mac simulation remains the gameplay authority and can replay the same scenario deterministically.
- The iOS scene reflects simulation state for:
  - Characters.
  - Stations.
  - Inventory objects.
  - Plant growth and harvest.
  - Weapon/gunsmithing state.
  - Water, food, power, structure, and contamination.
- Gunsmithing is visible at a workshop with gun/tool props.
- Plant watering is visible with a watering can or equivalent water prop.
- Plant fertilizing is visible with fertilizer feedback.
- Harvesting produces visible tomatoes, carrots, chilis, and basil.
- At least one complete three-day playthrough is possible from the bundled DSL scenario.
- The player can understand and recover from common shortages: low water, low food, low rest, low structure, low ammo, and neglected plants.
- Saves can be loaded without losing shelter, character, inventory, plant, or day state.
- `lastbreach-mac` builds and its tests pass.
- `lastbreach-ios` builds and runs without a blank scene.
- There are no critical UI overlaps on supported iPhone and iPad sizes.
- The README explains how to build, run, test, and play the first playable version.

## First Playable Scenario

The first playable scenario should be a short three-day loop:

- Day 1 teaches the shelter: water filtration, cooking/eating, plant watering, rest.
- Day 2 introduces maintenance pressure: fertilizer, gunsmithing, power management, structure repair.
- Day 3 tests readiness: harvest food, handle low stock, respond to a breach or overnight threat.

Required success path:

- Mara keeps hydroponics alive by watering and maintaining plants.
- Joel maintains weapons and structure.
- The player keeps both characters fed, hydrated, and rested enough to avoid collapse.
- The shelter survives at least one threat check.
- The player harvests at least one visible produce item.

Required failure path:

- If the player ignores water, food, rest, or structure, the game visibly shows why the shelter is becoming unstable.
- If the player ignores plants, harvest output falls or plants visibly decline.
- If the player ignores gunsmithing and ammo readiness, defensive outcomes become worse or riskier.

## Development Notes

- Prefer small, deterministic test scenarios over broad random playtests until the event bridge is stable.
- Keep visual names and simulation names connected through metadata, not scattered string comparisons.
- Keep the first playable focused on the shelter, avatars, stations, and visible task resolution.
- Do not turn the iOS app into a landing page or static viewer; the shelter scene is the game.
- Preserve the post-apocalypse mundanity tone: routine, maintenance, pressure, and small visible victories.
