// The elements a layer (Spawns, Effects, Territories) builds its blocks and changes from: those of the open project,
// the operation that restores them, or a scratch set that stands for no project (DZS_LayerTake). A restored element
// keeps the id of the block it was made of in its field BID, so a block made elsewhere keeps its id at the next save.
class DZS_LayerSrc : Managed
{
	static const string BID = "bid";

	DZS_Instance m_Inst;
	protected DZS_OpBuilder m_B;
	protected ref array<string> m_Ids = new array<string>();
	protected ref map<string, string> m_Kinds = new map<string, string>();
	protected ref map<string, ref map<string, string>> m_Fields = new map<string, ref map<string, string>>();

	static DZS_LayerSrc Of(DZS_Instance inst)
	{
		DZS_LayerSrc s = new DZS_LayerSrc();
		s.m_Inst = inst;
		return s;
	}

	static DZS_LayerSrc Into(DZS_OpBuilder b)
	{
		DZS_LayerSrc s = new DZS_LayerSrc();
		s.m_B = b;
		return s;
	}

	static DZS_LayerSrc Scratch()
	{
		return new DZS_LayerSrc();
	}

	array<string> All(string kind)
	{
		if (m_Inst)
			return DZS_SpawnsModel.Of(m_Inst, kind);
		array<string> r = new array<string>();
		foreach (string id : m_Ids)
		{
			if (m_Kinds.Get(id) == kind)
				r.Insert(id);
		}
		return r;
	}

	string Field(string id, string key)
	{
		if (m_Inst)
			return DZS_SpawnsModel.Field(m_Inst, id, key);
		map<string, string> f = m_Fields.Get(id);
		string v;
		if (f)
			f.Find(key, v);
		return v;
	}

	// A restored element; blockId: the block it is made of ("" for a change), prefix: what the editor's own block ids
	// of the kind start with.
	string Add(string kind, map<string, string> fields, string blockId = "", string prefix = "")
	{
		if (blockId != "")
			fields.Set(BID, blockId);
		if (m_B)
		{
			if (blockId != "")
				return DZS_SpawnsModel.CreateFor(m_B, kind, fields, blockId, prefix);
			return m_B.Create(kind, DZS_PROJECT_ID, DZS_END, fields);
		}
		string id = "e" + m_Ids.Count();
		m_Ids.Insert(id);
		m_Kinds.Set(id, kind);
		map<string, string> copy = new map<string, string>();
		copy.Copy(fields);
		m_Fields.Set(id, copy);
		return id;
	}

	// The opened file's extensions of a block or change into the element being restored.
	void TakeExts(map<string, string> fields, string slot, DZS_Json entity)
	{
		if (m_B)
			DZS_EntityExts.TakeInto(fields, slot, entity);
	}

	void Attach(DZS_Json entity, string id, string slot)
	{
		if (m_Inst)
			DZS_EntityExts.Attach(entity, m_Inst, id, slot);
	}

	// The block id of an element: the one of the block it was restored from, else prefix + its element id.
	string BlockId(string id, string prefix)
	{
		string b = Field(id, BID);
		if (b != "")
			return b;
		return prefix + DZS_ProjectV2.IdPart(id);
	}
}

// Opening a project: the layers' parts are taken from the file's own blocks and changes, never from the copy the
// editor part holds. They are the truth — dzbot's workbench edits them and leaves the editor part as it was — so a
// change made there since the last save here is not overwritten by the older copy, and blocks made there show up in
// the layers. A layer takes an entry when it would write it back exactly as it is (same block ids, same values; key
// order aside); every other entry stays carried over unchanged (DZS_KeptBlocks) and is shown read-only.
class DZS_LayerTake
{
	static const string SPAWNS = "spawns";
	static const string EFFECTS = "effects";
	static const string TERR = "territories";

	protected ref DZS_Json m_Blocks;
	protected ref DZS_Json m_Changes;
	protected ref array<bool> m_UsedB = new array<bool>();
	protected ref array<bool> m_UsedC = new array<bool>();

	// part.plugins.spawns/effects/territories made anew from the root's blocks and changes.
	static void Put(DZS_Json root, DZS_Json part)
	{
		DZS_Json plugins = part.Get("plugins");
		if (!plugins || !plugins.IsObj())
		{
			plugins = DZS_Json.Obj();
			part.Set("plugins", plugins);
		}
		DZS_LayerTake t = new DZS_LayerTake();
		t.m_Blocks = root.Get("blocks");
		if (!t.m_Blocks || !t.m_Blocks.IsArr())
			t.m_Blocks = DZS_Json.Arr();
		t.m_Changes = root.Get("changes");
		if (!t.m_Changes || !t.m_Changes.IsArr())
			t.m_Changes = DZS_Json.Arr();
		for (int i = 0; i < t.m_Blocks.Count(); i++)
			t.m_UsedB.Insert(false);
		for (int k = 0; k < t.m_Changes.Count(); k++)
			t.m_UsedC.Insert(false);
		t.PutLayer(plugins, TERR, t.Territories());
		t.PutLayer(plugins, SPAWNS, t.Spawns());
		t.PutLayer(plugins, EFFECTS, t.Effects());
	}

	protected void PutLayer(DZS_Json plugins, string name, DZS_Json frag)
	{
		if (!frag || frag.Get("blocks").Count() + frag.Get("changes").Count() == 0)
		{
			plugins.Remove(name);
			return;
		}
		plugins.Set(name, frag);
	}

	// ---- one layer each ----

	protected DZS_Json Spawns()
	{
		DZS_Json frag = Frag();
		map<string, ref DZS_Json> evById = new map<string, ref DZS_Json>();
		map<string, ref DZS_Json> evByName = new map<string, ref DZS_Json>();
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			DZS_Json ev = m_Blocks.At(i);
			if (m_UsedB[i] || !ev.IsObj() || ev.GetStr("kind") != "event" || !DataOf(ev) || !IsTop(ev))
				continue;
			string evId = ev.GetStr("id");
			array<int> unit = new array<int>();
			unit.Insert(i);
			for (int c = 0; c < m_Blocks.Count(); c++)
			{
				DZS_Json ch = m_Blocks.At(c);
				if (c != i && ch.IsObj() && ch.Get("parent") && ch.Get("parent").IsStr() && ch.Get("parent").m_Str == evId && ch.GetStr("kind") != "spawnPosition")
				{
					if (!DataOf(ch))
						unit.Clear();
					else if (unit.Count() > 0)
						unit.Insert(c);
				}
			}
			if (unit.Count() == 0)
				continue;
			DZS_Json units = Pick(unit, null);
			if (!Holds(SPAWNS, units, null))
				continue;
			Claim(frag, unit, null);
			DZS_Json both = units.Get("blocks");
			evById.Set(evId, both);
			evByName.Set(DataOf(ev).GetStr("name"), both);
		}
		for (int p = 0; p < m_Blocks.Count(); p++)
		{
			DZS_Json pos = m_Blocks.At(p);
			if (m_UsedB[p] || !pos.IsObj() || pos.GetStr("kind") != "spawnPosition" || !DataOf(pos))
				continue;
			DZS_Json par = pos.Get("parent");
			DZS_Json context = null;
			if (par && par.IsStr())
			{
				context = evById.Get(par.m_Str);
				if (!context)
					continue;
			}
			else if (par && par.IsObj())
				context = evByName.Get(par.GetStr("event"));
			else
				continue;
			array<int> one = new array<int>();
			one.Insert(p);
			DZS_Json pu = Pick(one, null);
			if (Holds(SPAWNS, pu, context))
				Claim(frag, one, null);
		}
		for (int k = 0; k < m_Changes.Count(); k++)
		{
			DZS_Json c2 = m_Changes.At(k);
			if (m_UsedC[k] || !c2.IsObj() || !Keyed(c2))
				continue;
			string blk = c2.GetStr("block");
			if (blk == "spawnPosition")
			{
				if (c2.GetStr("op") != "remove" && (!c2.Get("to") || !c2.Get("to").IsObj()))
					continue;
			}
			else if (blk == "event")
			{
				if (c2.GetStr("op") != "set" || c2.GetStr("field") != "nominal" || TerritoryEvent(c2.Get("key").GetStr("name")))
					continue;
			}
			else if (blk == "eventChild")
			{
				if (c2.GetStr("op") != "set" || c2.GetStr("field") != "max")
					continue;
			}
			else
				continue;
			array<int> cu = new array<int>();
			cu.Insert(k);
			DZS_Json cf = Pick(null, cu);
			if (Holds(SPAWNS, cf, null))
				Claim(frag, null, cu);
		}
		return frag;
	}

	protected DZS_Json Effects()
	{
		DZS_Json frag = Frag();
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			DZS_Json b = m_Blocks.At(i);
			if (m_UsedB[i] || !b.IsObj() || b.GetStr("kind") != "effectArea" || !IsTop(b))
				continue;
			DZS_Json d = DataOf(b);
			if (!d || !HasObj(d, "Data") || !HasObj(d, "PlayerData") || !d.Get("Data").Get("Radius") || !d.Get("Data").Get("Pos") || !d.Get("Data").Get("Pos").IsArr())
				continue;
			array<int> one = new array<int>();
			one.Insert(i);
			DZS_Json u = Pick(one, null);
			if (Holds(EFFECTS, u, null))
				Claim(frag, one, null);
		}
		// one element per base area: all its changes or none
		array<string> names = new array<string>();
		map<string, ref array<int>> byName = new map<string, ref array<int>>();
		for (int k = 0; k < m_Changes.Count(); k++)
		{
			DZS_Json c = m_Changes.At(k);
			if (m_UsedC[k] || !c.IsObj() || c.GetStr("block") != "effectArea" || !Keyed(c))
				continue;
			string n = c.Get("key").GetStr("name");
			array<int> list = byName.Get(n);
			if (!list)
			{
				list = new array<int>();
				byName.Set(n, list);
				names.Insert(n);
			}
			list.Insert(k);
		}
		foreach (string name : names)
		{
			array<int> group = byName.Get(name);
			bool ok = true;
			foreach (int at : group)
			{
				DZS_Json gc = m_Changes.At(at);
				if (gc.GetStr("op") == "remove")
					continue;
				if (!gc.Get("to") || !gc.Get("from"))
					ok = false;
			}
			if (!ok)
				continue;
			DZS_Json gu = Pick(null, group);
			if (Holds(EFFECTS, gu, null))
				Claim(frag, null, group);
		}
		return frag;
	}

	protected DZS_Json Territories()
	{
		DZS_Json frag = Frag();
		map<string, ref DZS_Json> terrs = new map<string, ref DZS_Json>();
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			DZS_Json t = m_Blocks.At(i);
			if (m_UsedB[i] || !t.IsObj() || t.GetStr("kind") != "territory" || !DataOf(t) || !IsTop(t))
				continue;
			array<int> one = new array<int>();
			one.Insert(i);
			DZS_Json tu = Pick(one, null);
			if (!Holds(TERR, tu, null))
				continue;
			Claim(frag, one, null);
			terrs.Set(t.GetStr("id"), tu.Get("blocks"));
		}
		for (int z = 0; z < m_Blocks.Count(); z++)
		{
			DZS_Json zb = m_Blocks.At(z);
			if (m_UsedB[z] || !zb.IsObj() || zb.GetStr("kind") != "territoryZone" || !DataOf(zb))
				continue;
			DZS_Json par = zb.Get("parent");
			DZS_Json context = null;
			if (par && par.IsStr())
			{
				context = terrs.Get(par.m_Str);
				if (!context)
					continue;
			}
			else if (!par || !par.IsObj())
				continue;
			array<int> zone = new array<int>();
			zone.Insert(z);
			DZS_Json zu = Pick(zone, null);
			if (Holds(TERR, zu, context))
				Claim(frag, zone, null);
		}
		array<string> keys = new array<string>();
		map<string, ref array<int>> byKey = new map<string, ref array<int>>();
		for (int k = 0; k < m_Changes.Count(); k++)
		{
			DZS_Json c = m_Changes.At(k);
			if (m_UsedC[k] || !c.IsObj() || !Keyed(c))
				continue;
			string blk = c.GetStr("block");
			if (blk == "event")
			{
				if (c.GetStr("op") != "set" || c.GetStr("field") != "nominal" || !TerritoryEvent(c.Get("key").GetStr("name")) || !c.Get("to"))
					continue;
				array<int> nu = new array<int>();
				nu.Insert(k);
				DZS_Json nf = Pick(null, nu);
				if (Holds(TERR, nf, null))
					Claim(frag, null, nu);
				continue;
			}
			if (blk != "territoryZone")
				continue;
			DZS_Json key = c.Get("key");
			if (!key.Get("x") || !key.Get("z") || (c.GetStr("op") != "remove" && !c.Get("to")))
				continue;
			string id = key.GetStr("file") + ":" + key.GetStr("name") + ":" + key.Get("x").m_Str + ":" + key.Get("z").m_Str;
			array<int> list = byKey.Get(id);
			if (!list)
			{
				list = new array<int>();
				byKey.Set(id, list);
				keys.Insert(id);
			}
			list.Insert(k);
		}
		foreach (string zk : keys)
		{
			array<int> group = byKey.Get(zk);
			DZS_Json gu = Pick(null, group);
			if (Holds(TERR, gu, null))
				Claim(frag, null, group);
		}
		return frag;
	}

	static bool TerritoryEvent(string name)
	{
		return name.IndexOf("Animal") == 0 || name.IndexOf("Ambient") == 0 || name.IndexOf("Infected") == 0;
	}

	// ---- the round trip ----

	// Whether the layer, restoring the unit's entries (after the context's blocks) and building them again, writes
	// exactly the unit and the context.
	static bool Holds(string layer, DZS_Json unit, DZS_Json contextBlocks)
	{
		DZS_Json input = Frag();
		if (contextBlocks)
		{
			for (int c = 0; c < contextBlocks.Count(); c++)
				input.Get("blocks").Add(contextBlocks.At(c));
		}
		DZS_Json ub = unit.Get("blocks");
		for (int b = 0; b < ub.Count(); b++)
			input.Get("blocks").Add(ub.At(b));
		DZS_Json uc = unit.Get("changes");
		for (int k = 0; k < uc.Count(); k++)
			input.Get("changes").Add(uc.At(k));
		DZS_LayerSrc s = DZS_LayerSrc.Scratch();
		DZS_Json built;
		if (layer == SPAWNS)
		{
			DZS_SpawnsModel.Read(s, input);
			built = DZS_SpawnsModel.Build(s);
		}
		else if (layer == EFFECTS)
		{
			DZS_EffectsModel.Read(s, input);
			built = DZS_EffectsModel.Build(s);
		}
		else
		{
			DZS_TerrModel.Read(s, input);
			built = DZS_TerrModel.Build(s);
		}
		if (!built)
			return false;
		return Same(input.Get("blocks"), built.Get("blocks")) && Same(input.Get("changes"), built.Get("changes"));
	}

	// The same entries in any order, extensions and a null parent aside, object members in any order.
	static bool Same(DZS_Json a, DZS_Json b)
	{
		if (a.Count() != b.Count())
			return false;
		array<string> ta = new array<string>();
		array<string> tb = new array<string>();
		for (int i = 0; i < a.Count(); i++)
		{
			ta.Insert(Canon(a.At(i), true));
			tb.Insert(Canon(b.At(i), true));
		}
		ta.Sort();
		tb.Sort();
		for (int k = 0; k < ta.Count(); k++)
		{
			if (ta[k] != tb[k])
				return false;
		}
		return true;
	}

	static string Canon(DZS_Json v, bool entry)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		WriteCanon(w, v, entry);
		array<string> parts = w.Finish();
		return DZS_Files.Join(parts);
	}

	protected static void WriteCanon(DZS_TextWriter w, DZS_Json v, bool entry)
	{
		if (v.m_Type == DZS_Json.NUM)
		{
			w.Append(DZS_ProjectRefs.PhpNumber(v.m_Str));
			return;
		}
		if (v.m_Type == DZS_Json.ARR)
		{
			w.Append("[");
			for (int i = 0; i < v.Count(); i++)
			{
				if (i > 0)
					w.Append(",");
				WriteCanon(w, v.At(i), false);
			}
			w.Append("]");
			return;
		}
		if (v.m_Type == DZS_Json.OBJ)
		{
			array<string> keys = new array<string>();
			foreach (string key : v.m_Keys)
			{
				DZS_Json m = v.Get(key);
				if (key == DZS_EntityExts.MEMBER || (entry && key == "parent" && m.m_Type == DZS_Json.NUL))
					continue;
				keys.Insert(key);
			}
			keys.Sort();
			w.Append("{");
			for (int k = 0; k < keys.Count(); k++)
			{
				if (k > 0)
					w.Append(",");
				DZS_Json.WriteString(w, keys[k]);
				w.Append(":");
				WriteCanon(w, v.Get(keys[k]), false);
			}
			w.Append("}");
			return;
		}
		v.Write(w, false, 0);
	}

	// ---- helpers ----

	static DZS_Json Frag()
	{
		DZS_Json f = DZS_Json.Obj();
		DZS_Json bl = DZS_Json.Arr();
		f.Set("blocks", bl);
		DZS_Json cl = DZS_Json.Arr();
		f.Set("changes", cl);
		return f;
	}

	protected DZS_Json Pick(array<int> blocks, array<int> changes)
	{
		DZS_Json f = Frag();
		if (blocks)
		{
			foreach (int b : blocks)
				f.Get("blocks").Add(m_Blocks.At(b));
		}
		if (changes)
		{
			foreach (int c : changes)
				f.Get("changes").Add(m_Changes.At(c));
		}
		return f;
	}

	protected void Claim(DZS_Json frag, array<int> blocks, array<int> changes)
	{
		if (blocks)
		{
			foreach (int b : blocks)
			{
				m_UsedB[b] = true;
				DZS_Json plain = DZS_PatchProject.WithoutExtensions(m_Blocks.At(b));
				frag.Get("blocks").Add(plain);
			}
		}
		if (changes)
		{
			foreach (int c : changes)
			{
				m_UsedC[c] = true;
				DZS_Json cplain = DZS_PatchProject.WithoutExtensions(m_Changes.At(c));
				frag.Get("changes").Add(cplain);
			}
		}
	}

	protected static DZS_Json DataOf(DZS_Json block)
	{
		DZS_Json d = block.Get("data");
		if (!d || !d.IsObj())
			return null;
		return d;
	}

	protected static bool HasObj(DZS_Json o, string key)
	{
		DZS_Json v = o.Get(key);
		return v && v.IsObj();
	}

	protected static bool IsTop(DZS_Json block)
	{
		DZS_Json p = block.Get("parent");
		return !p || p.m_Type == DZS_Json.NUL;
	}

	protected static bool Keyed(DZS_Json change)
	{
		DZS_Json k = change.Get("key");
		return k && k.IsObj();
	}
}
