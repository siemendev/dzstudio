// Loot edits as building-block changes: what turns one loot type into another, addressed by keys (names, ordinals,
// positions) instead of the indexes of one particular file, so the result applies to any server's files.
class DZS_LootBlocks
{
	// while set: own block id -> the part of 'to' it comes from, for DZS_EntityExts: "t", "c:<uid>", "p:<uid>" (the
	// identity of the container or point, DZS_LootEContainer.m_Uid); "c<i>", "c<i>p<j>", "c:<uid>/<j>" only for parts
	// without one
	static ref map<string, string> s_Paths;

	static string ContainerPath(DZS_LootEContainer c, int index)
	{
		if (c.m_Uid != "")
			return "c:" + c.m_Uid;
		return "c" + index.ToString();
	}

	static string PointPath(DZS_LootEPoint p, string containerPath, int index)
	{
		if (p.m_Uid != "")
			return "p:" + p.m_Uid;
		if (containerPath.IndexOf("c:") == 0)
			return containerPath + "/" + index.ToString();
		return containerPath + "p" + index.ToString();
	}

	// Appends to 'changes' and 'blocks' (JSON arrays) what turns 'from' into 'to'. 'from' null: the type is new;
	// 'to' null: the type goes. 'src' is the entry the points' m_Src indexes of 'to' refer to (the file the editor read
	// them from), so a moved point stays the same point; null when unknown. Own blocks get ids prefix + "-" + n.
	static void Diff(DZS_LootEEntry from, DZS_LootEEntry to, DZS_LootEEntry src, string prefix, DZS_Json changes, DZS_Json blocks)
	{
		if (!to)
		{
			if (from)
			{
				DZS_Json gk = TypeKey(from.m_Name);
				DZS_Json gone = Change("remove", "lootType", gk, "", null, false, null);
				changes.Add(gone);
			}
			return;
		}
		int next = 0;
		if (!from)
		{
			OwnType(to, prefix, blocks, next);
			return;
		}
		string g = from.m_Name;
		if (!SameNum(from.m_Lootmax, to.m_Lootmax))
		{
			DZS_Json lk = TypeKey(g);
			DZS_Json lf = NumOrNull(from.m_Lootmax);
			DZS_Json lt = NumOrNull(to.m_Lootmax);
			DZS_Json lm = Change("set", "lootType", lk, "lootmax", lf, true, lt);
			changes.Add(lm);
		}
		DZS_Json uk = TypeKey(g);
		ListChange(changes, "lootType", uk, "usage", from.m_Usages, to.m_Usages);
		DZS_Json vk = TypeKey(g);
		ListChange(changes, "lootType", vk, "value", from.m_Values, to.m_Values);
		array<int> pair = new array<int>();
		for (int j = 0; j < to.m_Containers.Count(); j++)
			pair.Insert(MatchContainer(from, to, j));
		for (int i = 0; i < from.m_Containers.Count(); i++)
		{
			if (pair.Find(i) >= 0)
				continue;
			DZS_Json rk = ContainerKey(g, from, i);
			DZS_Json rc = Change("remove", "lootContainer", rk, "", null, false, null);
			changes.Add(rc);
		}
		for (int c = 0; c < to.m_Containers.Count(); c++)
		{
			DZS_LootEContainer tc = to.m_Containers[c];
			if (pair[c] < 0)
			{
				DZS_Json parent = DZS_Json.Obj();
				DZS_Json gj = DZS_Json.Str(g);
				parent.Set("group", gj);
				OwnContainer(tc, ContainerPath(tc, c), parent, "", prefix, blocks, next);
				continue;
			}
			ContainerDiff(g, from, pair[c], tc, c, src, prefix, changes, blocks, next);
		}
	}

	// ---- containers ----

	// The container of 'from' that container j of 'to' continues: same name and the same place among equally named
	// containers; -1 when it is new.
	protected static int MatchContainer(DZS_LootEEntry from, DZS_LootEEntry to, int j)
	{
		string name = to.m_Containers[j].m_Name;
		int ord = 0;
		for (int a = 0; a <= j; a++)
		{
			if (to.m_Containers[a].m_Name == name)
				ord++;
		}
		int seen = 0;
		for (int i = 0; i < from.m_Containers.Count(); i++)
		{
			if (from.m_Containers[i].m_Name != name)
				continue;
			seen++;
			if (seen == ord)
				return i;
		}
		return -1;
	}

	// 1-based place among the equally named containers of the entry, or 0 when the name is unique.
	protected static int ContainerOrdinal(DZS_LootEEntry e, int i)
	{
		string name = e.m_Containers[i].m_Name;
		int count = 0;
		int at = 0;
		for (int k = 0; k < e.m_Containers.Count(); k++)
		{
			if (e.m_Containers[k].m_Name != name)
				continue;
			count++;
			if (k == i)
				at = count;
		}
		if (count < 2)
			return 0;
		return at;
	}

	protected static DZS_Json ContainerKey(string g, DZS_LootEEntry e, int i)
	{
		DZS_Json k = DZS_Json.Obj();
		DZS_Json gj = DZS_Json.Str(g);
		k.Set("group", gj);
		DZS_Json nj = DZS_Json.Str(e.m_Containers[i].m_Name);
		k.Set("name", nj);
		int ord = ContainerOrdinal(e, i);
		if (ord > 0)
		{
			DZS_Json oj = DZS_Json.Num(ord.ToString());
			k.Set("ordinal", oj);
		}
		return k;
	}

	protected static void ContainerDiff(string g, DZS_LootEEntry from, int fi, DZS_LootEContainer tc, int ti, DZS_LootEEntry src, string prefix, DZS_Json changes, DZS_Json blocks, out int next)
	{
		DZS_LootEContainer fc = from.m_Containers[fi];
		if (!SameNum(fc.m_Lootmax, tc.m_Lootmax))
		{
			DZS_Json ck = ContainerKey(g, from, fi);
			DZS_Json cf = NumOrNull(fc.m_Lootmax);
			DZS_Json ct = NumOrNull(tc.m_Lootmax);
			DZS_Json lm = Change("set", "lootContainer", ck, "lootmax", cf, true, ct);
			changes.Add(lm);
		}
		DZS_Json catKey = ContainerKey(g, from, fi);
		ListChange(changes, "lootContainer", catKey, "category", fc.m_Cats, tc.m_Cats);
		DZS_Json tagKey = ContainerKey(g, from, fi);
		ListChange(changes, "lootContainer", tagKey, "tag", fc.m_Tags, tc.m_Tags);
		// points: the same spot within 1 cm continues, the rest goes or comes
		array<int> taken = new array<int>();
		array<int> match = new array<int>();
		foreach (DZS_LootEPoint tp : tc.m_Points)
		{
			int hit = -1;
			for (int i = 0; i < fc.m_Points.Count() && hit < 0; i++)
			{
				if (taken.Find(i) < 0 && Near(fc.m_Points[i].m_Pos, tp.m_Pos))
					hit = i;
			}
			if (hit >= 0)
				taken.Insert(hit);
			match.Insert(hit);
		}
		// a point the editor moved: where it came from, if that spot is still unclaimed
		for (int m = 0; m < tc.m_Points.Count(); m++)
		{
			if (match[m] >= 0 || tc.m_Points[m].m_Src < 0 || !src)
				continue;
			string was = SrcPos(src, tc.m_Points[m].m_Src);
			for (int j = 0; j < fc.m_Points.Count(); j++)
			{
				if (was != "" && taken.Find(j) < 0 && Near(fc.m_Points[j].m_Pos, was))
				{
					taken.Insert(j);
					match[m] = j;
					break;
				}
			}
		}
		for (int r = 0; r < fc.m_Points.Count(); r++)
		{
			if (taken.Find(r) >= 0)
				continue;
			DZS_Json rk = PointKey(g, from, fi, r);
			DZS_Json rc = Change("remove", "lootPoint", rk, "", null, false, null);
			changes.Add(rc);
		}
		DZS_Json parent = DZS_Json.Obj();
		DZS_Json gj = DZS_Json.Str(g);
		parent.Set("group", gj);
		DZS_Json cn = DZS_Json.Str(fc.m_Name);
		parent.Set("container", cn);
		int ord = ContainerOrdinal(from, fi);
		if (ord > 0)
		{
			DZS_Json oj = DZS_Json.Num(ord.ToString());
			parent.Set("containerOrdinal", oj);
		}
		for (int p = 0; p < tc.m_Points.Count(); p++)
		{
			DZS_LootEPoint np = tc.m_Points[p];
			if (match[p] < 0)
			{
				string tcPath = "c" + ti.ToString();
				OwnPoint(np, PointPath(np, tcPath, p), parent, "", prefix, blocks, next);
				continue;
			}
			DZS_LootEPoint op = fc.m_Points[match[p]];
			DZS_Json k1 = PointKey(g, from, fi, match[p]);
			PointField(changes, k1, "pos", op.m_Pos, np.m_Pos, true);
			DZS_Json k2 = PointKey(g, from, fi, match[p]);
			PointField(changes, k2, "range", Opt(op.m_HasRange, op.m_Range), Opt(np.m_HasRange, np.m_Range), false);
			DZS_Json k3 = PointKey(g, from, fi, match[p]);
			PointField(changes, k3, "height", Opt(op.m_HasHeight, op.m_Height), Opt(np.m_HasHeight, np.m_Height), false);
			DZS_Json k4 = PointKey(g, from, fi, match[p]);
			PointField(changes, k4, "flags", op.m_Flags, np.m_Flags, false);
		}
	}

	// Position of the point with this global index (m_Src) in an entry, or "".
	protected static string SrcPos(DZS_LootEEntry e, int idx)
	{
		foreach (DZS_LootEContainer c : e.m_Containers)
		{
			foreach (DZS_LootEPoint p : c.m_Points)
			{
				if (p.m_Src == idx)
					return p.m_Pos;
			}
		}
		return "";
	}

	protected static string Opt(bool has, string v)
	{
		if (!has)
			return "";
		return v;
	}

	protected static void PointField(DZS_Json changes, DZS_Json key, string field, string was, string now, bool vec)
	{
		if (vec)
		{
			if (SameVec(was, now))
				return;
			DZS_Json wv = DZS_Json.Str(was);
			DZS_Json nv = DZS_Json.Str(now);
			DZS_Json cv = Change("set", "lootPoint", key, field, wv, true, nv);
			changes.Add(cv);
			return;
		}
		if (SameNum(was, now))
			return;
		DZS_Json wj = NumOrNull(was);
		DZS_Json nj = NumOrNull(now);
		DZS_Json c = Change("set", "lootPoint", key, field, wj, true, nj);
		changes.Add(c);
	}

	// A point of 'from' by its position; an ordinal only when another point of the container lies within 1 cm.
	protected static DZS_Json PointKey(string g, DZS_LootEEntry from, int fi, int pi)
	{
		DZS_LootEContainer fc = from.m_Containers[fi];
		DZS_Json k = DZS_Json.Obj();
		DZS_Json gj = DZS_Json.Str(g);
		k.Set("group", gj);
		DZS_Json cn = DZS_Json.Str(fc.m_Name);
		k.Set("container", cn);
		int ord = ContainerOrdinal(from, fi);
		if (ord > 0)
		{
			DZS_Json oj = DZS_Json.Num(ord.ToString());
			k.Set("containerOrdinal", oj);
		}
		string pos = fc.m_Points[pi].m_Pos;
		DZS_Json pj = DZS_Json.Str(pos);
		k.Set("pos", pj);
		int count = 0;
		int at = 0;
		for (int i = 0; i < fc.m_Points.Count(); i++)
		{
			if (!Near(fc.m_Points[i].m_Pos, pos))
				continue;
			count++;
			if (i == pi)
				at = count;
		}
		if (count > 1)
		{
			DZS_Json pord = DZS_Json.Num(at.ToString());
			k.Set("ordinal", pord);
		}
		return k;
	}

	// ---- own blocks ----

	protected static string NextId(string prefix, out int next)
	{
		next++;
		return prefix + "-" + next;
	}

	protected static DZS_Json Block(string id, string kind, DZS_Json parentKey, string parentId, DZS_Json data)
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
		else if (parentKey)
			b.Set("parent", parentKey);
		else
		{
			DZS_Json none = DZS_Json.Null();
			b.Set("parent", none);
		}
		b.Set("data", data);
		return b;
	}

	protected static void OwnType(DZS_LootEEntry e, string prefix, DZS_Json blocks, out int next)
	{
		DZS_Json d = DZS_Json.Obj();
		DZS_Json nj = DZS_Json.Str(e.m_Name);
		d.Set("name", nj);
		if (e.m_Lootmax != "")
		{
			DZS_Json lm = DZS_Json.Num(e.m_Lootmax);
			d.Set("lootmax", lm);
		}
		if (e.m_Usages.Count() > 0)
		{
			DZS_Json us = StrList(e.m_Usages);
			d.Set("usage", us);
		}
		if (e.m_Values.Count() > 0)
		{
			DZS_Json vs = StrList(e.m_Values);
			d.Set("value", vs);
		}
		string id = NextId(prefix, next);
		DZS_Json b = Block(id, "lootType", null, "", d);
		Own(b, "t", e.m_Ext);
		blocks.Add(b);
		for (int i = 0; i < e.m_Containers.Count(); i++)
		{
			DZS_LootEContainer c = e.m_Containers[i];
			OwnContainer(c, ContainerPath(c, i), null, id, prefix, blocks, next);
		}
	}

	// An own block's extensions from the part of the edited entry, and where they live.
	protected static void Own(DZS_Json block, string path, string ext)
	{
		if (s_Paths)
			s_Paths.Set(block.GetStr("id"), path);
		if (ext == "")
			return;
		string err;
		DZS_Json e = DZS_Json.ParseText(ext, err);
		if (e && e.IsObj() && e.Count() > 0)
			block.Set(DZS_EntityExts.MEMBER, e);
	}

	protected static void OwnContainer(DZS_LootEContainer c, string path, DZS_Json parentKey, string parentId, string prefix, DZS_Json blocks, out int next)
	{
		DZS_Json d = DZS_Json.Obj();
		DZS_Json nj = DZS_Json.Str(c.m_Name);
		d.Set("name", nj);
		if (c.m_Lootmax != "")
		{
			DZS_Json lm = DZS_Json.Num(c.m_Lootmax);
			d.Set("lootmax", lm);
		}
		if (c.m_Cats.Count() > 0)
		{
			DZS_Json cs = StrList(c.m_Cats);
			d.Set("category", cs);
		}
		if (c.m_Tags.Count() > 0)
		{
			DZS_Json ts = StrList(c.m_Tags);
			d.Set("tag", ts);
		}
		string id = NextId(prefix, next);
		DZS_Json b = Block(id, "lootContainer", parentKey, parentId, d);
		Own(b, path, c.m_Ext);
		blocks.Add(b);
		for (int k = 0; k < c.m_Points.Count(); k++)
		{
			DZS_LootEPoint p = c.m_Points[k];
			OwnPoint(p, PointPath(p, path, k), null, id, prefix, blocks, next);
		}
	}

	protected static void OwnPoint(DZS_LootEPoint p, string path, DZS_Json parentKey, string parentId, string prefix, DZS_Json blocks, out int next)
	{
		DZS_Json d = DZS_Json.Obj();
		DZS_Json pj = DZS_Json.Str(p.m_Pos);
		d.Set("pos", pj);
		string range = "0.1";
		if (p.m_HasRange)
			range = p.m_Range;
		string height = "0.1";
		if (p.m_HasHeight)
			height = p.m_Height;
		DZS_Json rj = DZS_Json.Num(range);
		d.Set("range", rj);
		DZS_Json hj = DZS_Json.Num(height);
		d.Set("height", hj);
		if (p.m_Flags != "")
		{
			DZS_Json fj = DZS_Json.Num(p.m_Flags);
			d.Set("flags", fj);
		}
		string id = NextId(prefix, next);
		DZS_Json b = Block(id, "lootPoint", parentKey, parentId, d);
		Own(b, path, p.m_Ext);
		blocks.Add(b);
	}

	// ---- values ----

	static DZS_Json Change(string op, string kind, DZS_Json key, string field, DZS_Json from, bool hasFrom, DZS_Json to)
	{
		DZS_Json c = DZS_Json.Obj();
		DZS_Json oj = DZS_Json.Str(op);
		c.Set("op", oj);
		DZS_Json bj = DZS_Json.Str(kind);
		c.Set("block", bj);
		c.Set("key", key);
		if (field != "")
		{
			DZS_Json fj = DZS_Json.Str(field);
			c.Set("field", fj);
		}
		if (hasFrom)
		{
			DZS_Json fv = from;
			if (!fv)
				fv = DZS_Json.Null();
			c.Set("from", fv);
		}
		if (op == "set")
		{
			DZS_Json tv = to;
			if (!tv)
				tv = DZS_Json.Null();
			c.Set("to", tv);
		}
		return c;
	}

	protected static DZS_Json TypeKey(string name)
	{
		DZS_Json k = DZS_Json.Obj();
		DZS_Json nj = DZS_Json.Str(name);
		k.Set("name", nj);
		return k;
	}

	protected static void ListChange(DZS_Json changes, string kind, DZS_Json key, string field, array<string> was, array<string> now)
	{
		if (SameList(was, now))
			return;
		array<string> tokens = new array<string>();
		foreach (string w : was)
			tokens.Insert("name=" + w);
		DZS_Json fj = DZS_Json.Str(DZS_PatcherBase.Fingerprint(tokens));
		DZS_Json lj = StrList(now);
		DZS_Json c = Change("set", kind, key, field, fj, true, lj);
		changes.Add(c);
	}

	static DZS_Json StrList(array<string> names)
	{
		DZS_Json a = DZS_Json.Arr();
		foreach (string n : names)
		{
			DZS_Json nj = DZS_Json.Str(n);
			a.Add(nj);
		}
		return a;
	}

	static DZS_Json NumOrNull(string t)
	{
		if (t == "")
			return DZS_Json.Null();
		return DZS_Json.Num(t);
	}

	static bool SameNum(string a, string b)
	{
		if (a == "" || b == "")
			return a == b;
		return DZS_PatchNum.Same(a, b);
	}

	static bool SameList(array<string> a, array<string> b)
	{
		if (a.Count() != b.Count())
			return false;
		for (int i = 0; i < a.Count(); i++)
		{
			if (a[i] != b[i])
				return false;
		}
		return true;
	}

	static bool SameVec(string a, string b)
	{
		array<string> pa = new array<string>();
		array<string> pb = new array<string>();
		if (!DZS_PatcherBase.VecText(a, pa) || !DZS_PatcherBase.VecText(b, pb))
			return a == b;
		return DZS_PatcherBase.NearVec(pa, pb, 0);
	}

	static bool Near(string a, string b)
	{
		array<string> pa = new array<string>();
		array<string> pb = new array<string>();
		if (!DZS_PatcherBase.VecText(a, pa) || !DZS_PatcherBase.VecText(b, pb))
			return a == b;
		return DZS_PatcherBase.NearVec(pa, pb, 10000);
	}

	// What a loot type amounts to, for comparing two ways of writing it: numbers by value, points as a sorted set.
	static string Canon(DZS_LootEEntry e)
	{
		if (!e)
			return "(none)";
		string s = "lootmax " + CanonNum(e.m_Lootmax) + " usage " + Join(e.m_Usages) + " value " + Join(e.m_Values);
		foreach (DZS_LootEContainer c : e.m_Containers)
		{
			s += " | " + c.m_Name + " " + CanonNum(c.m_Lootmax) + " cat " + Join(c.m_Cats) + " tag " + Join(c.m_Tags);
			array<string> pts = new array<string>();
			foreach (DZS_LootEPoint p : c.m_Points)
			{
				array<string> parts = new array<string>();
				string pos = p.m_Pos;
				if (DZS_PatcherBase.VecText(p.m_Pos, parts))
					pos = DZS_PatchNum.Fixed6(parts[0]) + " " + DZS_PatchNum.Fixed6(parts[1]) + " " + DZS_PatchNum.Fixed6(parts[2]);
				pts.Insert(pos + "/" + CanonNum(Opt(p.m_HasRange, p.m_Range)) + "/" + CanonNum(Opt(p.m_HasHeight, p.m_Height)) + "/" + p.m_Flags);
			}
			pts.Sort();
			s += " pts " + Join(pts);
		}
		return s;
	}

	protected static string CanonNum(string t)
	{
		if (t == "")
			return "-";
		return DZS_PatchNum.Canon(t);
	}

	protected static string Join(array<string> a)
	{
		string r = "";
		foreach (string x : a)
		{
			if (r != "")
				r += ",";
			r += x;
		}
		return r;
	}
}

// Proof that the building-block form of a loot edit gives the same loot as today's writer: every loot type of a
// mission folder's mapgroupproto.xml gets a different mix of edits, written once by DZS_LootDiff (the editor's writer)
// and once as changes through the patcher; both results must equal the edited entry.
class DZS_LootBlocksTest
{
	static void Run(string missionDir)
	{
		string protoPath = missionDir + "\\mapgroupproto.xml";
		array<string> chunks = DZS_Files.Read(protoPath);
		if (!chunks)
		{
			DZS_Log.Info("lootblocks: FAIL cannot read " + protoPath);
			return;
		}
		DZS_XmlDoc doc = DZS_Xml.Parse(chunks);
		array<ref DZS_XmlPatch> oldPatches = new array<ref DZS_XmlPatch>();
		DZS_LootDiff diff = new DZS_LootDiff(doc, oldPatches, "\t");
		DZS_Json changes = DZS_Json.Arr();
		DZS_Json blocks = DZS_Json.Arr();
		map<string, ref DZS_LootEEntry> want = new map<string, ref DZS_LootEEntry>();
		array<string> names = new array<string>();
		array<string> removed = new array<string>();
		int i = 0;
		foreach (DZS_XmlNode g : doc.m_Root.m_Children)
		{
			if (g.m_Name != "group")
				continue;
			string low = DZS_PatchText.Lower(g.Get("name"));
			if (want.Contains(low))
				continue;
			DZS_LootEEntry from = DZS_LootEEntry.FromNode(doc, g);
			i++;
			if (i % 19 == 6)
			{
				Diff(from, null, "r" + i, changes, blocks);
				removed.Insert(low);
				diff.Delete(g);
				continue;
			}
			DZS_LootEEntry to = from.Copy();
			Edit(to, i);
			want.Set(low, to);
			names.Insert(low);
			if (DZS_LootBlocks.Canon(from) != DZS_LootBlocks.Canon(to))
				diff.OneGroup(g, to);
			Diff(from, to, "t" + i, changes, blocks);
		}
		for (int n = 1; n <= 3; n++)
		{
			DZS_LootEEntry fresh = NewType(n);
			string fl = DZS_PatchText.Lower(fresh.m_Name);
			want.Set(fl, fresh);
			names.Insert(fl);
			Diff(null, fresh, "n" + n, changes, blocks);
		}
		// today's writer
		array<string> oldOut = DZS_XmlWriter.Apply(doc, oldPatches);
		// building blocks through the patcher
		DZS_Json root = DZS_Json.Obj();
		DZS_Json fmt = DZS_Json.Str(DZS_PatchProject.FORMAT);
		root.Set("format", fmt);
		DZS_Json ver = DZS_Json.Int(DZS_PatchProject.VERSION);
		root.Set("version", ver);
		root.Set("blocks", blocks);
		root.Set("changes", changes);
		string err;
		DZS_PatchProject project = DZS_PatchProject.FromJson(root, err);
		int t0 = TickCount(0);
		DZS_Patcher p = DZS_Patcher.Run(project, missionDir);
		int ms = (TickCount(0) - t0) / 10000;
		int notApplied = 0;
		foreach (DZS_PatchEntry e : p.m_Report.m_Entries)
		{
			if (e.m_Status != "applied")
			{
				notApplied++;
				if (notApplied <= 5)
					DZS_Log.Info("lootblocks: not applied: " + e.LogLine());
			}
		}
		array<string> newOut = p.m_Out.Get("mapgroupproto.xml");
		if (!newOut)
		{
			DZS_Log.Info("lootblocks: FAIL patcher wrote no mapgroupproto.xml");
			return;
		}
		map<string, ref DZS_LootEEntry> oldBy = ByName(oldOut);
		map<string, ref DZS_LootEEntry> newBy = ByName(newOut);
		int bad = 0;
		int oldBad = 0;
		foreach (string nm : names)
		{
			string w = DZS_LootBlocks.Canon(want.Get(nm));
			string o = DZS_LootBlocks.Canon(oldBy.Get(nm));
			string b = DZS_LootBlocks.Canon(newBy.Get(nm));
			if (o != w && nm.IndexOf("land_dzs_new") != 0)
				oldBad++;
			if (b != w)
			{
				bad++;
				if (bad <= 5)
				{
					DZS_Log.Info("lootblocks: DIFF " + nm);
					DZS_Log.Info("  want:    " + w);
					DZS_Log.Info("  writer:  " + o);
					DZS_Log.Info("  patcher: " + b);
				}
			}
		}
		foreach (string rn : removed)
		{
			if (newBy.Contains(rn))
			{
				bad++;
				DZS_Log.Info("lootblocks: DIFF " + rn + " should be gone");
			}
		}
		string line = "lootblocks: " + missionDir + ": " + names.Count() + " types, " + removed.Count() + " removed, ";
		line += changes.Count().ToString() + " changes, " + blocks.Count() + " own blocks, ";
		line += notApplied.ToString() + " not applied, " + bad + " differ from the edit (today's writer: " + oldBad + "), patcher " + ms + " ms";
		DZS_Log.Info(line);
	}

	protected static map<string, ref DZS_LootEEntry> ByName(array<string> chunks)
	{
		map<string, ref DZS_LootEEntry> r = new map<string, ref DZS_LootEEntry>();
		DZS_XmlDoc d = DZS_Xml.Parse(chunks);
		foreach (DZS_XmlNode g : d.m_Root.m_Children)
		{
			if (g.m_Name != "group")
				continue;
			string low = DZS_PatchText.Lower(g.Get("name"));
			if (!r.Contains(low))
				r.Set(low, DZS_LootEEntry.FromNode(d, g));
		}
		return r;
	}

	protected static void Diff(DZS_LootEEntry from, DZS_LootEEntry to, string prefix, DZS_Json changes, DZS_Json blocks)
	{
		DZS_LootBlocks.Diff(from, to, from, prefix, changes, blocks);
	}

	// A different mix of edits per type (i), the way the loot editor makes them: values as six decimals.
	static void Edit(DZS_LootEEntry e, int i)
	{
		if (i % 7 == 0)
		{
			if (e.m_Lootmax == "")
				e.m_Lootmax = "3";
			else
				e.m_Lootmax = "";
		}
		if (i % 5 == 1)
		{
			if (e.m_Usages.Count() > 0)
				e.m_Usages.Remove(e.m_Usages.Count() - 1);
			e.m_Usages.Insert("Town");
		}
		if (e.m_Containers.Count() > 0)
		{
			DZS_LootEContainer c0 = e.m_Containers[0];
			if (c0.m_Points.Count() > 0)
			{
				DZS_LootEPoint p0 = c0.m_Points[0];
				if (i % 3 == 0)
					c0.m_Points.Remove(0);
				else if (i % 3 == 1)
				{
					DZS_LootEPoint add = new DZS_LootEPoint();
					vector v = p0.P();
					v[0] = v[0] + 0.5;
					v[2] = v[2] + 0.5;
					add.SetP(v);
					add.SetRange(0.5);
					add.SetHeight(1);
					c0.m_Points.Insert(add);
				}
				else
				{
					vector mv = p0.P();
					mv[0] = mv[0] + 0.2;
					p0.SetP(mv);
					p0.SetRange(0.7);
				}
			}
			if (i % 17 == 5)
			{
				if (c0.m_Cats.Find("food") >= 0)
					c0.m_Cats.Remove(c0.m_Cats.Find("food"));
				else
					c0.m_Cats.Insert("food");
			}
		}
		if (i % 11 == 3 && e.m_Containers.Count() >= 2)
			e.m_Containers.Remove(e.m_Containers.Count() - 1);
		if (i % 13 == 4)
		{
			DZS_LootEContainer nc = new DZS_LootEContainer();
			nc.m_Name = "lootDzs";
			nc.m_Lootmax = "2";
			nc.m_Cats.Insert("tools");
			nc.m_Tags.Insert("floor");
			for (int k = 0; k < 2; k++)
			{
				DZS_LootEPoint np = new DZS_LootEPoint();
				np.SetP(Vector(k, 0.5, -k));
				np.SetRange(0.4);
				np.SetHeight(0.8);
				nc.m_Points.Insert(np);
			}
			e.m_Containers.Insert(nc);
		}
	}

	protected static DZS_LootEEntry NewType(int n)
	{
		DZS_LootEEntry e = new DZS_LootEEntry();
		e.m_Name = "Land_Dzs_New_" + n;
		e.m_Lootmax = n.ToString();
		e.m_Usages.Insert("Farm");
		DZS_LootEContainer c = new DZS_LootEContainer();
		c.m_Name = "lootFloor";
		c.m_Tags.Insert("floor");
		DZS_LootEPoint p = new DZS_LootEPoint();
		p.SetP(Vector(n, 0, n));
		p.SetRange(0.5);
		p.SetHeight(1);
		p.m_Flags = "32";
		c.m_Points.Insert(p);
		e.m_Containers.Insert(c);
		return e;
	}
}
