# Project file extensions

A dzStudio project file (`format: "dzstudio-project"`, `version: 2`) holds building blocks and changes. Data that
belongs to another application or to a plugin lives under `extensions`, one entry per extension id:

```json
{
  "format": "dzstudio-project", "version": 2, "map": "chernarusplus", "slug": "my-project",
  "blocks": [], "changes": [],
  "extensions": {
    "dzbot.rules": { "version": 1, "data": { "blocks": {}, "changes": {} } }
  }
}
```

## Rules for every extension

- The id is `vendor.name` (lower case, digits and `-` allowed), for example `dzbot.rules`.
- Each entry has an integer `version` and `data` in the shape its own spec defines. Each extension has a spec in this
  folder, `<id>.md`.
- `"required": true` means: a program that does not know this extension, or not this version, must not apply the
  project. Leave it out when the project still means the same without the extension.
- Whoever rewrites the file keeps entries it does not know as they are, and keeps the members of a known entry it does
  not use.
- The patcher (applying a project to a mission folder) ignores `extensions`.

## How dzStudio handles them

Opening a project hands every entry to the extension registered for its id, if that extension reads the entry's
version. Saving writes back what each extension holds, in the order the file had, followed by new entries. Entries no
registered extension reads are written back unchanged.

A `required` entry dzStudio cannot read: the project still opens, with a warning in the project's open notes, and the
entry stays in the file as it is. `DZS_ProjectExtensions.Blocked` names it, for anything that applies the project.

## Adding an extension from a plugin

Derive from `DZS_ProjectExtension` and register an instance once, for example in your plugin's `OnRegister`:

```c
class MyNotes : DZS_ProjectExtension
{
	protected ref DZS_Json m_Data;

	override string Id() { return "example.notes"; }
	override int Version() { return 1; }
	override bool IsActive() { return true; }
	override void Load(DZS_Json data, int version) { m_Data = data; }
	override void Reset() { m_Data = null; }
	override DZS_Json Data() { return m_Data; }
}

// in OnRegister:
DZS_ProjectExtensions.Add(new MyNotes());
```

| Member | What it does |
| --- | --- |
| `Id()` | The extension id. |
| `Version()` | The version dzStudio writes. |
| `Reads(version)` | The versions it reads (default: only `Version()`); others are carried over untouched. |
| `Required()` | Writes `"required": true`. |
| `IsActive()` | Whether its user interface applies to the open project. Inactive extensions still load and save. |
| `Load(data, version)` | The entry's `data` of the opened project (a copy). Not called when the project has no entry. |
| `Reset()` | The project was closed; forget its data. |
| `Data()` | What to save; `null` leaves the entry out. |
| `GroupMark(inst, groupId)` | A short text shown with a group in the project bar (`""` = none). |
| `GroupMenu(inst, groupId, items)` / `GroupMenuChosen(inst, groupId, tag)` | Entries for a group's context menu. |

After changing its data outside the undo history, an extension calls `DZS_ProjectExtensions.Changed(instance)`: the
project counts one unsaved change until it is saved. Menu entries, dialogs and panels are registered by the plugin
itself, as any plugin does (`DZS_UiRegistry.RegisterAction`, `AddMenuEntry`).

Specs: [`dzbot.rules`](dzbot.rules.md).
