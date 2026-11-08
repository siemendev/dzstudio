// The Effects layer's state: own effect areas made from a preset (gas, geyser, hot spring, volcanic vent, hearts,
// smoke) and changes to the base's areas (moved, resized, removed). Saved as a building-block fragment in
// project.plugins.effects, like the Spawns layer.
class DZS_FxArea : Managed
{
	string m_Name;
	string m_Type;
	string m_Preset;
	float m_X;
	float m_Y;
	float m_Z;
	float m_R;
	// the base's values (a moved or resized base area keeps them as the key and "from")
	string m_BX;
	string m_BY;
	string m_BZ;
	string m_BR;
	string m_Elem;
	bool m_Own;
	bool m_Changed;
	bool m_Gone;

	string Key()
	{
		if (m_Own)
			return "e:" + m_Elem;
		return "b:" + m_Name;
	}
}

class DZS_EffectsModel
{
	static const string PLUGIN = "effects";
	static const string AREA = "effects:area";
	static const string CHANGE = "effects:change";
	static const string OP = "effects:edit";

	static array<string> Presets()
	{
		array<string> r = {"gas", "geyser", "spring", "volcanic", "hearts", "smoke"};
		return r;
	}

	static string PresetName(string p)
	{
		if (p == "gas")
			return DZS_Texts.Pick("Gas", "Gas");
		if (p == "geyser")
			return DZS_Texts.Pick("Geysir", "Geyser");
		if (p == "spring")
			return DZS_Texts.Pick("Heiße Quelle", "Hot spring");
		if (p == "volcanic")
			return DZS_Texts.Pick("Vulkan", "Volcanic vent");
		if (p == "hearts")
			return DZS_Texts.Pick("Herzchen", "Hearts");
		if (p == "smoke")
			return DZS_Texts.Pick("Rauch", "Smoke");
		return p;
	}

	static float PresetRadius(string p)
	{
		if (p == "gas")
			return 75;
		if (p == "geyser")
			return 2;
		if (p == "spring" || p == "volcanic")
			return 8;
		if (p == "hearts")
			return 40;
		return 2;
	}

	// The preset an existing area looks like, for its colour and label.
	static string PresetOf(string type, string around)
	{
		if (type == "ContaminatedArea_Static")
			return "gas";
		if (type == "GeyserArea")
			return "geyser";
		if (type == "HotSpringArea")
			return "spring";
		if (type == "VolcanicArea")
			return "volcanic";
		if (type == "SpookyArea" && around.IndexOf("cupid") >= 0)
			return "hearts";
		if (type == "SpookyArea")
			return "smoke";
		return "";
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

	// A complete effect area of a preset (every field written, the engine has no usable defaults).
	static DZS_Json PresetData(string preset, string name, string x, string z, string r)
	{
		string type = "SpookyArea";
		string trig = "";
		string part = "";
		string around = "";
		string tiny = "";
		string ppe = "";
		string posH = "20";
		string negH = "5";
		string inner = "0";
		string innerDist = "0";
		string outerOff = "0";
		string interval = "0";
		string duration = "0";
		if (preset == "gas")
		{
			type = "ContaminatedArea_Static";
			trig = "ContaminatedTrigger";
			part = "graphics/particles/contaminated_area_gas_bigass";
			around = "graphics/particles/contaminated_area_gas_around";
			tiny = "graphics/particles/contaminated_area_gas_around_tiny";
			ppe = "PPERequester_ContaminatedAreaTint";
			negH = "3";
			innerDist = "80";
			outerOff = "30";
		}
		else if (preset == "geyser")
		{
			type = "GeyserArea";
			trig = "GeyserTrigger";
			interval = "3600";
			duration = "30";
		}
		else if (preset == "spring")
		{
			type = "HotSpringArea";
			trig = "HotSpringTrigger";
			posH = "5";
		}
		else if (preset == "volcanic")
		{
			type = "VolcanicArea";
			trig = "VolcanicTrigger";
		}
		else if (preset == "hearts")
		{
			trig = "SpookyTrigger";
			around = "graphics/particles/cupid_bolt";
			tiny = "graphics/particles/cupid_bolt";
			negH = "10";
		}
		else
		{
			part = "graphics/particles/smoke_heli_wreck_01";
			posH = "10";
			negH = "2";
			inner = "1";
			innerDist = "1";
		}
		DZS_Json d = DZS_Json.Obj();
		Str(d, "AreaName", name);
		Str(d, "Type", type);
		Str(d, "TriggerType", trig);
		DZS_Json data = DZS_Json.Obj();
		DZS_Json pos = DZS_Json.Arr();
		DZS_Json px = DZS_Json.Num(x);
		pos.Add(px);
		DZS_Json py = DZS_Json.Num("0");
		pos.Add(py);
		DZS_Json pz = DZS_Json.Num(z);
		pos.Add(pz);
		data.Set("Pos", pos);
		Num(data, "Radius", r);
		Num(data, "PosHeight", posH);
		Num(data, "NegHeight", negH);
		Num(data, "InnerRingCount", inner);
		Num(data, "InnerPartDist", innerDist);
		DZS_Json off = DZS_Json.Boolean(false);
		data.Set("OuterRingToggle", off);
		Num(data, "OuterPartDist", "0");
		Num(data, "OuterOffset", outerOff);
		Num(data, "VerticalLayers", "0");
		Num(data, "VerticalOffset", "0");
		Str(data, "ParticleName", part);
		Num(data, "EffectInterval", interval);
		Num(data, "EffectDuration", duration);
		DZS_Json mod = DZS_Json.Boolean(false);
		data.Set("EffectModifier", mod);
		d.Set("Data", data);
		DZS_Json pd = DZS_Json.Obj();
		Str(pd, "AroundPartName", around);
		Str(pd, "TinyPartName", tiny);
		Str(pd, "PPERequesterType", ppe);
		d.Set("PlayerData", pd);
		return d;
	}

	static array<string> Of(DZS_Instance inst, string kind)
	{
		return DZS_SpawnsModel.Of(inst, kind);
	}

	static string Field(DZS_Instance inst, string id, string key)
	{
		return DZS_SpawnsModel.Field(inst, id, key);
	}

	static DZS_Json NameKey(string name)
	{
		DZS_Json k = DZS_Json.Obj();
		Str(k, "name", name);
		return k;
	}

	static DZS_Json Vec(string x, string y, string z)
	{
		DZS_Json v = DZS_Json.Arr();
		DZS_Json a = DZS_Json.Num(x);
		v.Add(a);
		DZS_Json b = DZS_Json.Num(y);
		v.Add(b);
		DZS_Json c = DZS_Json.Num(z);
		v.Add(c);
		return v;
	}

	static DZS_Json Fragment(DZS_Instance inst)
	{
		DZS_Json blocks = DZS_Json.Arr();
		DZS_Json changes = DZS_Json.Arr();
		array<string> own = Of(inst, AREA);
		foreach (string oid : own)
		{
			DZS_Json data = PresetData(Field(inst, oid, "preset"), Field(inst, oid, "name"), Field(inst, oid, "x"), Field(inst, oid, "z"), Field(inst, oid, "r"));
			DZS_Json b = DZS_SpawnsModel.Block("fx-" + DZS_ProjectV2.IdPart(oid), "effectArea", null, "", data);
			blocks.Add(b);
		}
		array<string> chs = Of(inst, CHANGE);
		foreach (string cid : chs)
		{
			string name = Field(inst, cid, "area");
			if (Field(inst, cid, "gone") == "1")
			{
				DZS_Json rc = DZS_Json.Obj();
				Str(rc, "op", "remove");
				Str(rc, "block", "effectArea");
				DZS_Json rk = NameKey(name);
				rc.Set("key", rk);
				changes.Add(rc);
				continue;
			}
			string by = Field(inst, cid, "by");
			if (Field(inst, cid, "x") != Field(inst, cid, "bx") || Field(inst, cid, "z") != Field(inst, cid, "bz"))
			{
				DZS_Json pc = DZS_Json.Obj();
				Str(pc, "op", "set");
				Str(pc, "block", "effectArea");
				DZS_Json pk = NameKey(name);
				pc.Set("key", pk);
				Str(pc, "field", "Data.Pos");
				DZS_Json was = Vec(Field(inst, cid, "bx"), by, Field(inst, cid, "bz"));
				pc.Set("from", was);
				DZS_Json now = Vec(Field(inst, cid, "x"), by, Field(inst, cid, "z"));
				pc.Set("to", now);
				changes.Add(pc);
			}
			if (Field(inst, cid, "r") != Field(inst, cid, "br"))
			{
				DZS_Json qc = DZS_Json.Obj();
				Str(qc, "op", "set");
				Str(qc, "block", "effectArea");
				DZS_Json qk = NameKey(name);
				qc.Set("key", qk);
				Str(qc, "field", "Data.Radius");
				Num(qc, "from", Field(inst, cid, "br"));
				Num(qc, "to", Field(inst, cid, "r"));
				changes.Add(qc);
			}
		}
		if (blocks.Count() == 0 && changes.Count() == 0)
			return null;
		DZS_Json r = DZS_Json.Obj();
		r.Set("blocks", blocks);
		r.Set("changes", changes);
		return r;
	}

	static string VecPart(DZS_Json v, int i)
	{
		if (!v || !v.IsArr() || v.Count() <= i)
			return "";
		return v.At(i).m_Str;
	}

	static void Restore(DZS_OpBuilder b, DZS_Json frag)
	{
		if (!frag || !frag.IsObj())
			return;
		DZS_Json blocks = frag.Get("blocks");
		if (blocks && blocks.IsArr())
		{
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json bl = blocks.At(i);
				if (bl.GetStr("kind") != "effectArea")
					continue;
				DZS_Json d = bl.Get("data");
				DZS_Json data = d.Get("Data");
				DZS_Json pd = d.Get("PlayerData");
				DZS_Json pos = data.Get("Pos");
				map<string, string> f = new map<string, string>();
				f.Set("name", d.GetStr("AreaName"));
				f.Set("preset", PresetOf(d.GetStr("Type"), pd.GetStr("AroundPartName")));
				f.Set("x", VecPart(pos, 0));
				f.Set("z", VecPart(pos, 2));
				f.Set("r", data.Get("Radius").m_Str);
				b.Create(AREA, DZS_PROJECT_ID, DZS_END, f);
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
			string name = ch.Get("key").GetStr("name");
			map<string, string> f2 = by.Get(name);
			if (!f2)
			{
				f2 = new map<string, string>();
				f2.Set("area", name);
				by.Set(name, f2);
				order.Insert(name);
			}
			if (ch.GetStr("op") == "remove")
			{
				f2.Set("gone", "1");
				continue;
			}
			if (ch.GetStr("field") == "Data.Pos")
			{
				DZS_Json from = ch.Get("from");
				DZS_Json to = ch.Get("to");
				f2.Set("bx", VecPart(from, 0));
				f2.Set("by", VecPart(from, 1));
				f2.Set("bz", VecPart(from, 2));
				f2.Set("x", VecPart(to, 0));
				f2.Set("z", VecPart(to, 2));
			}
			else if (ch.GetStr("field") == "Data.Radius")
			{
				f2.Set("br", ch.Get("from").m_Str);
				f2.Set("r", ch.Get("to").m_Str);
			}
		}
		foreach (string n : order)
		{
			map<string, string> ff = by.Get(n);
			// a change that only moved keeps the radius as it was, one that only resized keeps the position
			if (!ff.Contains("r"))
			{
				ff.Set("r", "");
				ff.Set("br", "");
			}
			if (!ff.Contains("x"))
			{
				ff.Set("x", "");
				ff.Set("bx", "");
				ff.Set("z", "");
				ff.Set("bz", "");
			}
			b.Create(CHANGE, DZS_PROJECT_ID, DZS_END, ff);
		}
	}
}

class DZS_EffectsSection : DZS_ProjectSection
{
	override string Name() { return DZS_EffectsModel.PLUGIN; }

	override string Text(DZS_Instance inst)
	{
		DZS_Json frag = DZS_EffectsModel.Fragment(inst);
		if (!frag)
			return "";
		array<string> chunks = frag.ToChunks();
		return DZS_SpawnerFormat.Join(chunks);
	}
}

// The base's areas (cfgeffectarea.json of the vanilla mission or the mission folder).
class DZS_EffectsBase : Managed
{
	string m_Dir;
	ref array<ref DZS_FxArea> m_Areas = new array<ref DZS_FxArea>();

	static DZS_EffectsBase Load(string dir)
	{
		DZS_EffectsBase b = new DZS_EffectsBase();
		b.m_Dir = dir;
		if (dir == "")
			return b;
		DZS_PatchFiles files = new DZS_PatchFiles(dir);
		DZS_PatchFile f = files.Get("cfgeffectarea.json");
		if (!f.Ok() || !f.m_Json.m_Root.IsObj())
			return b;
		DZS_SrcNode areas = f.m_Json.m_Root.Get("Areas");
		if (!areas || !areas.IsArr())
			return b;
		foreach (DZS_SrcNode a : areas.m_Items)
		{
			if (!a.IsObj())
				continue;
			DZS_SrcNode data = a.Get("Data");
			DZS_SrcNode name = a.Get("AreaName");
			DZS_SrcNode type = a.Get("Type");
			if (!data || !data.IsObj() || !name || !name.IsStr())
				continue;
			DZS_SrcNode pos = data.Get("Pos");
			DZS_SrcNode rad = data.Get("Radius");
			if (!pos || !pos.IsArr() || pos.Count() != 3 || !rad)
				continue;
			DZS_FxArea fa = new DZS_FxArea();
			fa.m_Name = name.m_Str;
			if (type && type.IsStr())
				fa.m_Type = type.m_Str;
			string around = "";
			DZS_SrcNode pl = a.Get("PlayerData");
			if (pl && pl.IsObj() && pl.Get("AroundPartName") && pl.Get("AroundPartName").IsStr())
				around = pl.Get("AroundPartName").m_Str;
			fa.m_Preset = DZS_EffectsModel.PresetOf(fa.m_Type, around);
			fa.m_BX = pos.m_Items[0].m_Str;
			fa.m_BY = pos.m_Items[1].m_Str;
			fa.m_BZ = pos.m_Items[2].m_Str;
			fa.m_BR = rad.m_Str;
			fa.m_X = fa.m_BX.ToFloat();
			fa.m_Y = fa.m_BY.ToFloat();
			fa.m_Z = fa.m_BZ.ToFloat();
			fa.m_R = fa.m_BR.ToFloat();
			b.m_Areas.Insert(fa);
		}
		return b;
	}

	void Effective(DZS_Instance inst, array<ref DZS_FxArea> out_)
	{
		out_.Clear();
		map<string, string> changed = new map<string, string>();
		array<string> chs = DZS_EffectsModel.Of(inst, DZS_EffectsModel.CHANGE);
		foreach (string cid : chs)
			changed.Set(DZS_EffectsModel.Field(inst, cid, "area"), cid);
		foreach (DZS_FxArea ba : m_Areas)
		{
			DZS_FxArea a = new DZS_FxArea();
			a.m_Name = ba.m_Name;
			a.m_Type = ba.m_Type;
			a.m_Preset = ba.m_Preset;
			a.m_BX = ba.m_BX;
			a.m_BY = ba.m_BY;
			a.m_BZ = ba.m_BZ;
			a.m_BR = ba.m_BR;
			a.m_X = ba.m_X;
			a.m_Y = ba.m_Y;
			a.m_Z = ba.m_Z;
			a.m_R = ba.m_R;
			string cid2 = "";
			if (changed.Find(ba.m_Name, cid2))
			{
				a.m_Elem = cid2;
				a.m_Changed = true;
				if (DZS_EffectsModel.Field(inst, cid2, "gone") == "1")
					a.m_Gone = true;
				string cx = DZS_EffectsModel.Field(inst, cid2, "x");
				if (cx != "")
				{
					a.m_X = cx.ToFloat();
					a.m_Z = DZS_EffectsModel.Field(inst, cid2, "z").ToFloat();
				}
				string cr = DZS_EffectsModel.Field(inst, cid2, "r");
				if (cr != "")
					a.m_R = cr.ToFloat();
			}
			out_.Insert(a);
		}
		array<string> own = DZS_EffectsModel.Of(inst, DZS_EffectsModel.AREA);
		foreach (string oid : own)
		{
			DZS_FxArea o = new DZS_FxArea();
			o.m_Own = true;
			o.m_Elem = oid;
			o.m_Name = DZS_EffectsModel.Field(inst, oid, "name");
			o.m_Preset = DZS_EffectsModel.Field(inst, oid, "preset");
			o.m_X = DZS_EffectsModel.Field(inst, oid, "x").ToFloat();
			o.m_Z = DZS_EffectsModel.Field(inst, oid, "z").ToFloat();
			o.m_R = DZS_EffectsModel.Field(inst, oid, "r").ToFloat();
			out_.Insert(o);
		}
	}
}
