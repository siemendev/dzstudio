# dzStudio project format, version 2

A dzStudio project is one JSON file (`<name>.dzstudio.json`). It does not hold server files. It holds **building
blocks** — things the project creates — and **changes** to entries that already exist in a mission's files. A
**patcher** applies a project to any mission folder (its *base*: Bohemia's vanilla mission or a server's own) and
writes the changed files plus a report. The same project gives the right result on a base that differs from the one
it was made on: every change finds its target by a key, not by a line or an index.

dzStudio and [dzbot](https://dzbot.de) implement the patcher and are checked against the same conformance tests
(`tests/patcher/`).

```json
{
  "format": "dzstudio-project",
  "version": 2,
  "map": "chernarusplus",
  "slug": "koth-arena",
  "blocks": [
    { "id": "g-arena", "kind": "spawnerGroup", "data": { "name": "Arena", "objects": [ … ] } },
    { "id": "ev-heli", "kind": "event", "data": { "name": "StaticHeliCrashArena", "nominal": 1, … } },
    { "id": "pos-1", "kind": "spawnPosition", "parent": { "event": "VehicleCivilianSedan" }, "data": { "x": 4520.5, "z": 10210.25, "a": 90 } }
  ],
  "changes": [
    { "op": "set", "block": "event", "key": { "name": "VehicleCivilianSedan" }, "field": "nominal", "from": 8, "to": 12 },
    { "op": "remove", "block": "spawnPosition", "key": { "event": "VehicleSedan02", "x": 10182.458985, "z": 1987.5271 } }
  ],
  "extensions": {
    "https://developers.dzbot.de/specs/dzstudio/editor/1.json": { "data": { … } }
  }
}
```

## Top level

| Member | Type | Meaning |
| --- | --- | --- |
| `format` | string | Always `"dzstudio-project"`. |
| `version` | integer | The format version, `2`. Keys, tolerances and patcher rules belong to it; changing any of them is a new version. A reader refuses a version it does not know. |
| `map` | string | The map the project was made for, lower case (`chernarusplus`, `enoch`, `sakhal`, `deerisle` …). |
| `slug` | string | Lower-case name used for the project's own files: `custom/dzstudio-<slug>.<block id>.json`. A program that applies the same project more than once to one mission gives the second application the slug `<slug>-2`, the third `<slug>-3` …, so each writes files of its own (below). |
| `blocks` | array | Own blocks, in order (below). |
| `changes` | array | Changes to existing entries, applied in order (below). |
| `extensions` | object | Data of other programs and plugins for the whole project (below). The patcher ignores it. |

Members a program does not know stay in the file when it rewrites it.

## Blocks

```json
{ "id": "pos-1", "kind": "spawnPosition", "parent": { "event": "VehicleCivilianSedan" }, "data": { "x": 4520.5, "z": 10210.25, "a": 90 } }
```

- `id` — unique in the project, `[A-Za-z0-9_-]`. A block that is a file of its own is written to
  `custom/dzstudio-<slug>.<id>.json`, so renaming a group never makes a new file. It lies in `custom/` because console
  servers load own mission files reliably only there (not two folders deep, not in other folders); the dot cannot
  occur in a slug or an id, so two projects never share a file. Where one project is applied several times to the same
  mission — dzStudio applying two projects that have the same slug, or dzbot running a project mod installed more than
  once on a server — the first keeps `<slug>`, the second writes `custom/dzstudio-<slug>-2.<id>.json`, the third
  `<slug>-3` …, each registered beside the others (case `registration-second-application`). The patcher itself only
  ever sees the slug it is given.
- `kind` — one of the kinds below.
- `parent` — for kinds that live inside another entry: either the **key** of an entry of the base
  (`{"event": "VehicleCivilianSedan"}`: an own spawn position in a vanilla event) or the `id` of an own block (a string:
  a child of an own event). Absent or `null` for top-level kinds.
- `data` — the block's fields, named as in the mission file (XML attributes and elements, JSON members). Own blocks
  are written complete: every field the game needs is required, nothing is left to engine defaults — except the
  optional fields of an effect area, which Bohemia's own areas leave out too (below). An element of a
  list in `data` (`objects[i]`, `children[i]` …) may carry `extensions` of its own; the patcher leaves it out.
- `duplicate` — optional, `true`: the block is written even where its key is taken (below).
- `extensions` — optional, data of other programs and plugins for this block (below).

An own block whose key already exists in the base (an own event named like an existing one) is not written
(`collision`), and neither is an own block with the key of an earlier own block of the project. Own `message` blocks
are the exception to the second rule: several of them may share a text (the same message at different times), and all
are written; a text the base already has still collides. A block of an XML file with `"duplicate": true` never
collides: it is written beside the entry with its key, and the file then holds the key twice — as some mods write
`globals.xml` or `cfgrandompresets.xml`, so a project can give the game exactly the file it got before. Which copy the
game uses is the game's business; only `true` counts.

A program that rewrites a project keeps the blocks and changes it does not edit exactly as they are, `extensions`
included, and keeps the file's `slug` when the project belongs to a server (its own files are already there as
`custom/dzstudio-<slug>.<id>.json`). dzStudio carries such entries through every save and lists their rules read-only. Its
layers (spawns, effect areas, territories) take the entries they edit from the file's blocks and changes at every
open — never from the copy in dzStudio's own extension, which other programs leave as it is — and show the entries of
their kinds they cannot write back unchanged read-only.

## Changes

```json
{ "op": "set", "block": "event", "key": { "name": "VehicleCivilianSedan" }, "field": "nominal", "from": 8, "to": 12 }
{ "op": "remove", "block": "spawnPosition", "key": { "event": "VehicleSedan02", "x": 10182.458985, "z": 1987.5271 } }
```

| Member | Meaning |
| --- | --- |
| `op` | `set` changes one field of the entry; `remove` removes the entry with everything inside it. |
| `block` | The kind of the entry. |
| `key` | The entry's identity key (below). A field that is part of the key, like a position, is still addressed by the *old* key. |
| `field` | For `set`: the field, as a path inside the entry (`nominal`, `flags@deletable`, `pos`; a gameplay setting has its path in the key and no field). |
| `from` | Optional: the value when the change was made. A list records the whole list or `sha256:` of its members. |
| `to` | For `set`: the new value. `null` removes an optional attribute where the kind allows it (`lootmax`). |
| `extensions` | Optional: data of other programs and plugins for this change (below). |

Adding something to an existing entry is an own block with that entry as `parent`, never a change.

The patcher resolves the key in the base:

- no match: `remove` → `gone` (the goal is reached), `set` → `orphaned` (nothing written);
- one match: applied; the current value differing from `from` is `overrode`, already equal to `to` is `already`;
- more than one match: `ambiguous`, nothing written — an `ordinal` in the key picks one.

## Identity keys

Every kind declares its key; the patcher finds entries only through it.

- **Names** compare case-insensitively for class names (item types, spawnable types, event children, event groups,
  loot types, ignored types); event names, presets, globals and effect areas compare exactly.
- **Coordinates** match within a tolerance per axis, inclusive: spawn positions, player spawn positions and territory
  zones 0.5 m, event-group children 0.25 m, loot points and building loot rows 1 cm. Numbers compare as numbers
  (`341` = `341.000000`).
- **Duplicates** are a multiset: `"ordinal": n` (1-based, in file order among entries with the same key) picks one.
  Without it, more than one match is `ambiguous`; a `remove` of kinds that may repeat takes every copy.
- **Parents scope keys**: a spawn position belongs to its event (`event`), a loot point to its container (`group`,
  `container`).
- Commented-out entries do not exist.

### Kinds

| Kind | File | Key |
| --- | --- | --- |
| `itemType` | `db/types.xml` | `name` |
| `spawnable` | `cfgspawnabletypes.xml` | `name` (the last of two counts) |
| `spawnableAttachments`, `spawnableCargo` | inside a `spawnable` | `type`, `ordinal` when the entry has more than one |
| `cargoPreset`, `attachmentsPreset` | `cfgrandompresets.xml` | `name` |
| `spawnableItem` | an `<item>` of a spawnable's cargo or attachments | `name`, `type` optional (remove only) |
| `presetItem` | an `<item>` of a cargo or attachments preset | `name`, `preset` optional (remove only) |
| `ignoredType` | `cfgignorelist.xml` | `name` |
| `global` | `db/globals.xml` | `name` |
| `economy` | `db/economy.xml` | `name` (the element: `dynamic` … `player`); set only |
| `ceDefault`, `ceRootClass` | `cfgeconomycore.xml` | `name` |
| `event` | `db/events.xml` | `name` |
| `eventChild` | inside an event | `event`, `type` |
| `spawnList` | `cfgeventspawns.xml` | `name` |
| `spawnPosition` | inside a spawn list | `event`, `x`, `z` |
| `eventGroup` | `cfgeventgroups.xml` | `name` |
| `eventGroupChild` | inside an event group | `group`, `type`, `x`, `z` |
| `territory` | `env/<x>_territories.xml` | `file`, `color` |
| `territoryZone` | inside a territory | `file`, `color`, `name`, `x`, `z` |
| `weather` | `cfgweather.xml` | `{}` (one per file) |
| `message` | `db/messages.xml` | `text` (the text in the base) |
| `playerSpawnSection` | `cfgplayerspawnpoints.xml` | `section` (`fresh`, `hop`, `travel`); set only |
| `playerSpawnGroup` | inside a section | `section`, `name` |
| `playerSpawnPosition` | inside a group | `section`, `group`, `x`, `z` |
| `playerSpawnLoose` | directly in a section's `generator_posbubbles`, in no group | `section`, `x`, `z` |
| `lootType` | `mapgroupproto.xml` | `name` (the first of two counts) |
| `lootContainer` | inside a loot type | `group`, `name`, `ordinal` |
| `lootPoint` | inside a container | `group`, `container`, `containerOrdinal`, `pos` |
| `lootRow` | `mapgrouppos.xml` | `name`, `pos` |
| `effectArea` | `cfgeffectarea.json` | `name` (`AreaName`) |
| `safePosition` | `cfgeffectarea.json` `SafePositions` | `x`, `z` |
| `gameplaySetting` | `cfggameplay.json` | `path` (`PlayerData.StaminaData.staminaMax`) |
| `spawnerGroup` | own file | its block `id` |
| `gearPreset` | own file | its block `id` |
| `restrictedArea` | own file | its block `id` |

The fields of each kind and their value types are pinned by the conformance tests. An event may carry
`ignore="true"` (field `ignore`, only the value `true`): the game does not read it, dzbot's vehicle wipe leaves such an
event's name alone.

**Items of lists.** The items of a spawnable's cargo and attachments and of a random preset are the list field `item`
of their entry; a `set` of that field writes the whole list. An item is a class name or `{"name", "chance"}`, and may
carry what the game reads inside it:

```json
{ "name": "GunCase_Black", "chance": 0.5, "damage": { "min": 0, "max": 0 },
  "attachments": [ { "preset": "x" } ],
  "cargo": [ { "chance": 1, "item": [ "SCARH", { "name": "Mag_SCARH_20Rnd", "chance": 1 } ] } ] }
```

- `damage` — `{"min", "max"}`, each optional, numbers: `<damage min="…" max="…" />` inside the item.
- `attachments`, `cargo` — lists of blocks `{"chance", "preset", "item"}` (all optional), written inside the item
  like a spawnable's own; their items take the same form, to any depth.
- They are written as `<damage>`, then the `attachments`, then the `cargo` blocks, each in its list's order. A value
  of the wrong type makes the change or block `invalid` (`bad-value`).
- Comparing a list (`from`, `already`) and its fingerprint take the content in: a member is its first line
  `name=…` (`\tchance=…`), followed by one line per element of its content in this canonical form (numbers as in
  `PatchNum.canon`, attributes `min`, `max` / `chance`, `preset` in that order, `" />"` for an empty element), each
  indented by one tab per level below the item: `name=Grenade_ChemGas\tchance=1\n<damage min="1" max="1" />`.
  Other attributes and elements inside an item (an `identifier`) are not read. Removing an item
  (`spawnableItem`) leaves the content of the others as it is. `spawnableItem` and `presetItem` name single items by their
class, for one thing only: a `remove` takes the class out of every list that holds it (case-insensitively, like class
names), or only out of the lists of the spawnable `type` or the `preset` its key names. The lists keep their element
and their other items; nothing found is `gone`. A `set` of them is `invalid` (`bad-op`), an own block of them is no
kind.

**A loose player spawn position** (`playerSpawnLoose`) stands directly in a section's `generator_posbubbles`, beside
the groups: `<pos x="…" z="…" />`. An own one names its section as `parent` (`{"section": "fresh"}`), and the patcher
makes the `generator_posbubbles` when the section has none.

### Effect areas above the ground

An own `effectArea` can stand a number of metres above the ground instead of at a fixed height:

```json
{ "id": "gas", "kind": "effectArea", "data": { "AreaName": "Gas-1", "Type": "ContaminatedArea_Static", "TriggerType": "ContaminatedTrigger",
  "aboveGround": 2.5, "Data": { "Pos": [4520.5, 10210.25], "Radius": 100, … }, "PlayerData": { … } } }
```

- `aboveGround` — metres above the ground, a number. It is part of the block's data, never of the file.
- `Data.Pos` is `[x, z]`; a `[x, y, z]` is accepted and its `y` left aside.
- The patcher writes `"Pos": [x, y, z]` with `y` = the ground height at x/z plus `aboveGround`. Where the ground comes
  from is up to the program running it: dzStudio takes the game's terrain, dzbot its height map of the map. A run
  without a ground writes `y` = `aboveGround` and reports the block `applied` with `detail` `no-ground`.
- A value that is not a number is `invalid` (`bad-value`).

The conformance runners use a flat ground at height 0 (case `effect-area-above-ground`).

### Effect areas: what an own area needs

An own `effectArea` needs what every one of Bohemia's areas has: `AreaName`, `Type`, `TriggerType` and `Data.Pos`,
`Data.Radius`, `Data.PosHeight`, `Data.NegHeight`. Every other field — `InnerRingCount`, `InnerPartDist`,
`OuterRingToggle`, `OuterPartDist`, `OuterOffset`, `VerticalLayers`, `VerticalOffset`, `ParticleName`,
`EffectInterval`, `EffectDuration`, `EffectModifier` and the three of `PlayerData` — may be left out (or `null`), as
Bohemia leaves them out of Badlands' oil pits and Sakhal's hot springs. The patcher then leaves it out too: it never
writes a default, so the game reads its own (0, `false`, none); a `PlayerData` with no field is not written. A required
field that is missing is `invalid` (`incomplete`), a field of the wrong type `invalid` (`bad-value`). Case
`effect-area-optional`.

## Applying a project

1. The patcher reads every file the project touches from the base. A file that is missing or unreadable is reported;
   what needs it is skipped.
2. Changes, in project order. Several sets of fields inside an element the entry lacks (`damage.min`, then
   `damage.max` of a spawnable without `<damage>`) make that element once, with every value.
3. Own blocks: appended to their shared file (events, positions, zones, effect areas …) or written as a file of their
   own (`spawnerGroup`, `gearPreset`, `restrictedArea`).
4. Registrations, never stored in the project: own files are added to `objectSpawnersArr`,
   `spawnGearPresetFiles` and `playerRestrictedAreaFiles` in `cfggameplay.json` (deduplicated, existing entries kept;
   the server needs `enableCfgGameplayFile`).
5. Writing: byte-preserving — everything outside a changed span stays as it was, new entries take the indent of their
   neighbours, unchanged numbers keep their text. A line break inside a text value (a message's `text`) is part of the
   value: it is written as it is, and the text after it is not indented like the lines around it. JSON is checked with the game's rules before it is written; output
   that would not parse is not written (`invalid`).

### Report

One entry per change, own block and registration:

| Status | Meaning |
| --- | --- |
| `applied` | Written. |
| `already` | The base already had the new value. |
| `gone` | A removed entry was not there. |
| `orphaned` | The target of a change, or the parent of a block, was not found; nothing written. |
| `overrode` | Written over a value that differed from `from` (something else had changed it). |
| `ambiguous` | More than one target; nothing written. |
| `collision` | An own block's key already exists (in the base, or for most kinds in an earlier own block); not written. |
| `invalid` | A value or block the game would not accept; not written. |
| `deduplicated` | A registration that was listed more than once. |

Per file: `written`, `unchanged`, `created`, `invalid` (output refused), `broken` (base unreadable) or `missing`.
An entry may carry a `detail` code (`case-mismatch`, `bad-value`, `unknown-field`, `parent-missing` …).

dzStudio applies projects straight into a mission folder and keeps the originals beside the files, so a project can be
taken out again: [`apply-layout.md`](apply-layout.md).

## Extensions

`extensions` holds data of other programs and plugins. An extension **is the URL of its spec file**: the project file
references the spec, it does not define the extension. The project, every block, every change and every element of a
list in a block's `data` may have `extensions`:

```json
"extensions": {
  "https://developers.dzbot.de/specs/dzstudio/editor/1.json": { "data": { … } },
  "https://example.com/specs/weather/2.json": { "required": true, "data": { … } }
}
```

```json
{ "id": "g-arena", "kind": "spawnerGroup", "data": { "objects": [ { "name": "…", "pos": [ … ], "extensions": { "<url>": { … } } } ] },
  "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "if": { … } } } }
```

- The key is the URL of the extension's spec file. The version is part of the URL; an incompatible new version is a new
  URL (`…/rules/2.json`), so a program knows exactly what it reads.
- On the project the entry is `{"data": …}`, `data` in the shape the spec defines, and optionally `"required": true`.
  On a block, change or element the entry is the extension's object for that entity itself, without `data` and
  without `required`.
- `"required": true` — a program that does not know the URL **must not apply** the project: the extension changes what
  the project means. It stays in the project file so that a program that cannot fetch or does not know the spec still
  knows. Leave it out when the project means the same without the extension.
- Programs match extensions by URL and never need to fetch the spec to work. Whoever rewrites the file keeps entries
  it does not know as they are, and keeps members of a known entry it does not use. An entity that is removed takes its
  extensions with it.
- The patcher ignores `extensions` everywhere and never writes an `extensions` member into a mission file.

### Spec files

A spec file is JSON served at its own URL (https), checked by the JSON Schema
`https://developers.dzbot.de/specs/extension.schema.json`:

```json
{
  "$id": "https://developers.dzbot.de/specs/examples/server-info/1.json",
  "kind": "dzstudio-project-extension",
  "title": { "en": "Server info (example)", "de": "Server-Info (Beispiel)" },
  "description": { "en": "…", "de": "…" },
  "owner": "dzbot",
  "spec": "https://github.com/siemendev/dzstudio/blob/main/docs/extensions/README.md#spec-files",
  "fields": [
    { "id": "welcome", "attach": "project", "type": "text", "label": { "en": "Welcome text", "de": "Willkommenstext" }, "edit": "studio" },
    { "id": "lootWeight", "attach": "block:spawnerGroup", "type": "number", "min": 0, "max": 10, "default": 1, "label": { "en": "Loot weight", "de": "Loot-Gewicht" }, "edit": "studio" }
  ]
}
```

`$id` is its own URL, `title` and `description` name it for people, `spec` links the human-readable spec, `required`
is the default when a program adds the extension. `fields` describe values on the project, blocks, changes or list
elements, so a program without a plugin for the extension can show them and — where a field says `"edit": "studio"` —
edit them; the field language is in [`extensions/README.md`](extensions/README.md#fields).

Extensions specified here:

| URL | Spec | What |
| --- | --- | --- |
| `https://developers.dzbot.de/specs/dzstudio/editor/1.json` | [`dzstudio.editor`](extensions/dzstudio.editor.md) | dzStudio's view of the project |
| `https://developers.dzbot.de/specs/dzbot/rules/1.json` | [`dzbot.rules`](extensions/dzbot.rules.md) | dzbot's rules for a project mod |

How dzStudio handles extensions and how a plugin adds one: [`extensions/README.md`](extensions/README.md).
