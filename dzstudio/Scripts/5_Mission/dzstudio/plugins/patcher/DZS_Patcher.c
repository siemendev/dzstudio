// Applies a patch project to a mission folder: changes in project order, then own blocks, then registrations; every
// touched file is edited in place (byte-preserving) and checked. Nothing is written to disk here: the result is the
// new file contents and the report. Same input, same bytes.
class DZS_Patcher : DZS_PatcherJson
{
	ref map<string, ref array<string>> m_Out = new map<string, ref array<string>>();
	ref array<string> m_OutPaths = new array<string>();
	protected ref array<ref DZS_POwn> m_Owns = new array<ref DZS_POwn>();
	protected ref array<DZS_POwn> m_AllOwns = new array<DZS_POwn>();
	protected ref array<DZS_PatchFile> m_SpawnerFiles = new array<DZS_PatchFile>();
	protected ref array<DZS_PatchEntry> m_SpawnerEntries = new array<DZS_PatchEntry>();
	protected bool m_Gameplay;

	static DZS_Patcher Run(DZS_PatchProject project, string baseDir, map<string, string> over = null)
	{
		DZS_Patcher p = new DZS_Patcher();
		p.m_Project = project;
		p.m_Files = new DZS_PatchFiles(baseDir);
		if (over)
			p.m_Files.m_Over.Copy(over);
		p.Work();
		return p;
	}

	protected void Work()
	{
		for (int i = 0; i < m_Project.m_Changes.Count(); i++)
		{
			DZS_PatchChange c = m_Project.m_Changes[i];
			DZS_PatchEntry e = ChangeEntry(i, c);
			Change(e, c);
		}
		OwnBlocks();
		Registrations();
		FinishFiles();
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
		if (remove && k.m_Singleton)
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

	protected void OwnBlocks()
	{
		array<DZS_PatchEntry> entries = new array<DZS_PatchEntry>();
		for (int i = 0; i < m_Project.m_Blocks.Count(); i++)
		{
			DZS_PatchBlock b = m_Project.m_Blocks[i];
			DZS_PatchEntry e = BlockEntry(i, b);
			entries.Insert(e);
		}
		array<string> areaNames = new array<string>();
		array<string> safeKeys = new array<string>();
		array<string> ids = new array<string>();
		bool weather = false;
		for (int a = 0; a < m_Project.m_Blocks.Count(); a++)
		{
			DZS_PatchBlock ba = m_Project.m_Blocks[a];
			DZS_PatchEntry ea = entries[a];
			if (ids.Find(ba.m_Id) >= 0 || ba.m_Id == "")
			{
				ea.m_Status = "invalid";
				ea.m_Detail = "bad-id";
				continue;
			}
			ids.Insert(ba.m_Id);
			if (ba.m_Kind == "effectArea")
				OwnArea(ea, ba, areaNames);
			else if (ba.m_Kind == "safePosition")
				OwnSafe(ea, ba, safeKeys);
			else if (ba.m_Kind == "spawnerGroup")
				OwnSpawner(ea, ba);
			else if (ba.m_Kind == "weather")
			{
				if (weather)
				{
					ea.m_Status = "collision";
					continue;
				}
				weather = true;
				OwnWeather(ea, ba);
			}
			else
			{
				DZS_PKind k = DZS_PKinds.Get(ba.m_Kind);
				if (!k || k.m_Singleton)
				{
					ea.m_Status = "invalid";
					ea.m_Detail = "unknown-kind";
				}
				else if (k.m_ParentKind == "")
					OwnTop(ea, ba, k);
			}
		}
		// nested blocks by depth, so a loot point can go into an own container of an own loot type
		for (int depth = 1; depth <= 2; depth++)
		{
			for (int n = 0; n < m_Project.m_Blocks.Count(); n++)
			{
				DZS_PatchBlock bn = m_Project.m_Blocks[n];
				DZS_PatchEntry en = entries[n];
				if (en.m_Status != "")
					continue;
				DZS_PKind kn = DZS_PKinds.Get(bn.m_Kind);
				if (kn && kn.m_ParentKind != "" && Depth(kn) == depth)
					OwnNested(en, bn, kn);
			}
		}
		foreach (DZS_POwn o : m_Owns)
		{
			if (!o.m_Ok)
				continue;
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

	protected void OwnSpawner(DZS_PatchEntry e, DZS_PatchBlock b)
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
		array<string> chunks = SpawnerText(b, err);
		if (err != "")
		{
			e.m_Status = "invalid";
			e.m_Detail = err;
			return;
		}
		string path = SpawnerPath(b);
		DZS_PatchFile made = m_Files.Create(path, chunks);
		m_SpawnerFiles.Insert(made);
		m_SpawnerEntries.Insert(e);
		e.m_Status = "applied";
		Depends(e, made);
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
		if (m_SpawnerFiles.Count() == 0)
			return;
		DZS_PatchFile g = File(GAMEPLAY_FILE);
		bool ok = true;
		DZS_SrcNode list = null;
		if (!g.Ok())
			ok = false;
		else
		{
			list = JsonAt(g, g.m_Json.m_Root, SPAWNERS);
			if (list && !list.IsArr())
				ok = false;
		}
		if (!ok)
		{
			for (int i = 0; i < m_SpawnerEntries.Count(); i++)
			{
				DZS_PatchEntry se = m_SpawnerEntries[i];
				Usable(se, g);
				if (se.m_Status != "invalid")
				{
					se.m_Status = "invalid";
					se.m_Detail = "base-invalid";
				}
				m_SpawnerFiles[i].m_Out = null;
			}
			return;
		}
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
					de.m_Field = SPAWNERS;
					de.m_Value = item.m_Str;
					Depends(de, g);
					continue;
				}
				seen.Insert(norm);
			}
		}
		DZS_Json added = DZS_Json.Arr();
		for (int n = 0; n < m_SpawnerFiles.Count(); n++)
		{
			DZS_PatchFile sf = m_SpawnerFiles[n];
			DZS_PatchEntry re = m_Report.Add("registration", -1, "applied");
			re.m_Field = SPAWNERS;
			re.m_Value = sf.m_Path;
			re.m_Id = m_SpawnerEntries[n].m_Id;
			m_SpawnerEntries[n].m_File = sf.m_Path;
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
			JsonSet(g, g.m_Json.m_Root, SPAWNERS, added);
		foreach (DZS_PatchEntry sp : m_SpawnerEntries)
		{
			if (g.m_Entries.Find(sp) < 0)
				g.m_Entries.Insert(sp);
		}
		m_Report.Note("enableCfgGameplayFile");
	}

	// ---- finishing ----

	protected void FinishFiles()
	{
		array<DZS_PatchFile> all = m_Files.All();
		DZS_PatchFile gameplay = null;
		foreach (DZS_PatchFile f : all)
		{
			if (!f.m_Exists)
			{
				if (f.m_Error != "")
					m_Report.FileNote(f.m_Path, "broken", f.m_Error);
				else
					m_Report.FileNote(f.m_Path, "missing", "");
				continue;
			}
			if (f.m_Error != "")
			{
				m_Report.FileNote(f.m_Path, "broken", f.m_Error);
				continue;
			}
			if (f.m_Created)
				continue;
			bool changed = f.Finish();
			if (changed && f.m_IsJson)
			{
				string bad = EngineCheck(DZS_PatchText.Lower(f.m_Path), f.m_Out);
				if (bad != "")
				{
					Refuse(f, bad);
					continue;
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
		DZS_PatchFile g = m_Files.m_Files.Get(GAMEPLAY_FILE);
		bool gameplayOk = !g || m_Out.Contains(g.m_Path) || !g.m_Changed;
		foreach (DZS_PatchFile c : all)
		{
			if (!c.m_Created || !c.m_Out)
				continue;
			if (m_SpawnerFiles.Find(c) >= 0 && !gameplayOk)
			{
				Refuse(c, "registration");
				continue;
			}
			string bad2 = "";
			if (c.m_IsJson && DZS_PatchText.Lower(c.m_Path).IndexOf(".json") > 0)
				bad2 = EngineCheck(DZS_PatchText.Lower(c.m_Path), c.m_Out);
			if (bad2 != "")
			{
				Refuse(c, bad2);
				continue;
			}
			m_Report.FileNote(c.m_Path, "created", "");
			Output(c.m_Path, c.m_Out);
		}
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
