// The open project as a building-block project (format version 2): every spawner group an own spawner block, every
// loot edit changes against the loot type as it was before the project, every object with loot an own loot row.
// The result applies to any mission folder of the map.
class DZS_ProjectV2
{
	// The project's slug: lower-case letters, digits and dashes from its name.
	static string Slug(string name)
	{
		string low = DZS_PatchText.Lower(name);
		string ok = "abcdefghijklmnopqrstuvwxyz0123456789";
		string r = "";
		bool dash = false;
		for (int i = 0; i < low.Length(); i++)
		{
			string c = low.Get(i);
			if (ok.Contains(c))
			{
				if (dash && r != "")
					r += "-";
				r += c;
				dash = false;
			}
			else
				dash = true;
		}
		if (r == "")
			r = "project";
		return r;
	}

	// A block id from a text: letters, digits, '-' and '_', the rest as '-'.
	static string IdPart(string t)
	{
		string ok = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
		string r = "";
		for (int i = 0; i < t.Length(); i++)
		{
			string c = t.Get(i);
			if (ok.Contains(c))
				r += c;
			else
				r += "-";
		}
		return r;
	}

	// The block id of a spawner group from its file's stem: a stem that already is a block id stays as it is, so a group
	// read from the working folder keeps its id at the next save.
	static string GroupId(string stem)
	{
		string low = DZS_PatchText.Lower(stem);
		if (low.IndexOf("g-") == 0)
			return IdPart(low);
		return "g-" + IdPart(low);
	}

	static string StemOf(string file)
	{
		string stem = file;
		int slash = stem.LastIndexOf("/");
		if (slash >= 0)
			stem = stem.Substring(slash + 1, stem.Length() - slash - 1);
		int dot = stem.LastIndexOf(".");
		if (dot > 0)
			stem = stem.Substring(0, dot);
		return stem;
	}

	// Block id per spawner group (group element id -> block id), unique; groups whose stem is a block id already
	// (read from a working folder) take theirs first.
	static map<string, string> GroupIds(DZS_Instance inst)
	{
		map<string, string> r = new map<string, string>();
		array<string> used = new array<string>();
		array<string> groups = inst.GetGroupIds();
		for (int pass = 0; pass < 2; pass++)
		{
			foreach (string gid : groups)
			{
				if (inst.GetKind(gid) != DZS_KIND_GROUP || r.Contains(gid))
					continue;
				string format, file, gname;
				inst.GetField(gid, DZS_Obj.GROUP_FORMAT, format);
				inst.GetField(gid, DZS_Obj.GROUP_FILE, file);
				inst.GetField(gid, DZS_Obj.GROUP_NAME, gname);
				if (format != "" && format != "spawner")
					continue;
				string stem = StemOf(file);
				if (stem == "")
					stem = gname;
				bool own = DZS_PatchText.Lower(stem).IndexOf("g-") == 0;
				if ((pass == 0) != own)
					continue;
				string id = GroupId(stem);
				string unique = id;
				int n = 2;
				while (used.Find(unique) >= 0)
				{
					unique = id + "-" + n;
					n++;
				}
				used.Insert(unique);
				r.Set(gid, unique);
			}
		}
		return r;
	}

	// format, version, map, slug and empty block and change lists.
	static DZS_Json Empty(string mapKey, string name)
	{
		DZS_Json root = DZS_Json.Obj();
		DZS_Json fmt = DZS_Json.Str(DZS_PatchProject.FORMAT);
		root.Set("format", fmt);
		DZS_Json ver = DZS_Json.Int(DZS_PatchProject.VERSION);
		root.Set("version", ver);
		DZS_Json mp = DZS_Json.Str(mapKey);
		root.Set("map", mp);
		DZS_Json slug = DZS_Json.Str(Slug(name));
		root.Set("slug", slug);
		DZS_Json blocks = DZS_Json.Arr();
		root.Set("blocks", blocks);
		DZS_Json changes = DZS_Json.Arr();
		root.Set("changes", changes);
		return root;
	}

	// noLoot: the loot of the project is left out (a copy saved with loot off).
	static DZS_Json Build(DZS_Kernel kernel, DZS_Instance inst, string name, string mapKey, bool noLoot = false)
	{
		DZS_Json root = Empty(mapKey, name);
		DZS_Json blocks = root.Get("blocks");
		DZS_Json changes = root.Get("changes");
		DZS_EntityExts.Resolve(inst);
		Groups(kernel, inst, blocks);
		if (!noLoot)
			Loot(kernel, inst, blocks, changes);
		Sections(inst, blocks, changes);
		// the extensions of blocks, changes and list elements as the project file had them (or they were edited)
		DZS_EntityExts.Attach(root);
		return root;
	}

	// Layers that keep their part as a building-block fragment {blocks, changes} in project.plugins (Spawns …).
	protected static void Sections(DZS_Instance inst, DZS_Json blocks, DZS_Json changes)
	{
		array<ref DZS_ProjectSection> sections = DZS_ProjectSections.All();
		foreach (DZS_ProjectSection s : sections)
		{
			string text = s.Text(inst);
			if (text == "")
				continue;
			string err;
			DZS_Json frag = DZS_Json.ParseText(text, err);
			if (!frag || !frag.IsObj())
				continue;
			DZS_Json fb = frag.Get("blocks");
			if (fb && fb.IsArr())
			{
				for (int i = 0; i < fb.Count(); i++)
				{
					DZS_Json one = fb.At(i);
					blocks.Add(one);
				}
			}
			DZS_Json fc = frag.Get("changes");
			if (fc && fc.IsArr())
			{
				for (int k = 0; k < fc.Count(); k++)
				{
					DZS_Json two = fc.At(k);
					changes.Add(two);
				}
			}
		}
	}

	// ---- spawner groups ----

	protected static void Groups(DZS_Kernel kernel, DZS_Instance inst, DZS_Json blocks)
	{
		DZS_SpawnerFormat sf = DZS_SpawnerFormat.Cast(kernel.GetFormat("spawner"));
		if (!sf)
			return;
		float worldSize = 0;
		if (GetGame().GetWorld())
			worldSize = GetGame().GetWorld().GetWorldSize();
		map<string, string> blockIds = GroupIds(inst);
		array<string> groups = inst.GetGroupIds();
		foreach (string gid : groups)
		{
			string unique;
			if (!blockIds.Find(gid, unique))
				continue;
			string gname;
			inst.GetField(gid, DZS_Obj.GROUP_NAME, gname);
			DZS_Json objects = DZS_Json.Arr();
			DZS_Issues issues = new DZS_Issues();
			array<string> ids = inst.GetChildren(gid);
			foreach (string oid : ids)
			{
				if (inst.GetKind(oid) != DZS_KIND_OBJECT)
					continue;
				DZS_WriteResult res = new DZS_WriteResult();
				string text = sf.Entry(inst, oid, issues, worldSize, res);
				string err;
				DZS_Json entry = DZS_Json.ParseText(text, err);
				if (!entry || err != "")
					continue;
				DZS_Json obj = DZS_Json.Obj();
				CopyMember(entry, obj, "name");
				CopyMember(entry, obj, "pos");
				CopyMember(entry, obj, "ypr");
				CopyMember(entry, obj, "scale");
				CopyMember(entry, obj, "enableCEPersistency");
				CopyMember(entry, obj, "customString");
				DZS_EntityExts.PutOn(obj, DZS_EntityExts.OfObject(oid));
				objects.Add(obj);
			}
			DZS_Json data = DZS_Json.Obj();
			DZS_Json gn = DZS_Json.Str(gname);
			data.Set("name", gn);
			data.Set("objects", objects);
			DZS_Json b = Block(unique, "spawnerGroup", data);
			DZS_EntityExts.PutOn(b, DZS_EntityExts.OfGroup(gid));
			blocks.Add(b);
		}
	}

	protected static void CopyMember(DZS_Json from, DZS_Json to, string key)
	{
		DZS_Json v = from.Get(key);
		if (v)
			to.Set(key, v);
	}

	protected static DZS_Json Block(string id, string kind, DZS_Json data)
	{
		DZS_Json b = DZS_Json.Obj();
		DZS_Json ij = DZS_Json.Str(id);
		b.Set("id", ij);
		DZS_Json kj = DZS_Json.Str(kind);
		b.Set("kind", kj);
		DZS_Json none = DZS_Json.Null();
		b.Set("parent", none);
		b.Set("data", data);
		return b;
	}

	// ---- loot ----

	protected static void Loot(DZS_Kernel kernel, DZS_Instance inst, DZS_Json blocks, DZS_Json changes)
	{
		DZS_LootPlugin lp = DZS_LootPlugin.Cast(kernel.GetPlugin("loot"));
		if (!lp || !lp.IsOn())
			return;
		array<string> types = lp.EditedTypes(inst);
		foreach (string type : types)
		{
			string start;
			DZS_LootEEntry from = null;
			if (lp.StartText(type, start))
				from = DZS_LootPlugin.FromSource(start);
			DZS_LootEEntry to = lp.Effective(inst, type);
			if (DZS_LootBlocks.Canon(from) == DZS_LootBlocks.Canon(to))
				continue;
			DZS_LootEEntry src = lp.OpenEntry(type);
			DZS_LootBlocks.Diff(from, to, src, "l-" + IdPart(DZS_PatchText.Lower(type)), changes, blocks);
		}
		array<string> objs = lp.ClassObjects(inst);
		int rows = 0;
		foreach (string oid : objs)
		{
			if (lp.SettingOf(inst, oid) == DZS_LootPlugin.NONE)
				continue;
			string oname, pos, ypr;
			inst.GetField(oid, DZS_Obj.NAME, oname);
			inst.GetField(oid, DZS_Obj.POS, pos);
			inst.GetField(oid, DZS_Obj.YPR, ypr);
			vector p = DZS_Obj.QuickVec(pos);
			vector a = DZS_Obj.QuickVec(ypr);
			string rowText = DZS_LootFiles.RowText(oname, p, a, "");
			array<string> chunks = DZS_Files.Chunk(rowText);
			DZS_XmlDoc doc = DZS_Xml.Parse(chunks);
			if (!doc.m_Root)
				continue;
			DZS_Json data = DZS_Json.Obj();
			DZS_Json nj = DZS_Json.Str(doc.m_Root.Get("name"));
			data.Set("name", nj);
			DZS_Json pj = DZS_Json.Str(doc.m_Root.Get("pos"));
			data.Set("pos", pj);
			DZS_Json rj = DZS_Json.Str(doc.m_Root.Get("rpy"));
			data.Set("rpy", rj);
			DZS_Json aj = DZS_Json.Num(doc.m_Root.Get("a"));
			data.Set("a", aj);
			rows++;
			DZS_Json b = Block("row-" + rows, "lootRow", data);
			blocks.Add(b);
		}
	}

	// ---- applying ----

	// Patches 'baseDir' with the project and writes the changed files and report.json into 'outDir' (never in place).
	static DZS_Patcher Apply(DZS_Json project, string baseDir, string outDir, out string err)
	{
		DZS_PatchProject pp = DZS_PatchProject.FromJson(project, err);
		if (!pp)
			return null;
		DZS_Patcher p = DZS_Patcher.Run(pp, baseDir);
		DZS_Files.MakeDirs(outDir);
		foreach (string rel : p.m_OutPaths)
		{
			string full = outDir + "\\" + rel;
			full.Replace("/", "\\");
			int slash = full.LastIndexOf("\\");
			DZS_Files.MakeDirs(full.Substring(0, slash));
			array<string> data = p.m_Out.Get(rel);
			DZS_Files.Write(full, data);
		}
		array<string> report = p.m_Report.Write();
		DZS_Files.Write(outDir + "\\report.json", report);
		return p;
	}
}
