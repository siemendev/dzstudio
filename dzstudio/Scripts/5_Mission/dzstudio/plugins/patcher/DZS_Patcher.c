// Applies a patch project to a mission folder: changes in project order, then own blocks, then registrations; every
// touched file is edited in place (byte-preserving) and checked. Nothing is written to disk here: the result is the
// new file contents and the report. Same input, same bytes.
class DZS_Patcher : DZS_PatcherJson
{
	ref map<string, ref array<string>> m_Out = new map<string, ref array<string>>();
	ref array<string> m_OutPaths = new array<string>();
	protected ref array<ref DZS_POwn> m_Owns = new array<ref DZS_POwn>();
	protected ref array<DZS_POwn> m_AllOwns = new array<DZS_POwn>();
	// own files to register, with their entries and the cfggameplay.json list they go into
	protected ref array<DZS_PatchFile> m_RegFiles = new array<DZS_PatchFile>();
	protected ref array<DZS_PatchEntry> m_RegEntries = new array<DZS_PatchEntry>();
	protected ref array<string> m_RegLists = new array<string>();
	protected bool m_Gameplay;

	// steps of a run (Step): read ahead, changes, own blocks, registrations, files
	static const int READ = 0;
	static const int CHANGES = 1;
	static const int OWN = 2;
	static const int REGISTER = 3;
	static const int FILES = 4;
	static const int DONE = 5;
	protected int m_Phase;
	protected int m_At;
	protected ref array<string> m_Reads;
	protected ref DZS_PatchFileRead m_Reading;
	// own blocks in steps: their entries, the pass (0 top level, 1 and 2 nested by depth, 3 the text of each)
	protected ref array<DZS_PatchEntry> m_OEntries;
	protected ref array<string> m_OAreaNames;
	protected ref array<string> m_OSafeKeys;
	protected ref array<string> m_OIds;
	protected bool m_OWeather;
	protected int m_OPass;
	// finishing in steps: the files, the pass (0 changed files, 1 created files)
	protected ref array<DZS_PatchFile> m_FAll;
	protected int m_FPass;
	protected bool m_FGameplayOk;

	// 'ground' defaults to the game's terrain; the conformance runner passes a flat one. The whole run at once.
	static DZS_Patcher Run(DZS_PatchProject project, string baseDir, map<string, string> over = null, DZS_PatchGround ground = null)
	{
		DZS_Patcher p = Begin(project, baseDir, over, ground);
		while (!p.Step(1000000))
		{
		}
		return p;
	}

	// A run to be stepped over several frames (Step); the same result as Run.
	static DZS_Patcher Begin(DZS_PatchProject project, string baseDir, map<string, string> over = null, DZS_PatchGround ground = null)
	{
		DZS_Patcher p = new DZS_Patcher();
		p.m_Project = project;
		p.m_Ground = ground;
		if (!p.m_Ground)
			p.m_Ground = new DZS_PatchGroundGame();
		p.m_Files = new DZS_PatchFiles(baseDir);
		if (over)
			p.m_Files.m_Over.Copy(over);
		p.PlanReads();
		return p;
	}

	bool IsDone() { return m_Phase == DONE; }

	// How far the run is, 0 … 1 (reading the base's XML files is most of the time).
	float Share()
	{
		float part = 0;
		if (m_Phase == READ)
		{
			float reads = Math.Max(1, m_Reads.Count());
			float inFile = 0;
			if (m_Reading && m_Reading.m_Parse)
				inFile = m_Reading.m_Parse.Progress();
			return 0.6 * (m_At + inFile) / reads;
		}
		if (m_Phase == CHANGES)
		{
			float changes = Math.Max(1, m_Project.m_Changes.Count());
			return 0.6 + 0.15 * m_At / changes;
		}
		if (m_Phase == OWN)
		{
			float blocks = Math.Max(1, m_Project.m_Blocks.Count());
			part = (Math.Max(0, m_OPass) + m_At / blocks) / 4.0;
			return 0.75 + 0.15 * part;
		}
		if (m_Phase == REGISTER)
			return 0.9;
		if (m_Phase == FILES && m_FAll)
		{
			float files = Math.Max(1, m_FAll.Count());
			part = (m_FPass + m_At / files) / 2.0;
			return 0.9 + 0.1 * part;
		}
		if (m_Phase == DONE)
			return 1;
		return 0;
	}

	// One slice of about 'ms' milliseconds; true when the run is complete. The pieces: a part of an XML file of the
	// base (read ahead), one change, one own block, the registrations, one file finished.
	bool Step(float ms)
	{
		int t0 = TickCount(0);
		while (m_Phase != DONE)
		{
			if (DZS_Job.Over(t0, ms))
				return false;
			if (m_Phase == READ)
			{
				if (!StepReads(t0, ms))
					return false;
				m_Phase = CHANGES;
				m_At = 0;
			}
			else if (m_Phase == CHANGES)
			{
				if (m_At >= m_Project.m_Changes.Count())
				{
					m_Phase = OWN;
					m_At = 0;
					m_OPass = -1;
					continue;
				}
				DZS_PatchChange c = m_Project.m_Changes[m_At];
				DZS_PatchEntry e = ChangeEntry(m_At, c);
				Change(e, c);
				m_At++;
			}
			else if (m_Phase == OWN)
			{
				if (OwnStep())
				{
					m_Phase = REGISTER;
					m_At = 0;
				}
			}
			else if (m_Phase == REGISTER)
			{
				Registrations();
				m_Phase = FILES;
				m_At = 0;
				m_FPass = 0;
				m_FAll = m_Files.All();
			}
			else if (m_Phase == FILES)
			{
				if (FinishStep())
					m_Phase = DONE;
			}
		}
		return true;
	}

	// The base's XML files the changes and own blocks name, read ahead in steps (a file the run then does not ask for
	// changes nothing; one it asks for that is not here is read when asked for).
	protected void PlanReads()
	{
		m_Reads = new array<string>();
		foreach (DZS_PatchChange c : m_Project.m_Changes)
			AddRead(PlannedFile(c.m_Block));
		foreach (DZS_PatchBlock b : m_Project.m_Blocks)
			AddRead(PlannedFile(b.m_Kind));
	}

	protected static string PlannedFile(string kind)
	{
		DZS_PKind k = DZS_PKinds.Get(kind);
		if (!k || k.m_FileInKey)
			return "";
		return k.m_File;
	}

	protected void AddRead(string rel)
	{
		if (rel == "")
			return;
		string low = DZS_PatchText.Lower(rel);
		int n = low.Length();
		if (n < 4 || low.Substring(n - 4, 4) != ".xml")
			return;
		if (m_Reads.Find(low) < 0)
			m_Reads.Insert(low);
	}

	protected bool StepReads(int t0, float ms)
	{
		while (m_At < m_Reads.Count())
		{
			if (!m_Reading)
			{
				m_Reading = m_Files.BeginRead(m_Reads[m_At]);
				if (!m_Reading)
				{
					m_At++;
					continue;
				}
			}
			if (!m_Files.StepRead(m_Reading, t0, ms))
				return false;
			m_Reading = null;
			m_At++;
			if (DZS_Job.Over(t0, ms))
				return m_At >= m_Reads.Count();
		}
		return true;
	}

	// ---- changes ----

	protected void Change(DZS_PatchEntry e, DZS_PatchChange c)
	{
		if (c.m_Op != "set" && c.m_Op != "remove")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-op";
			return;
		}
		if (c.m_Block == "effectArea")
		{
			ChangeArea(e, c);
			return;
		}
		if (c.m_Block == "safePosition")
		{
			ChangeSafe(e, c);
			return;
		}
		if (c.m_Block == "gameplaySetting")
		{
			ChangeGameplay(e, c);
			if (e.m_Status == "applied" || e.m_Status == "overrode")
				m_Gameplay = true;
			return;
		}
		DZS_PKind k = DZS_PKinds.Get(c.m_Block);
		if (!k)
		{
			e.m_Status = "invalid";
			e.m_Detail = "unknown-kind";
			return;
		}
		ChangeXml(e, c, k);
	}

	protected void ChangeXml(DZS_PatchEntry e, DZS_PatchChange c, DZS_PKind k)
	{
		string rel = k.m_File;
		if (k.m_FileInKey)
			rel = DZS_PKind.EnvFile(KeyStr(c.m_Key, "file"));
		if (rel == "")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-key";
			return;
		}
		DZS_PatchFile f = File(rel);
		if (!Usable(e, f))
			return;
		bool remove = c.m_Op == "remove";
		if (remove && (k.m_Singleton || k.m_Fixed))
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-op";
			return;
		}
		array<DZS_PField> fields = new array<DZS_PField>();
		array<ref DZS_Json> tos = new array<ref DZS_Json>();
		array<ref DZS_Json> froms = new array<ref DZS_Json>();
		if (!remove)
		{
			string bad = SetFields(k, c, fields, tos, froms);
			if (bad != "")
			{
				e.m_Status = "invalid";
				e.m_Detail = bad;
				return;
			}
		}
		DZS_PTarget t = Resolve(f, k, c.m_Key, remove);
		if (t.m_BadKey)
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-key";
			return;
		}
		if (t.m_Case)
			e.m_Detail = "case-mismatch";
		if (t.m_Ambiguous)
		{
			e.m_Status = "ambiguous";
			return;
		}
		if (t.m_Nodes.Count() == 0)
		{
			if (remove)
				e.m_Status = "gone";
			else
				e.m_Status = "orphaned";
			return;
		}
		if (remove)
		{
			foreach (DZS_XmlNode rn : t.m_Nodes)
				f.RemoveNode(rn);
			e.m_Status = "applied";
			Depends(e, f);
			return;
		}
		if (k.m_Name == "global" && !GlobalOk(f, k, t.m_Nodes, fields, tos))
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-value";
			return;
		}
		bool same = true;
		foreach (DZS_XmlNode sn : t.m_Nodes)
		{
			for (int i = 0; i < fields.Count(); i++)
			{
				if (!FieldIs(f, sn, fields[i], tos[i]))
					same = false;
			}
		}
		if (same)
		{
			e.m_Status = "already";
			return;
		}
		e.m_Status = "applied";
		DZS_XmlNode last = t.m_Nodes[t.m_Nodes.Count() - 1];
		if (c.m_HasFrom)
		{
			bool fromOk = true;
			for (int j = 0; j < fields.Count(); j++)
			{
				if (!FieldIs(f, last, fields[j], froms[j]))
					fromOk = false;
			}
			if (!fromOk)
			{
				e.m_Status = "overrode";
				e.m_Current = CurrentJson(f, last, fields);
			}
		}
		foreach (DZS_XmlNode n : t.m_Nodes)
		{
			for (int m = 0; m < fields.Count(); m++)
			{
				if (FieldIs(f, n, fields[m], tos[m]))
					continue;
				string err = SetField(f, k, n, fields[m], tos[m], null, null);
				if (err != "")
				{
					e.m_Status = "invalid";
					e.m_Detail = err;
					return;
				}
			}
		}
		Depends(e, f);
	}

	// A global's value stays a whole number where its type says integer, also when the type is what changes.
	protected bool GlobalOk(DZS_PatchFile f, DZS_PKind k, array<DZS_XmlNode> nodes, array<DZS_PField> fields, array<ref DZS_Json> tos)
	{
		DZS_PField tf = k.Field("type");
		DZS_PField vf = k.Field("value");
		foreach (DZS_XmlNode n : nodes)
		{
			bool hasType;
			string type = Current(f, n, tf, hasType);
			bool hasVal;
			string val = Current(f, n, vf, hasVal);
			for (int i = 0; i < fields.Count(); i++)
			{
				DZS_Json to = tos[i];
				if (fields[i].m_Name == "type" && to)
					type = to.m_Str;
				if (fields[i].m_Name == "value" && to)
					val = to.m_Str;
			}
			if (!DZS_PKind.GlobalValueOk(type, val))
				return false;
		}
		return true;
	}

	// The fields a set touches with their new and expected values; "pos" of a spawn position sets x/z/a/y together.
	protected string SetFields(DZS_PKind k, DZS_PatchChange c, array<DZS_PField> fields, array<ref DZS_Json> tos, array<ref DZS_Json> froms)
	{
		DZS_PField one = k.Field(c.m_Field);
		if (one)
		{
			fields.Insert(one);
			tos.Insert(c.m_To);
			froms.Insert(c.m_From);
		}
		else if (c.m_Field == "pos" && k.Field("x") && c.m_To && c.m_To.IsObj())
		{
			foreach (string name : c.m_To.m_Keys)
			{
				DZS_PField pf = k.Field(name);
				if (!pf || pf.m_Attr == "" || pf.m_Elems.Count() > 0 || name == "group")
					return "unknown-field";
				fields.Insert(pf);
				DZS_Json tv = c.m_To.Get(name);
				tos.Insert(tv);
				DZS_Json fv = null;
				if (c.m_From && c.m_From.IsObj())
					fv = c.m_From.Get(name);
				froms.Insert(fv);
			}
			if (fields.Count() == 0)
				return "bad-value";
		}
		else
			return "unknown-field";
		for (int i = 0; i < fields.Count(); i++)
		{
			DZS_PField f = fields[i];
			DZS_Json v = tos[i];
			if (!v || v.m_Type == DZS_Json.NUL)
			{
				if (f.m_Req)
					return "bad-value";
				continue;
			}
			if (f.m_List)
			{
				array<string> tokens = new array<string>();
				if (!ListTokens(v, tokens))
					return "bad-value";
				continue;
			}
			string err;
			Render(f, v, err);
			if (err != "")
				return err;
		}
		return "";
	}

	protected bool FieldIs(DZS_PatchFile f, DZS_XmlNode n, DZS_PField fld, DZS_Json v)
	{
		if (fld.m_List)
		{
			array<string> cur = new array<string>();
			CurrentList(f, n, fld, cur);
			if (!v || v.m_Type == DZS_Json.NUL)
				return cur.Count() == 0;
			return SameList(cur, v);
		}
		bool has;
		string val = Current(f, n, fld, has);
		return Equal(fld, val, has, v);
	}

	protected DZS_Json CurrentJson(DZS_PatchFile f, DZS_XmlNode n, array<DZS_PField> fields)
	{
		if (fields.Count() == 1)
			return CurrentOne(f, n, fields[0]);
		DZS_Json o = DZS_Json.Obj();
		foreach (DZS_PField fld : fields)
		{
			DZS_Json v = CurrentOne(f, n, fld);
			o.Set(fld.m_Name, v);
		}
		return o;
	}

	protected DZS_Json CurrentOne(DZS_PatchFile f, DZS_XmlNode n, DZS_PField fld)
	{
		if (fld.m_List)
		{
			array<string> cur = new array<string>();
			CurrentList(f, n, fld, cur);
			return DZS_Json.Str(Fingerprint(cur));
		}
		bool has;
		string val = Current(f, n, fld, has);
		if (!has)
			return DZS_Json.Null();
		return DZS_Json.Str(val);
	}

	// ---- own blocks ----

	// One piece of the own blocks: their entries, then one block of the top level, one nested block per depth, one
	// own element's text; true when all are done (the order of the whole run at once).
	protected bool OwnStep()
	{
		int count = m_Project.m_Blocks.Count();
		if (m_OPass < 0)
		{
			m_OEntries = new array<DZS_PatchEntry>();
			for (int i = 0; i < count; i++)
			{
				DZS_PatchBlock b = m_Project.m_Blocks[i];
				DZS_PatchEntry e = BlockEntry(i, b);
				m_OEntries.Insert(e);
			}
			m_OAreaNames = new array<string>();
			m_OSafeKeys = new array<string>();
			m_OIds = new array<string>();
			m_OWeather = false;
			m_OPass = 0;
			m_At = 0;
			return false;
		}
		if (m_OPass == 0)
		{
			if (m_At >= count)
			{
				m_OPass = 1;
				m_At = 0;
				return false;
			}
			OwnFirst(m_Project.m_Blocks[m_At], m_OEntries[m_At]);
			m_At++;
			return false;
		}
		// nested blocks by depth, so a loot point can go into an own container of an own loot type
		if (m_OPass == 1 || m_OPass == 2)
		{
			if (m_At >= count)
			{
				m_OPass++;
				m_At = 0;
				return false;
			}
			DZS_PatchBlock bn = m_Project.m_Blocks[m_At];
			DZS_PatchEntry en = m_OEntries[m_At];
			m_At++;
			if (en.m_Status != "")
				return false;
			DZS_PKind kn = DZS_PKinds.Get(bn.m_Kind);
			if (kn && kn.m_ParentKind != "" && Depth(kn) == m_OPass)
				OwnNested(en, bn, kn);
			return false;
		}
		if (m_At >= m_Owns.Count())
		{
			m_OEntries = null;
			return true;
		}
		DZS_POwn o = m_Owns[m_At];
		m_At++;
		if (!o.m_Ok)
			return false;
		DZS_PatchFile f = File(o.m_File);
		// one level below the root is the neighbours' indent, also where it differs from the rest of the file
		string unit = f.m_Unit;
		string near = f.ChildIndent(f.m_Xml.m_Root);
		if (near != "" && !o.m_Into)
			unit = near;
		string text = GenOwn(o, unit);
		if (o.m_Into)
			InsertInto(f, DZS_PKinds.Get(o.m_Kind.m_ParentKind), o.m_Kind, o.m_Into, text);
		else
			f.AppendChild(f.m_Xml.m_Root, text);
		DependsAll(o, f);
		return false;
	}

	// The first pass over the own blocks: ids, and every block that is no nested one.
	protected void OwnFirst(DZS_PatchBlock ba, DZS_PatchEntry ea)
	{
		if (m_OIds.Find(ba.m_Id) >= 0 || ba.m_Id == "")
		{
			ea.m_Status = "invalid";
			ea.m_Detail = "bad-id";
			return;
		}
		m_OIds.Insert(ba.m_Id);
		if (ba.m_Kind == "effectArea")
			OwnArea(ea, ba, m_OAreaNames);
		else if (ba.m_Kind == "safePosition")
			OwnSafe(ea, ba, m_OSafeKeys);
		else if (ba.m_Kind == "spawnerGroup" || ba.m_Kind == "gearPreset" || ba.m_Kind == "restrictedArea")
			OwnFile(ea, ba);
		else if (ba.m_Kind == "weather")
		{
			if (m_OWeather)
			{
				ea.m_Status = "collision";
				return;
			}
			m_OWeather = true;
			OwnWeather(ea, ba);
		}
		else
		{
			DZS_PKind k = DZS_PKinds.Get(ba.m_Kind);
			if (!k || k.m_Singleton || k.m_Fixed)
			{
				ea.m_Status = "invalid";
				ea.m_Detail = "unknown-kind";
			}
			else if (k.m_ParentKind == "")
				OwnTop(ea, ba, k);
		}
	}

	protected static int Depth(DZS_PKind k)
	{
		int d = 0;
		DZS_PKind cur = k;
		while (cur && cur.m_ParentKind != "")
		{
			d++;
			cur = DZS_PKinds.Get(cur.m_ParentKind);
		}
		return d;
	}

	// The text of an own block with its own children. Fields are read into locals first: Enforce can lose a
	// caller's parameters across a recursive call.
	protected string GenOwn(DZS_POwn o, string unit)
	{
		DZS_PKind kind = o.m_Kind;
		map<string, ref DZS_Json> values = o.m_Values;
		string u = unit;
		array<DZS_POwn> kids = new array<DZS_POwn>();
		foreach (DZS_POwn k0 : o.m_Kids)
			kids.Insert(k0);
		array<string> texts = new array<string>();
		foreach (DZS_POwn kid : kids)
		{
			string kt = GenOwn(kid, u);
			texts.Insert(kt);
		}
		array<string> path = new array<string>();
		return Gen(kind, path, kind.m_Elem, values, texts, u);
	}

	protected void DependsAll(DZS_POwn o, DZS_PatchFile f)
	{
		DZS_PatchFile file = f;
		if (o.m_Entry)
			Depends(o.m_Entry, file);
		array<DZS_POwn> kids = new array<DZS_POwn>();
		foreach (DZS_POwn k0 : o.m_Kids)
			kids.Insert(k0);
		foreach (DZS_POwn kid : kids)
			DependsAll(kid, file);
	}

	protected DZS_POwn NewOwn(DZS_PatchBlock b, DZS_PatchEntry e, DZS_PKind k, map<string, ref DZS_Json> values, string file)
	{
		DZS_POwn o = new DZS_POwn();
		o.m_File = file;
		o.m_Block = b;
		o.m_Entry = e;
		o.m_Kind = k;
		o.m_Values = values;
		if (k.m_KeyAttr != "" && values.Get(k.m_KeyAttr))
			o.m_Name = values.Get(k.m_KeyAttr).m_Str;
		o.m_Ok = true;
		m_AllOwns.Insert(o);
		return o;
	}

	// An own top-level block of a kind found by name (an own event a spawn position or child names as its parent).
	protected DZS_POwn OwnByName(string kind, string name, bool ci)
	{
		string low = DZS_PatchText.Lower(name);
		foreach (DZS_POwn o : m_Owns)
		{
			if (!o.m_Ok || o.m_Kind.m_Name != kind || o.m_Into)
				continue;
			if (o.m_Name == name || (ci && DZS_PatchText.Lower(o.m_Name) == low))
				return o;
		}
		return null;
	}

	protected void OwnTop(DZS_PatchEntry e, DZS_PatchBlock b, DZS_PKind k)
	{
		string rel = k.m_File;
		if (k.m_FileInKey)
			rel = DZS_PKind.EnvFile(b.m_Data.GetStr("file"));
		if (rel == "")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-file";
			return;
		}
		DZS_PatchFile f = File(rel);
		if (!f.m_Exists && f.m_Error == "" && k.m_Skeleton != "")
		{
			array<string> skeleton = DZS_Files.Chunk(k.m_Skeleton);
			f.Load(skeleton, false);
			f.m_Exists = true;
			f.m_Fresh = true;
		}
		if (!Usable(e, f))
			return;
		string err;
		map<string, ref DZS_Json> values = Values(k, b.m_Data, err);
		if (err == "")
		{
			k.Complete(values);
			err = CheckValues(k, values);
		}
		if (err != "")
		{
			e.m_Status = "invalid";
			e.m_Detail = err;
			return;
		}
		DZS_Json key = OwnKey(k, values, "");
		DZS_PTarget t = Resolve(f, k, key, true);
		bool taken = t.m_Nodes.Count() > 0;
		foreach (DZS_POwn other : m_Owns)
		{
			if (other.m_Ok && other.m_Kind == k && !other.m_Into && SameKey(k, values, other.m_Values))
				taken = true;
		}
		if (taken)
		{
			e.m_Status = "collision";
			return;
		}
		if (t.m_Case)
			e.m_Detail = "case-mismatch";
		DZS_POwn o = NewOwn(b, e, k, values, rel);
		m_Owns.Insert(o);
		e.m_Status = "applied";
	}

	protected void OwnNested(DZS_PatchEntry e, DZS_PatchBlock b, DZS_PKind k)
	{
		string rel = k.m_File;
		if (k.m_FileInKey)
		{
			rel = "";
			if (b.m_ParentId != "")
			{
				foreach (DZS_POwn po : m_AllOwns)
				{
					if (po.m_Block && po.m_Block.m_Id == b.m_ParentId)
						rel = po.m_File;
				}
			}
			else
				rel = DZS_PKind.EnvFile(KeyStr(b.m_Parent, "file"));
		}
		if (rel == "")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-parent";
			return;
		}
		DZS_PatchFile f = File(rel);
		if (!Usable(e, f))
			return;
		string err;
		map<string, ref DZS_Json> values = Values(k, b.m_Data, err);
		if (err == "")
		{
			k.Complete(values);
			err = CheckValues(k, values);
		}
		if (err != "")
		{
			e.m_Status = "invalid";
			e.m_Detail = err;
			return;
		}
		DZS_PKind pk = DZS_PKinds.Get(k.m_ParentKind);
		string pname = "";
		DZS_POwn own = null;
		DZS_Json pkey = null;
		if (b.m_ParentId != "")
		{
			DZS_POwn byId = null;
			foreach (DZS_POwn o : m_AllOwns)
			{
				if (o.m_Block && o.m_Block.m_Id == b.m_ParentId)
					byId = o;
			}
			if (!byId || !byId.m_Ok)
			{
				e.m_Status = "orphaned";
				e.m_Detail = "parent-missing";
				return;
			}
			pname = byId.m_Name;
			if (byId.m_Kind == pk)
				own = byId;
			else if (k.m_Name == "spawnPosition" && byId.m_Kind.m_Name == "event")
			{
				pkey = DZS_Json.Obj();
				DZS_Json listName = DZS_Json.Str(pname);
				pkey.Set(pk.m_KeyAttr, listName);
			}
			else
			{
				e.m_Status = "invalid";
				e.m_Detail = "bad-parent";
				return;
			}
		}
		else
			pkey = ParentKeyOf(k, pk, b.m_Parent, pname);
		if (pname == "")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-parent";
			return;
		}
		if (k.m_Name == "spawnPosition" && DZS_PKinds.NoPositions(pname))
		{
			e.m_Status = "invalid";
			e.m_Detail = "no-positions";
			return;
		}
		DZS_XmlNode into = null;
		if (!own)
		{
			DZS_PTarget pt = Resolve(f, pk, pkey, pk.m_Dups == DZS_PKind.MERGE);
			if (pt.m_BadKey)
			{
				e.m_Status = "invalid";
				e.m_Detail = "bad-parent";
				return;
			}
			if (pt.m_Ambiguous)
			{
				e.m_Status = "ambiguous";
				return;
			}
			if (pt.m_Nodes.Count() > 0)
				into = pt.m_Nodes[pt.m_Nodes.Count() - 1];
			else
			{
				if (pk.m_ParentKind == "")
					own = OwnByName(pk.m_Name, pname, pk.m_KeyCi);
				if (!own && k.m_Name == "spawnPosition")
				{
					map<string, ref DZS_Json> listValues = new map<string, ref DZS_Json>();
					DZS_Json listJ = DZS_Json.Str(pname);
					listValues.Set("name", listJ);
					own = NewOwn(null, null, pk, listValues, rel);
					m_Owns.Insert(own);
				}
				if (!own)
				{
					e.m_Status = "orphaned";
					e.m_Detail = "parent-missing";
					if (pt.m_Case)
						e.m_Detail = "case-mismatch";
					return;
				}
			}
		}
		if (into)
		{
			DZS_Json key = OwnKey(k, values, pname);
			array<DZS_XmlNode> pool = new array<DZS_XmlNode>();
			Members(f, into, k, pool);
			DZS_PTarget hit = new DZS_PTarget();
			Filter(f, k, key, pool, hit);
			if (!k.m_Repeats && (hit.m_Nodes.Count() > 0 || Attached(k, values, null, into.m_Start)))
			{
				e.m_Status = "collision";
				return;
			}
			RememberAttached(k, values, into.m_Start);
			DZS_POwn placed = NewOwn(b, e, k, values, rel);
			placed.m_Into = into;
			m_Owns.Insert(placed);
			e.m_Status = "applied";
			return;
		}
		if (!k.m_Repeats && Attached(k, values, own, -1))
		{
			e.m_Status = "collision";
			return;
		}
		DZS_POwn child = NewOwn(b, e, k, values, rel);
		own.m_Kids.Insert(child);
		e.m_Status = "applied";
	}

	// The identity key of an own block, from its values ('pname' fills the parent part, if any).
	protected DZS_Json OwnKey(DZS_PKind k, map<string, ref DZS_Json> values, string pname)
	{
		DZS_Json key = DZS_Json.Obj();
		if (k.m_ParentKey != "")
		{
			DZS_Json pj = DZS_Json.Str(pname);
			key.Set(k.m_ParentKey, pj);
		}
		if (k.m_KeyAttr != "")
			key.Set(k.m_KeyAttr, values.Get(k.m_KeyAttr));
		if (k.IsCoord())
		{
			key.Set("x", values.Get("x"));
			key.Set("z", values.Get("z"));
		}
		if (k.IsVec())
			key.Set(k.m_Vec, values.Get(k.m_Vec));
		return key;
	}

	// Own children added to base parents in this run, by parent position, to catch two own blocks with one key.
	protected ref map<int, ref array<ref map<string, ref DZS_Json>>> m_AddedTo = new map<int, ref array<ref map<string, ref DZS_Json>>>();

	protected void RememberAttached(DZS_PKind k, map<string, ref DZS_Json> values, int parentStart)
	{
		array<ref map<string, ref DZS_Json>> list = m_AddedTo.Get(parentStart);
		if (!list)
		{
			list = new array<ref map<string, ref DZS_Json>>();
			m_AddedTo.Set(parentStart, list);
		}
		list.Insert(values);
	}

	protected bool Attached(DZS_PKind k, map<string, ref DZS_Json> values, DZS_POwn own, int parentStart)
	{
		if (own)
		{
			foreach (DZS_POwn kid : own.m_Kids)
			{
				if (kid.m_Kind == k && SameKey(k, values, kid.m_Values))
					return true;
			}
			return false;
		}
		array<ref map<string, ref DZS_Json>> list = m_AddedTo.Get(parentStart);
		if (!list)
			return false;
		foreach (map<string, ref DZS_Json> other : list)
		{
			if (SameKey(k, values, other))
				return true;
		}
		return false;
	}

	protected static bool SameKey(DZS_PKind k, map<string, ref DZS_Json> a, map<string, ref DZS_Json> b)
	{
		if (k.m_KeyAttr != "")
		{
			string na = a.Get(k.m_KeyAttr).m_Str;
			string nb = b.Get(k.m_KeyAttr).m_Str;
			if (k.m_KeyCi)
			{
				if (DZS_PatchText.Lower(na) != DZS_PatchText.Lower(nb))
					return false;
			}
			else if (na != nb)
				return false;
		}
		if (k.IsCoord())
		{
			string ax = a.Get("x").m_Str;
			string bx = b.Get("x").m_Str;
			string az = a.Get("z").m_Str;
			string bz = b.Get("z").m_Str;
			if (!DZS_PatchNum.Near(ax, bx, k.m_Tol) || !DZS_PatchNum.Near(az, bz, k.m_Tol))
				return false;
		}
		if (k.IsVec())
		{
			array<string> va = new array<string>();
			array<string> vb = new array<string>();
			if (!VecOf(a.Get(k.m_Vec), va) || !VecOf(b.Get(k.m_Vec), vb) || !NearVec(va, vb, k.m_Tol))
				return false;
		}
		return true;
	}

	// Puts an own child into a base parent: into its container element (made when missing), before siblings the
	// kind's order puts after it (a loot container goes before the fixed parts), or at the end.
	protected void InsertInto(DZS_PatchFile f, DZS_PKind pk, DZS_PKind k, DZS_XmlNode parent, string text)
	{
		if (k.m_Container == "")
		{
			int mine = pk.m_Order.Find(k.m_Elem);
			bool later = false;
			foreach (DZS_XmlNode s : parent.m_Children)
			{
				if (!f.IsRemoved(s) && mine >= 0 && pk.m_Order.Find(s.m_Name) > mine)
					later = true;
			}
			if (later)
				InsertOrdered(f, pk, parent, k.m_Elem, text);
			else
				f.AppendChild(parent, text);
			return;
		}
		foreach (DZS_XmlNode c : parent.m_Children)
		{
			if (c.m_Name == k.m_Container && !f.IsRemoved(c))
			{
				f.AppendChild(c, text);
				return;
			}
		}
		string box = "<" + k.m_Container + ">\n" + f.m_Unit + DZS_PatchFile.Reindent(text, f.m_Unit, "\n") + "\n</" + k.m_Container + ">";
		InsertOrdered(f, pk, parent, k.m_Container, box);
	}

	protected void OwnWeather(DZS_PatchEntry e, DZS_PatchBlock b)
	{
		DZS_PKind k = DZS_PKinds.Get("weather");
		DZS_PatchFile f = File(k.m_File);
		string err;
		map<string, ref DZS_Json> values = Values(k, b.m_Data, err);
		if (err == "")
		{
			DZS_Json on = DZS_Json.Num("1");
			values.Set("enable", on);
			err = CheckValues(k, values);
		}
		if (err != "")
		{
			e.m_Status = "invalid";
			e.m_Detail = err;
			return;
		}
		if (!f.m_Exists && f.m_Error == "")
		{
			array<string> none = new array<string>();
			string body = Gen(k, none, "weather", values, null, "    ");
			string text = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n" + body + "\n";
			array<string> chunks = DZS_Files.Chunk(text);
			DZS_PatchFile made = m_Files.Create(k.m_File, chunks);
			e.m_Status = "applied";
			Depends(e, made);
			return;
		}
		if (!Usable(e, f))
			return;
		DZS_XmlNode root = f.m_Xml.m_Root;
		if (root.m_Name != "weather")
		{
			e.m_Status = "invalid";
			e.m_Detail = "base-invalid";
			return;
		}
		bool same = true;
		foreach (DZS_PField fc : k.m_Fields)
		{
			if (!FieldIs(f, root, fc, values.Get(fc.m_Name)))
				same = false;
		}
		if (same)
		{
			e.m_Status = "already";
			return;
		}
		array<string> done = new array<string>();
		foreach (DZS_PField fld : k.m_Fields)
		{
			DZS_Json v = values.Get(fld.m_Name);
			if (FieldIs(f, root, fld, v))
				continue;
			string serr = SetField(f, k, root, fld, v, values, done);
			if (serr != "")
			{
				e.m_Status = "invalid";
				e.m_Detail = serr;
				return;
			}
		}
		e.m_Status = "applied";
		Depends(e, f);
	}

	// An own block that is a file of its own (spawner group, gear preset, restricted area), registered in cfggameplay.json.
	protected void OwnFile(DZS_PatchEntry e, DZS_PatchBlock b)
	{
		if (!SafeName(m_Project.m_Slug, true))
		{
			e.m_Status = "invalid";
			e.m_Detail = "no-slug";
			return;
		}
		if (!SafeName(b.m_Id, false))
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-id";
			return;
		}
		string err;
		string regList = SPAWNERS;
		array<string> chunks;
		if (b.m_Kind == "gearPreset")
		{
			regList = GEAR;
			chunks = GearText(b, err);
		}
		else if (b.m_Kind == "restrictedArea")
		{
			regList = PRA;
			chunks = PraText(b, err);
		}
		else
			chunks = SpawnerText(b, err);
		if (err != "")
		{
			e.m_Status = "invalid";
			e.m_Detail = err;
			return;
		}
		string path = SpawnerPath(b);
		DZS_PatchFile made = m_Files.Create(path, chunks);
		AddRegistration(made, e, regList);
		e.m_Status = "applied";
		Depends(e, made);
	}

	protected void AddRegistration(DZS_PatchFile f, DZS_PatchEntry e, string list)
	{
		m_RegFiles.Insert(f);
		m_RegEntries.Insert(e);
		m_RegLists.Insert(list);
	}

	// ---- registrations ----

	static string NormPath(string p)
	{
		string r = p;
		r.Replace("\\", "/");
		while (r.IndexOf("/") == 0)
			r = r.Substring(1, r.Length() - 1);
		return r;
	}

	protected void Registrations()
	{
		if (m_RegFiles.Count() == 0)
			return;
		DZS_PatchFile g = File(GAMEPLAY_FILE);
		DZS_SrcNode root = null;
		if (g.Ok())
			root = g.m_Json.m_Root;
		array<string> lists = new array<string>();
		lists.Insert(SPAWNERS);
		lists.Insert(GEAR);
		lists.Insert(PRA);
		array<string> done = new array<string>();
		foreach (string path : lists)
		{
			array<int> mine = new array<int>();
			for (int i = 0; i < m_RegLists.Count(); i++)
			{
				if (m_RegLists[i] == path)
					mine.Insert(i);
			}
			if (mine.Count() == 0)
				continue;
			bool ok = g.Ok() && root != null;
			DZS_SrcNode list = null;
			if (ok)
			{
				list = JsonAt(g, root, path);
				if (list && !list.IsArr())
					ok = false;
			}
			if (!ok)
			{
				foreach (int m : mine)
				{
					DZS_PatchEntry se = m_RegEntries[m];
					Usable(se, g);
					if (se.m_Status != "invalid")
					{
						se.m_Status = "invalid";
						se.m_Detail = "base-invalid";
					}
					m_RegFiles[m].m_Out = null;
				}
				continue;
			}
			RegisterIn(g, root, path, list, mine);
			done.Insert(path);
		}
		if (done.Count() == 0)
			return;
		for (int n = 0; n < m_RegEntries.Count(); n++)
		{
			DZS_PatchEntry sp = m_RegEntries[n];
			if (done.Find(m_RegLists[n]) >= 0 && g.m_Entries.Find(sp) < 0)
				g.m_Entries.Insert(sp);
		}
		m_Report.Note("enableCfgGameplayFile");
	}

	// Lists the own files of one registration list: duplicates in the base go, every file is listed once, existing
	// entries keep their order. A restricted-area list made new on Sakhal keeps the map's warhead storage.
	protected void RegisterIn(DZS_PatchFile g, DZS_SrcNode root, string path, DZS_SrcNode list, array<int> mine)
	{
		array<string> seen = new array<string>();
		if (list)
		{
			for (int k = 0; k < list.Count(); k++)
			{
				DZS_SrcNode item = list.m_Items[k];
				if (!item.IsStr())
					continue;
				string norm = NormPath(item.m_Str);
				if (seen.Find(norm) >= 0)
				{
					g.RemoveItem(list, k);
					DZS_PatchEntry de = m_Report.Add("registration", -1, "deduplicated");
					de.m_Field = path;
					de.m_Value = item.m_Str;
					Depends(de, g);
					continue;
				}
				seen.Insert(norm);
			}
		}
		DZS_Json added = DZS_Json.Arr();
		if (!list && path == PRA && DZS_PatchText.Lower(m_Project.m_Map) == "sakhal")
		{
			DZS_PatchEntry wh = m_Report.Add("registration", -1, "applied");
			wh.m_Field = path;
			wh.m_Value = SAKHAL_PRA;
			Depends(wh, g);
			seen.Insert(SAKHAL_PRA);
			DZS_Json whj = DZS_Json.Str(SAKHAL_PRA);
			added.Add(whj);
		}
		foreach (int n : mine)
		{
			DZS_PatchFile sf = m_RegFiles[n];
			DZS_PatchEntry re = m_Report.Add("registration", -1, "applied");
			re.m_Field = path;
			re.m_Value = sf.m_Path;
			re.m_Id = m_RegEntries[n].m_Id;
			m_RegEntries[n].m_File = sf.m_Path;
			Depends(re, g);
			if (seen.Find(sf.m_Path) >= 0)
			{
				re.m_Status = "already";
				continue;
			}
			seen.Insert(sf.m_Path);
			DZS_Json pj = DZS_Json.Str(sf.m_Path);
			if (list)
				g.AppendItem(list, "", pj);
			else
				added.Add(pj);
			m_Gameplay = true;
		}
		if (!list && added.Count() > 0)
			JsonSet(g, root, path, added);
	}

	// ---- finishing ----

	// One file finished: first every file read from the base, then every file the run created (registered ones only
	// when cfggameplay.json could take them). True when all are done.
	protected bool FinishStep()
	{
		if (m_FPass == 0)
		{
			if (m_At >= m_FAll.Count())
			{
				DZS_PatchFile g = m_Files.m_Files.Get(GAMEPLAY_FILE);
				m_FGameplayOk = !g || m_Out.Contains(g.m_Path) || !g.m_Changed;
				m_FPass = 1;
				m_At = 0;
				return false;
			}
			DZS_PatchFile f = m_FAll[m_At];
			m_At++;
			FinishRead(f);
			return false;
		}
		if (m_At >= m_FAll.Count())
		{
			m_FAll = null;
			return true;
		}
		DZS_PatchFile c = m_FAll[m_At];
		m_At++;
		if (!c.m_Created || !c.m_Out)
			return false;
		if (m_RegFiles.Find(c) >= 0 && !m_FGameplayOk)
		{
			Refuse(c, "registration");
			return false;
		}
		string bad2 = "";
		if (c.m_IsJson && DZS_PatchText.Lower(c.m_Path).IndexOf(".json") > 0)
			bad2 = EngineCheck(DZS_PatchText.Lower(c.m_Path), c.m_Out);
		if (bad2 != "")
		{
			Refuse(c, bad2);
			return false;
		}
		m_Report.FileNote(c.m_Path, "created", "");
		Output(c.m_Path, c.m_Out);
		return false;
	}

	protected void FinishRead(DZS_PatchFile f)
	{
		if (!f.m_Exists)
		{
			if (f.m_Error != "")
				m_Report.FileNote(f.m_Path, "broken", f.m_Error);
			else
				m_Report.FileNote(f.m_Path, "missing", "");
			return;
		}
		if (f.m_Error != "")
		{
			m_Report.FileNote(f.m_Path, "broken", f.m_Error);
			return;
		}
		if (f.m_Created)
			return;
		bool changed = f.Finish();
		if (f.m_Fresh)
		{
			if (changed)
			{
				m_Report.FileNote(f.m_Path, "created", "");
				Output(f.m_Path, f.m_Out);
			}
			else
				m_Report.FileNote(f.m_Path, "missing", "");
			return;
		}
		if (changed && f.m_IsJson)
		{
			string bad = EngineCheck(DZS_PatchText.Lower(f.m_Path), f.m_Out);
			if (bad != "")
			{
				Refuse(f, bad);
				return;
			}
		}
		if (changed)
		{
			m_Report.FileNote(f.m_Path, "written", "");
			Output(f.m_Path, f.m_Out);
			if (DZS_PatchText.Lower(f.m_Path) == GAMEPLAY_FILE)
				m_Report.Note("enableCfgGameplayFile");
		}
		else
			m_Report.FileNote(f.m_Path, "unchanged", "");
	}

	protected void Refuse(DZS_PatchFile f, string why)
	{
		m_Report.FileNote(f.m_Path, "invalid", why);
		foreach (DZS_PatchEntry e : f.m_Entries)
		{
			e.m_Status = "invalid";
			e.m_Detail = "output-invalid";
		}
		f.m_Out = null;
	}

	protected void Output(string path, array<string> chunks)
	{
		m_Out.Set(path, chunks);
		m_OutPaths.Insert(path);
		m_OutPaths.Sort();
	}
}
