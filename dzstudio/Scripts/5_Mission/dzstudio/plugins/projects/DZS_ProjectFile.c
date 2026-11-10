// The project file: <id>.dzstudio.json in the project list, project.dzstudio.json inside a folder (a backup, the
// working folder). Version 2 is the saved form of every project, local, a provider's and dzbot's mods and placements:
// the building blocks ({format, version, map, slug, blocks, changes}, DZS_ProjectV2) plus dzStudio's editor extension
// (EDITOR_EXT) with what only dzStudio needs: provider part, base, project part (groups by block id, loot switches,
// plugin parts) and view part (docs/extensions/dzstudio.editor.md). Version 1 (provider part, project part and view
// part): the project file the working folder composes for the open path, and projects of older versions.
class DZS_Maps
{
	static array<string> Keys()
	{
		array<string> k = {"chernarus", "livonia", "sakhal", "badlands"};
		return k;
	}

	static string WorldOf(string key)
	{
		if (key == "chernarus") return "ChernarusPlus";
		if (key == "livonia") return "Enoch";
		if (key == "sakhal") return "Sakhal";
		if (key == "badlands") return "Nasdara";
		return "";
	}

	// The map as the project format writes it: the world name in lower case ("chernarusplus", "enoch", "nasdara").
	static string FormatKey(string key)
	{
		string w = WorldOf(key);
		if (w == "")
			return key;
		w.ToLower();
		return w;
	}

	// dzStudio's map key ("chernarus") of a project file's map, which is the world name in lower case; a key itself is
	// taken as it is. "" when the map is unknown.
	static string FromFormat(string mapName)
	{
		string low = mapName;
		low.ToLower();
		if (WorldOf(low) != "")
			return low;
		array<string> keys = Keys();
		foreach (string k : keys)
		{
			string kw = WorldOf(k);
			kw.ToLower();
			if (kw == low)
				return k;
		}
		return "";
	}

	static string KeyOfWorld(string world)
	{
		array<string> keys = DZS_Maps.Keys();
		foreach (string k : keys)
		{
			if (DZS_Maps.WorldOf(k) == world)
				return k;
		}
		return "";
	}

	static string Display(string key)
	{
		if (key == "chernarus") return "Chernarus";
		if (key == "livonia") return "Livonia";
		if (key == "sakhal") return "Sakhal";
		if (key == "badlands") return "Nasdara";
		return key;
	}

	// Installed and owned (Sakhal and Nasdara are DLCs). -dzsMapMissing=sakhal simulates a missing map.
	static bool Available(string key)
	{
		string missing;
		if (GetCLIParam("dzsMapMissing", missing) && missing.Contains(key))
			return false;
		string world = DZS_Maps.WorldOf(key);
		if (world == "" || !GetGame().ConfigIsExisting("CfgWorlds " + world))
			return false;
		ContentDLC dlc = GetGame().GetContentDLCService();
		if (dlc)
		{
			if (key == "sakhal" && !dlc.IsDLCInstalled(EDLCId.FROSTLINE))
				return false;
			if (key == "badlands" && !dlc.IsDLCInstalled(EDLCId.BADLANDS))
				return false;
		}
		return true;
	}

	// Fixed sizes of the four maps (measured in DayZ 1.30), independent of the loaded world.
	static float Size(string key)
	{
		if (key == "livonia")
			return 12800;
		if (key == "badlands")
			return 16384;
		return 15360;
	}
}

class DZS_Palette
{
	static array<string> Colors()
	{
		array<string> c = {"#60A5FA", "#FB923C", "#2DD4BF", "#F472B6", "#A3E635", "#FDE047", "#94A3B8", "#E879F9"};
		return c;
	}

	// First colour of the row no group carries; if all are used, the one at (count mod 8) + 1.
	static string Next(array<string> used)
	{
		array<string> row = DZS_Palette.Colors();
		foreach (string c : row)
		{
			bool taken = false;
			foreach (string u : used)
			{
				string a = u;
				a.ToUpper();
				if (a == c)
					taken = true;
			}
			if (!taken)
				return c;
		}
		return row[used.Count() % 8];
	}

	static bool Valid(string c)
	{
		if (c.Length() != 7 || c.Get(0) != "#")
			return false;
		string hex = "0123456789abcdefABCDEF";
		for (int i = 1; i < 7; i++)
		{
			if (!hex.Contains(c.Get(i)))
				return false;
		}
		return true;
	}

	static int ToARGB(string c)
	{
		string hex = "0123456789ABCDEF";
		string u = c;
		u.ToUpper();
		string h1 = u.Get(1);
		string h2 = u.Get(2);
		string h3 = u.Get(3);
		string h4 = u.Get(4);
		string h5 = u.Get(5);
		string h6 = u.Get(6);
		int r = hex.IndexOf(h1) * 16 + hex.IndexOf(h2);
		int g = hex.IndexOf(h3) * 16 + hex.IndexOf(h4);
		int b = hex.IndexOf(h5) * 16 + hex.IndexOf(h6);
		return ARGB(255, r, g, b);
	}
}

class DZS_PFObject : Managed
{
	int m_Index;
	string m_Name;
	float m_X;
	float m_Y;
	float m_Z;
	bool m_Hidden;
	bool m_Locked;
}

class DZS_PFGroup : Managed
{
	string m_File;
	// version 2: the spawner block of the group
	string m_Block;
	string m_Name;
	string m_Color;
	bool m_Hidden;
	bool m_Locked;
	int m_ObjectCount = -1;
	ref array<ref DZS_PFObject> m_Objects;
	string m_Plugins;
	string m_RawEntry;
	string m_Dropped;

	void DZS_PFGroup()
	{
		m_Objects = new array<ref DZS_PFObject>();
	}
}

class DZS_PFData : Managed
{
	// ok, missing, unreadable, newer
	string m_Status;
	string m_Name;
	string m_MapKey;
	string m_MapRaw;
	bool m_HasProject;
	ref array<ref DZS_PFGroup> m_Groups;
	bool m_LootEnabled;
	bool m_LootHidden;
	bool m_LootLocked;
	ref array<string> m_WrittenTypes;
	// original text per written type ("" = there was none before the project)
	ref array<string> m_WrittenOrig;
	string m_ProjectId;
	bool m_ProjectIdBad;
	string m_Plugins;
	bool m_HasCamera;
	vector m_CamPos;
	float m_Heading;
	float m_Pitch;
	ref DZS_Issues m_Issues;
	string m_RawProject;
	string m_RawView;
	bool m_V2;
	// version 2: the project's base ("vanilla" or "mission:<folder>")
	string m_Base;

	void DZS_PFData()
	{
		m_Groups = new array<ref DZS_PFGroup>();
		m_WrittenTypes = new array<string>();
		m_WrittenOrig = new array<string>();
		m_Issues = new DZS_Issues();
		m_Plugins = "[]";
	}
}

class DZS_ProjectFile
{
	static const string NAME = "project.dzstudio.json";
	static const int VERSION = 1;
	static const int VERSION2 = 2;
	// dzStudio's own part of a version 2 file, a built-in extension: the URL of its spec
	static const string EDITOR_EXT = "https://developers.dzbot.de/specs/dzstudio/editor/1.json";

	// The editor part of a version 2 file: the data of its extension EDITOR_EXT. Null when there is none.
	static DZS_Json EditorOf(DZS_Json root)
	{
		if (!root || !root.IsObj())
			return null;
		DZS_Json list = root.Get("extensions");
		if (list && list.IsObj())
		{
			DZS_Json entry = list.Get(EDITOR_EXT);
			if (entry && entry.IsObj())
			{
				DZS_Json data = entry.Get("data");
				if (data && data.IsObj())
					return data;
			}
		}
		return null;
	}

	static DZS_SrcNode EditorNodeOf(DZS_SrcNode root)
	{
		if (!root || !root.IsObj())
			return null;
		DZS_SrcNode list = root.Get("extensions");
		if (list && list.IsObj())
		{
			DZS_SrcNode entry = list.Get(EDITOR_EXT);
			if (entry && entry.IsObj())
			{
				DZS_SrcNode data = entry.Get("data");
				if (data && data.IsObj())
					return data;
			}
		}
		return null;
	}

	// Writes the editor part into its extension (added after the other extensions when the file has none yet).
	static void SetEditor(DZS_Json root, DZS_Json ed)
	{
		DZS_Json list = root.Get("extensions");
		if (!list || !list.IsObj())
		{
			list = DZS_Json.Obj();
			root.Set("extensions", list);
		}
		DZS_Json entry = list.Get(EDITOR_EXT);
		if (!entry || !entry.IsObj())
		{
			entry = DZS_Json.Obj();
			list.Set(EDITOR_EXT, entry);
		}
		if (entry.Get("data") != ed)
			entry.Set("data", ed);
	}

	static bool IsNeverGroup(string relPath)
	{
		string low = relPath;
		low.ToLower();
		int slash = low.LastIndexOf("/");
		string leaf = low.Substring(slash + 1, low.Length() - slash - 1);
		if (low.IndexOf("vanilla/") == 0)
			return true;
		return leaf == NAME || leaf == DZS_LegacyNames.PROJECT_FILE || leaf == "cfggameplay.json" || leaf == "mapgroupproto.xml" || leaf == "mapgrouppos.xml" || leaf == "cfglimitsdefinition.xml";
	}

	// ---- reading ----

	static DZS_PFData Read(array<string> chunks)
	{
		DZS_PFData d = new DZS_PFData();
		if (!chunks)
		{
			d.m_Status = "missing";
			return d;
		}
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc.m_Root || !doc.m_Root.IsObj())
		{
			d.m_Status = "unreadable";
			return d;
		}
		DZS_SrcNode root = doc.m_Root;
		DZS_SrcNode fmt = root.Get("format");
		if (!fmt || !fmt.IsStr() || fmt.m_Str != "dzstudio-project")
		{
			d.m_Status = "unreadable";
			return d;
		}
		DZS_SrcNode ver = root.Get("version");
		if (!ver || !ver.IsNum() || ver.m_Str.Contains(".") || ver.m_Str.Contains("e") || ver.m_Str.Contains("-") || ver.m_Str.ToInt() < 1)
		{
			d.m_Status = "unreadable";
			return d;
		}
		if (ver.m_Str.Length() > 6 || ver.m_Str.ToInt() > VERSION2)
		{
			d.m_Status = "newer";
			return d;
		}
		d.m_Status = "ok";
		if (ver.m_Str.ToInt() == VERSION2)
		{
			ReadV2(doc, root, d);
			return d;
		}
		DupWarnings(root, "", d.m_Issues);
		array<string> known0 = {"format", "version", "provider", "project", "view"};
		UnknownHints(root, "", known0, d.m_Issues);
		DZS_SrcNode prov = root.Get("provider");
		if (prov && prov.IsObj())
		{
			DupWarnings(prov, "provider.", d.m_Issues);
			array<string> known1 = {"name", "map"};
		UnknownHints(prov, "provider.", known1, d.m_Issues);
			DZS_SrcNode nm = prov.Get("name");
			if (nm && nm.IsStr() && DZS_Names.ValidProjectName(nm.m_Str))
				d.m_Name = nm.m_Str;
			else
				d.m_Issues.Add(DZS_Issue.WARNING, "Projektname fehlt oder ist ungültig — der Ordnername gilt", "Project name missing or invalid — the folder name counts");
			DZS_SrcNode mp = prov.Get("map");
			if (mp && mp.IsStr())
			{
				d.m_MapRaw = mp.m_Str;
				d.m_MapKey = DZS_Maps.FromFormat(mp.m_Str);
			}
			else
				d.m_Issues.Add(DZS_Issue.WARNING, "Karte fehlt in der Projektdatei — die geladene Karte gilt", "Map missing in the project file — the loaded map counts");
		}
		else
			d.m_Issues.Add(DZS_Issue.WARNING, "Angaben des Anbieters fehlen — Name und Karte aus Ordner und Karte", "Provider part missing — name and map from folder and map");
		DZS_SrcNode proj = root.Get("project");
		if (proj && proj.IsObj())
		{
			d.m_HasProject = true;
			d.m_RawProject = doc.Raw(proj);
			ReadProject(doc, proj, d);
		}
		DZS_SrcNode view = root.Get("view");
		if (view && view.IsObj())
		{
			d.m_RawView = doc.Raw(view);
			ReadView(view, d);
		}
		return d;
	}

	// The editor part of a version 2 file; the blocks are read by the working folder (DZS_WorkFolder).
	protected static void ReadV2(DZS_SrcDoc doc, DZS_SrcNode root, DZS_PFData d)
	{
		d.m_V2 = true;
		d.m_Base = "vanilla";
		DZS_SrcNode rm = root.Get("map");
		if (rm && rm.IsStr())
		{
			d.m_MapRaw = rm.m_Str;
			d.m_MapKey = DZS_Maps.FromFormat(rm.m_Str);
		}
		DZS_SrcNode ed = EditorNodeOf(root);
		if (!ed)
			return;
		DZS_SrcNode prov = ed.Get("provider");
		if (prov && prov.IsObj())
		{
			DZS_SrcNode nm = prov.Get("name");
			if (nm && nm.IsStr() && DZS_Names.ValidProjectName(nm.m_Str))
				d.m_Name = nm.m_Str;
		}
		DZS_SrcNode bs = ed.Get("base");
		if (bs && bs.IsStr() && bs.m_Str != "")
			d.m_Base = bs.m_Str;
		DZS_SrcNode proj = ed.Get("project");
		if (proj && proj.IsObj())
		{
			d.m_HasProject = true;
			d.m_RawProject = doc.Raw(proj);
			ReadProject(doc, proj, d);
		}
		DZS_SrcNode view = ed.Get("view");
		if (view && view.IsObj())
		{
			d.m_RawView = doc.Raw(view);
			ReadView(view, d);
		}
	}

	// Whether the text is a project file of version 2.
	static bool IsV2(array<string> chunks)
	{
		if (!chunks)
			return false;
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc.m_Root || !doc.m_Root.IsObj())
			return false;
		DZS_SrcNode fmt = doc.m_Root.Get("format");
		DZS_SrcNode ver = doc.m_Root.Get("version");
		return fmt && fmt.IsStr() && fmt.m_Str == "dzstudio-project" && ver && ver.IsNum() && ver.m_Str == "2";
	}

	protected static void DupWarnings(DZS_SrcNode obj, string path, DZS_Issues issues)
	{
		array<string> seen = new array<string>();
		foreach (string k : obj.m_Keys)
		{
			if (obj.CountKey(k) > 1 && seen.Find(k) < 0)
			{
				seen.Insert(k);
				issues.Add(DZS_Issue.WARNING, "Feld „" + path + k + "“ steht mehrfach — das letzte gilt", "Field \"" + path + k + "\" appears more than once — the last one counts");
			}
		}
	}

	protected static void UnknownHints(DZS_SrcNode obj, string path, array<string> known, DZS_Issues issues)
	{
		foreach (string k : obj.m_Keys)
		{
			if (known.Find(k) < 0)
				issues.Add(DZS_Issue.HINT, "Unbekanntes Feld " + path + k + " fällt weg", "Unknown field " + path + k + " is dropped");
		}
	}

	protected static bool Bool(DZS_SrcNode n, out bool value)
	{
		if (n && n.IsBool())
		{
			value = n.m_Bool;
			return true;
		}
		value = false;
		return false;
	}

	protected static void ReadProject(DZS_SrcDoc doc, DZS_SrcNode proj, DZS_PFData d)
	{
		DupWarnings(proj, "project.", d.m_Issues);
		array<string> known2 = {"groups", "loot", "plugins"};
		UnknownHints(proj, "project.", known2, d.m_Issues);
		DZS_SrcNode groups = proj.Get("groups");
		if (!groups || !groups.IsArr())
			d.m_Issues.Add(DZS_Issue.WARNING, "project.groups fehlt oder ist keine Liste", "project.groups is missing or not a list");
		else
		{
			for (int i = 0; i < groups.Count(); i++)
			{
				DZS_SrcNode g = groups.m_Items[i];
				string path = "project.groups[" + i + "].";
				if (!g.IsObj())
				{
					d.m_Issues.Add(DZS_Issue.WARNING, "Gruppen-Eintrag " + i + " ist kein Objekt — fällt weg", "Group entry " + i + " is not an object — dropped");
					continue;
				}
				DupWarnings(g, path, d.m_Issues);
				array<string> known3 = {"file", "block", "name", "color", "hidden", "locked", "objectCount", "objects", "plugins"};
		UnknownHints(g, path, known3, d.m_Issues);
				DZS_PFGroup pg = new DZS_PFGroup();
				pg.m_RawEntry = doc.Raw(g);
				DZS_SrcNode f = g.Get("file");
				if (f && f.IsStr())
					pg.m_File = f.m_Str;
				DZS_SrcNode gb = g.Get("block");
				if (gb && gb.IsStr())
					pg.m_Block = gb.m_Str;
				DZS_SrcNode n = g.Get("name");
				if (n && n.IsStr())
					pg.m_Name = n.m_Str;
				else
					pg.m_Name = "";
				DZS_SrcNode c = g.Get("color");
				if (c && c.IsStr() && DZS_Palette.Valid(c.m_Str))
				{
					pg.m_Color = c.m_Str;
					pg.m_Color.ToUpper();
				}
				else
					pg.m_Color = "";
				if (!Bool(g.Get("hidden"), pg.m_Hidden))
					d.m_Issues.Add(DZS_Issue.WARNING, path + "hidden ist kein Wahrheitswert — false", path + "hidden is not a boolean — false");
				if (!Bool(g.Get("locked"), pg.m_Locked))
					d.m_Issues.Add(DZS_Issue.WARNING, path + "locked ist kein Wahrheitswert — false", path + "locked is not a boolean — false");
				DZS_SrcNode oc = g.Get("objectCount");
				if (oc && oc.IsNum() && !oc.m_Str.Contains(".") && !oc.m_Str.Contains("-") && !oc.m_Str.Contains("e") && oc.m_Str.Length() < 10)
					pg.m_ObjectCount = oc.m_Str.ToInt();
				else
					d.m_Issues.Add(DZS_Issue.WARNING, path + "objectCount ist ungültig", path + "objectCount is invalid");
				DZS_SrcNode objs = g.Get("objects");
				if (objs && objs.IsArr())
				{
					foreach (DZS_SrcNode o : objs.m_Items)
					{
						DZS_PFObject po = ReadObjectEntry(o, path, d.m_Issues);
						if (po)
							pg.m_Objects.Insert(po);
						else
							pg.m_Dropped += "x";
					}
				}
				else
					d.m_Issues.Add(DZS_Issue.WARNING, path + "objects fehlt oder ist keine Liste", path + "objects is missing or not a list");
				pg.m_Plugins = PluginEntries(doc, g.Get("plugins"));
				d.m_Groups.Insert(pg);
			}
		}
		DZS_SrcNode loot = proj.Get("loot");
		if (loot && loot.IsObj())
		{
			if (!Bool(loot.Get("enabled"), d.m_LootEnabled))
				d.m_Issues.Add(DZS_Issue.WARNING, "project.loot.enabled ist ungültig — Loot aus", "project.loot.enabled is invalid — loot off");
			Bool(loot.Get("hidden"), d.m_LootHidden);
			Bool(loot.Get("locked"), d.m_LootLocked);
			DZS_SrcNode pid = loot.Get("projectId");
			if (pid && pid.IsStr() && IsProjectId(pid.m_Str))
				d.m_ProjectId = pid.m_Str;
			else if (!pid || !pid.IsNull())
				d.m_ProjectIdBad = true;
			DZS_SrcNode wt = loot.Get("writtenTypes");
			if (wt && wt.IsArr())
			{
				array<string> lows = new array<string>();
				foreach (DZS_SrcNode t : wt.m_Items)
				{
					DZS_SrcNode tn = null;
					DZS_SrcNode to = null;
					if (t.IsObj())
					{
						tn = t.Get("name");
						to = t.Get("original");
					}
					bool okName = tn && tn.IsStr() && tn.m_Str != "";
					bool okOrig = !to || to.IsNull() || to.IsStr();
					if (!okName || !okOrig)
					{
						d.m_Issues.Add(DZS_Issue.WARNING, "Ungültiger Eintrag in writtenTypes fällt weg", "Invalid element of writtenTypes dropped");
						continue;
					}
					string lt = tn.m_Str;
					lt.ToLower();
					if (lows.Find(lt) >= 0)
					{
						d.m_Issues.Add(DZS_Issue.WARNING, "Doppelter Typ in writtenTypes: " + tn.m_Str, "Duplicate type in writtenTypes: " + tn.m_Str);
						continue;
					}
					lows.Insert(lt);
					d.m_WrittenTypes.Insert(tn.m_Str);
					string orig = "";
					if (to && to.IsStr())
						orig = to.m_Str;
					d.m_WrittenOrig.Insert(orig);
				}
			}
			else
				d.m_Issues.Add(DZS_Issue.WARNING, "project.loot.writtenTypes ist ungültig", "project.loot.writtenTypes is invalid");
		}
		else
			d.m_Issues.Add(DZS_Issue.WARNING, "project.loot fehlt — Loot aus", "project.loot is missing — loot off");
		d.m_Plugins = PluginEntries(doc, proj.Get("plugins"));
	}

	protected static DZS_PFObject ReadObjectEntry(DZS_SrcNode o, string path, DZS_Issues issues)
	{
		if (!o.IsObj())
			return null;
		DZS_SrcNode idx = o.Get("index");
		DZS_SrcNode nm = o.Get("name");
		DZS_SrcNode pos = o.Get("pos");
		if (!idx || !idx.IsNum() || idx.m_Str.Contains(".") || idx.m_Str.Contains("-") || !nm || !nm.IsStr() || !pos || !pos.IsArr() || pos.Count() != 3)
			return null;
		DZS_PFObject po = new DZS_PFObject();
		po.m_Index = idx.m_Str.ToInt();
		po.m_Name = nm.m_Str;
		for (int i = 0; i < 3; i++)
		{
			if (!pos.m_Items[i].IsNum())
				return null;
		}
		if (!DZS_F32.FromText(pos.m_Items[0].m_Str, po.m_X) || !DZS_F32.FromText(pos.m_Items[1].m_Str, po.m_Y) || !DZS_F32.FromText(pos.m_Items[2].m_Str, po.m_Z))
			return null;
		if (!Bool(o.Get("hidden"), po.m_Hidden))
			issues.Add(DZS_Issue.WARNING, path + "objects: hidden ist kein Wahrheitswert — false", path + "objects: hidden is not a boolean — false");
		if (!Bool(o.Get("locked"), po.m_Locked))
			issues.Add(DZS_Issue.WARNING, path + "objects: locked ist kein Wahrheitswert — false", path + "objects: locked is not a boolean — false");
		return po;
	}

	// Plugin data as a list of [quoted name, exact source of the value] (no plugin keeps data here yet,
	// so every entry is carried on unchanged).
	protected static string PluginEntries(DZS_SrcDoc doc, DZS_SrcNode p)
	{
		DZS_Json list = DZS_Json.Arr();
		if (p && p.IsObj())
		{
			for (int i = 0; i < p.m_Keys.Count(); i++)
			{
				if (p.CountKey(p.m_Keys[i]) > 1 && p.Get(p.m_Keys[i]) != p.m_Items[i])
					continue;
				DZS_SrcNode v = p.m_Items[i];
				DZS_Json one = DZS_Json.Arr();
				one.Add(DZS_Json.Str(p.m_Keys[i]));
				one.Add(DZS_Json.Str(doc.Slice(v.m_KeyStart, v.m_KeyEnd)));
				one.Add(DZS_Json.Str(doc.Raw(v)));
				list.Add(one);
			}
		}
		array<string> t456 = list.ToChunks();
		return DZS_SpawnerFormat.Join(t456);
	}

	protected static void ReadView(DZS_SrcNode view, DZS_PFData d)
	{
		DZS_SrcNode cam = view.Get("camera");
		if (!cam || !cam.IsObj())
			return;
		DZS_SrcNode pos = cam.Get("pos");
		DZS_SrcNode h = cam.Get("heading");
		DZS_SrcNode p = cam.Get("pitch");
		if (!pos || !pos.IsArr() || pos.Count() != 3 || !h || !h.IsNum() || !p || !p.IsNum())
			return;
		float x, y, z, hd, pt;
		for (int i = 0; i < 3; i++)
		{
			if (!pos.m_Items[i].IsNum())
				return;
		}
		if (!DZS_F32.FromText(pos.m_Items[0].m_Str, x) || !DZS_F32.FromText(pos.m_Items[1].m_Str, y) || !DZS_F32.FromText(pos.m_Items[2].m_Str, z))
			return;
		if (!DZS_F32.FromText(h.m_Str, hd) || !DZS_F32.FromText(p.m_Str, pt))
			return;
		d.m_HasCamera = true;
		d.m_CamPos = Vector(x, y, z);
		while (hd < 0)
			hd += 360;
		while (hd >= 360)
			hd -= 360;
		d.m_Heading = hd;
		d.m_Pitch = Math.Clamp(pt, -89, 89);
	}

	// ---- writing ----

	static string I(int depth) { return DZS_Json.Indent(depth); }

	static string B(bool b) { return DZS_Str.Bool(b); }

	static string ProviderPart(string name, string mapKey)
	{
		string s = I(1) + "\"provider\": {\n";
		s += I(2) + "\"name\": " + DZS_TextForm.Str(name) + ",\n";
		s += I(2) + "\"map\": " + DZS_TextForm.Str(mapKey) + "\n";
		s += I(1) + "}";
		return s;
	}

	static string ViewPart(vector pos, float heading, float pitch)
	{
		string s = "{\n";
		s += I(2) + "\"camera\": {\n";
		string p0 = DZS_F32.ToText(pos[0]);
		string p1 = DZS_F32.ToText(pos[1]);
		string p2 = DZS_F32.ToText(pos[2]);
		s += I(3) + "\"pos\": [" + p0 + ", " + p1 + ", " + p2 + "],\n";
		s += I(3) + "\"heading\": " + DZS_F32.ToText(heading) + ",\n";
		s += I(3) + "\"pitch\": " + DZS_F32.ToText(pitch) + "\n";
		s += I(2) + "}\n";
		s += I(1) + "}";
		return s;
	}

	static string Plugins(string entries, int depth)
	{
		string err;
		DZS_Json list = DZS_Json.ParseText(entries, err);
		if (!list || !list.IsArr() || list.Count() == 0)
			return "{}";
		array<string> names = new array<string>();
		map<string, int> at = new map<string, int>();
		for (int i = 0; i < list.Count(); i++)
		{
			names.Insert(list.At(i).At(0).m_Str);
			at.Set(list.At(i).At(0).m_Str, i);
		}
		names.Sort();
		string s = "{\n";
		for (int k = 0; k < names.Count(); k++)
		{
			DZS_Json e = list.At(at.Get(names[k]));
			s += I(depth + 1) + e.At(1).m_Str + ": " + e.At(2).m_Str;
			if (k < names.Count() - 1)
				s += ",";
			s += "\n";
		}
		s += I(depth) + "}";
		return s;
	}

	static bool IsProjectId(string t)
	{
		if (t.Length() != 16)
			return false;
		string hex = "0123456789abcdef";
		for (int i = 0; i < 16; i++)
		{
			string c = t.Get(i);
			if (hex.IndexOf(c) < 0)
				return false;
		}
		return true;
	}

	// Written types sorted by the lower-case name byte for byte, then by the name itself.
	static array<int> WrittenOrder(array<string> names)
	{
		array<int> order = new array<int>();
		for (int i = 0; i < names.Count(); i++)
		{
			int j = order.Count() - 1;
			order.Insert(i);
			while (j >= 0 && DZS_LocalStore.ComparePaths(names[order[j]], names[i]) > 0)
			{
				order[j + 1] = order[j];
				j--;
			}
			order[j + 1] = i;
		}
		return order;
	}

	// The project part, from "{" to its closing "}" (no comma); depends only on the project state. originals "" =
	// null; namesOnly for the dzbot manifest (no projectId, no original texts).
	static string ProjectPart(array<string> groupEntries, bool lootEnabled, bool lootHidden, bool lootLocked, string projectId, array<string> writtenTypes, array<string> originals, bool namesOnly, string pluginEntries)
	{
		string s = "{\n";
		if (groupEntries.Count() == 0)
			s += I(2) + "\"groups\": [],\n";
		else
		{
			s += I(2) + "\"groups\": [\n";
			for (int i = 0; i < groupEntries.Count(); i++)
			{
				s += I(3) + groupEntries[i];
				if (i < groupEntries.Count() - 1)
					s += ",";
				s += "\n";
			}
			s += I(2) + "],\n";
		}
		s += I(2) + "\"loot\": {\n";
		s += I(3) + "\"enabled\": " + B(lootEnabled) + ",\n";
		s += I(3) + "\"hidden\": " + B(lootEnabled && lootHidden) + ",\n";
		s += I(3) + "\"locked\": " + B(lootEnabled && lootLocked) + ",\n";
		if (projectId == "" || namesOnly)
			s += I(3) + "\"projectId\": null,\n";
		else
			s += I(3) + "\"projectId\": " + DZS_TextForm.Str(projectId) + ",\n";
		if (writtenTypes.Count() == 0)
			s += I(3) + "\"writtenTypes\": []\n";
		else
		{
			s += I(3) + "\"writtenTypes\": [\n";
			array<int> order = WrittenOrder(writtenTypes);
			for (int t = 0; t < order.Count(); t++)
			{
				int k = order[t];
				s += I(4) + "{\n";
				if (namesOnly)
					s += I(5) + "\"name\": " + DZS_TextForm.Str(writtenTypes[k]) + "\n";
				else
				{
					s += I(5) + "\"name\": " + DZS_TextForm.Str(writtenTypes[k]) + ",\n";
					string orig = originals[k];
					if (orig == "")
						s += I(5) + "\"original\": null\n";
					else
						s += I(5) + "\"original\": " + DZS_TextForm.Str(orig) + "\n";
				}
				s += I(4) + "}";
				if (t < order.Count() - 1)
					s += ",";
				s += "\n";
			}
			s += I(3) + "]\n";
		}
		s += I(2) + "},\n";
		s += I(2) + "\"plugins\": " + Plugins(pluginEntries, 2) + "\n";
		s += I(1) + "}";
		return s;
	}

	// One group entry at depth 3, from "{" to "}"; fileKey "block" names the group's spawner block (version 2).
	static string GroupEntry(string file, string name, string color, bool hidden, bool locked, int count, array<ref DZS_PFObject> objects, string pluginEntries, string fileKey = "file")
	{
		string s = "{\n";
		s += I(4) + DZS_TextForm.Str(fileKey) + ": " + DZS_TextForm.Str(file) + ",\n";
		s += I(4) + "\"name\": " + DZS_TextForm.Str(name) + ",\n";
		s += I(4) + "\"color\": " + DZS_TextForm.Str(color) + ",\n";
		s += I(4) + "\"hidden\": " + B(hidden) + ",\n";
		s += I(4) + "\"locked\": " + B(locked) + ",\n";
		s += I(4) + "\"objectCount\": " + count + ",\n";
		if (objects.Count() == 0)
			s += I(4) + "\"objects\": [],\n";
		else
		{
			s += I(4) + "\"objects\": [\n";
			for (int i = 0; i < objects.Count(); i++)
			{
				DZS_PFObject o = objects[i];
				s += I(5) + "{\n";
				s += I(6) + "\"index\": " + o.m_Index + ",\n";
				s += I(6) + "\"name\": " + DZS_TextForm.Str(o.m_Name) + ",\n";
				string ox = DZS_F32.ToText(o.m_X);
				string oy = DZS_F32.ToText(o.m_Y);
				string oz = DZS_F32.ToText(o.m_Z);
				s += I(6) + "\"pos\": [" + ox + ", " + oy + ", " + oz + "],\n";
				s += I(6) + "\"hidden\": " + B(o.m_Hidden) + ",\n";
				s += I(6) + "\"locked\": " + B(o.m_Locked) + "\n";
				s += I(5) + "}";
				if (i < objects.Count() - 1)
					s += ",";
				s += "\n";
			}
			s += I(4) + "],\n";
		}
		s += I(4) + "\"plugins\": " + Plugins(pluginEntries, 4) + "\n";
		s += I(3) + "}";
		return s;
	}

	// The whole file; project or view "" leaves the part out (rename without a saved part).
	static array<string> Compose(string name, string mapKey, string projectPart, string viewPart)
	{
		string s = "{\n";
		s += I(1) + "\"format\": \"dzstudio-project\",\n";
		s += I(1) + "\"version\": " + VERSION + ",\n";
		s += ProviderPart(name, mapKey);
		if (projectPart != "")
			s += ",\n" + I(1) + "\"project\": " + projectPart;
		if (viewPart != "")
			s += ",\n" + I(1) + "\"view\": " + viewPart;
		s += "\n}\n";
		return DZS_Files.Chunk(s);
	}

	// Version 2: the building blocks plus the extensions (DZS_ProjectExtensions.Save, null = none) with the editor part
	// (name, map, base, project part, view part; view "" = none) as the editor extension (EDITOR_EXT).
	static array<string> ComposeV2(DZS_Json blocks, string name, string mapKey, string base, string projectPart, string viewPart, DZS_Json extensions = null)
	{
		DZS_Json root = blocks.Clone();
		DZS_Json slug = DZS_Json.Str(DZS_ProjectV2.Slug(name));
		root.Set("slug", slug);
		DZS_Json ed = DZS_Json.Obj();
		DZS_Json prov = DZS_Json.Obj();
		DZS_Json nj = DZS_Json.Str(name);
		prov.Set("name", nj);
		DZS_Json mj = DZS_Json.Str(DZS_Maps.FormatKey(mapKey));
		prov.Set("map", mj);
		ed.Set("provider", prov);
		if (mapKey != "")
		{
			DZS_Json rmj = DZS_Json.Str(DZS_Maps.FormatKey(mapKey));
			root.Set("map", rmj);
		}
		if (base == "")
			base = "vanilla";
		DZS_Json bj = DZS_Json.Str(base);
		ed.Set("base", bj);
		string err;
		DZS_Json proj = DZS_Json.ParseText(projectPart, err);
		if (!proj || !proj.IsObj())
			proj = DZS_Json.Obj();
		ed.Set("project", proj);
		if (viewPart != "")
		{
			DZS_Json view = DZS_Json.ParseText(viewPart, err);
			if (view && view.IsObj())
				ed.Set("view", view);
		}
		if (extensions)
			root.Set("extensions", extensions);
		SetEditor(root, ed);
		return Text(root);
	}

	static array<string> Text(DZS_Json root)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		DZS_TextForm.Value(w, root, 0);
		w.Append("\n");
		return w.Finish();
	}

	// A version 2 file with another name (and slug), map, base or view ("" keeps each); null when it is not version 2.
	static array<string> RewriteV2(array<string> chunks, string name, string mapKey, string viewPart, string base = "")
	{
		if (!IsV2(chunks))
			return null;
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		DZS_Json root = doc.m_Root.ToJson();
		DZS_Json ed = EditorOf(root);
		if (!ed)
			ed = DZS_Json.Obj();
		DZS_Json prov = ed.Get("provider");
		if (!prov || !prov.IsObj())
		{
			prov = DZS_Json.Obj();
			ed.Set("provider", prov);
		}
		if (name != "")
		{
			DZS_Json nj = DZS_Json.Str(name);
			prov.Set("name", nj);
			DZS_Json sj = DZS_Json.Str(DZS_ProjectV2.Slug(name));
			root.Set("slug", sj);
		}
		if (mapKey != "")
		{
			DZS_Json mj = DZS_Json.Str(DZS_Maps.FormatKey(mapKey));
			prov.Set("map", mj);
			DZS_Json rj = DZS_Json.Str(DZS_Maps.FormatKey(mapKey));
			root.Set("map", rj);
		}
		if (base != "")
		{
			DZS_Json bj = DZS_Json.Str(base);
			ed.Set("base", bj);
		}
		if (viewPart != "")
		{
			string err;
			DZS_Json view = DZS_Json.ParseText(viewPart, err);
			if (view && view.IsObj())
				ed.Set("view", view);
		}
		SetEditor(root, ed);
		return Text(root);
	}

	// The view part of a project file of either version ("" = none).
	static string ViewRawOf(array<string> chunks)
	{
		if (!chunks)
			return "";
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc.m_Root || !doc.m_Root.IsObj())
			return "";
		DZS_SrcNode v = doc.m_Root.Get("view");
		DZS_SrcNode ed = EditorNodeOf(doc.m_Root);
		if (ed)
			v = ed.Get("view");
		if (!v || !v.IsObj())
			return "";
		return doc.Raw(v);
	}
}
