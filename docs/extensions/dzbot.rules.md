# dzbot rules, version 1

Extension URL: `https://developers.dzbot.de/specs/dzbot/rules/1.json` (spec file served there).

The rules [dzbot](https://dzbot.de) applies to a project mod on each server it is installed on: whether a block,
a change or a single element of a block takes part, values computed from the mod's settings, repetition. They live in
the project file itself — a dzbot project mod is that one file, `dzstudio/project.json` — and each rule sits next to
what it belongs to: in the `extensions` of that block, that change or that list element, under this extension's URL.
The project's own entry holds the mod's settings. dzStudio edits rules and settings when the open project is a dzbot
project mod (Project › dzbot rules …), dzbot's web workbench and AI tools edit them there too; other programs keep the
entries as they are. Not `required`: without the rules the
project means "everything, with the values as saved".

```json
"blocks": [
  {
    "id": "g-teleporter", "kind": "spawnerGroup",
    "data": {
      "objects": [
        { "name": "Land_Teleporter", "pos": [0, 0, 0],
          "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": {
            "fields": { "pos": { "setting": "source" } } } } },
        { "name": "Sign_Back", "pos": [2, 0, 0],
          "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": {
            "if": { "field": "reverse", "op": "eq", "value": true } } } }
      ]
    },
    "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": {
      "if": { "field": "teleport", "op": "eq", "value": true } } }
  }
],
"changes": [
  { "op": "set", "block": "event", "key": { "name": "VehicleCivilianSedan" }, "field": "nominal", "from": 8, "to": 12,
    "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "formula": "round(vanilla * boost)", "required": true } } }
]
```

The entry under the URL is the rule itself (an object). The project's own `extensions` entry of this URL holds the
mod's settings (below): `{"data": {"settings": [ … ]}}`.

## A rule

- On a **block**:
  - `if` — a condition (below); the block and its child blocks take part only while it holds.
  - `required` — `true`: a block that goes into an existing entry (its `parent` is a key) must find exactly one
    target, or the whole project is not applied on that server.
  - `repeat` — `{"count": "<formula>"}` or `{"over": "<list setting>"}`: the block (with its child blocks) is repeated.
    Copies are named `<id>-1`, `<id>-2` …; formulas in them read `index` (0, 1, …) and, over a list, `item` and
    `item.<property>`. Optional `"name"`: a template for the copies' names (below). Optional `"where"`: a condition
    each entry must meet to make a copy (it reads `item`, `item.<property>` and `index`, the entry's place in the
    list); the copies kept count `index` from 0 again. Optional `"split"` (only with `over`): a text setting is cut into
    the list — `","` makes `"Knife,Rag"` two entries, `[",", ":"]` also cuts each entry into its parts, read as
    `item.0`, `item.1` … (`"Knife:1"`: `item` is `Knife:1`, `item.0` `Knife`, `item.1` `1`; a part that is not there
    has no value). `"\n"` cuts a text into its lines. Nothing is trimmed unless `"trim": true` (only with `split`):
    then every entry and part loses its surrounding white space (with `"where": {"field": "item", "op": "not_empty"}`
    blank lines make no copy). An empty text has no entries.
- **Names of repeated blocks.** A block whose kind is identified by a name — an event, a spawn zone, an event group, an
  item type, a loot type, a spawnable type, a random preset, a global, a message (its text), a player spawn group, a CE
  default or root class, an ignored type, an event child (its `type`) — would collide with its own copies, so each
  copy gets a name of its own: `<name>_1`, `<name>_2` … by default (no hyphen: game identifiers may not allow it), or what the repeat's `name` template makes:
  `{name}` is the block's name, `{n}` the copy's number from 1, `{index}` from 0 — `"name": "StaticHeli_{n}"` gives
  `StaticHeli_1`, `StaticHeli_2` … A template must hold `{n}` or `{index}` (so the names differ), nothing else in
  braces, and besides the placeholders only letters, digits and `_` (at most 200 characters); an event's name must still start with its prefix (`Vehicle`, `Static`, …). Kinds whose entries may share a
  name (loot containers, territories) or are known by a position keep it, and a template there is refused, as on a
  list element. A block whose `fields` set the name (`"name": {"setting": "item.name"}`) keeps that value and takes
  no template. Child blocks follow their parent's copy: those placed in it by its id, and those whose `parent` key
  names the repeated block (`{"event": "StaticHeli"}`, `{"group": "…"}`) — they are repeated with it and their key
  names the copy.
  - `fields` — per path in the block's `data`, `{"setting": "<name>"}` (the setting's value as it is; a coordinate is a
    whole position), `{"formula": "<formula>"}` (a number; `base` is the value in the project) or
    `{"template": "<template>"}` (text, below), each optionally with an `"if"`: while that condition does not hold the
    value stays as the project has it (`{"formula": "round(item.slot)", "if": {"field": "item.slot", "op":
    "not_empty"}}`). Settings and templates are applied before formulas. A block's paths
    stay outside objects in lists (`nominal`, `flags.deletable`, `PRABoxes[0][2]`); a value of such an object is set by
    its own rule.
- On a **list element** of a block's data (an object of a group, an item of a preset — any object inside a list):
  `if`, `repeat` and `fields`, with paths in `fields` starting at the element (`pos`, `pos[0]`).
- On a **change**: `if`, `formula` (only for `op: "set"`; names: the settings, `base` = the value found on the server,
  `vanilla` = Bohemia's value) or `template` (only for `op: "set"`, not beside a formula: the value is the text it
  makes — a number where the change's `to` is a number and the text is one), `key` and `required`.
  - A formula on a change whose `to` is a list of numbers computes each of them: `index` is the number's place (0, 1,
    …), `base` and `vanilla` the number at that place; the list keeps the length of `to`.
    `{"op": "set", "block": "gameplaySetting", "key": {"path": "WorldsData.environmentMinTemps"}, "to": [0, 0, …],
    "extensions": {"<url>": {"formula": "min_temp"}}}` writes the setting twelve times.
  - `key` — per member of the change's key, `{"setting": …}`, `{"template": …}` or `{"formula": …}`: the member's value
    on the server, so a setting names what the change targets (`"key": {"name": {"setting": "item1"}}` removes the
    item type a server admin names). The members not listed keep the value the project has; reports still name the
    change by its key in the project. A member without a value (an empty setting) is an error: the change is left out,
    with the reason in the mod's log — give it an `if` that leaves it out first.

Members not listed here are kept by every program that rewrites the entry. Patchers ignore `extensions` everywhere —
of the project, of blocks and changes and of elements inside block data — and never write them into a mission file.

**Paths** name a value inside a block's data, or inside a list element for the element's own rule: keys joined with
dots — `nominal`, `flags.deletable`, `pos` — and indexes from 0 only into lists of plain values or of lists, like a
position's numbers (`pos[1]`) or a restricted area's boxes, each `[[size x, y, z], [yaw, pitch, roll], [centre x, y, z]]`
(`PRABoxes[0][2]` is the first box's centre, `PRABoxes[0][2][1]` its height; `safePositions3D[0]` a whole position).
These are tuples of a fixed shape: their places do not shift. No path reaches into an object of a list by its place
(`objects[2].pos`, `children[0].max`, `objects[2]`): deleting an object before it would make it mean another one. dzbot
refuses such a path: its preview names the element whose own rule takes the value, and on a server a block with one is
left out, with the reason in the mod's log. Without the block's data at hand an index followed by a key counts as such
a path. Nothing else in the rules addresses an object by its place either: an element's rule is in its own
`extensions`, and an `items` member in a block's rule is not read.

**Conditions** are JSON trees: `{"field": "<setting>", "op": "<op>", "value": …}` with `op` one of `eq`, `neq`, `in`,
`not_in` (value a list), `empty`, `not_empty` (no value), `gt`, `gte`, `lt`, `lte`, `contains`; groups
`{"all": [ … ]}`, `{"any": [ … ]}` and `{"not": { … }}`. Values compare loosely (`"1"`, `true` and `1` are the same
for a yes/no setting).

**Formulas**: numbers, names (also with parts: `source.x`, `item.0`), `+ - * /`, parentheses and `min`, `max`, `round`,
`floor`, `ceil`, `abs`, `clamp`; at most 500 characters.

**Templates** build text: literal text with `{…}` parts, at most 1000 characters. A part is a name — a setting (also
with parts: `source.x`), in a repeated block `index`, `item`, `item.<property>`, or a server value (below) — or a
function of names and quoted text (`"…"` or `'…'`, `\` escapes the quote): `lower(x)`, `upper(x)`, `trim(x)`,
`replace(x, "from", "to")` and `default(x, y, …)`, the first of its values that is not empty. `{{` and `}}` are a
brace. A name without a value (unset, empty, `null`) gives empty text — use `default` for a fallback; yes/no gives
`true`/`false`, a number is written as a mission file writes it (`2`, `4500.5`). A name that holds several values (a
whole coordinate, a list) is an error: the block is left out, or the change is `invalid`, with the reason in the mod's
log.

```json
{ "fields": { "areaName": { "template": "Teleport-{default(identifier, installation_id)}" } } }
{ "fields": { "text": { "template": "Connect your account now: {manager_url}" } } }
{ "if": { "field": "color", "op": "eq", "value": "custom" }, "template": "0xff{lower(replace(trim(custom_color), \"#\", \"\"))}" }
```

**Server values** are names every rule reads besides the settings — in templates, conditions, `setting` and formulas —
filled in by dzbot for the server the mod is installed on: `manager_url` (the address of the server's dzbot site),
`mission` (its mission folder, `dayzOffline.chernarusplus`) and `installation_id` (the mod's installation on that
server, a number as text; `preview` in previews). A mod installed several times on a server has an `installation_id` per
instance: a name that must differ between them (a restricted area's `areaName`, an event's name) takes it in a template. A setting with one of these names is shadowed by the server value.

## Settings

The settings are what a server admin fills in when installing the mod — the names conditions, formulas, `fields` and
`repeat` read. They live in the project file, in the project's own entry, in the order of dzbot's install form:

```json
"extensions": {
  "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "data": { "settings": [
    { "identifier": "weapons", "title": "Weapons", "type": "section" },
    { "identifier": "boost", "title": "Car boost", "type": "number", "required": true, "default": 1.5,
      "help": "1 = vanilla", "typeConfig": { "min": 0, "max": 5 } },
    { "identifier": "mode", "title": "Mode", "type": "generic_select", "default": "easy",
      "typeConfig": { "options": [ { "value": "easy", "label": "Easy" }, { "value": "hard", "label": "Hard" } ] },
      "visibleIf": { "field": "boost", "op": "gt", "value": 1 } }
  ] } }
}
```

| Member | Meaning |
| --- | --- |
| `identifier` | The name rules read: letters, digits and `_`, not starting with a digit, unique among the settings. A `section` may repeat one. |
| `title` | What server admins see (up to 255 characters). Absent: the identifier is shown. |
| `type` | A dzbot config field type — `number`, `boolean`, `string`, `textarea`, `coordinate`, `datetime`, `email`, `generic_select`, `object_list`, `item_class` … — or `section`, a heading over the settings below it. |
| `help` | Optional explanation under the field (up to 255 characters). |
| `required` | Optional, default `false`: an installation needs a value. Never on a `section`. |
| `default` | Optional: the value a new installation starts with — a number, `true`/`false` or a position `[x, y, z]` for those types, text for every other. |
| `visibleIf` | Optional: when the field (or a `section` with everything in it) is shown — a condition as above, on settings above it. |
| `typeConfig` | Optional: the type's configuration — `generic_select`: `options` (`[{"value", "label"}]`) and `multiple`; `object_list`: `properties` (`[{"name", "type", "label"}]`); `number`: `min`, `max` … |

Readers also take a simpler shape: `id` for a missing `identifier`, the type `select` for `generic_select`, and
`options` (objects or plain values), `multiple` and `props` (`[{"name"}]`) beside the entry for the same members of
`typeConfig`. dzbot writes the shape of the table; members it does not know stay in the entry. A project without
`settings` leaves the mod's settings as they are in dzbot.

## Names in reports

dzbot's reports and problems name a change `<block> <key> <field>`, trimmed: its `block` kind, its `key` as compact JSON
and its `field` (empty for a `remove`) — `event {"name":"VehicleCivilianSedan"} nominal`. The rules themselves need no
names: they sit on what they belong to.

## In dzStudio

Project › dzbot rules … lists the project's own blocks and changes (the selected object's group first) with a
summary of their rules; a group's context menu in the project bar opens the same list with that group first, and
groups with rules are marked "dzbot rules". A rule opens in its own dialog:

- **Takes part when** — comparisons of a setting (picked from the mod's settings), an operator and a value, joined by
  "all apply" or "at least one applies". Nested groups and `not` are shown read-only.
- **Formula for the value** (changes that set a value), **Target required** (changes, and blocks inside an existing
  entry).
- **Text for the value** in place of the formula, for a change whose rule has a `template`.
- **Repeat**, **Values from settings and formulas** (each value from a setting, a formula or a text; paths picked from
  the block's data outside its lists, or from the element in an element's rule; a path typed through an object of a
  list is marked as not applied) and **Single elements** (each with its own condition, repetition and values; the
  selected objects first).

Every edit — a rule, a setting added, changed or removed — is a step of the history and saved with the project.
Renaming a setting renames it in every rule that names it (inside a template only in its `{…}` parts) in the same step, so undo puts back the settings and the rules
together. The dialogs hold on to their block, change, list element (by its element, not its place) or setting (by its
identifier); one whose target is deleted meanwhile — by undo or by a partner in a live session — closes with "This
element was deleted" and writes nothing.

**Copying.** The spec sets `"copy": "same-project"`: an object duplicated or pasted in the same project keeps its rules;
pasted into another project it starts without them, because rules name the settings of their own project. Since every
value of an object sits in the object's own rule, it goes along with the object and stays with it when others are
deleted.

## In dzbot

The project file is where a dzbot project mod keeps its settings. Every save of the file — from dzStudio, the
workbench, an import or dzbot's AI tools — makes the mod's config fields match the list: new identifiers become new
fields, changed ones are updated, missing ones removed, and the order follows the list. A setting replaced in its place
by one new identifier of the same type counts as renamed, so installations keep their value. A list dzbot cannot take
(two settings with one identifier, an identifier rules cannot read, a condition on a setting further down) refuses the
whole save with the reason. The workbench's config panel writes into the same list (and only into it); renaming a
setting there can rename it in every rule too. New project mods start with an empty list; dzStudio is sent the
settings with the project as well — the entries whole, in the shape above (the file's own list as written, or the
mod's fields written in that shape when the file has none).

dzbot reads the rules when it applies the project at a transfer and in the preview; a later version would be another
URL, which this dzbot does not read. dzbot also knows dzStudio's editor extension (`https://developers.dzbot.de/specs/dzstudio/editor/1.json`) and ignores it; any other
extension marked `required` keeps the project off every server ("Project not applied: it needs an extension dzbot does not support"), in
the server's mod log and in the preview (only the project's own entries can be required; an entity's entries cannot).
The workbench rewrites only the blocks and changes it edits and keeps every other byte of the file; renaming a mod
setting rewrites its name in the rules the same way, each rule in its own layout. Export downloads the
project file as it is, rules included; import replaces it with another project file.
