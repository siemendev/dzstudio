// The Spawns layer's state: own fixed events (one class, one or more points), points added to events of the base,
// base points moved or removed, and changed values of base events. Kept as elements (undo, collab), saved as a
// building-block fragment {blocks, changes} in project.plugins.spawns, which DZS_ProjectV2 takes as it is.
class DZS_SpawnsModel
{
	static const string PLUGIN = "spawns";
	static const string EVENT = "spawns:event";
	static const string POINT = "spawns:point";
	static const string SET = "spawns:set";
	static const string OP = "spawns:edit";

	// ---- reading the state ----

	static array<string> Of(DZS_Instance inst, string kind)
	{
		array<string> r = new array<string>();
		array<string> top = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string id : top)
		{
			if (inst.GetKind(id) == kind)
				r.Insert(id);
		}
		return r;
	}

	static string Field(DZS_Instance inst, string id, string key)
	{
		string v;
		inst.GetField(id, key, v);
		return v;
	}

	// The own event of that name, or "".
	static string OwnEvent(DZS_Instance inst, string name)
	{
		array<string> evs = Of(inst, EVENT);
		foreach (string id : evs)
		{
			if (Field(inst, id, "name") == name)
				return id;
		}
		return "";
	}

	static string BlockId(string elementId)
	{
		return "sp-" + DZS_ProjectV2.IdPart(elementId);
	}

	// ---- to building blocks ----

	static DZS_Json OwnEventData(string name, string nominal)
	{
		DZS_Json d = DZS_Json.Obj();
		DZS_Json nj = DZS_Json.Str(name);
		d.Set("name", nj);
		string n = nominal;
		if (n == "")
			n = "1";
		Num(d, "nominal", n);
		Num(d, "min", n);
		Num(d, "max", n);
		Num(d, "lifetime", "3888000");
		Num(d, "restock", "0");
		Num(d, "saferadius", "500");
		Num(d, "distanceradius", "500");
		Num(d, "cleanupradius", "200");
		DZS_Json flags = DZS_Json.Obj();
		Num(flags, "deletable", "0");
		Num(flags, "init_random", "0");
		Num(flags, "remove_damaged", "1");
		d.Set("flags", flags);
		DZS_Json pos = DZS_Json.Str("fixed");
		d.Set("position", pos);
		DZS_Json lim = DZS_Json.Str("child");
		d.Set("limit", lim);
		Num(d, "active", "1");
		return d;
	}

	static void Num(DZS_Json o, string key, string text)
	{
		DZS_Json v = DZS_Json.Num(text);
		o.Set(key, v);
	}

	static DZS_Json Block(string id, string kind, DZS_Json parent, string parentId, DZS_Json data)
	{
		DZS_Json b = DZS_Json.Obj();
		DZS_Json ij = DZS_Json.Str(id);
		b.Set("id", ij);
		DZS_Json kj = DZS_Json.Str(kind);
		b.Set("kind", kj);
		if (parentId != "")
		{
			DZS_Json pj = DZS_Json.Str(parentId);
			b.Set("parent", pj);
		}
		else if (parent)
			b.Set("parent", parent);
		else
		{
			DZS_Json none = DZS_Json.Null();
			b.Set("parent", none);
		}
		b.Set("data", data);
		return b;
	}

	static DZS_Json PointKey(string ev, string x, string z)
	{
		DZS_Json k = DZS_Json.Obj();
		DZS_Json ej = DZS_Json.Str(ev);
		k.Set("event", ej);
		Num(k, "x", x);
		Num(k, "z", z);
		return k;
	}

	static DZS_Json Pos(string x, string z, string a)
	{
		DZS_Json p = DZS_Json.Obj();
		Num(p, "x", x);
		Num(p, "z", z);
		if (a != "")
			Num(p, "a", a);
		return p;
	}

	// The fragment {blocks, changes}; null when the project has nothing of this layer.
	static DZS_Json Fragment(DZS_Instance inst)
	{
		DZS_Json blocks = DZS_Json.Arr();
		DZS_Json changes = DZS_Json.Arr();
		array<string> evs = Of(inst, EVENT);
		foreach (string eid : evs)
		{
			string name = Field(inst, eid, "name");
			string cls = Field(inst, eid, "class");
			string bid = BlockId(eid);
			DZS_Json ed = OwnEventData(name, Field(inst, eid, "nominal"));
			DZS_Json eb = Block(bid, "event", null, "", ed);
			blocks.Add(eb);
			DZS_Json cd = DZS_Json.Obj();
			DZS_Json cj = DZS_Json.Str(cls);
			cd.Set("type", cj);
			Num(cd, "min", "1");
			Num(cd, "max", "1");
			Num(cd, "lootmin", "0");
			Num(cd, "lootmax", "0");
			DZS_Json cb = Block(bid + "-c", "eventChild", null, bid, cd);
			blocks.Add(cb);
		}
		array<string> pts = Of(inst, POINT);
		foreach (string pid : pts)
		{
			string ev = Field(inst, pid, "event");
			string x = Field(inst, pid, "x");
			string z = Field(inst, pid, "z");
			string a = Field(inst, pid, "a");
			if (Field(inst, pid, "origin") == "base")
			{
				DZS_Json key = PointKey(ev, Field(inst, pid, "bx"), Field(inst, pid, "bz"));
				DZS_Json ch = DZS_Json.Obj();
				DZS_Json blk = DZS_Json.Str("spawnPosition");
				if (Field(inst, pid, "gone") == "1")
				{
					DZS_Json rop = DZS_Json.Str("remove");
					ch.Set("op", rop);
					ch.Set("block", blk);
					ch.Set("key", key);
				}
				else
				{
					DZS_Json sop = DZS_Json.Str("set");
					ch.Set("op", sop);
					ch.Set("block", blk);
					ch.Set("key", key);
					DZS_Json fj = DZS_Json.Str("pos");
					ch.Set("field", fj);
					DZS_Json was = Pos(Field(inst, pid, "bx"), Field(inst, pid, "bz"), Field(inst, pid, "ba"));
					ch.Set("from", was);
					DZS_Json now = Pos(x, z, a);
					ch.Set("to", now);
				}
				changes.Add(ch);
				continue;
			}
			DZS_Json pd = Pos(x, z, a);
			string own = OwnEvent(inst, ev);
			if (own != "")
			{
				DZS_Json ob = Block(BlockId(pid), "spawnPosition", null, BlockId(own), pd);
				blocks.Add(ob);
			}
			else
			{
				DZS_Json par = DZS_Json.Obj();
				DZS_Json pe = DZS_Json.Str(ev);
				par.Set("event", pe);
				DZS_Json bb = Block(BlockId(pid), "spawnPosition", par, "", pd);
				blocks.Add(bb);
			}
		}
		array<string> sets = Of(inst, SET);
		foreach (string sid : sets)
		{
			string sev = Field(inst, sid, "event");
			string field = Field(inst, sid, "field");
			DZS_Json sc = DZS_Json.Obj();
			DZS_Json sopj = DZS_Json.Str("set");
			sc.Set("op", sopj);
			DZS_Json skey = DZS_Json.Obj();
			string kind = "event";
			string leaf = field;
			if (field.IndexOf("child:") == 0)
			{
				// child:<type>:<field>
				array<string> parts = new array<string>();
				field.Split(":", parts);
				kind = "eventChild";
				DZS_Json se = DZS_Json.Str(sev);
				skey.Set("event", se);
				DZS_Json st = DZS_Json.Str(parts[1]);
				skey.Set("type", st);
				leaf = parts[2];
			}
			else
			{
				DZS_Json sn = DZS_Json.Str(sev);
				skey.Set("name", sn);
			}
			DZS_Json kj2 = DZS_Json.Str(kind);
			sc.Set("block", kj2);
			sc.Set("key", skey);
			DZS_Json lj = DZS_Json.Str(leaf);
			sc.Set("field", lj);
			string from = Field(inst, sid, "from");
			if (from != "")
				Num(sc, "from", from);
			Num(sc, "to", Field(inst, sid, "to"));
			changes.Add(sc);
		}
		if (blocks.Count() == 0 && changes.Count() == 0)
			return null;
		DZS_Json r = DZS_Json.Obj();
		r.Set("blocks", blocks);
		r.Set("changes", changes);
		return r;
	}

	// ---- from building blocks (opening a project) ----

	static void Restore(DZS_OpBuilder b, DZS_Json frag)
	{
		if (!frag || !frag.IsObj())
			return;
		DZS_Json blocks = frag.Get("blocks");
		DZS_Json changes = frag.Get("changes");
		map<string, string> evName = new map<string, string>();
		map<string, string> evClass = new map<string, string>();
		map<string, string> evNominal = new map<string, string>();
		array<string> evOrder = new array<string>();
		if (blocks && blocks.IsArr())
		{
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json bl = blocks.At(i);
				string kind = bl.GetStr("kind");
				DZS_Json data = bl.Get("data");
				if (kind == "event")
				{
					evName.Set(bl.GetStr("id"), data.GetStr("name"));
					DZS_Json nom = data.Get("nominal");
					if (nom)
						evNominal.Set(bl.GetStr("id"), nom.m_Str);
					evOrder.Insert(bl.GetStr("id"));
				}
				else if (kind == "eventChild")
					evClass.Set(bl.GetStr("parent"), data.GetStr("type"));
			}
			foreach (string eb : evOrder)
			{
				map<string, string> f = new map<string, string>();
				f.Set("name", evName.Get(eb));
				f.Set("class", evClass.Get(eb));
				f.Set("nominal", evNominal.Get(eb));
				b.Create(EVENT, DZS_PROJECT_ID, DZS_END, f);
			}
			for (int k = 0; k < blocks.Count(); k++)
			{
				DZS_Json pb = blocks.At(k);
				if (pb.GetStr("kind") != "spawnPosition")
					continue;
				DZS_Json pd = pb.Get("data");
				DZS_Json par = pb.Get("parent");
				string ev = "";
				if (par && par.IsStr())
					ev = evName.Get(par.m_Str);
				else if (par && par.IsObj())
					ev = par.GetStr("event");
				map<string, string> pf = new map<string, string>();
				pf.Set("event", ev);
				pf.Set("origin", "own");
				pf.Set("x", NumOf(pd, "x"));
				pf.Set("z", NumOf(pd, "z"));
				pf.Set("a", NumOf(pd, "a"));
				b.Create(POINT, DZS_PROJECT_ID, DZS_END, pf);
			}
		}
		if (!changes || !changes.IsArr())
			return;
		for (int c = 0; c < changes.Count(); c++)
		{
			DZS_Json ch = changes.At(c);
			string blk = ch.GetStr("block");
			DZS_Json key = ch.Get("key");
			if (blk == "spawnPosition")
			{
				map<string, string> bf = new map<string, string>();
				bf.Set("event", key.GetStr("event"));
				bf.Set("origin", "base");
				bf.Set("bx", NumOf(key, "x"));
				bf.Set("bz", NumOf(key, "z"));
				if (ch.GetStr("op") == "remove")
				{
					bf.Set("gone", "1");
					bf.Set("x", NumOf(key, "x"));
					bf.Set("z", NumOf(key, "z"));
				}
				else
				{
					DZS_Json to = ch.Get("to");
					DZS_Json from = ch.Get("from");
					bf.Set("x", NumOf(to, "x"));
					bf.Set("z", NumOf(to, "z"));
					bf.Set("a", NumOf(to, "a"));
					bf.Set("ba", NumOf(from, "a"));
				}
				b.Create(POINT, DZS_PROJECT_ID, DZS_END, bf);
				continue;
			}
			map<string, string> sf = new map<string, string>();
			if (blk == "eventChild")
			{
				sf.Set("event", key.GetStr("event"));
				sf.Set("field", "child:" + key.GetStr("type") + ":" + ch.GetStr("field"));
			}
			else
			{
				sf.Set("event", key.GetStr("name"));
				sf.Set("field", ch.GetStr("field"));
			}
			sf.Set("from", NumOf(ch, "from"));
			sf.Set("to", NumOf(ch, "to"));
			b.Create(SET, DZS_PROJECT_ID, DZS_END, sf);
		}
	}

	static string NumOf(DZS_Json o, string key)
	{
		if (!o || !o.IsObj())
			return "";
		DZS_Json v = o.Get(key);
		if (!v)
			return "";
		return v.m_Str;
	}
}

// project.plugins.spawns
class DZS_SpawnsSection : DZS_ProjectSection
{
	override string Name() { return DZS_SpawnsModel.PLUGIN; }

	override string Text(DZS_Instance inst)
	{
		DZS_Json frag = DZS_SpawnsModel.Fragment(inst);
		if (!frag)
			return "";
		array<string> chunks = frag.ToChunks();
		return DZS_SpawnerFormat.Join(chunks);
	}
}
