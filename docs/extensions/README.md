# Project file extensions

A dzStudio project file (`format: "dzstudio-project"`, `version: 2`, spec: [`../project-format.md`](../project-format.md))
holds building blocks and changes. Data that belongs to another application, to a plugin or to dzStudio's own view of
the project lives in `extensions`. An extension is the URL of its spec file.

Every entity of the project has its own optional `extensions`, so an extension keeps its data next to what it is
about:

```json
{
  "format": "dzstudio-project", "version": 2, "map": "chernarusplus", "slug": "my-project",
  "blocks": [
    { "id": "g-arena", "kind": "spawnerGroup", "parent": null,
      "data": { "name": "Arena", "objects": [
        { "name": "Land_Mil_Tower_Small", "pos": [4520.5, 312.25, 10210.0], "ypr": [0, 0, 0],
          "extensions": { "https://developers.dzbot.de/specs/examples/server-info/1.json": { "label": "North tower" } } }
      ] },
      "extensions": { "https://developers.dzbot.de/specs/examples/server-info/1.json": { "lootWeight": 2.5 } } }
  ],
  "changes": [
    { "op": "set", "block": "event", "key": { "name": "VehicleCivilianSedan" }, "field": "nominal", "from": 8, "to": 12,
      "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "formula": "round(vanilla * boost)" } } }
  ],
  "extensions": {
    "https://developers.dzbot.de/specs/dzstudio/editor/1.json": { "data": { "provider": { "name": "My project", "map": "chernarusplus" } } },
    "https://developers.dzbot.de/specs/examples/server-info/1.json": { "data": { "welcome": "Hello" } }
  }
}
```

## Rules for every extension

- The key is the URL of the extension's spec file. The version is part of the URL: an incompatible new version gets a
  new URL. Programs match extensions by URL.
- **The project** (`extensions` at the top level): the entry is `{"data": …}` with `data` in the shape its spec
  defines, optionally `"required": true`. This is the project-wide part, and the list of extensions the project uses.
- **An entity** — a block, a change, or an element of a list in a block's `data` (`data.objects[i]`,
  `data.children[i]` …): `"extensions": {"<url>": {…}}`, the object being the extension's data for that entity as its
  spec defines it. There is no `data` wrapper and no `required` there.
- `"required": true` means: a program that does not know this URL must not apply the project. It sits in the project
  file, so a program that cannot fetch the spec still knows. Leave it out when the project still means the same
  without the extension.
- Whoever rewrites the file keeps entries it does not know as they are, and keeps the members of a known entry it does
  not use — on the project and on every entity it writes back.
- An entity that is gone is gone with its extensions: deleting a block, a change or an element drops what extensions
  held on it.
- The patcher (applying a project to a mission folder) ignores `extensions` everywhere and never writes an
  `extensions` member into a mission file (a spawner group's objects, gear presets, restricted areas …).

## Spec files

Each extension publishes a JSON spec file at its URL (https). JSON Schema of spec files:
[`https://developers.dzbot.de/specs/extension.schema.json`](https://developers.dzbot.de/specs/extension.schema.json).

| Member | Meaning |
| --- | --- |
| `$id` | The URL the file is served at — the extension itself. A program refuses a spec whose `$id` is not the URL it fetched. |
| `kind` | Always `"dzstudio-project-extension"`. |
| `title`, `description` | For people: `{"en": …, "de": …}` (a plain string stands for both). |
| `owner` | Who maintains it. |
| `spec` | Link to the human-readable spec, a markdown file like the ones in this folder. |
| `required` | Optional, default `false`: what a program writes as the project entry's `required` when it adds the extension. |
| `copy` | Optional, default `same-project`: whether an entity's values go along when the entity is copied (below, "Copying"). |
| `fields` | Optional: input fields a program offers without a plugin for the extension (below). |

dzbot's and dzStudio's own are served from `https://developers.dzbot.de/specs/…`:

| URL | Spec |
| --- | --- |
| `https://developers.dzbot.de/specs/dzstudio/editor/1.json` | [`dzstudio.editor.md`](dzstudio.editor.md) |
| `https://developers.dzbot.de/specs/dzbot/rules/1.json` | [`dzbot.rules.md`](dzbot.rules.md) |
| `https://developers.dzbot.de/specs/examples/server-info/1.json` | an example with fields (below) |

### Fields

A field is one value an extension keeps on an entity, described well enough that a program can show and edit it
without knowing the extension:

```json
{ "id": "lootWeight", "attach": "block:spawnerGroup", "type": "number", "min": 0, "max": 10, "step": 0.5, "default": 1,
  "label": { "en": "Loot weight", "de": "Loot-Gewicht" }, "help": { "en": "…", "de": "…" }, "edit": "studio" }
```

| Member | Meaning |
| --- | --- |
| `id` | `[A-Za-z0-9_-]`, unique in the spec. |
| `attach` | Which entity's `extensions` holds the value: `project` (inside the entry's `data`), `block` (any block) or `block:<kind>`, `change` (any change) or `change:<kind>` (by the change's `block`), `element:<block kind>.<list>` (each element of that list in such a block's `data`, e.g. `element:spawnerGroup.objects`, `element:event.children`). |
| `path` | Where in the entity's extension object: keys joined by dots (`map.zone`); no list indexes. Default: the `id`. For `project` the path starts inside `data`. |
| `type` | `number`, `integer`, `boolean`, `string`, `text` (a string that may hold line breaks), `select`, `position` (`[x, y, z]`), `list` (an array of `items`). |
| `items` | For `list`: `string` (default), `number` or `integer`. |
| `label`, `help` | For people, `{"en", "de"}`; `help` optional. |
| `min`, `max` | For `number`, `integer` and lists of numbers: the allowed range, inclusive. |
| `step` | A hint for sliders and spinners; not enforced. |
| `options` | For `select` (required there): `[{"value": "safe", "label": {…}}]`, values strings, numbers or booleans. |
| `default` | What a missing value means. Programs show it but do not write it: a value the user clears is removed, not set to the default. |
| `edit` | `studio`: dzStudio edits it. `tools` (the default): only other programs and installed plugins write it; dzStudio shows it read-only. |
| `copy` | Overrides the spec's `copy` for this field's value (the value at its `path`); absent: the spec's. |

A value that does not fit its field (wrong type, out of range) is shown as it is and replaced only when someone edits
it. Fields a program cannot read (unknown `type` or `attach`, `select` without options, an `id` used twice) are left
out; the rest of the spec still counts. Members the extension keeps beside its fields stay as they are.

The example above as a project file holds, for a spawner group and one of its objects:

```json
{ "id": "g-arena", "kind": "spawnerGroup", "data": { "objects": [ { "name": "…", "extensions": { "<url>": { "label": "North tower" } } } ] },
  "extensions": { "<url>": { "lootWeight": 2.5, "map": { "zone": "safe" } } } }
```

### Copying

An entity copied inside a program — an object duplicated, copied and pasted, taken with the pipette — is a new entity.
Whether an extension's value goes along with it is the extension's decision, `copy` in its spec:

| `copy` | The value goes along when … |
| --- | --- |
| `always` | … the copy is made anywhere: in the same project or pasted into another. |
| `same-project` (default) | … the copy stays in the project it came from (duplicating, pasting into the same project). Pasted into another project, the copy starts without it. |
| `never` | … never: a copy starts without it. |

A field's `copy` decides for its own value (the value at its `path`); members of the value no field names follow the
spec. A program without the spec (offline, never fetched) treats the extension as `same-project`. Copying a whole
project (Save a copy) is not copying entities: the copy keeps everything.

`same-project` is right for values that point into the project — dzbot's rules name the mod's settings, which live in
the project's own entry, so a rule pasted into another project would name settings that are not there. `always` suits
values that mean the same anywhere (a label), `never` values that belong to exactly one entity (an id issued for it).

## How dzStudio handles them

**Opening.** Every entry of the project's `extensions` goes to the plugin registered for its URL. The editor extension
is built in: the project file itself reads and writes it (`DZS_ProjectFile.EDITOR_EXT`, `EditorOf`, `SetEditor`).
Every other URL — in the project's `extensions` or on any entity — gets a generic handler (`DZS_ExtGeneric`) when no
plugin is registered for it; a plugin always takes precedence for its URL.

**Spec files.** For every URL of the opened project (except the editor's) dzStudio reads its copy of the spec from
`$profile:dzstudio\specs\<sha256 of the URL>.json` and fetches the spec once per session (`https://` only, the game's
RestApi). A fetched spec that parses and names the URL as `$id` replaces the copy; offline, the copy is used. Without
spec, the extension is listed with its URL and its data stays as it is.

**Entity data lives on the entity.** dzStudio builds blocks and changes anew from its model on every save
(`DZS_ProjectV2.Build`), so an entity's `extensions` are kept on the thing in the model the entity is made from, as
one more value of it (`DZS_EntityExts`):

- a spawner group and each of its objects carry their own; so does the loot row of an object with loot;
- each element of a layer (an own event, a spawn point, an effect area, a territory or zone, a changed value) carries
  those of the blocks and changes it makes;
- loot: an own loot type, container or point carries its own; a change of a loot type sits with that type, under the
  change's name (`<block> <key as compact JSON> <field>`, as in [dzbot's reports](dzbot.rules.md#names-in-reports) — the
  key of an entry of the base, never a position in a list).

So the data goes wherever the entity goes: moving, renaming or reordering keeps it, deleting an entity takes it along,
undo and redo restore it, a moved object keeps it, a live session shares it with every participant, and backups hold
it. Editing it is a step of the history. The project's own entries live in the model too (a field of the project
element per URL, from opening on), so editing them — the fields of an extension under Project › Extensions, dzbot's
mod settings — is a step of the history as well, shared in a live session.

**Copies** follow the spec's `copy` policy (above; dzStudio reads it from its copy of the spec, `same-project`
without one): duplicating objects, the pipette and pasting into the project the copy came from count as the same
project; pasting into another project as another; a loot point duplicated in the loot editor as the same. The
clipboard holds only the spawner JSON, so other tools can paste it; dzStudio keeps the copied objects' extensions
beside it in its memory, found again by the SHA-256 of the clipboard text (the last 16 copies). Pasting that exact text
brings them along (filtered by the policy, the source project compared with the open one); any other text, or the same
text after dzStudio was restarted, pastes without them.

**Identity.** Every entity that can carry extensions is addressed by where its extensions live in the model, never by
a place in a list: a spawner group and each object by their element id, a layer's block or change by the element that
makes it, a loot type's change by its ref, an own loot container or point by an id it keeps in the model (`m_Uid`,
stored with the loot entry's text, kept by every edit; the project file still numbers own loot blocks `l-<type>-<n>`
and spawner group blocks by name — block ids are a file concern). A dialog that edits an entity's values (the fields
dialog of Project › Extensions, dzbot's rules dialogs) takes that identity when it opens, reads and writes through it,
asks before every write whether the entity is still there, and closes with "This element was deleted" when it is gone
— deleted by the user, by undo, or by a partner in a live session. It never writes to whatever took the entity's place.
A list element (an object of a group) is held as `<list>@<element id>`; its place (`objects[3]`) is only shown. A
dzbot setting is held by its identifier.

Opening a project hands the file's entity data to the model once: layers take theirs while their elements are
restored (by block id and change ref of the same file), spawner groups by block id and their objects by their place in
the group's list, loot changes by ref and own loot blocks by their content. What finds no entity is dropped (the log
counts it).

**Project › Extensions …** lists the project's extensions: title (from the spec), URL, whether a plugin handles it, how
many fields its spec offers, `required`, and the state of the spec (loading, offline copy, not readable).

- **Add extension …** takes the URL of a spec file, loads it and shows title, description, owner, its fields (how many
  dzStudio may edit) and whether it is required. **Add** writes `"<url>": {"data": {}}` into the project, with
  `"required": true` when the spec says so. A required extension no plugin reads blocks applying the project to a
  mission folder in this dzStudio, as for a file that came with one.
- **Remove** (after a confirmation) takes out the project's entry and the extension's data on every entity and on the
  project (one step of the history).
- **Fields** (extensions without plugin whose spec has fields) lists what the fields apply to: the project, each block
  with block or element fields (the selected object's group first), each change with fields. **Edit** opens the
  fields of one of them; a block's dialog also lists its elements (a group's selected objects first). `studio` fields
  are edited: numbers and texts in a field (empty = default; `\n` in a `text` stands for a line break, lists comma
  separated, a position as `x, y, z` or taken from the selected object), yes/no as a switch, a choice from its
  options; the × removes a value. `tools` fields and values in the way of another member are shown read-only.
  **Apply** stores the values as a step of the history (undo takes it back), on the project as on an entity. A block's
  dialog lists its elements as they are now, each held by its identity; an element or block deleted while its dialog is
  open closes the dialog.
- A spawner group's context menu in the project bar offers the extension's fields of the group; groups with values are
  marked with the extension's title.

## Adding an extension from a plugin

Derive from `DZS_ProjectExtension` and register an instance once, for example in your plugin's `OnRegister`:

```c
class MyNotes : DZS_ProjectExtension
{
	protected ref DZS_Json m_Data;

	override string Url() { return "https://example.com/specs/notes/1.json"; }
	override bool IsActive() { return true; }
	override void Load(DZS_Json data) { m_Data = data; }
	override void Reset() { m_Data = null; }
	override DZS_Json Data() { return m_Data; }
}

// in OnRegister:
DZS_ProjectExtensions.Add(new MyNotes());
```

| Member | What it does |
| --- | --- |
| `Url()` | The URL of the extension's spec file — its identity, version included. To read two versions, register two extensions. |
| `Required()` | Writes `"required": true`. |
| `IsActive()` | Whether its user interface applies to the open project. Inactive extensions still load and save. |
| `Load(data)` | The project entry's `data` of the opened project (a copy). Not called when the project has no entry. |
| `Opened(inst)` | The opened project's model is complete; its entities carry their extensions. |
| `Reset()` | The project was closed (or the extension removed from it); forget its data. |
| `Data()` | What to save as the project entry's `data`; `null` leaves the entry out. |
| `GroupMark(inst, groupId)` | A short text shown with a group in the project bar (`""` = none). |
| `GroupMenu(inst, groupId, items)` / `GroupMenuChosen(inst, groupId, tag)` | Entries for a group's context menu. |

Data on entities is read and written through `DZS_EntityExts`, addressed by the block ids, element indexes and change
refs `DZS_ProjectV2.Build` gives for the current state: `Block` / `SetBlock (inst, blockId, url, value)`, `Element` /
`SetElement (inst, blockId, list, index, url, value)`, `Change` / `SetChange (inst, change, url, value)` and
`ChangeByRef` / `SetChangeByRef (inst, ref, url, value)`; `null` removes the extension's entry from the entity. Setting
collects the edits; `DZS_EntityExts.Commit(inst, nameDe, nameEn)` makes them one step of the history — everything
staged since the last commit, so several entities and the project data change together and undo together.

Those addresses are the file's and hold only for the state they were read from. Whatever keeps an entity across user
actions (a dialog) takes its owner instead — `BlockOwner`, `ChangeOwner`, `Owner(inst, section, id)`, and for a block's
list elements `ItemKeys` / `ItemOwner` / `ItemPath (inst, blockOwner, "<list>@<element id>")` — and reads and writes
with `Value` / `SetValue (inst, owner, url, value)`. `Alive(inst, owner)` says whether the entity is still in the
project; ask it before writing. A `DZS_LinesDialog` that calls `Watch(inst)` and overrides `TargetAlive()` closes with
the notice when its entity is gone.

Project-wide data that should be undoable lives in the model too: `ProjectData` / `SetProjectData (inst, url, data)`
(staged like entity edits, committed with them). Copy it there in `Opened(inst)` from what `Load` gave (a system
change, `Commit(inst, "", "", true)`), and return it from `Data()`. An extension that keeps project data outside the
history calls `DZS_ProjectExtensions.Changed(instance)` after changing it: the project counts one unsaved change until
it is saved. Menu entries, dialogs and panels are registered by the plugin itself, as any plugin does
(`DZS_UiRegistry.RegisterAction`, `AddMenuEntry`).

Specs: [`dzstudio.editor`](dzstudio.editor.md), [`dzbot.rules`](dzbot.rules.md).
