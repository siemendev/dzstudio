// Brush sets.
class DZS_BrushEntry : Managed
{
	string m_Name;
	int m_Share = 1;
	bool m_RandomYaw = true;
	float m_Tilt;
	bool m_Slope;
	float m_SlopeMax = 15;
	float m_Offset;
	float m_ScaleMin = 1;
	float m_ScaleMax = 1;
}

class DZS_BrushSet : Managed
{
	string m_NameDe;
	string m_NameEn;
	bool m_Bundled = true;
	string m_Id;
	ref array<ref DZS_BrushEntry> m_Entries = new array<ref DZS_BrushEntry>();

	string Name() { return DZS_Texts.Pick(m_NameDe, m_NameEn); }

	// Remembered-value key: the English name stays the same in both languages; own sets by their file id.
	string Key()
	{
		if (!m_Bundled)
			return "own:" + m_Id;
		return m_NameEn;
	}

	// Keys written before the rename: "eigen:<id>" and the German name of a bundled set.
	string LegacyKey()
	{
		if (!m_Bundled)
			return "eigen:" + m_Id;
		return m_NameDe;
	}

	void ForgetNames()
	{
		m_Low = null;
	}

	DZS_BrushEntry Find(string name)
	{
		string low = name;
		low.ToLower();
		foreach (DZS_BrushEntry e : m_Entries)
		{
			string n = e.m_Name;
			n.ToLower();
			if (n == low)
				return e;
		}
		return null;
	}

	int ShareSum()
	{
		int sum = 0;
		foreach (DZS_BrushEntry e : m_Entries)
			sum += e.m_Share;
		return sum;
	}

	protected ref map<string, bool> m_Low;

	bool HasName(string name)
	{
		if (!m_Low || m_Low.Count() != m_Entries.Count())
		{
			m_Low = new map<string, bool>();
			foreach (DZS_BrushEntry e : m_Entries)
			{
				string n = e.m_Name;
				n.ToLower();
				m_Low.Set(n, true);
			}
		}
		string low = name;
		low.ToLower();
		return m_Low.Contains(low);
	}

	// Entries that stand in the catalog now; with share above 0 for drawing.
	array<DZS_BrushEntry> Usable(bool withShare)
	{
		array<DZS_BrushEntry> r = new array<DZS_BrushEntry>();
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		foreach (DZS_BrushEntry e : m_Entries)
		{
			if (cat && !cat.InCatalog(e.m_Name))
				continue;
			if (withShare && e.m_Share <= 0)
				continue;
			r.Insert(e);
		}
		return r;
	}

	int Kinds() { return m_Entries.Count(); }
}

// The bundled sets come as a file with the plugin (made by editor/mod-tools/brush/build_sets.py); a file in the profile
// folder replaces it (test version for the acceptance checks).
class DZS_BrushStore
{
	static const string IN_MOD = "dzstudio/data/brush-sets.txt";
	static const string IN_PROFILE = "$profile:dzstudio\\brush-sets.txt";

	static array<ref DZS_BrushSet> LoadBundled()
	{
		array<ref DZS_BrushSet> sets = new array<ref DZS_BrushSet>();
		string path = IN_MOD;
		if (FileExist(IN_PROFILE))
			path = IN_PROFILE;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (!fh)
		{
			DZS_Log.Error("brush: sets not found at " + path);
			return sets;
		}
		string line;
		DZS_BrushSet cur;
		while (FGets(fh, line) >= 0)
		{
			line.Replace("\r", "");
			if (line == "" || line.IndexOf("#") == 0)
				continue;
			array<string> p = new array<string>();
			line.Split("\t", p);
			if (p[0] == "set" && p.Count() >= 3)
			{
				cur = new DZS_BrushSet();
				cur.m_NameDe = p[1];
				cur.m_NameEn = p[2];
				sets.Insert(cur);
				continue;
			}
			if (!cur || p.Count() < 9)
				continue;
			DZS_BrushEntry e = new DZS_BrushEntry();
			e.m_Name = p[0];
			e.m_Share = Math.Clamp(p[1].ToInt(), 0, 100);
			e.m_RandomYaw = p[2] == "1";
			e.m_Tilt = Math.Clamp(p[3].ToFloat(), 0, 15);
			e.m_Slope = p[4] == "1";
			e.m_SlopeMax = Math.Clamp(p[5].ToFloat(), 0, 45);
			e.m_Offset = Math.Clamp(p[6].ToFloat(), -10, 10);
			float a = Math.Clamp(p[7].ToFloat(), DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX);
			float b = Math.Clamp(p[8].ToFloat(), DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX);
			e.m_ScaleMin = Math.Min(a, b);
			e.m_ScaleMax = Math.Max(a, b);
			cur.m_Entries.Insert(e);
		}
		CloseFile(fh);
		DZS_Log.Info("brush: " + sets.Count() + " sets from " + path);
		return sets;
	}
}

// Own sets: one file per set in $profile:dzstudio\brushes\, named by a random
// 16-character id; written to <id>.tmp, copied over and the .tmp deleted, like the templates.
class DZS_BrushOwnStore
{
	static const string DIR = "$profile:dzstudio\\brushes";
	static const int VERSION = 1;
	static const int MAX_ENTRIES = 50;

	static string PathOf(string id, string ext)
	{
		return DIR + "\\" + id + ext;
	}

	static array<ref DZS_BrushSet> LoadAll()
	{
		array<ref DZS_BrushSet> sets = new array<ref DZS_BrushSet>();
		DZS_Files.MakeDirs(DIR);
		array<string> names;
		array<bool> folders;
		DZS_Files.List(DIR, names, folders);
		map<string, bool> ids = new map<string, bool>();
		for (int i = 0; i < names.Count(); i++)
		{
			if (folders[i])
				continue;
			string low = names[i];
			low.ToLower();
			int dot = low.LastIndexOf(".");
			if (dot <= 0)
				continue;
			string ext = low.Substring(dot, low.Length() - dot);
			if (ext == ".json" || ext == ".tmp")
			{
				string base = names[i].Substring(0, dot);
				ids.Set(base, true);
			}
		}
		array<string> order = new array<string>();
		for (int k = 0; k < ids.Count(); k++)
			order.Insert(ids.GetKey(k));
		order.Sort();
		foreach (string id : order)
		{
			DZS_BrushSet s = LoadOne(id);
			if (s)
				sets.Insert(s);
		}
		DZS_Log.Info("brush: " + sets.Count() + " own set(s) from " + DIR);
		return sets;
	}

	static DZS_Json ParseFile(string path, out string error)
	{
		array<string> chunks = DZS_Files.Read(path);
		if (!chunks)
		{
			error = "cannot open";
			return null;
		}
		return DZS_Json.Parse(chunks, error);
	}

	static bool Valid(DZS_Json j)
	{
		if (!j || !j.IsObj() || j.GetInt("version", 0) < 1)
			return false;
		string name = j.GetStr("name");
		name.TrimInPlace();
		DZS_Json en = j.Get("entries");
		return name != "" && en && en.IsArr();
	}

	// A readable .tmp replaces an unreadable or missing file of the same id; otherwise it is deleted.
	static DZS_BrushSet LoadOne(string id)
	{
		string main = PathOf(id, ".json");
		string tmp = PathOf(id, ".tmp");
		string error;
		DZS_Json mj = null;
		if (FileExist(main))
			mj = ParseFile(main, error);
		if (FileExist(tmp))
		{
			string terr;
			DZS_Json tj = ParseFile(tmp, terr);
			if (tj && Valid(tj) && tj.GetInt("version", 0) <= VERSION && (!mj || !Valid(mj)))
			{
				if (DZS_Files.Copy(tmp, main))
				{
					DeleteFile(tmp);
					mj = tj;
					DZS_Log.Info("brush: own set " + id + " restored from its .tmp");
				}
			}
			else
				DeleteFile(tmp);
		}
		if (!mj)
		{
			if (FileExist(main))
				DZS_Log.Warn("brush: own set file " + id + ".json unreadable, skipped: " + error);
			return null;
		}
		int version = mj.GetInt("version", 0);
		if (version > VERSION)
		{
			DZS_Log.Warn("brush: own set file " + id + ".json has version " + version + ", skipped");
			return null;
		}
		if (!Valid(mj))
		{
			DZS_Log.Warn("brush: own set file " + id + ".json is no brush set, skipped");
			return null;
		}
		DZS_BrushSet s = new DZS_BrushSet();
		s.m_Bundled = false;
		s.m_Id = id;
		string name = mj.GetStr("name");
		name.TrimInPlace();
		s.m_NameDe = name;
		s.m_NameEn = name;
		DZS_Json list = mj.Get("entries");
		for (int i = 0; i < list.Count() && s.m_Entries.Count() < MAX_ENTRIES; i++)
		{
			DZS_Json ej = list.At(i);
			if (!ej || !ej.IsObj())
				continue;
			string en = ej.GetStr("name");
			if (en == "" || s.Find(en))
				continue;
			DZS_BrushEntry e = new DZS_BrushEntry();
			e.m_Name = en;
			e.m_Share = Math.Clamp(Math.Round(Num(ej, "share", 1)), 0, 100);
			e.m_RandomYaw = Flag(ej, "randomYaw", true);
			e.m_Tilt = Math.Clamp(Num(ej, "tilt", 0), 0, 15);
			e.m_Slope = Flag(ej, "slope", false);
			e.m_SlopeMax = Math.Clamp(Num(ej, "slopeMax", 15), 0, 45);
			e.m_Offset = Math.Clamp(Num(ej, "heightOffset", 0), -10, 10);
			float a = Math.Clamp(Num(ej, "scaleMin", 1), DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX);
			float b = Math.Clamp(Num(ej, "scaleMax", 1), DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX);
			e.m_ScaleMin = Math.Min(a, b);
			e.m_ScaleMax = Math.Max(a, b);
			s.m_Entries.Insert(e);
			s.ForgetNames();
		}
		if (s.m_Entries.Count() == 0)
		{
			DZS_Log.Warn("brush: own set file " + id + ".json has no entries, skipped");
			return null;
		}
		return s;
	}

	protected static float Num(DZS_Json o, string key, float def)
	{
		DZS_Json v = o.Get(key);
		if (!v || !v.IsNum())
			return def;
		return v.m_Str.ToFloat();
	}

	protected static bool Flag(DZS_Json o, string key, bool def)
	{
		DZS_Json v = o.Get(key);
		if (!v || !v.IsBool())
			return def;
		return v.m_Bool;
	}

	static DZS_Json NumJson(float v)
	{
		string t = DZS_Num.Fixed(v, 3, true);
		return DZS_Json.Num(t);
	}

	static DZS_Json ToJson(DZS_BrushSet s)
	{
		DZS_Json j = DZS_Json.Obj();
		j.Set("version", DZS_Json.Int(VERSION));
		j.Set("name", DZS_Json.Str(s.m_NameDe));
		DZS_Json arr = DZS_Json.Arr();
		foreach (DZS_BrushEntry e : s.m_Entries)
		{
			DZS_Json o = DZS_Json.Obj();
			o.Set("name", DZS_Json.Str(e.m_Name));
			o.Set("share", DZS_Json.Int(e.m_Share));
			o.Set("randomYaw", DZS_Json.Boolean(e.m_RandomYaw));
			DZS_Json t343 = NumJson(e.m_Tilt);
			o.Set("tilt", t343);
			o.Set("slope", DZS_Json.Boolean(e.m_Slope));
			DZS_Json t345 = NumJson(e.m_SlopeMax);
			o.Set("slopeMax", t345);
			DZS_Json t346 = NumJson(e.m_Offset);
			o.Set("heightOffset", t346);
			DZS_Json t347 = NumJson(e.m_ScaleMin);
			o.Set("scaleMin", t347);
			DZS_Json t348 = NumJson(e.m_ScaleMax);
			o.Set("scaleMax", t348);
			arr.Add(o);
		}
		j.Set("entries", arr);
		return j;
	}

	static bool Save(DZS_BrushSet s)
	{
		if (s.m_Bundled)
			return false;
		if (s.m_Id == "")
			s.m_Id = DZS_Random.Code(16);
		DZS_Files.MakeDirs(DIR);
		string tmp = PathOf(s.m_Id, ".tmp");
		DZS_Json j = ToJson(s);
		array<string> chunks = j.ToChunks(true);
		if (!DZS_Files.Write(tmp, chunks))
		{
			DZS_Log.Warn("brush: cannot write " + tmp);
			return false;
		}
		string main = PathOf(s.m_Id, ".json");
		if (!DZS_Files.Copy(tmp, main))
			return false;
		DeleteFile(tmp);
		return true;
	}

	static void Delete(DZS_BrushSet s)
	{
		if (s.m_Bundled || s.m_Id == "")
			return;
		string main = PathOf(s.m_Id, ".json");
		string tmp = PathOf(s.m_Id, ".tmp");
		DeleteFile(main);
		DeleteFile(tmp);
	}
}
