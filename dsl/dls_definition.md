# lastbreach DSL specification (v0.1)

This document defines the Domain Specific Language (DSL) used to run **lastbreach** as a deterministic, tick-based shelter-management simulation.
A game “run” loads two separate **character plan** files (one per survivor). The two plans are compiled into a joint day schedule and simulated.

- Required: `survivor_A.lbp`, `survivor_B.lbp`
- Optional: `world.lbw` (world/shelter config), `catalog.lbc` (item/task extensions)

---

## 1) What you write to run a game

### Required inputs
1. `survivor_A.lbp` — Character A plan script
2. `survivor_B.lbp` — Character B plan script

### Optional inputs
- `world.lbw` — World/shelter config (if omitted, engine uses defaults)
- `catalog.lbc` — Item/task catalog extensions (if omitted, engine uses built-in catalogs)

### Load order
1. Load base rules (engine rules from “Rules Versioned 0.102” behavior)
2. Load base catalog (tasks/items) from canonical lists
3. Load optional `catalog.lbc` (adds/overrides definitions)
4. Load optional `world.lbw`
5. Load the two plan scripts (`.lbp`) and compile into a **joint schedule**
6. Simulate day tick-by-tick; trigger events; apply outcomes; advance to next day

---

## 2) Language overview

### File types (same syntax, different “top blocks”)
- **Plan file (`.lbp`)**: must contain exactly one `character { ... }` block.
- **World file (`.lbw`)**: contains one `world { ... }` block.
- **Catalog file (`.lbc`)**: contains `itemdef { ... }` and `taskdef { ... }` blocks.

All three share the same lexical rules and expression language.

---

## 3) Lexical rules

### Comments
- `# comment to end of line`
- `// comment to end of line`
- `/* block comment */`

### Identifiers
- `snake_case` recommended for IDs: `water_filter`, `defensive_shooting`
- Strings for display names: `"Water filter"`, `"Defensive shooting"`

### Literals
- **Int**: `12`
- **Float**: `0.35`
- **Bool**: `true`, `false`
- **String**: `"text"`
- **Duration** (ticks): `4t` = 4 ticks
- **Percent**: `35%`
- **Ranges**: `0..6` (inclusive start, exclusive end)

### Time model
- Engine default: `DAY_TICKS = 24` (0..23)
- A “block” is just a named range; you can define any ranges you like.

---

## 4) Core data model

### Character state vector (built-in fields)
Numeric fields (0–100 unless otherwise stated):
- `hunger` (higher = more full)
- `hydration`
- `fatigue` (higher = more tired)
- `morale`
- `injury` (0 = none; higher = worse)
- `illness` (0 = none; higher = worse)

Derived modifiers (engine computes):
- `efficiency` (task output multiplier)
- `error_rate` (affects failures/accidents)
- `combat_effectiveness`

### Shelter state (built-in)
- `shelter.temp_c` (float)
- `shelter.signature` (float; “noise/light” attractor)
- `shelter.power` (float; available stored power)
- `shelter.water_safe` (float; safe/filtered water units)
- `shelter.water_raw` (float; raw water units)
- `shelter.structure` (0–100 condition)
- `shelter.contamination` (0–100; higher = worse)

### Items
Every item instance has:
- `kind` (string name, e.g. `"Rifle"`)
- `max_condition` (default 100)
- `condition` (0..max)
- `quantity` (stack count for consumables)

Suggested condition thresholds:
- 80–100%: optimal
- 50–79%: minor penalties
- 20–49%: major penalties
- 0–19%: broken/unusable (or hazardous)

---

## 5) Expression language

Expressions are used in `when`, `if`, `let`, and numeric fields.

### Operators
- Arithmetic: `+ - * /`
- Comparisons: `== != < <= > >=`
- Boolean: `and or not`
- Grouping: `( ... )`

### Built-in query functions
- `stock("Item Name") -> float`
- `has("Item Name") -> bool`
- `cond("Item Name") -> float`
- `char.<field>` (e.g. `char.fatigue`)
- `shelter.<field>` (e.g. `shelter.structure`)
- `weather.<field>` (if world defines weather; else neutral defaults)
- `event("name") -> bool` (true only during the tick an event fires)
- `breach.level -> int` (during breach)
- `tick -> int` (current day tick)
- `day -> int` (day count since start)

---

## 6) Task resolution rules (engine semantics)

Each task has:
- **time cost** (ticks)
- **location**: `inside | near | outside`
- **risk**: `none | low | med | high`
- **requirements**: tools, inputs, stations
- **outputs**: items, stat changes, wear to used tools

### Success model
For each task execution:
1. Compute `base_success` from task definition.
2. Modify by: skill, tool condition, environment pressure, fatigue/morale/injury/illness
3. Roll outcome:
   - Success: apply outputs, wear, skill gain
   - Failure: time spent; may waste inputs; may damage tools; may trigger injury/illness/structure damage depending on risk

### Breach model
Breach resolution uses two phases:
- **Hold Phase**: defensive tasks reduce breach pressure
- **Repair Phase**: restore structure and reduce future breach odds

---

## 7) DSL syntax specification (EBNF)

### Top level
```ebnf
file         := (catalog_block | world_block | character_block)* ;

catalog_block := itemdef_block | taskdef_block ;

world_block   := "world" string? "{" world_stmt* "}" ;
character_block := "character" string "{" char_stmt* "}" ;
```

### World statements (optional)
```ebnf
world_stmt :=
    version_stmt
  | constants_stmt
  | shelter_stmt
  | inventory_stmt
  | weather_stmt
  | events_stmt
  ;

version_stmt := "version" number ";" ;

constants_stmt := "constants" "{" (ident ":" expr ";")* "}" ;

shelter_stmt := "shelter" "{" (ident ":" expr ";")* "}" ;

inventory_stmt := "inventory" "{" inv_entry* "}" ;
inv_entry := string ":" ( "qty" expr ("," "cond" expr)? ) ";" ;

weather_stmt := "weather" "{" (ident ":" expr ";")* "}" ;

events_stmt := "events" "{" event_rule* "}" ;
event_rule := "daily" string "chance" percent ("when" expr)? ";"
            | "overnight_threat_check" "chance" percent ("when" expr)? ";" ;
```

### Catalog extensions (optional)
```ebnf
itemdef_block := "itemdef" string "{" itemdef_stmt* "}" ;
itemdef_stmt  := "max_condition" ":" expr ";"
               | "stackable" ":" bool ";"
               | "tags" ":" "[" string_list? "]" ";" ;

taskdef_block := "taskdef" string "{" taskdef_stmt* "}" ;
taskdef_stmt  :=
    "time" ":" expr ";"
  | "location" ":" ("inside"|"near"|"outside") ";"
  | "risk" ":" ("none"|"low"|"med"|"high") ";"
  | "station" ":" ident ";"
  | "base_success" ":" percent ";"
  | "requires_tools" ":" "[" string_list? "]" ";"
  | "consumes" ":" "{" mat_list? "}" ";"
  | "produces" ":" "{" mat_list? "}" ";"
  | "wear" ":" "{" mat_list? "}" ";"
  | "effects" ":" "{" effect_list? "}" ";" ;

mat_list := (string ":" expr ";")* ;
effect_list := (ident ":" expr ";")* ;
```

### Character plan (`.lbp`) — required
```ebnf
char_stmt :=
    version_stmt
  | skills_stmt
  | traits_stmt
  | thresholds_stmt
  | defaults_stmt
  | plan_stmt
  | on_event_stmt
  ;

skills_stmt := "skills" "{" (ident ":" expr ";")* "}" ;
traits_stmt := "traits" ":" "[" string_list? "]" ";" ;

thresholds_stmt := "thresholds" "{" threshold_rule* "}" ;
threshold_rule := "when" expr "do" action_stmt ";" ;

defaults_stmt := "defaults" "{" (ident ":" expr ";")* "}" ;

plan_stmt := "plan" "{" plan_rule* "}" ;
plan_rule :=
    "block" ident range "{" stmt* "}"
  | "rule" string? "priority" expr "{" stmt* "}"
  ;

on_event_stmt := "on" string ("when" expr)? "priority" expr "{" stmt* "}" ;

range := expr ".." expr ;
```

### Statements & actions
```ebnf
stmt :=
    let_stmt ";"
  | if_stmt
  | action_stmt ";"
  | break_stmt ";"
  ;

let_stmt := "let" ident "=" expr ;

if_stmt := "if" expr "{" stmt* "}" ("else" "{" stmt* "}")? ;

break_stmt := "stop_block" | "yield_tick" ;

action_stmt :=
    "set" ident "=" expr
  | "task" string task_args?
  | "reserve" "{" mat_list? "}"
  | "handoff" string "{" mat_list? "}" ;

task_args :=
    ("for" expr)?
    ("using" "{" mat_list? "}")?
    ("consume" "{" mat_list? "}")?
    ("produce" "{" mat_list? "}")?
    ("target" string)?
    ("quiet" | "loud")?
    ("priority" expr)?
    ;
```

### Lists
```ebnf
string_list := string ("," string)* ;
```

---

## 8) Multi-character execution semantics

### 8.1 Joint scheduler
For each tick:
1. Collect candidate actions from both scripts whose `block` range includes `tick`, plus any `rule` blocks whose conditions are true.
2. Expand each `task` into a task request with character id and computed priority.
3. Resolve conflicts:
   - If two tasks require the same exclusive `station`, higher priority wins; loser becomes `yield_tick` unless it has a fallback.
   - If consuming a scarce item, allocate by priority; if insufficient, task fails preflight and is skipped.

### 8.2 Shared inventory and handoffs
- Inventory is shared shelter inventory by default.
- `handoff "OtherName" { ... }` marks outputs as preferred for the target character for a short window (default 6 ticks).

### 8.3 Defense posture
- `defaults { defense_posture: quiet; }` sets a character preference.
- Daily posture is majority vote; ties default to `quiet`.

---

## 9) Canonical task and item names

The engine should validate task/item names against catalogs:
- Item names: from canonical items list
- Task names: from canonical tasks list
- Extend/override via `itemdef`/`taskdef`

---

## 10) Example character plan files (two survivors)

### Example 1: `mara.lbp`
```text
version 0.1;

character "Mara" {
  skills {
    cooking: 3;
    gardening: 2;
    medical: 2;
    repair: 1;
    guns: 1;
    scouting: 0;
  }

  traits: ["cautious", "tidy", "routine_builder"];

  defaults {
    defense_posture: "quiet";
  }

  thresholds {
    when char.hunger < 45 do task "Eating" for 1t priority 95;
    when char.hydration < 50 do task "Eating" for 1t priority 94;
    when char.fatigue > 75 do task "Resting" for 2t priority 93;
    when char.illness > 25 do task "Medical treatment" for 2t priority 98;
    when char.injury  > 25 do task "First aid" for 1t priority 97;
  }

  plan {

    block morning 0..6 {
      if shelter.water_safe < 6 and has("Water filter") {
        task "Water filtration" for 2t using { "Water filter": 1; } priority 90;
      }

      if stock("Food") > 2 {
        task "Meal prep" for 2t priority 80;
        task "Cooking"  for 2t priority 79;
      }

      if shelter.contamination > 25 {
        task "Cleaning" for 2t priority 78;
      }
    }

    block mid 6..14 {
      if has("Hydroponic planter") and (stock("Seeds") > 0 or stock("Plant") > 0) {
        task "Watering plants" for 1t priority 70;
        task "Hydroponics maintenance" for 2t priority 69;
      }

      if char.morale < 45 and has("Books") {
        task "Reading" for 1t priority 60;
      } else if char.morale < 45 {
        task "Talking" for 1t priority 59;
      }

      task "General shelter chores" for 2t priority 55;
    }

    block evening 14..20 {
      if has("Food storage containers") {
        task "Food preservation" for 2t priority 65;
      }

      if char.morale < 60 {
        task "Socializing" for 1t priority 58;
      }

      if shelter.structure < 70 {
        task "Maintenance chores" for 2t priority 72;
      }
    }

    block night 20..24 {
      task "Sleeping" for 4t priority 100;
    }

    rule "cold_buffer" priority 85 {
      if shelter.temp_c < 2.0 {
        if has("Barrel heater") and stock("Firewood") > 0 {
          task "Heating" for 2t priority 85;
        } else {
          task "Tending a fire" for 2t priority 80;
        }
      }
    }
  }

  on "breach" priority 99 {
    if breach.level >= 2 and stock("Ammunition") < 4 {
      task "Defensive combat" for 2t priority 88;
    }

    task "Cleaning" for 2t priority 96;
    task "Maintenance chores" for 2t priority 95;

    if has("Gun cleaning kit") {
      task "Gun smithing" for 2t priority 94;
    }

    if char.fatigue > 60 or char.morale < 45 {
      task "Resting" for 2t priority 93;
    }
  }
}
```

### Example 2: `joel.lbp`
```text
version 0.1;

character "Joel" {
  skills {
    cooking: 1;
    gardening: 0;
    medical: 1;
    repair: 3;
    guns: 3;
    scouting: 2;
    power: 2;
  }

  traits: ["pragmatic", "mechanic", "guardian"];

  defaults {
    defense_posture: "loud";
  }

  thresholds {
    when char.hunger < 40 do task "Eating" for 1t priority 92;
    when char.hydration < 45 do task "Eating" for 1t priority 91;
    when char.fatigue > 80 do task "Resting" for 2t priority 95;
    when char.injury  > 30 do task "First aid" for 1t priority 96;
  }

  plan {

    block morning 0..6 {
      if has("Battery") and (has("Solar panel") or has("Generator")) {
        task "Power management" for 2t priority 85;
      }

      if shelter.power < 20 and (has("Multimeter") or has("Electrical gear")) {
        task "Electrical diagnostics" for 2t priority 80;
      }
    }

    block mid 6..14 {
      if shelter.structure >= 60 and char.fatigue < 60 {
        if has("Fishing rod") and has("Fishing hooks") and has("Bucket") {
          task "Fishing" for 3t priority 70;
          task "Fish cleaning" for 1t priority 69;
        } else {
          task "Scouting outside" for 3t priority 68;
        }
      } else {
        task "Maintenance chores" for 2t priority 75;
      }

      if has("Soldering iron") and has("Solder wire") {
        task "Electronics repair" for 2t priority 62;
      }
    }

    block evening 14..20 {
      if shelter.structure < 75 {
        task "Maintenance chores" for 3t priority 82;
      }

      if has("Gun cleaning kit") {
        task "Gun smithing" for 2t priority 78;
      }
    }

    block night 20..24 {
      task "Sleeping" for 4t priority 100;
    }

    rule "ammo_discipline" priority 90 {
      if stock("Ammunition") < 3 {
        set defaults.defense_posture = "quiet";
      }
    }
  }

  on "breach" priority 100 {
    if stock("Ammunition") >= 2 and (has("Rifle") or has("Pistol") or has("Revolver")) {
      task "Defensive shooting" for 3t priority 100;
    } else {
      task "Defensive combat" for 3t priority 99;
    }

    task "Maintenance chores" for 2t priority 98;

    if has("Gun cleaning kit") {
      task "Gun smithing" for 2t priority 97;
    }

    if char.fatigue > 70 or char.injury > 25 {
      task "Resting" for 2t priority 96;
    }
  }
}
```
