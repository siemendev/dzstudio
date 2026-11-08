// The Territories layer: where animals and infected live. A kind is one thing — a herd (event AnimalX + territory
// type X in cfgenvironment.xml + its env file) or an infected category (event InfectedX + zones named InfectedX in
// zombie_territories.xml). State: own zones (in a base territory or an own one), own territories, changes to base
// zones (moved, resized, counts, removed). Saved as a building-block fragment in project.plugins.territories.
class DZS_TerrZone : Managed
{
	string m_File;
	string m_Color;
	string m_Name;
	float m_X;
	float m_Z;
	float m_R;
	string m_Dmin;
	string m_Dmax;
	string m_Smin;
	string m_Smax;
	// the kind the zone belongs to: "infected:InfectedCity", "herd:Wolf", "ambient:AmbientHare"
	string m_Kind;
	string m_BX;
	string m_BZ;
	string m_BR;
	string m_BDmin;
	string m_BDmax;
	string m_Elem;
	bool m_Own;
	bool m_Changed;
	bool m_Gone;

	string Key()
	{
		if (m_Own)
			return "e:" + m_Elem;
		return "b:" + m_File + ":" + m_Name + ":" + m_BX + ":" + m_BZ;
	}
}

class DZS_TerrKind : Managed
{
	string m_Id;
	string m_Label;
	string m_Event;
	string m_File;
	// a colour of a territory holding this kind's zones (infected), "" for herds (one territory per herd)
	string m_Color;
	ref array<string> m_Classes = new array<string>();
	string m_Nominal;
	string m_BaseNominal;
}

class DZS_TerrModel
{
	static const string PLUGIN = "territories";
	static const string ZONE = "territories:zone";
	static const string TERR = "territories:territory";
	static const string CHANGE = "territories:change";
	static const string OP = "territories:edit";
	static const string ZOMBIES = "env/zombie_territories.xml";

	static array<string> Of(DZS_Instance inst, string kind)
	{
		return DZS_SpawnsModel.Of(inst, kind);
	}

	static string Field(DZS_Instance inst, string id, string key)
	{
		return DZS_SpawnsModel.Field(inst, id, key);
	}

	static void Num(DZS_Json o, string key, string text)
	{
		DZS_Json v = DZS_Json.Num(text);
		o.Set(key, v);
	}

	static void Str(DZS_Json o, string key, string text)
	{
		DZS_Json v = DZS_Json.Str(text);
		o.Set(key, v);
	}

	static DZS_Json ZoneData(DZS_Instance inst, string id)
	{
		DZS_Json d = DZS_Json.Obj();
		Str(d, "name", Field(inst, id, "name"));
		Num(d, "smin", Or(Field(inst, id, "smin"), "0"));
		Num(d, "smax", Or(Field(inst, id, "smax"), "0"));
		Num(d, "dmin", Or(Field(inst, id, "dmin"), "0"));
		Num(d, "dmax", Or(Field(inst, id, "dmax"), "0"));
		Num(d, "x", Field(inst, id, "x"));
		Num(d, "z", Field(inst, id, "z"));
		Num(d, "r", Field(inst, id, "r"));
		return d;
	}

	static string Or(string a, string b)
	{
		if (a == "")
			return b;
		return a;
	}

	static DZS_Json ZoneKey(string file, string name, string x, string z)
	{
		DZS_Json k = DZS_Json.Obj();
		Str(k, "file", file);
		Str(k, "name", name);
		Num(k, "x", x);
		Num(k, "z", z);
		return k;
	}

	static void SetChange(DZS_Json changes, DZS_Json key, string field, string from, string to)
	{
		if (to == "" || to == from)
			return;
		DZS_Json c = DZS_Json.Obj();
		Str(c, "op", "set");
		Str(c, "block", "territoryZone");
		c.Set("key", key);
		Str(c, "field", field);
		if (from != "")
			Num(c, "from", from);
		Num(c, "to", to);
		changes.Add(c);
	}

	static DZS_Json Fragment(DZS_Instance inst)
	{
		DZS_Json blocks = DZS_Json.Arr();
		DZS_Json changes = DZS_Json.Arr();
		array<string> terrs = Of(inst, TERR);
		foreach (string tid : terrs)
		{
			DZS_Json td = DZS_Json.Obj();
			Str(td, "file", Field(inst, tid, "file"));
			Str(td, "color", Field(inst, tid, "color"));
			DZS_Json tb = DZS_SpawnsModel.Block("tr-" + DZS_ProjectV2.IdPart(tid), "territory", null, "", td);
			blocks.Add(tb);
		}
		array<string> zones = Of(inst, ZONE);
		foreach (string zid : zones)
		{
			DZS_Json zd = ZoneData(inst, zid);
			string owner = Field(inst, zid, "territory");
			DZS_Json zb;
			if (owner != "")
				zb = DZS_SpawnsModel.Block("tz-" + DZS_ProjectV2.IdPart(zid), "territoryZone", null, "tr-" + DZS_ProjectV2.IdPart(owner), zd);
			else
			{
				DZS_Json par = DZS_Json.Obj();
				Str(par, "file", Field(inst, zid, "file"));
				Str(par, "color", Field(inst, zid, "color"));
				zb = DZS_SpawnsModel.Block("tz-" + DZS_ProjectV2.IdPart(zid), "territoryZone", par, "", zd);
			}
			blocks.Add(zb);
		}
		array<string> chs = Of(inst, CHANGE);
		foreach (string cid : chs)
		{
			string file = Field(inst, cid, "file");
			string name = Field(inst, cid, "name");
			string bx = Field(inst, cid, "bx");
			string bz = Field(inst, cid, "bz");
			if (Field(inst, cid, "gone") == "1")
			{
				DZS_Json rc = DZS_Json.Obj();
				Str(rc, "op", "remove");
				Str(rc, "block", "territoryZone");
				DZS_Json rk = ZoneKey(file, name, bx, bz);
				rc.Set("key", rk);
				changes.Add(rc);
				continue;
			}
			DZS_Json k1 = ZoneKey(file, name, bx, bz);
			SetChange(changes, k1, "x", bx, Field(inst, cid, "x"));
			DZS_Json k2 = ZoneKey(file, name, bx, bz);
			SetChange(changes, k2, "z", bz, Field(inst, cid, "z"));
			DZS_Json k3 = ZoneKey(file, name, bx, bz);
			SetChange(changes, k3, "r", Field(inst, cid, "br"), Field(inst, cid, "r"));
			DZS_Json k4 = ZoneKey(file, name, bx, bz);
			SetChange(changes, k4, "dmin", Field(inst, cid, "bdmin"), Field(inst, cid, "dmin"));
			DZS_Json k5 = ZoneKey(file, name, bx, bz);
			SetChange(changes, k5, "dmax", Field(inst, cid, "bdmax"), Field(inst, cid, "dmax"));
		}
		array<string> nominals = Of(inst, "territories:nominal");
		foreach (string nid : nominals)
		{
			DZS_Json nc = DZS_Json.Obj();
			Str(nc, "op", "set");
			Str(nc, "block", "event");
			DZS_Json nk = DZS_Json.Obj();
			Str(nk, "name", Field(inst, nid, "event"));
			nc.Set("key", nk);
			Str(nc, "field", "nominal");
			string nf = Field(inst, nid, "from");
			if (nf != "")
				Num(nc, "from", nf);
			Num(nc, "to", Field(inst, nid, "to"));
			changes.Add(nc);
		}
		if (blocks.Count() == 0 && changes.Count() == 0)
			return null;
		DZS_Json r = DZS_Json.Obj();
		r.Set("blocks", blocks);
		r.Set("changes", changes);
		return r;
	}

	static void Restore(DZS_OpBuilder b, DZS_Json frag)
	{
		if (!frag || !frag.IsObj())
			return;
		map<string, string> terrIds = new map<string, string>();
		DZS_Json blocks = frag.Get("blocks");
		if (blocks && blocks.IsArr())
		{
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json bl = blocks.At(i);
				if (bl.GetStr("kind") != "territory")
					continue;
				DZS_Json td = bl.Get("data");
				map<string, string> tf = new map<string, string>();
				tf.Set("file", td.GetStr("file"));
				tf.Set("color", td.GetStr("color"));
				string tid = b.Create(TERR, DZS_PROJECT_ID, DZS_END, tf);
				terrIds.Set(bl.GetStr("id"), tid);
			}
			for (int k = 0; k < blocks.Count(); k++)
			{
				DZS_Json zb = blocks.At(k);
				if (zb.GetStr("kind") != "territoryZone")
					continue;
				DZS_Json zd = zb.Get("data");
				map<string, string> zf = new map<string, string>();
				array<string> keys = {"name", "smin", "smax", "dmin", "dmax", "x", "z", "r"};
				foreach (string key : keys)
				{
					DZS_Json v = zd.Get(key);
					if (v)
						zf.Set(key, v.m_Str);
				}
				DZS_Json par = zb.Get("parent");
				if (par && par.IsStr())
					zf.Set("territory", terrIds.Get(par.m_Str));
				else if (par && par.IsObj())
				{
					zf.Set("file", par.GetStr("file"));
					zf.Set("color", par.GetStr("color"));
				}
				b.Create(ZONE, DZS_PROJECT_ID, DZS_END, zf);
			}
		}
		DZS_Json changes = frag.Get("changes");
		if (!changes || !changes.IsArr())
			return;
		map<string, ref map<string, string>> by = new map<string, ref map<string, string>>();
		array<string> order = new array<string>();
		for (int c = 0; c < changes.Count(); c++)
		{
			DZS_Json ch = changes.At(c);
			DZS_Json key2 = ch.Get("key");
			if (ch.GetStr("block") == "event")
			{
				map<string, string> nf = new map<string, string>();
				nf.Set("event", key2.GetStr("name"));
				DZS_Json from = ch.Get("from");
				if (from)
					nf.Set("from", from.m_Str);
				nf.Set("to", ch.Get("to").m_Str);
				b.Create("territories:nominal", DZS_PROJECT_ID, DZS_END, nf);
				continue;
			}
			string id = key2.GetStr("file") + ":" + key2.GetStr("name") + ":" + key2.Get("x").m_Str + ":" + key2.Get("z").m_Str;
			map<string, string> f = by.Get(id);
			if (!f)
			{
				f = new map<string, string>();
				f.Set("file", key2.GetStr("file"));
				f.Set("name", key2.GetStr("name"));
				f.Set("bx", key2.Get("x").m_Str);
				f.Set("bz", key2.Get("z").m_Str);
				by.Set(id, f);
				order.Insert(id);
			}
			if (ch.GetStr("op") == "remove")
			{
				f.Set("gone", "1");
				continue;
			}
			string field = ch.GetStr("field");
			f.Set(field, ch.Get("to").m_Str);
			DZS_Json was = ch.Get("from");
			if (was && field != "x" && field != "z")
				f.Set("b" + field, was.m_Str);
		}
		foreach (string oid : order)
		{
			map<string, string> ff = by.Get(oid);
			b.Create(CHANGE, DZS_PROJECT_ID, DZS_END, ff);
		}
	}
}

class DZS_TerrSection : DZS_ProjectSection
{
	override string Name() { return DZS_TerrModel.PLUGIN; }

	override string Text(DZS_Instance inst)
	{
		DZS_Json frag = DZS_TerrModel.Fragment(inst);
		if (!frag)
			return "";
		array<string> chunks = frag.ToChunks();
		return DZS_SpawnerFormat.Join(chunks);
	}
}

// The base's kinds and zones: cfgenvironment.xml (herd and ambient types with their files), the registered env
// files, db/events.xml (classes and counts of Animal*/Ambient*/Infected* events).
class DZS_TerrBase : Managed
{
	string m_Dir;
	ref map<string, ref DZS_TerrKind> m_Kinds = new map<string, ref DZS_TerrKind>();
	ref array<ref DZS_TerrZone> m_Zones = new array<ref DZS_TerrZone>();

	static DZS_TerrBase Load(string dir)
	{
		DZS_TerrBase b = new DZS_TerrBase();
		b.m_Dir = dir;
		if (dir == "")
			return b;
		DZS_PatchFiles files = new DZS_PatchFiles(dir);
		DZS_PatchFile ev = files.Get("db/events.xml");
		map<string, ref array<string>> classes = new map<string, ref array<string>>();
		map<string, string> nominals = new map<string, string>();
		if (ev.Ok())
		{
			foreach (DZS_XmlNode e : ev.m_Xml.m_Root.m_Children)
			{
				if (e.m_Name != "event")
					continue;
				string en = e.Get("name");
				array<string> cl = new array<string>();
				foreach (DZS_XmlNode c : e.m_Children)
				{
					if (c.m_Name == "nominal")
						nominals.Set(en, ev.Text(c));
					if (c.m_Name != "children")
						continue;
					foreach (DZS_XmlNode k : c.m_Children)
					{
						if (k.m_Name == "child")
							cl.Insert(k.Get("type"));
					}
				}
				classes.Set(en, cl);
			}
		}
		DZS_PatchFile env = files.Get("cfgenvironment.xml");
		map<string, string> fileOf = new map<string, string>();
		if (env.Ok())
		{
			foreach (DZS_XmlNode t0 : env.m_Xml.m_Root.m_Children)
			{
				if (t0.m_Name != "territories")
					continue;
				foreach (DZS_XmlNode t : t0.m_Children)
				{
					if (t.m_Name == "file")
					{
						string path = DZS_PKind.EnvFile(t.Get("path"));
						if (path != "")
						{
							string base = path.Substring(4, path.Length() - 8);
							fileOf.Set(base, path);
						}
					}
				}
				foreach (DZS_XmlNode tt : t0.m_Children)
				{
					if (tt.m_Name != "territory")
						continue;
					string tname = tt.Get("name");
					string usable = "";
					foreach (DZS_XmlNode u : tt.m_Children)
					{
						if (u.m_Name == "file")
							usable = DZS_PatchText.Lower(u.Get("usable"));
					}
					string tfile = "";
					fileOf.Find(usable, tfile);
					if (tfile == "" || tname == "ZombieTest")
						continue;
					DZS_TerrKind hk = new DZS_TerrKind();
					string evName = tname;
					if (tt.Get("type") == "Herd")
					{
						hk.m_Id = "herd:" + tname;
						evName = "Animal" + tname;
					}
					else
						hk.m_Id = "ambient:" + tname;
					hk.m_Label = tname;
					hk.m_Event = evName;
					hk.m_File = tfile;
					array<string> hc = classes.Get(evName);
					if (hc)
						hk.m_Classes.Copy(hc);
					string hn;
					if (nominals.Find(evName, hn))
					{
						hk.m_Nominal = hn;
						hk.m_BaseNominal = hn;
					}
					b.m_Kinds.Set(hk.m_Id, hk);
				}
			}
		}
		foreach (string en2, array<string> cls : classes)
		{
			if (en2.IndexOf("Infected") != 0)
				continue;
			DZS_TerrKind ik = new DZS_TerrKind();
			ik.m_Id = "infected:" + en2;
			ik.m_Label = en2;
			ik.m_Event = en2;
			ik.m_File = ZOMBIES_FILE();
			ik.m_Classes.Copy(cls);
			b.m_Kinds.Set(ik.m_Id, ik);
		}
		// zones of every kind's file
		array<string> done = new array<string>();
		foreach (string kid, DZS_TerrKind kk : b.m_Kinds)
		{
			if (done.Find(kk.m_File) >= 0)
				continue;
			done.Insert(kk.m_File);
			DZS_PatchFile zf = files.Get(kk.m_File);
			if (!zf.Ok())
				continue;
			foreach (DZS_XmlNode terr : zf.m_Xml.m_Root.m_Children)
			{
				if (terr.m_Name != "territory")
					continue;
				string color = terr.Get("color");
				foreach (DZS_XmlNode z : terr.m_Children)
				{
					if (z.m_Name != "zone")
						continue;
					DZS_TerrZone tz = new DZS_TerrZone();
					tz.m_File = kk.m_File;
					tz.m_Color = color;
					tz.m_Name = z.Get("name");
					tz.m_BX = z.Get("x");
					tz.m_BZ = z.Get("z");
					tz.m_BR = z.Get("r");
					tz.m_BDmin = z.Get("dmin");
					tz.m_BDmax = z.Get("dmax");
					tz.m_Smin = z.Get("smin");
					tz.m_Smax = z.Get("smax");
					tz.m_Dmin = tz.m_BDmin;
					tz.m_Dmax = tz.m_BDmax;
					tz.m_X = tz.m_BX.ToFloat();
					tz.m_Z = tz.m_BZ.ToFloat();
					tz.m_R = tz.m_BR.ToFloat();
					tz.m_Kind = b.KindOfZone(kk.m_File, tz.m_Name);
					if (tz.m_Kind == "infected:" + tz.m_Name)
					{
						DZS_TerrKind owner = b.m_Kinds.Get(tz.m_Kind);
						if (owner && owner.m_Color == "")
							owner.m_Color = color;
					}
					b.m_Zones.Insert(tz);
				}
			}
		}
		return b;
	}

	static string ZOMBIES_FILE()
	{
		return DZS_TerrModel.ZOMBIES;
	}

	// The kind a zone belongs to: infected zones by their name, the others by their file's herd or ambient type.
	string KindOfZone(string file, string name)
	{
		if (file == DZS_TerrModel.ZOMBIES)
			return "infected:" + name;
		foreach (string id, DZS_TerrKind k : m_Kinds)
		{
			if (k.m_File == file && id.IndexOf("infected:") != 0)
				return id;
		}
		return "";
	}

	// The kind whose event has this class as a child, "" when none (for placing an animal or an infected).
	string KindOfClass(string cls)
	{
		string low = DZS_PatchText.Lower(cls);
		foreach (string id, DZS_TerrKind k : m_Kinds)
		{
			foreach (string c : k.m_Classes)
			{
				if (DZS_PatchText.Lower(c) == low)
					return id;
			}
		}
		return "";
	}

	void Effective(DZS_Instance inst, array<ref DZS_TerrZone> zones, map<string, ref DZS_TerrKind> kinds)
	{
		zones.Clear();
		kinds.Clear();
		foreach (string kid, DZS_TerrKind bk : m_Kinds)
		{
			DZS_TerrKind ck = new DZS_TerrKind();
			ck.m_Id = bk.m_Id;
			ck.m_Label = bk.m_Label;
			ck.m_Event = bk.m_Event;
			ck.m_File = bk.m_File;
			ck.m_Color = bk.m_Color;
			ck.m_Classes.Copy(bk.m_Classes);
			ck.m_Nominal = bk.m_Nominal;
			ck.m_BaseNominal = bk.m_BaseNominal;
			kinds.Set(kid, ck);
		}
		array<string> noms = DZS_TerrModel.Of(inst, "territories:nominal");
		foreach (string nid : noms)
		{
			string nev = DZS_TerrModel.Field(inst, nid, "event");
			foreach (string kid2, DZS_TerrKind k2 : kinds)
			{
				if (k2.m_Event == nev)
					k2.m_Nominal = DZS_TerrModel.Field(inst, nid, "to");
			}
		}
		map<string, string> touched = new map<string, string>();
		array<string> chs = DZS_TerrModel.Of(inst, DZS_TerrModel.CHANGE);
		foreach (string cid : chs)
		{
			string tk = DZS_TerrModel.Field(inst, cid, "file") + ":" + DZS_TerrModel.Field(inst, cid, "name") + ":" + DZS_TerrModel.Field(inst, cid, "bx") + ":" + DZS_TerrModel.Field(inst, cid, "bz");
			touched.Set(tk, cid);
		}
		foreach (DZS_TerrZone bz : m_Zones)
		{
			DZS_TerrZone z = new DZS_TerrZone();
			z.m_File = bz.m_File;
			z.m_Color = bz.m_Color;
			z.m_Name = bz.m_Name;
			z.m_Kind = bz.m_Kind;
			z.m_BX = bz.m_BX;
			z.m_BZ = bz.m_BZ;
			z.m_BR = bz.m_BR;
			z.m_BDmin = bz.m_BDmin;
			z.m_BDmax = bz.m_BDmax;
			z.m_Smin = bz.m_Smin;
			z.m_Smax = bz.m_Smax;
			z.m_X = bz.m_X;
			z.m_Z = bz.m_Z;
			z.m_R = bz.m_R;
			z.m_Dmin = bz.m_Dmin;
			z.m_Dmax = bz.m_Dmax;
			string cid2 = "";
			if (touched.Find(bz.m_File + ":" + bz.m_Name + ":" + bz.m_BX + ":" + bz.m_BZ, cid2))
			{
				z.m_Elem = cid2;
				z.m_Changed = true;
				if (DZS_TerrModel.Field(inst, cid2, "gone") == "1")
					z.m_Gone = true;
				string v;
				v = DZS_TerrModel.Field(inst, cid2, "x");
				if (v != "")
					z.m_X = v.ToFloat();
				v = DZS_TerrModel.Field(inst, cid2, "z");
				if (v != "")
					z.m_Z = v.ToFloat();
				v = DZS_TerrModel.Field(inst, cid2, "r");
				if (v != "")
					z.m_R = v.ToFloat();
				v = DZS_TerrModel.Field(inst, cid2, "dmin");
				if (v != "")
					z.m_Dmin = v;
				v = DZS_TerrModel.Field(inst, cid2, "dmax");
				if (v != "")
					z.m_Dmax = v;
			}
			zones.Insert(z);
		}
		map<string, string> terrFile = new map<string, string>();
		array<string> terrs = DZS_TerrModel.Of(inst, DZS_TerrModel.TERR);
		foreach (string tid : terrs)
			terrFile.Set(tid, DZS_TerrModel.Field(inst, tid, "file"));
		array<string> own = DZS_TerrModel.Of(inst, DZS_TerrModel.ZONE);
		foreach (string zid : own)
		{
			DZS_TerrZone o = new DZS_TerrZone();
			o.m_Own = true;
			o.m_Elem = zid;
			o.m_Name = DZS_TerrModel.Field(inst, zid, "name");
			string owner = DZS_TerrModel.Field(inst, zid, "territory");
			if (owner != "")
				o.m_File = terrFile.Get(owner);
			else
				o.m_File = DZS_TerrModel.Field(inst, zid, "file");
			o.m_Color = DZS_TerrModel.Field(inst, zid, "color");
			o.m_X = DZS_TerrModel.Field(inst, zid, "x").ToFloat();
			o.m_Z = DZS_TerrModel.Field(inst, zid, "z").ToFloat();
			o.m_R = DZS_TerrModel.Field(inst, zid, "r").ToFloat();
			o.m_Dmin = DZS_TerrModel.Field(inst, zid, "dmin");
			o.m_Dmax = DZS_TerrModel.Field(inst, zid, "dmax");
			o.m_Kind = KindOfZone(o.m_File, o.m_Name);
			zones.Insert(o);
		}
	}
}
