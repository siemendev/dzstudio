// The hidden working folder of a project saved in version 2: the project file holds only building blocks and the
// editor part, so opening builds below $profile:dzstudio\Work what the open path reads — one spawner file per
// spawner group, the loot files of the project's base with the project's loot applied by the patcher, and a project
// file of version 1 composed from the editor part. Saving never writes here; the project folder keeps the one file.
class DZS_WorkResult : Managed
{
	string m_Dir;
	ref array<string> m_Manifest;
	string m_Base;
	// the project's loot entries and rows, with their text in the base
	ref DZS_LootOrigin m_Origin = new DZS_LootOrigin();
	string m_Error;
}

class DZS_WorkFolder
{
	static const string ROOT = "$profile:dzstudio\\Work";

	// One folder per project folder (a project's backup folder gets its own).
	static string DirOf(string folder)
	{
		string safe = "";
		string bad = "\\/:*?\"<>|. $";
		for (int i = 0; i < folder.Length(); i++)
		{
			string c = folder.Get(i);
			if (bad.Contains(c))
				safe += "_";
			else
				safe += c;
		}
		return ROOT + "\\" + safe;
	}

	static string SpawnerPath(string slug, string blockId)
	{
		return "dzstudio/" + slug + "/" + blockId + ".json";
	}

	// The mission folder a base names, "" when it is not on this computer (the vanilla mission not fetched yet, a
	// mission folder that is gone).
	static string BaseDir(string base, string mapKey)
	{
		if (base.IndexOf("mission:") == 0)
		{
			string dir = DZS_PatchApply.MISSIONS + "\\" + base.Substring(8, base.Length() - 8);
			if (DZS_Files.IsFolder(dir))
				return dir;
			return "";
		}
		if (mapKey != "" && DZS_LootVanilla.HaveMission(mapKey))
			return DZS_LootVanilla.MissionDir(mapKey);
		return "";
	}

	// Whether opening needs the base's files: loot is on in the editor part (a file without one: it has loot blocks or
	// changes).
	static bool NeedsBase(DZS_Json root)
	{
		DZS_Json proj = ProjectOf(root);
		if (!proj)
			return HasLoot(root);
		DZS_Json loot = proj.Get("loot");
		return loot && loot.IsObj() && loot.GetBool("enabled");
	}

	static DZS_Json ProjectOf(DZS_Json root)
	{
		DZS_Json ed = root.Get("editor");
		if (!ed || !ed.IsObj())
			return null;
		DZS_Json proj = ed.Get("project");
		if (!proj || !proj.IsObj())
			return null;
		return proj;
	}

	static bool HasLoot(DZS_Json root)
	{
		array<string> lists = {"blocks", "changes"};
		foreach (string ln : lists)
		{
			DZS_Json list = root.Get(ln);
			if (!list || !list.IsArr())
				continue;
			for (int i = 0; i < list.Count(); i++)
			{
				DZS_Json one = list.At(i);
				if (!one.IsObj())
					continue;
				string kind = one.GetStr("kind");
				if (ln == "changes")
					kind = one.GetStr("block");
				if (kind.IndexOf("loot") == 0)
					return true;
			}
		}
		return false;
	}

	static DZS_Json RootOf(array<string> chunks)
	{
		if (!chunks)
			return null;
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc.m_Root || !doc.m_Root.IsObj())
			return null;
		return doc.m_Root.ToJson();
	}

	static string MapOf(DZS_Json root)
	{
		string mk = root.GetStr("map");
		DZS_Json ed = root.Get("editor");
		if (mk == "" && ed && ed.IsObj() && ed.Get("provider"))
			mk = ed.Get("provider").GetStr("map");
		mk.ToLower();
		return mk;
	}

	static string BaseOf(DZS_Json root)
	{
		DZS_Json ed = root.Get("editor");
		string b = "";
		if (ed && ed.IsObj())
			b = ed.GetStr("base");
		if (b == "")
			b = "vanilla";
		return b;
	}

	// The base the open path uses: the named one, the vanilla mission when a named mission folder is gone (noted in
	// 'issues'); "" when neither is here.
	static string UsableBase(string base, string mapKey, DZS_Issues issues)
	{
		string dir = BaseDir(base, mapKey);
		if (dir != "" || base == "vanilla")
			return dir;
		if (issues)
		{
			string shown = base.Substring(8, base.Length() - 8);
			issues.Add(DZS_Issue.WARNING, "Basis " + shown + " fehlt in dzstudio\\Missions — Vanilla gilt", "Base " + shown + " is missing in dzstudio\\Missions — vanilla applies");
		}
		return BaseDir("vanilla", mapKey);
	}

	// Copies the loot files of a base into the working folder (cfglimitsdefinition.xml from the fetched vanilla state
	// when the base has none; the vanilla mission also as vanilla\). False when mapgroupproto.xml or mapgrouppos.xml
	// is missing there.
	static bool CopyLoot(string baseDir, string mapKey, string work)
	{
		DZS_PatchFiles look = new DZS_PatchFiles(baseDir);
		array<string> names = {DZS_LootFiles.PROTO, DZS_LootFiles.POS, DZS_LootFiles.LIMITS};
		bool ok = true;
		foreach (string rel : names)
		{
			string real = look.Resolve(rel);
			string dst = work + "\\" + rel;
			if (real != "")
			{
				string src = baseDir + "\\" + real;
				src.Replace("/", "\\");
				if (DZS_Files.Copy(src, dst))
					continue;
			}
			if (rel == DZS_LootFiles.LIMITS)
			{
				string fetched = DZS_LootVanilla.PathOf(mapKey, rel);
				if (FileExist(fetched))
					DZS_Files.Copy(fetched, dst);
				continue;
			}
			ok = false;
		}
		// on the vanilla mission the base is the vanilla state the loot editor compares with
		if (ok && baseDir == BaseDir("vanilla", mapKey))
		{
			DZS_Files.MakeDirs(work + "\\vanilla");
			DZS_Files.Copy(work + "\\" + DZS_LootFiles.PROTO, work + "\\vanilla\\" + DZS_LootFiles.PROTO);
			DZS_Files.Copy(work + "\\" + DZS_LootFiles.LIMITS, work + "\\vanilla\\" + DZS_LootFiles.LIMITS);
		}
		return ok;
	}

	// Builds the working folder of the project file in 'folder'. m_Error set when the file is no readable version 2.
	static DZS_WorkResult Build(string folder, array<string> chunks, string loadedMap, DZS_Issues issues)
	{
		DZS_WorkResult res = new DZS_WorkResult();
		int t0 = TickCount(0);
		DZS_Json root = RootOf(chunks);
		if (!root)
		{
			res.m_Error = "unreadable";
			return res;
		}
		string err;
		DZS_PatchProject pp = DZS_PatchProject.FromJson(root, err);
		if (!pp)
		{
			res.m_Error = err;
			return res;
		}
		string slug = pp.m_Slug;
		if (slug == "")
			slug = "project";
		string mapKey = MapOf(root);
		if (DZS_Maps.WorldOf(mapKey) == "")
			mapKey = loadedMap;
		res.m_Base = BaseOf(root);
		string dir = DirOf(folder);
		if (DZS_Files.IsFolder(dir))
			DZS_Files.DeleteTree(dir);
		DZS_Files.MakeDirs(dir);
		res.m_Dir = dir;
		map<string, string> paths = new map<string, string>();
		int groups = 0;
		foreach (DZS_PatchBlock b : pp.m_Blocks)
		{
			if (b.m_Kind != "spawnerGroup")
				continue;
			array<string> text = GroupText(b);
			string rel = SpawnerPath(slug, b.m_Id);
			string full = dir + "\\" + rel;
			full.Replace("/", "\\");
			DZS_Files.MakeDirs(full.Substring(0, full.LastIndexOf("\\")));
			DZS_Files.Write(full, text);
			paths.Set(b.m_Id, rel);
			groups++;
		}
		res.m_Manifest = Manifest(root, mapKey, paths);
		Loot(root, pp, mapKey, dir, res, issues);
		DZS_Log.Info("work folder " + dir + ": " + groups + " group file(s), base " + res.m_Base + ", " + res.m_Origin.m_OwnTypes.Count() + " own loot type(s), " + res.m_Origin.m_OwnRows.Count() + " own row(s) in " + ((TickCount(0) - t0) / 10000) + " ms");
		return res;
	}

	// The spawner file of a group as the editor reads and writes it: the block's objects as they stand (the spawner
	// format's own entries, enableCEPersistency 0 or 1), not the server's form the patcher writes.
	static array<string> GroupText(DZS_PatchBlock b)
	{
		DZS_Json root = DZS_Json.Obj();
		DZS_Json objs = b.m_Data.Get("objects");
		if (!objs || !objs.IsArr())
			objs = DZS_Json.Arr();
		root.Set("Objects", objs);
		return DZS_ProjectFile.Text(root);
	}

	// The project file of version 1 for the open path: the editor part with every group's block as its file
	// ('paths': block id -> relative file; a block without a path keeps "<id>.json").
	static array<string> Manifest(DZS_Json root, string mapKey, map<string, string> paths)
	{
		string name = "";
		string viewText = "";
		DZS_Json ed = root.Get("editor");
		if (ed && ed.IsObj())
		{
			DZS_Json prov = ed.Get("provider");
			if (prov && prov.IsObj())
				name = prov.GetStr("name");
			DZS_Json view = ed.Get("view");
			if (view && view.IsObj())
				viewText = TextOf(view, 1);
		}
		DZS_Json proj = ProjectOf(root);
		if (!proj)
		{
			proj = DZS_Json.Obj();
			DZS_Json loot = DZS_Json.Obj();
			DZS_Json on = DZS_Json.Boolean(HasLoot(root));
			loot.Set("enabled", on);
			proj.Set("loot", loot);
		}
		DZS_Json part = PartV1(proj, paths);
		Unlisted(root, part, paths);
		string projText = TextOf(part, 1);
		return DZS_ProjectFile.Compose(name, mapKey, projText, viewText);
	}

	// Spawner blocks the editor part names no group for (a project file written elsewhere): a group entry each, named
	// after the block.
	protected static void Unlisted(DZS_Json root, DZS_Json part, map<string, string> paths)
	{
		DZS_Json groups = part.Get("groups");
		if (!groups || !groups.IsArr())
		{
			groups = DZS_Json.Arr();
			part.Set("groups", groups);
		}
		array<string> files = new array<string>();
		array<string> colors = new array<string>();
		for (int g = 0; g < groups.Count(); g++)
		{
			DZS_Json ge = groups.At(g);
			if (!ge.IsObj())
				continue;
			files.Insert(ge.GetStr("file"));
			colors.Insert(ge.GetStr("color"));
		}
		DZS_Json blocks = root.Get("blocks");
		if (!blocks || !blocks.IsArr())
			return;
		for (int i = 0; i < blocks.Count(); i++)
		{
			DZS_Json b = blocks.At(i);
			if (!b.IsObj() || b.GetStr("kind") != "spawnerGroup")
				continue;
			string rel;
			if (!paths.Find(b.GetStr("id"), rel) || files.Find(rel) >= 0)
				continue;
			DZS_Json data = b.Get("data");
			string gname = b.GetStr("id");
			int count = 0;
			if (data && data.IsObj())
			{
				if (data.GetStr("name") != "")
					gname = data.GetStr("name");
				DZS_Json objs = data.Get("objects");
				if (objs && objs.IsArr())
					count = objs.Count();
			}
			string color = DZS_Palette.Next(colors);
			colors.Insert(color);
			files.Insert(rel);
			array<ref DZS_PFObject> none = new array<ref DZS_PFObject>();
			string text = DZS_ProjectFile.GroupEntry(rel, gname, color, false, false, count, none, "[]");
			string err;
			DZS_Json entry = DZS_Json.ParseText(text, err);
			if (entry)
				groups.Add(entry);
		}
	}

	// The project part with "block" replaced by "file" and the loot part in the form of version 1.
	static DZS_Json PartV1(DZS_Json proj, map<string, string> paths)
	{
		DZS_Json part = proj.Clone();
		DZS_Json groups = part.Get("groups");
		if (groups && groups.IsArr())
		{
			for (int i = 0; i < groups.Count(); i++)
			{
				DZS_Json g = groups.At(i);
				if (!g.IsObj() || !g.Has("block"))
					continue;
				string id = g.GetStr("block");
				string rel;
				if (!paths.Find(id, rel))
					rel = id + ".json";
				DZS_Json fresh = DZS_Json.Obj();
				for (int k = 0; k < g.m_Keys.Count(); k++)
				{
					string key = g.m_Keys[k];
					if (key == "block")
					{
						DZS_Json fj = DZS_Json.Str(rel);
						fresh.Set("file", fj);
						continue;
					}
					fresh.Set(key, g.Get(key));
				}
				groups.m_Items[i] = fresh;
			}
		}
		DZS_Json loot = part.Get("loot");
		if (!loot || !loot.IsObj())
		{
			loot = DZS_Json.Obj();
			DZS_Json off = DZS_Json.Boolean(false);
			loot.Set("enabled", off);
			part.Set("loot", loot);
		}
		if (!loot.Has("hidden"))
		{
			DZS_Json h = DZS_Json.Boolean(false);
			loot.Set("hidden", h);
		}
		if (!loot.Has("locked"))
		{
			DZS_Json l = DZS_Json.Boolean(false);
			loot.Set("locked", l);
		}
		DZS_Json none = DZS_Json.Null();
		loot.Set("projectId", none);
		DZS_Json empty = DZS_Json.Arr();
		loot.Set("writtenTypes", empty);
		return part;
	}

	static string TextOf(DZS_Json v, int depth)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		DZS_TextForm.Value(w, v, depth);
		array<string> chunks = w.Finish();
		return DZS_Files.Join(chunks);
	}

	// The loot files of the base with the project's loot blocks and changes applied, and the project's loot origin.
	protected static void Loot(DZS_Json root, DZS_PatchProject pp, string mapKey, string dir, DZS_WorkResult res, DZS_Issues issues)
	{
		string baseDir = UsableBase(res.m_Base, mapKey, issues);
		if (baseDir == "")
		{
			if (NeedsBase(root))
				issues.Add(DZS_Issue.WARNING, "Basis des Projekts fehlt — Loot lässt sich erst nach dem Laden der Vanilla-Mission bearbeiten", "The project's base is missing — loot can be edited only after the vanilla mission is fetched");
			return;
		}
		if (!CopyLoot(baseDir, mapKey, dir))
		{
			if (NeedsBase(root))
				issues.Add(DZS_Issue.WARNING, "Die Basis hat keine Loot-Dateien (mapgroupproto.xml, mapgrouppos.xml)", "The base has no loot files (mapgroupproto.xml, mapgrouppos.xml)");
			return;
		}
		DZS_PatchProject lp = new DZS_PatchProject();
		lp.m_Map = pp.m_Map;
		lp.m_Slug = pp.m_Slug;
		array<string> types = new array<string>();
		foreach (DZS_PatchBlock b : pp.m_Blocks)
		{
			if (b.m_Kind.IndexOf("loot") != 0)
				continue;
			lp.m_Blocks.Insert(b);
			if (b.m_Kind == "lootType")
				AddType(types, b.m_Data.GetStr("name"));
			else if (b.m_Kind == "lootRow")
				AddRow(res.m_Origin, b.m_Data);
			else if (b.m_Parent)
				AddType(types, b.m_Parent.GetStr("group"));
		}
		foreach (DZS_PatchChange c : pp.m_Changes)
		{
			if (c.m_Block.IndexOf("loot") != 0)
				continue;
			lp.m_Changes.Insert(c);
			if (c.m_Block == "lootType")
				AddType(types, c.m_Key.GetStr("name"));
			else if (c.m_Block != "lootRow")
				AddType(types, c.m_Key.GetStr("group"));
		}
		if (lp.m_Blocks.Count() + lp.m_Changes.Count() == 0)
			return;
		DZS_Patcher p = DZS_Patcher.Run(lp, baseDir);
		foreach (string rel : p.m_OutPaths)
		{
			string low = DZS_PatchText.Lower(rel);
			if (low != DZS_LootFiles.PROTO && low != DZS_LootFiles.POS)
				continue;
			array<string> data = p.m_Out.Get(rel);
			DZS_Files.Write(dir + "\\" + low, data);
		}
		int bad = 0;
		foreach (DZS_PatchEntry e : p.m_Report.m_Entries)
		{
			if (e.m_Status == "invalid" || e.m_Status == "ambiguous" || e.m_Status == "collision" || e.m_Status == "orphaned")
				bad++;
		}
		if (bad > 0)
		{
			string de = bad.ToString() + DZS_Issues.Plural(bad, " Loot-Änderung passt", " Loot-Änderungen passen") + " nicht zur Basis und gilt nicht";
			string en = bad.ToString() + DZS_Issues.Plural(bad, " loot change does", " loot changes do") + " not fit the base and do not apply";
			issues.Add(DZS_Issue.WARNING, de, en);
		}
		if (types.Count() == 0)
			return;
		DZS_PatchFiles look = new DZS_PatchFiles(baseDir);
		string protoReal = look.Resolve(DZS_LootFiles.PROTO);
		DZS_LootFiles basis = DZS_LootFiles.ReadBase(baseDir + "\\" + protoReal, dir + "\\" + DZS_LootFiles.LIMITS);
		foreach (string t : types)
		{
			string orig = "";
			if (basis.m_Error == "")
				orig = basis.SourceOf(t);
			res.m_Origin.m_OwnTypes.Insert(t);
			res.m_Origin.m_Origs.Insert(orig);
		}
	}

	protected static void AddType(array<string> types, string name)
	{
		if (name == "")
			return;
		string low = DZS_PatchText.Lower(name);
		foreach (string t : types)
		{
			if (DZS_PatchText.Lower(t) == low)
				return;
		}
		types.Insert(name);
	}

	// A row as the patcher writes it: the name, the position with six decimals.
	protected static void AddRow(DZS_LootOrigin o, DZS_Json data)
	{
		DZS_PField f = new DZS_PField("@pos", DZS_PField.VEC3, true);
		string err;
		string pos = DZS_PatcherBase.Render(f, data.Get("pos"), err);
		if (err != "")
			return;
		o.m_OwnRows.Set(data.GetStr("name") + "\n" + pos, true);
	}
}
