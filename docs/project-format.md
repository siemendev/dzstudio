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
| `slug` | string | Lower-case name used for the project's own files: `dzstudio/<slug>/<block id>.json`. |
| `blocks` | array | Own blocks, in order (below). |
| `changes` | array | Changes to existing entries, applied in order (below). |
| `extensions` | object | Data of other programs and plugins (below). The patcher ignores it. |

Members a program does not know stay in the file when it rewrites it.

## Blocks

```json
{ "id": "pos-1", "kind": "spawnPosition", "parent": { "event": "VehicleCivilianSedan" }, "data": { "x": 4520.5, "z": 10210.25, "a": 90 } }
```

- `id` — unique in the project, `[A-Za-z0-9_-]`. A block that is a file of its own is written to
  `dzstudio/<slug>/<id>.json`, so renaming a group never makes a new file.
- `kind` — one of the kinds below.
- `parent` — for kinds that live inside another entry: either the **key** of an entry of the base
  (`{"event": "VehicleCivilianSedan"}`: an own spawn position in a vanilla event) or the `id` of an own block (a string:
  a child of an own event). Absent or `null` for top-level kinds.
- `data` — the block's fields, named as in the mission file (XML attributes and elements, JSON members). Own blocks
  are written complete: every field the game needs is required, nothing is left to engine defaults.

An own block whose key already exists in the base (an own event named like an existing one) is not written
(`collision`).

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

The fields of each kind and their value types are pinned by the conformance tests.

## Applying a project

1. The patcher reads every file the project touches from the base. A file that is missing or unreadable is reported;
   what needs it is skipped.
2. Changes, in project order.
3. Own blocks: appended to their shared file (events, positions, zones, effect areas …) or written as a file of their
   own (`spawnerGroup`, `gearPreset`, `restrictedArea`).
4. Registrations, never stored in the project: own files are added to `objectSpawnersArr`,
   `spawnGearPresetFiles` and `playerRestrictedAreaFiles` in `cfggameplay.json` (deduplicated, existing entries kept;
   the server needs `enableCfgGameplayFile`).
5. Writing: byte-preserving — everything outside a changed span stays as it was, new entries take the indent of their
   neighbours, unchanged numbers keep their text. JSON is checked with the game's rules before it is written; output
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
| `collision` | An own block's key already exists; not written. |
| `invalid` | A value or block the game would not accept; not written. |
| `deduplicated` | A registration that was listed more than once. |

Per file: `written`, `unchanged`, `created`, `invalid` (output refused), `broken` (base unreadable) or `missing`.
An entry may carry a `detail` code (`case-mismatch`, `bad-value`, `unknown-field`, `parent-missing` …).

## Extensions

`extensions` holds data of other programs and plugins. An extension **is the URL of its spec file**: the project file
references the spec, it does not define the extension.

```json
"extensions": {
  "https://developers.dzbot.de/specs/dzstudio/editor/1.json": { "data": { … } },
  "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "data": { "blocks": { … }, "changes": { … } } },
  "https://example.com/specs/weather/2.json": { "required": true, "data": { … } }
}
```

- The key is the URL of the extension's spec file. The version is part of the URL; an incompatible new version is a new
  URL (`…/rules/2.json`), so a program knows exactly what it reads.
- The entry is `{"data": …}`, `data` in the shape the spec defines, and optionally `"required": true`.
- `"required": true` — a program that does not know the URL **must not apply** the project: the extension changes what
  the project means. It stays in the project file so that a program that cannot fetch or does not know the spec still
  knows. Leave it out when the project means the same without the extension.
- Programs match extensions by URL and never need to fetch the spec to work. Whoever rewrites the file keeps entries
  it does not know as they are, and keeps members of a known entry it does not use.
- The patcher ignores `extensions`.

### Spec files

A spec file is JSON served at its own URL:

```json
{
  "$id": "https://developers.dzbot.de/specs/dzbot/rules/1.json",
  "kind": "dzstudio-project-extension",
  "title": { "en": "dzbot rules", "de": "dzbot-Regeln" },
  "description": { "en": "…", "de": "…" },
  "owner": "dzbot",
  "spec": "https://github.com/siemendev/dzstudio/blob/main/docs/extensions/dzbot.rules.md"
}
```

`$id` is its own URL, `title` and `description` name it for people, `spec` links the human-readable spec. Planned: a
`fields` list describing the data, so dzStudio can show and edit an extension it has no plugin for — each field marked
as editable in the studio or only by tools and plugins. Until then a program needs to know an extension to use it.

Extensions specified here:

| URL | Spec | What |
| --- | --- | --- |
| `https://developers.dzbot.de/specs/dzstudio/editor/1.json` | [`dzstudio.editor`](extensions/dzstudio.editor.md) | dzStudio's view of the project |
| `https://developers.dzbot.de/specs/dzbot/rules/1.json` | [`dzbot.rules`](extensions/dzbot.rules.md) | dzbot's rules for a project mod |

How dzStudio handles extensions and how a plugin adds one: [`extensions/README.md`](extensions/README.md).
