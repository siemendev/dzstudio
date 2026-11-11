// Vehicle events of a base (db/events.xml + cfgeventspawns.xml of the vanilla mission or a mission folder) and the
// points the layer shows: the base's points with the project's moves and removals, plus the project's own points.
// cfgspawnabletypes.xml gives the parts a server puts on each vehicle class (the cars are shown complete).
class DZS_SpawnEvent : Managed
{
	string m_Name;
	string m_Nominal;
	string m_BaseNominal;
	ref array<string> m_Types = new array<string>();
	ref array<string> m_Max = new array<string>();
	ref array<string> m_BaseMax = new array<string>();
	// an own fixed event of the project (its element), else ""
	string m_Elem;
}

class DZS_SpawnPoint : Managed
{
	string m_Event;
	float m_X;
	float m_Z;
	float m_A;
	// the base point's text (key of a change); "" for an own point
	string m_BX;
	string m_BZ;
	string m_BA;
	// the state element for this point (own point, or a moved/removed base point), "" for an untouched base point
	string m_Elem;
	bool m_Own;
	bool m_Moved;
	bool m_Gone;
	// carried over from the file, not editable here (DZS_KeptBlocks): an own point of the file (m_Elem its block id)
	// or a base point a kept change touches
	bool m_Kept;

	string Key()
	{
		if (m_Kept && m_Own)
			return "k:" + m_Elem;
		if (m_Elem != "")
			return "e:" + m_Elem;
		return "b:" + m_Event + ":" + m_BX + ":" + m_BZ;
	}
}

// Reads a base over several frames (the three files parsed in slices), then builds it.
class DZS_SpawnsLoad : Managed
{
	string m_Dir;
	protected ref DZS_PatchFiles m_Files;
	protected ref array<string> m_Todo = new array<string>();
	protected ref DZS_PatchFileRead m_Read;
	protected bool m_ReadAll;
	int m_Frames;
	float m_Slowest;

	void DZS_SpawnsLoad(string dir)
	{
		m_Dir = dir;
		m_Todo.Insert("db/events.xml");
		m_Todo.Insert("cfgeventspawns.xml");
		m_Todo.Insert("cfgspawnabletypes.xml");
		m_Files = new DZS_PatchFiles(dir);
		if (dir != "")
			m_Files.m_Over.Copy(DZS_ApplyStore.Originals(dir));
	}

	// One slice of about 'ms'; the base when it is read, else null.
	DZS_SpawnsBase Step(float ms)
	{
		int t0 = TickCount(0);
		m_Frames++;
		DZS_SpawnsBase done = null;
		if (m_Dir == "")
			done = new DZS_SpawnsBase();
		while (!done)
		{
			if (!m_Read)
			{
				// the build in a frame of its own
				if (m_Todo.Count() == 0)
				{
					if (m_ReadAll)
						done = DZS_SpawnsBase.From(m_Dir, m_Files);
					m_ReadAll = true;
					break;
				}
				string rel = m_Todo[0];
				m_Todo.Remove(0);
				m_Read = m_Files.BeginRead(rel);
				if (m_Read)
					break;
				continue;
			}
			if (!m_Files.StepRead(m_Read, t0, ms))
				break;
			m_Read = null;
			if ((TickCount(0) - t0) / 10000.0 > ms)
				break;
		}
		m_Slowest = Math.Max(m_Slowest, (TickCount(0) - t0) / 10000.0);
		return done;
	}
}

class DZS_SpawnsBase : Managed
{
	string m_Dir;
	ref map<string, ref DZS_SpawnEvent> m_Events = new map<string, ref DZS_SpawnEvent>();
	ref array<ref DZS_SpawnPoint> m_Points = new array<ref DZS_SpawnPoint>();
	// cfgspawnabletypes.xml: type entries by lower-case name, and the parts worked out per class
	protected ref map<string, DZS_XmlNode> m_Spawnable = new map<string, DZS_XmlNode>();
	protected ref DZS_PatchFile m_SpawnableFile;
	protected ref map<string, ref array<string>> m_Parts = new map<string, ref array<string>>();

	static bool IsVehicle(string cls)
	{
		if (cls == "")
			return false;
		return GetGame().IsKindOf(cls, "Transport");
	}

	// A folder projects were applied to shows its originals, as the patcher reads them (DZS_SpawnsLoad).
	static DZS_SpawnsBase From(string dir, DZS_PatchFiles files)
	{
		DZS_SpawnsBase b = new DZS_SpawnsBase();
		b.m_Dir = dir;
		if (dir == "")
			return b;
		DZS_PatchFile st = files.Get("cfgspawnabletypes.xml");
		if (st.Ok())
		{
			b.m_SpawnableFile = st;
			foreach (DZS_XmlNode t : st.m_Xml.m_Root.m_Children)
			{
				if (t.m_Name == "type")
					b.m_Spawnable.Set(DZS_PatchText.Lower(t.Get("name")), t);
			}
		}
		DZS_PatchFile ev = files.Get("db/events.xml");
		DZS_PatchFile sp = files.Get("cfgeventspawns.xml");
		if (!ev.Ok() || !sp.Ok())
			return b;
		foreach (DZS_XmlNode e : ev.m_Xml.m_Root.m_Children)
		{
			if (e.m_Name != "event")
				continue;
			string name = e.Get("name");
			if (b.m_Events.Contains(name))
				continue;
			DZS_SpawnEvent se = new DZS_SpawnEvent();
			se.m_Name = name;
			bool vehicle = false;
			foreach (DZS_XmlNode c : e.m_Children)
			{
				if (c.m_Name == "nominal")
				{
					se.m_Nominal = ev.Text(c);
					se.m_BaseNominal = se.m_Nominal;
				}
				if (c.m_Name != "children")
					continue;
				foreach (DZS_XmlNode k : c.m_Children)
				{
					if (k.m_Name != "child")
						continue;
					string type = k.Get("type");
					se.m_Types.Insert(type);
					se.m_Max.Insert(k.Get("max"));
					se.m_BaseMax.Insert(k.Get("max"));
					if (IsVehicle(type))
						vehicle = true;
				}
			}
			if (vehicle)
				b.m_Events.Set(name, se);
		}
		foreach (DZS_XmlNode list : sp.m_Xml.m_Root.m_Children)
		{
			if (list.m_Name != "event")
				continue;
			string en = list.Get("name");
			if (!b.m_Events.Contains(en))
				continue;
			foreach (DZS_XmlNode p : list.m_Children)
			{
				if (p.m_Name != "pos" || p.Has("group"))
					continue;
				DZS_SpawnPoint sp0 = new DZS_SpawnPoint();
				sp0.m_Event = en;
				sp0.m_BX = p.Get("x");
				sp0.m_BZ = p.Get("z");
				sp0.m_BA = p.Get("a");
				sp0.m_X = sp0.m_BX.ToFloat();
				sp0.m_Z = sp0.m_BZ.ToFloat();
				sp0.m_A = sp0.m_BA.ToFloat();
				b.m_Points.Insert(sp0);
			}
		}
		return b;
	}

	// The parts of a class as the base's cfgspawnabletypes.xml attaches them, every chance ignored: of each
	// attachments block its likeliest item. Null when the file has no entry (the game's own debug parts are used).
	array<string> Parts(string cls)
	{
		string low = DZS_PatchText.Lower(cls);
		if (m_Parts.Contains(low))
			return m_Parts.Get(low);
		DZS_XmlNode t = m_Spawnable.Get(low);
		array<string> parts = null;
		if (t)
		{
			parts = new array<string>();
			foreach (DZS_XmlNode block : t.m_Children)
			{
				if (block.m_Name != "attachments")
					continue;
				string best = "";
				float bestChance = -1;
				foreach (DZS_XmlNode item : block.m_Children)
				{
					if (item.m_Name != "item" || item.Get("name") == "")
						continue;
					float ch = 1;
					if (item.Has("chance"))
						ch = item.Get("chance").ToFloat();
					if (ch > bestChance)
					{
						bestChance = ch;
						best = item.Get("name");
					}
				}
				if (best != "")
					parts.Insert(best);
			}
		}
		m_Parts.Set(low, parts);
		return parts;
	}

	// The layer's view: base events and points with the project's state laid over them.
	void Effective(DZS_Instance inst, map<string, ref DZS_SpawnEvent> events, array<ref DZS_SpawnPoint> points)
	{
		events.Clear();
		points.Clear();
		foreach (string en, DZS_SpawnEvent be : m_Events)
		{
			DZS_SpawnEvent ce = new DZS_SpawnEvent();
			ce.m_Name = be.m_Name;
			ce.m_Nominal = be.m_Nominal;
			ce.m_BaseNominal = be.m_BaseNominal;
			ce.m_Types.Copy(be.m_Types);
			ce.m_Max.Copy(be.m_Max);
			ce.m_BaseMax.Copy(be.m_BaseMax);
			events.Set(en, ce);
		}
		array<string> own = DZS_SpawnsModel.Of(inst, DZS_SpawnsModel.EVENT);
		foreach (string oid : own)
		{
			DZS_SpawnEvent oe = new DZS_SpawnEvent();
			oe.m_Name = DZS_SpawnsModel.Field(inst, oid, "name");
			oe.m_Nominal = DZS_SpawnsModel.Field(inst, oid, "nominal");
			oe.m_Elem = oid;
			oe.m_Types.Insert(DZS_SpawnsModel.Field(inst, oid, "class"));
			oe.m_Max.Insert("1");
			oe.m_BaseMax.Insert("");
			events.Set(oe.m_Name, oe);
		}
		array<string> sets = DZS_SpawnsModel.Of(inst, DZS_SpawnsModel.SET);
		foreach (string sid : sets)
		{
			DZS_SpawnEvent te = events.Get(DZS_SpawnsModel.Field(inst, sid, "event"));
			if (!te)
				continue;
			string field = DZS_SpawnsModel.Field(inst, sid, "field");
			string to = DZS_SpawnsModel.Field(inst, sid, "to");
			if (field == "nominal")
				te.m_Nominal = to;
			else if (field.IndexOf("child:") == 0)
			{
				array<string> parts = new array<string>();
				field.Split(":", parts);
				int at = te.m_Types.Find(parts[1]);
				if (at >= 0 && parts[2] == "max")
					te.m_Max[at] = to;
			}
		}
		// base points the project moved or removed, by their base key
		map<string, string> touched = new map<string, string>();
		array<string> pts = DZS_SpawnsModel.Of(inst, DZS_SpawnsModel.POINT);
		foreach (string pid : pts)
		{
			if (DZS_SpawnsModel.Field(inst, pid, "origin") != "base")
				continue;
			string bk = DZS_SpawnsModel.Field(inst, pid, "event") + ":" + DZS_SpawnsModel.Field(inst, pid, "bx") + ":" + DZS_SpawnsModel.Field(inst, pid, "bz");
			touched.Set(bk, pid);
		}
		foreach (DZS_SpawnPoint bp : m_Points)
		{
			DZS_SpawnPoint cp = new DZS_SpawnPoint();
			cp.m_Event = bp.m_Event;
			cp.m_BX = bp.m_BX;
			cp.m_BZ = bp.m_BZ;
			cp.m_BA = bp.m_BA;
			cp.m_X = bp.m_X;
			cp.m_Z = bp.m_Z;
			cp.m_A = bp.m_A;
			string tk = bp.m_Event + ":" + bp.m_BX + ":" + bp.m_BZ;
			string te2 = "";
			if (touched.Find(tk, te2))
			{
				cp.m_Elem = te2;
				if (DZS_SpawnsModel.Field(inst, te2, "gone") == "1")
					cp.m_Gone = true;
				else
				{
					cp.m_Moved = true;
					cp.m_X = DZS_SpawnsModel.Field(inst, te2, "x").ToFloat();
					cp.m_Z = DZS_SpawnsModel.Field(inst, te2, "z").ToFloat();
					cp.m_A = DZS_SpawnsModel.Field(inst, te2, "a").ToFloat();
				}
			}
			points.Insert(cp);
		}
		foreach (string qid : pts)
		{
			if (DZS_SpawnsModel.Field(inst, qid, "origin") == "base")
				continue;
			DZS_SpawnPoint np = new DZS_SpawnPoint();
			np.m_Event = DZS_SpawnsModel.Field(inst, qid, "event");
			np.m_Elem = qid;
			np.m_Own = true;
			np.m_X = DZS_SpawnsModel.Field(inst, qid, "x").ToFloat();
			np.m_Z = DZS_SpawnsModel.Field(inst, qid, "z").ToFloat();
			np.m_A = DZS_SpawnsModel.Field(inst, qid, "a").ToFloat();
			points.Insert(np);
		}
		Kept(inst, points);
	}

	protected void Kept(DZS_Instance inst, array<ref DZS_SpawnPoint> points)
	{
		array<ref DZS_Json> kch = DZS_KeptBlocks.Entries(inst, "changes", "spawnPosition");
		foreach (DZS_Json kc : kch)
		{
			DZS_Json key = kc.Get("key");
			string kx = DZS_KeptBlocks.Num(key, "x");
			string kz = DZS_KeptBlocks.Num(key, "z");
			if (kx == "" || kz == "")
				continue;
			foreach (DZS_SpawnPoint bp : points)
			{
				if (bp.m_Own || bp.m_Event != key.GetStr("event") || Math.AbsFloat(bp.m_BX.ToFloat() - kx.ToFloat()) > 0.5 || Math.AbsFloat(bp.m_BZ.ToFloat() - kz.ToFloat()) > 0.5)
					continue;
				bp.m_Kept = true;
				string field = kc.GetStr("field");
				DZS_Json to = kc.Get("to");
				if (kc.GetStr("op") == "remove")
					bp.m_Gone = true;
				else if ((field == "x" || field == "z" || field == "a") && to && to.IsNum())
				{
					bp.m_Moved = true;
					if (field == "x")
						bp.m_X = to.m_Str.ToFloat();
					else if (field == "z")
						bp.m_Z = to.m_Str.ToFloat();
					else
						bp.m_A = to.m_Str.ToFloat();
				}
				else if (field == "pos" && to && to.IsObj())
				{
					bp.m_Moved = true;
					if (DZS_KeptBlocks.Num(to, "x") != "")
						bp.m_X = DZS_KeptBlocks.Num(to, "x").ToFloat();
					if (DZS_KeptBlocks.Num(to, "z") != "")
						bp.m_Z = DZS_KeptBlocks.Num(to, "z").ToFloat();
					if (DZS_KeptBlocks.Num(to, "a") != "")
						bp.m_A = DZS_KeptBlocks.Num(to, "a").ToFloat();
				}
			}
		}
		map<string, string> evNames = new map<string, string>();
		array<ref DZS_Json> kev = DZS_KeptBlocks.Entries(inst, "blocks", "event");
		foreach (DZS_Json ke : kev)
		{
			DZS_Json ed = ke.Get("data");
			if (ed && ed.IsObj())
				evNames.Set(ke.GetStr("id"), ed.GetStr("name"));
		}
		array<ref DZS_Json> kbl = DZS_KeptBlocks.Entries(inst, "blocks", "spawnPosition");
		foreach (DZS_Json kb : kbl)
		{
			DZS_Json d = kb.Get("data");
			string x = DZS_KeptBlocks.Num(d, "x");
			string z = DZS_KeptBlocks.Num(d, "z");
			if (x == "" || z == "")
				continue;
			DZS_Json par = kb.Get("parent");
			string ev = "";
			if (par && par.IsObj())
				ev = par.GetStr("event");
			else if (par && par.IsStr())
				ev = evNames.Get(par.m_Str);
			DZS_SpawnPoint kp = new DZS_SpawnPoint();
			kp.m_Event = ev;
			kp.m_Elem = kb.GetStr("id");
			kp.m_Own = true;
			kp.m_Kept = true;
			kp.m_X = x.ToFloat();
			kp.m_Z = z.ToFloat();
			kp.m_A = DZS_KeptBlocks.Num(d, "a").ToFloat();
			points.Insert(kp);
		}
	}
}
