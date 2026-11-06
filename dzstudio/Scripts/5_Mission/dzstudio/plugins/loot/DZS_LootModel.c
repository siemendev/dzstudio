// Loot files of the project folder: the parsed mapgroupproto.xml and
// mapgrouppos.xml with an index of entries by type, the rows of mapgrouppos.xml, the name lists of
// cfglimitsdefinition.xml. Entries are read from the parsed file when asked for (DZS_LootEEntry); no object per entry
// is kept — freeing a map of 436 entry objects with their containers and points crashed DayZ 1.30 (access violation).
class DZS_LootRow : Managed
{
	string m_Name;
	string m_Key;
	vector m_Pos;
	vector m_Rpy;
	bool m_Valid;
	DZS_XmlNode m_Node;
	// id of the project object this row belongs to ("" = foreign row)
	string m_Obj;
	// value of the attribute dzstudio
	bool m_Marked;
	string m_Mark;
	// a row of the project by the provider's word (DZS_LootOrigin) instead of a mark
	bool m_Own;
}

class DZS_LootFiles : Managed
{
	static const string PROTO = "mapgroupproto.xml";
	static const string POS = "mapgrouppos.xml";
	static const string LIMITS = "cfglimitsdefinition.xml";

	ref DZS_XmlDoc m_Proto;
	ref DZS_XmlDoc m_PosDoc;
	string m_ProtoSum;
	string m_PosSum;
	// SHA-256 of the same bytes: the names of the texts in the state and in a session
	string m_ProtoSha;
	string m_PosSha;
	// the text as read (kept for the text of a checksum)
	ref array<string> m_ProtoChunks;
	ref array<string> m_PosChunks;
	// type key (lower case) -> index of its group element among the children of <prototype>
	ref map<string, int> m_Index = new map<string, int>();
	ref array<ref DZS_LootRow> m_Rows = new array<ref DZS_LootRow>();
	ref map<string, int> m_RowsOfType = new map<string, int>();
	ref map<string, int> m_Foreign = new map<string, int>();
	int m_DefGroup = -1;
	int m_DefContainer = -1;
	ref array<string> m_Categories = new array<string>();
	ref array<string> m_Tags = new array<string>();
	ref array<string> m_Usages = new array<string>();
	ref array<string> m_Values = new array<string>();
	string m_Error;
	string m_ErrorFile;
	string m_PosIndent;
	string m_ProtoIndent;
	string m_ProtoUnit = "\t";
	protected string m_CacheKey;
	protected int m_CacheGen = -1;
	protected ref DZS_LootEEntry m_Cache;
	int m_Gen;
	// rows of the project kept by a storage provider ("name\npos" raw); null = the mark decides
	ref map<string, bool> m_OwnRows;
	// probe only (projtest lootprobe): 1 no checksums, 2 no entry index, 4 no rows, 8 no name lists, 16 no pos file
	static int s_Skip;

	static bool Present(string folder)
	{
		string a = DZS_LocalStore.Path(folder, PROTO);
		string b = DZS_LocalStore.Path(folder, POS);
		return FileExist(a) && FileExist(b);
	}

	static DZS_LootFiles Read(string folder)
	{
		DZS_LootFiles f = new DZS_LootFiles();
		string a = DZS_LocalStore.Path(folder, PROTO);
		string b = DZS_LocalStore.Path(folder, POS);
		string c = DZS_LocalStore.Path(folder, LIMITS);
		f.Load(a, b, c);
		return f;
	}

	// From texts fetched in a shared session (no folder); the name lists from the vanilla state of the map, if fetched.
	static DZS_LootFiles FromChunks(array<string> pc, array<string> qc, string limitsPath)
	{
		DZS_LootFiles f = new DZS_LootFiles();
		f.LoadChunks(pc, qc, limitsPath);
		return f;
	}

	// Only mapgroupproto.xml and the name lists (the base state from the vanilla folder).
	static DZS_LootFiles ReadBase(string protoPath, string limitsPath)
	{
		DZS_LootFiles f = new DZS_LootFiles();
		f.Load(protoPath, "", limitsPath, false);
		return f;
	}

	protected void Load(string protoPath, string posPath, string limPath, bool sha = true)
	{
		array<string> pc = DZS_Files.Read(protoPath);
		array<string> qc = null;
		if (posPath != "" && (s_Skip & 16) == 0)
			qc = DZS_Files.Read(posPath);
		else
			posPath = "";
		if (!pc || (posPath != "" && !qc))
		{
			m_Error = "file missing";
			return;
		}
		LoadChunks(pc, qc, limPath, sha);
	}

	protected void LoadChunks(array<string> pc, array<string> qc, string limPath, bool sha = true)
	{
		if (!pc)
		{
			m_Error = "file missing";
			return;
		}
		m_ProtoChunks = pc;
		m_PosChunks = qc;
		int t0 = TickCount(0);
		if ((s_Skip & 1) == 0)
		{
			m_ProtoSum = DZS_Files.Checksum(pc);
			if (sha)
			{
				m_ProtoSha = DZS_Sha256.Of(pc);
				if (qc)
					m_PosSha = DZS_Sha256.Of(qc);
				DZS_Log.Info("loot: SHA-256 of the files in " + ((TickCount(0) - t0) / 10000) + " ms");
				t0 = TickCount(0);
			}
		}
		m_Proto = DZS_Xml.Parse(pc);
		DZS_Log.Info("loot: " + PROTO + " parsed in " + ((TickCount(0) - t0) / 10000) + " ms, error '" + m_Proto.m_Error + "'");
		if (m_Proto.m_Error != "")
		{
			m_Error = m_Proto.ErrorText();
			m_ErrorFile = PROTO;
			return;
		}
		if (m_Proto.m_Root.m_Name != "prototype")
		{
			m_Error = "different root element";
			m_ErrorFile = PROTO;
			return;
		}
		if (qc)
		{
			if ((s_Skip & 1) == 0)
				m_PosSum = DZS_Files.Checksum(qc);
			int t1 = TickCount(0);
			m_PosDoc = DZS_Xml.Parse(qc);
			DZS_Log.Info("loot: " + POS + " parsed in " + ((TickCount(0) - t1) / 10000) + " ms, error '" + m_PosDoc.m_Error + "'");
			if (m_PosDoc.m_Error != "")
			{
				m_Error = m_PosDoc.ErrorText();
				m_ErrorFile = POS;
				return;
			}
			if (m_PosDoc.m_Root.m_Name != "map")
			{
				m_Error = "different root element";
				m_ErrorFile = POS;
				return;
			}
		}
		if ((s_Skip & 2) == 0)
			ReadProto();
		if (m_PosDoc && (s_Skip & 4) == 0)
			ReadRows();
		array<string> lc = null;
		if ((s_Skip & 8) == 0)
			lc = DZS_Files.Read(limPath);
		if (lc)
		{
			DZS_XmlDoc lim = DZS_Xml.Parse(lc);
			if (lim.m_Error == "" && lim.m_Root)
				ReadLimits(lim.m_Root);
		}
	}

	protected void ReadProto()
	{
		DZS_XmlNode root = m_Proto.m_Root;
		foreach (DZS_XmlNode d : root.m_Children)
		{
			if (d.m_Name != "defaults")
				continue;
			foreach (DZS_XmlNode def : d.m_Children)
			{
				string dn = def.Get("name");
				string dv = def.Get("lootmax");
				if (dn == "group")
					m_DefGroup = dv.ToInt();
				else if (dn == "container")
					m_DefContainer = dv.ToInt();
			}
		}
		Reindex();
		foreach (DZS_XmlNode g : root.m_Children)
		{
			if (g.m_Name != "group")
				continue;
			m_ProtoIndent = m_Proto.IndentOf(g);
			if (g.m_Children.Count() > 0)
			{
				string ci = m_Proto.IndentOf(g.m_Children[0]);
				if (ci.Length() > m_ProtoIndent.Length())
					m_ProtoUnit = ci.Substring(m_ProtoIndent.Length(), ci.Length() - m_ProtoIndent.Length());
			}
			break;
		}
	}

	// Index of entries: the first group of each type (later ones stay as text).
	void Reindex()
	{
		m_Index.Clear();
		m_Gen++;
		DZS_XmlNode root = m_Proto.m_Root;
		for (int i = 0; i < root.m_Children.Count(); i++)
		{
			DZS_XmlNode g = root.m_Children[i];
			if (g.m_Name != "group")
				continue;
			string key = g.Get("name");
			if (key == "")
				continue;
			key.ToLower();
			if (!m_Index.Contains(key))
				m_Index.Set(key, i);
		}
	}

	protected void ReadRows()
	{
		m_Rows.Clear();
		foreach (DZS_XmlNode g : m_PosDoc.m_Root.m_Children)
		{
			if (g.m_Name != "group")
				continue;
			DZS_LootRow r = RowOf(g);
			m_Rows.Insert(r);
			if (m_PosIndent == "")
				m_PosIndent = m_PosDoc.IndentOf(g);
		}
		CountRows();
	}

	DZS_LootRow RowOf(DZS_XmlNode g)
	{
		DZS_LootRow r = new DZS_LootRow();
		r.m_Node = g;
		r.m_Name = g.Get("name");
		string key = r.m_Name;
		key.ToLower();
		r.m_Key = key;
		float px, py, pz, rr, rp, ry;
		string posText = g.Get("pos");
		string rpyText = g.Get("rpy");
		bool okPos = Three(posText, px, py, pz);
		bool okRpy = Three(rpyText, rr, rp, ry);
		r.m_Valid = okPos && okRpy;
		r.m_Marked = g.Has("dzstudio");
		r.m_Mark = g.Get("dzstudio");
		r.m_Pos = Vector(px, py, pz);
		r.m_Rpy = Vector(rr, rp, ry);
		return r;
	}

	void CountRows()
	{
		m_RowsOfType.Clear();
		foreach (DZS_LootRow r : m_Rows)
		{
			if (!r.m_Valid)
				continue;
			int n = 0;
			m_RowsOfType.Find(r.m_Key, n);
			m_RowsOfType.Set(r.m_Key, n + 1);
		}
	}

	// Three numbers (read quickly; rows and points are compared within 0.5 mm, never written back as read).
	static bool Three(string text, out float a, out float b, out float c)
	{
		array<string> parts = new array<string>();
		text.Split(" ", parts);
		array<string> nums = new array<string>();
		foreach (string p : parts)
		{
			if (p != "")
				nums.Insert(p);
		}
		if (nums.Count() != 3)
			return false;
		a = nums[0].ToFloat();
		b = nums[1].ToFloat();
		c = nums[2].ToFloat();
		return true;
	}

	protected void ReadLimits(DZS_XmlNode root)
	{
		foreach (DZS_XmlNode list : root.m_Children)
		{
			array<string> into = null;
			if (list.m_Name == "categories")
				into = m_Categories;
			else if (list.m_Name == "tags")
				into = m_Tags;
			else if (list.m_Name == "usageflags")
				into = m_Usages;
			else if (list.m_Name == "valueflags")
				into = m_Values;
			if (!into)
				continue;
			foreach (DZS_XmlNode item : list.m_Children)
			{
				string nm = item.Get("name");
				if (nm != "")
					into.Insert(nm);
			}
		}
	}

	bool HasEntry(string type)
	{
		string key = type;
		key.ToLower();
		return m_Index.Contains(key);
	}

	DZS_XmlNode Group(string type)
	{
		string key = type;
		key.ToLower();
		int i;
		if (!m_Index.Find(key, i))
			return null;
		return m_Proto.m_Root.m_Children[i];
	}

	// The entry of a type as it stands in this file (a copy the caller may change), or null.
	DZS_LootEEntry Entry(string type)
	{
		DZS_XmlNode g = Group(type);
		if (!g)
			return null;
		string key = type;
		key.ToLower();
		if (key != m_CacheKey || m_CacheGen != m_Gen || !m_Cache)
		{
			m_Cache = DZS_LootEEntry.FromNode(m_Proto, g);
			m_CacheKey = key;
			m_CacheGen = m_Gen;
		}
		return m_Cache.Copy();
	}

	// Number of points of a type's entry without building it.
	int PointsOf(string type)
	{
		DZS_XmlNode g = Group(type);
		if (!g)
			return 0;
		int n = 0;
		foreach (DZS_XmlNode c : g.m_Children)
		{
			if (c.m_Name != "container")
				continue;
			foreach (DZS_XmlNode k : c.m_Children)
			{
				if (k.m_Name == "point")
					n++;
			}
		}
		return n;
	}

	// Value of dzstudio at a type's group element; false when it has none.
	bool MarkOf(string type, out string mark)
	{
		mark = "";
		DZS_XmlNode g = Group(type);
		if (!g || !g.Has("dzstudio"))
			return false;
		mark = g.Get("dzstudio");
		return true;
	}

	// Rows with this mark that belong to no object.
	int StrayRows(string mark)
	{
		int n = 0;
		foreach (DZS_LootRow r : m_Rows)
		{
			if (r.m_Obj == "" && r.m_Marked && r.m_Mark == mark && mark != "")
				n++;
		}
		return n;
	}

	string SourceOf(string type)
	{
		DZS_XmlNode g = Group(type);
		if (!g)
			return "";
		return m_Proto.Raw(g);
	}

	// Rows of a type that belong to no object of the project (buildings of the map, other mods).
	int ForeignRows(string type)
	{
		string key = type;
		key.ToLower();
		int n = 0;
		m_Foreign.Find(key, n);
		return n;
	}

	int Rows(string type)
	{
		string key = type;
		key.ToLower();
		int n = 0;
		m_RowsOfType.Find(key, n);
		return n;
	}

	// Rows to objects: only rows with the project's mark, same name regardless of case
	// and pos within 1 cm on every axis; several candidates pair up in file and project order. Rows already paired keep
	// their object.
	void Match(DZS_Instance inst, array<string> objIds, string mark, bool keep = false)
	{
		map<string, ref array<string>> byKey = new map<string, ref array<string>>();
		map<string, bool> taken = new map<string, bool>();
		if (keep)
		{
			foreach (DZS_LootRow kr : m_Rows)
			{
				if (kr.m_Obj != "")
					taken.Set(kr.m_Obj, true);
			}
		}
		foreach (string id : objIds)
		{
			if (taken.Contains(id))
				continue;
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			string key = name;
			key.ToLower();
			array<string> list = byKey.Get(key);
			if (!list)
			{
				list = new array<string>();
				byKey.Set(key, list);
			}
			list.Insert(id);
		}
		foreach (DZS_LootRow r : m_Rows)
		{
			if (keep && r.m_Obj != "")
				continue;
			r.m_Obj = "";
			r.m_Own = false;
			if (m_OwnRows)
			{
				DZS_XmlAttr an = r.m_Node.Attr("name");
				DZS_XmlAttr ap = r.m_Node.Attr("pos");
				if (an && ap)
				{
					string rk = an.m_Raw + "\n" + ap.m_Raw;
					r.m_Own = m_OwnRows.Contains(rk);
				}
				if (!r.m_Own)
					continue;
			}
			else if (mark == "" || !r.m_Marked || r.m_Mark != mark)
				continue;
			array<string> cands = byKey.Get(r.m_Key);
			if (!r.m_Valid || !cands)
				continue;
			foreach (string cid : cands)
			{
				if (taken.Contains(cid))
					continue;
				string pos;
				inst.GetField(cid, DZS_Obj.POS, pos);
				vector p = DZS_Obj.QuickVec(pos);
				bool near = Math.AbsFloat(p[0] - r.m_Pos[0]) <= 0.01;
				if (near)
					near = Math.AbsFloat(p[1] - r.m_Pos[1]) <= 0.01;
				if (near)
					near = Math.AbsFloat(p[2] - r.m_Pos[2]) <= 0.01;
				if (near)
				{
					r.m_Obj = cid;
					taken.Set(cid, true);
					break;
				}
			}
		}
		CountForeign();
	}

	void CountForeign()
	{
		m_Foreign.Clear();
		foreach (DZS_LootRow fr : m_Rows)
		{
			if (fr.m_Valid && fr.m_Obj == "")
			{
				int n = 0;
				m_Foreign.Find(fr.m_Key, n);
				m_Foreign.Set(fr.m_Key, n + 1);
			}
		}
	}

	DZS_LootRow RowOfObject(string objId)
	{
		foreach (DZS_LootRow r : m_Rows)
		{
			if (r.m_Obj == objId)
				return r;
		}
		return null;
	}

	// After a save: rows whose element went are dropped, rows written for objects are added (patch tag = object id).
	void FollowRows(array<ref DZS_XmlPatch> patches)
	{
		for (int i = m_Rows.Count() - 1; i >= 0; i--)
		{
			DZS_LootRow r = m_Rows[i];
			if (!r.m_Node)
				m_Rows.Remove(i);
		}
		foreach (DZS_XmlPatch p : patches)
		{
			if (!p.m_Made)
				continue;
			array<string> objs = new array<string>();
			p.m_Tag.Split(",", objs);
			for (int k = 0; k < p.m_Made.Count(); k++)
			{
				DZS_XmlNode n = p.m_Made[k];
				if (!n || n.m_Name != "group")
					continue;
				DZS_LootRow nr = RowOf(n);
				if (k < objs.Count())
					nr.m_Obj = objs[k];
				m_Rows.Insert(nr);
			}
		}
		CountRows();
		CountForeign();
	}

	// "-12.345678" as -12345678 (six decimals, as Fixed6 writes them)
	static int Micro(string fixed6)
	{
		bool neg = fixed6.IndexOf("-") == 0;
		string t = fixed6;
		if (neg)
			t = t.Substring(1, t.Length() - 1);
		t.Replace(".", "");
		int v = t.ToInt();
		if (neg)
			v = -v;
		return v;
	}

	static string FromMicro(int v)
	{
		bool neg = v < 0;
		if (neg)
			v = -v;
		int whole = v / 1000000;
		int frac = v % 1000000;
		string r = whole.ToString() + "." + frac.ToStringLen(6);
		if (neg && v != 0)
			r = "-" + r;
		return r;
	}

	// Row text as the editor writes it.
	static string RowText(string name, vector pos, vector ypr, string mark)
	{
		float yaw = DZS_PlaceMath.Norm180(ypr[0]);
		if (yaw <= -180)
			yaw += 360;
		string yawText = DZS_XmlWriter.Fixed6(yaw);
		int micro = Micro(yawText);
		int am = 90000000 - micro;
		if (am > 180000000)
			am -= 360000000;
		if (am <= -180000000)
			am += 360000000;
		string aText = FromMicro(am);
		string px = DZS_XmlWriter.Fixed6(pos[0]);
		string py = DZS_XmlWriter.Fixed6(pos[1]);
		string pz = DZS_XmlWriter.Fixed6(pos[2]);
		string roll = DZS_XmlWriter.Fixed6(ypr[2]);
		string pitch = DZS_XmlWriter.Fixed6(ypr[1]);
		string pt = px + " " + py + " " + pz;
		string rpy = roll + " " + pitch + " " + yawText;
		string nm = DZS_Xml.Escape(name);
		string mk = "";
		if (mark != "")
			mk = " dzstudio=\"" + mark + "\"";
		return "<group name=\"" + nm + "\" pos=\"" + pt + "\" rpy=\"" + rpy + "\" a=\"" + aText + "\"" + mk + " />";
	}
}
