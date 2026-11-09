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
          "extensions": { "https://example.com/specs/server-info/1.json": { "label": "North tower" } } }
      ] },
      "extensions": { "https://example.com/specs/server-info/1.json": { "lootWeight": 2.5 } } }
  ],
  "changes": [
    { "op": "set", "block": "event", "key": { "name": "VehicleCivilianSedan" }, "field": "nominal", "from": 8, "to": 12,
      "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "formula": "round(vanilla * boost)" } } }
  ],
  "extensions": {
    "https://developers.dzbot.de/specs/dzstudio/editor/1.json": { "data": { "provider": { "name": "My project", "map": "chernarusplus" } } },
    "https://example.com/specs/server-info/1.json": { "data": { "welcome": "Hello" } }
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

A value that does not fit its field (wrong type, out of range) is shown as it is and replaced only when someone edits
it. Fields a program cannot read (unknown `type` or `attach`, `select` without options, an `id` used twice) are left
out; the rest of the spec still counts. Members the extension keeps beside its fields stay as they are.

The example above as a project file holds, for a spawner group and one of its objects:

```json
{ "id": "g-arena", "kind": "spawnerGroup", "data": { "objects": [ { "name": "…", "extensions": { "<url>": { "label": "North tower" } } } ] },
  "extensions": { "<url>": { "lootWeight": 2.5, "map": { "zone": "safe" } } } }
```

## How dzStudio handles them

**Opening.** Every entry of the project's `extensions` goes to the plugin registered for its URL. The editor extension
is built in: the project file itself reads and writes it (`DZS_ProjectFile.EDITOR_EXT`, `EditorOf`, `SetEditor`).
Every other URL — in the project's `extensions` or on any entity — gets a generic handler (`DZS_ExtGeneric`) when no
plugin is registered for it; a plugin always takes precedence for its URL.

**Spec files.** For every URL of the opened project (except the editor's) dzStudio reads its copy of the spec from
`$profile:dzstudio\specs\<sha256 of the URL>.json` and fetches the spec once per session (`https://` only, the game's
RestApi). A fetched spec that parses and names the URL as `$id` replaces the copy; offline, the copy is used. Without
spec, the extension is listed with its URL and its data stays as it is.

**Keeping entity data.** dzStudio builds blocks and changes anew from the scene on every save
(`DZS_ProjectV2.Build`), so it keeps the entities' `extensions` from opening (`DZS_EntityExts`) and puts them back
on the rebuilt entities:

- spawner groups and their objects by their scene ids — an object keeps its data when it is moved, renamed, or other
  objects of its group are added or deleted; a copied object starts without;
- other blocks by their `id`, their list elements by list and index;
- changes by their ref (`<block> <key as compact JSON> <field>`, see [`dzbot.rules.md`](dzbot.rules.md) "Change refs").

What sits on an entity that no longer exists at saving is dropped.

**Project › Extensions …** lists the project's extensions: title (from the spec), URL, whether a plugin handles it, how
many fields its spec offers, `required`, and the state of the spec (loading, offline copy, not readable).

- **Add extension …** takes the URL of a spec file, loads it and shows title, description, owner, its fields (how many
  dzStudio may edit) and whether it is required. **Add** writes `"<url>": {"data": {}}` into the project, with
  `"required": true` when the spec says so. A required extension no plugin reads blocks applying the project to a
  mission folder in this dzStudio, as for a file that came with one.
- **Remove** (after a confirmation) takes out the project's entry and the extension's data on every entity, at the next
  save.
- **Fields** (extensions without plugin whose spec has fields) lists what the fields apply to: the project, each block
  with block or element fields (the selected object's group first), each change with fields. **Edit** opens the
  fields of one of them; a block's dialog also lists its elements (a group's selected objects first). `studio` fields
  are edited: numbers and texts in a field (empty = default; `\n` in a `text` stands for a line break, lists comma
  separated, a position as `x, y, z` or taken from the selected object), yes/no as a switch, a choice from its
  options; the × removes a value. `tools` fields and values in the way of another member are shown read-only.
  **Apply** stores the values; the project counts one unsaved change.
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
| `Opened(inst)` | The opened project's scene is shown; entities' extensions are bound to it. |
| `Reset()` | The project was closed (or the extension removed from it); forget its data. |
| `Data()` | What to save as the project entry's `data`; `null` leaves the entry out. |
| `GroupMark(inst, groupId)` | A short text shown with a group in the project bar (`""` = none). |
| `GroupMenu(inst, groupId, items)` / `GroupMenuChosen(inst, groupId, tag)` | Entries for a group's context menu. |

Data on entities is read and written through `DZS_EntityExts`: `Block` / `SetBlock (inst, blockId, url, value)`,
`Element` / `SetElement (inst, blockId, list, index, url, value)`, `Change` / `SetChange (change, url, value)` and
`ChangeByRef` / `SetChangeByRef`; `null` removes the extension's entry from the entity. Block ids, element indexes and
change refs are the ones `DZS_ProjectV2.Build` gives for the current scene.

After changing data outside the undo history, an extension calls `DZS_ProjectExtensions.Changed(instance)`: the
project counts one unsaved change until it is saved. Menu entries, dialogs and panels are registered by the plugin
itself, as any plugin does (`DZS_UiRegistry.RegisterAction`, `AddMenuEntry`).

Specs: [`dzstudio.editor`](dzstudio.editor.md), [`dzbot.rules`](dzbot.rules.md).
