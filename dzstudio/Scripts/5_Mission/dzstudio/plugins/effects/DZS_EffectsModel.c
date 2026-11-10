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
	// carried over from the file, not editable here (DZS_KeptBlocks): an own area of the file or a base area a kept
	// change touches
	bool m_Kept;

	string Key()
	{
		if (m_Kept && m_Own)
			return "k:" + m_Elem;
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
	// 'above': metres above the ground (aboveGround, the position is then [x, z]), or "" for a position on height 0.
	static DZS_Json PresetData(string preset, string name, string x, string z, string r, string above = "")
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
		if (above != "")
			Num(d, "aboveGround", above);
		DZS_Json data = DZS_Json.Obj();
		DZS_Json pos = DZS_Json.Arr();
		DZS_Json px = DZS_Json.Num(x);
		pos.Add(px);
		if (above == "")
		{
			DZS_Json py = DZS_Json.Num("0");
			pos.Add(py);
		}
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
		return Build(DZS_LayerSrc.Of(inst));
	}

	static DZS_Json Build(DZS_LayerSrc s)
	{
		DZS_Json blocks = DZS_Json.Arr();
		DZS_Json changes = DZS_Json.Arr();
		array<string> own = s.All(AREA);
		foreach (string oid : own)
		{
			DZS_Json data = PresetData(s.Field(oid, "preset"), s.Field(oid, "name"), s.Field(oid, "x"), s.Field(oid, "z"), s.Field(oid, "r"), s.Field(oid, "above"));
			DZS_Json b = DZS_SpawnsModel.Block(s.BlockId(oid, "fx-"), "effectArea", null, "", data);
			s.Attach(b, oid, DZS_EntityExts.SLOT);
			blocks.Add(b);
		}
		array<string> chs = s.All(CHANGE);
		foreach (string cid : chs)
		{
			string name = s.Field(cid, "area");
			if (s.Field(cid, "gone") == "1")
			{
				DZS_Json rc = DZS_Json.Obj();
				Str(rc, "op", "remove");
				Str(rc, "block", "effectArea");
				DZS_Json rk = NameKey(name);
				rc.Set("key", rk);
				s.Attach(rc, cid, DZS_EntityExts.ChangeSlot(rc));
				changes.Add(rc);
				continue;
			}
			string by = s.Field(cid, "by");
			if (s.Field(cid, "x") != s.Field(cid, "bx") || s.Field(cid, "z") != s.Field(cid, "bz"))
			{
				DZS_Json pc = DZS_Json.Obj();
				Str(pc, "op", "set");
				Str(pc, "block", "effectArea");
				DZS_Json pk = NameKey(name);
				pc.Set("key", pk);
				Str(pc, "field", "Data.Pos");
				DZS_Json was = Vec(s.Field(cid, "bx"), by, s.Field(cid, "bz"));
				pc.Set("from", was);
				DZS_Json now = Vec(s.Field(cid, "x"), by, s.Field(cid, "z"));
				pc.Set("to", now);
				s.Attach(pc, cid, DZS_EntityExts.ChangeSlot(pc));
				changes.Add(pc);
			}
			if (s.Field(cid, "r") != s.Field(cid, "br"))
			{
				DZS_Json qc = DZS_Json.Obj();
				Str(qc, "op", "set");
				Str(qc, "block", "effectArea");
				DZS_Json qk = NameKey(name);
				qc.Set("key", qk);
				Str(qc, "field", "Data.Radius");
				Num(qc, "from", s.Field(cid, "br"));
				Num(qc, "to", s.Field(cid, "r"));
				s.Attach(qc, cid, DZS_EntityExts.ChangeSlot(qc));
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
		Read(DZS_LayerSrc.Into(b), frag);
	}

	static void Read(DZS_LayerSrc s, DZS_Json frag)
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
				// a position above the ground is [x, z]
				int zi = 2;
				if (pos && pos.IsArr() && pos.Count() == 2)
					zi = 1;
				f.Set("x", VecPart(pos, 0));
				f.Set("z", VecPart(pos, zi));
				DZS_Json ab = d.Get("aboveGround");
				if (ab && ab.IsNum())
					f.Set("above", ab.m_Str);
				f.Set("r", data.Get("Radius").m_Str);
				s.TakeExts(f, DZS_EntityExts.SLOT, bl);
				s.Add(AREA, f, bl.GetStr("id"), "fx-");
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
			s.TakeExts(f2, DZS_EntityExts.ChangeSlot(ch), ch);
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
			s.Add(CHANGE, ff);
		}
	}
}

class DZS_EffectsSection : DZS_ProjectSection
{
	override string Name() { return DZS_EffectsModel.PLUGIN; }

	override DZS_Json Fragment(DZS_Instance inst)
	{
		return DZS_EffectsModel.Fragment(inst);
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
		Kept(inst, out_);
	}

	protected void Kept(DZS_Instance inst, array<ref DZS_FxArea> out_)
	{
		array<ref DZS_Json> kch = DZS_KeptBlocks.Entries(inst, "changes", "effectArea");
		foreach (DZS_Json kc : kch)
		{
			DZS_Json key = kc.Get("key");
			if (!key || !key.IsObj())
				continue;
			foreach (DZS_FxArea ba : out_)
			{
				if (ba.m_Own || ba.m_Name != key.GetStr("name"))
					continue;
				ba.m_Kept = true;
				if (kc.GetStr("op") == "remove")
					ba.m_Gone = true;
				else if (kc.GetStr("field") == "Data.Pos")
				{
					DZS_Json to = kc.Get("to");
					if (to && to.IsArr() && to.Count() >= 2)
					{
						ba.m_X = to.At(0).m_Str.ToFloat();
						ba.m_Z = to.At(to.Count() - 1).m_Str.ToFloat();
					}
				}
				else if (kc.GetStr("field") == "Data.Radius" && DZS_KeptBlocks.Num(kc, "to") != "")
					ba.m_R = DZS_KeptBlocks.Num(kc, "to").ToFloat();
			}
		}
		array<ref DZS_Json> kbl = DZS_KeptBlocks.Entries(inst, "blocks", "effectArea");
		foreach (DZS_Json kb : kbl)
		{
			DZS_Json d = kb.Get("data");
			if (!d || !d.IsObj())
				continue;
			DZS_Json data = d.Get("Data");
			if (!data || !data.IsObj())
				continue;
			DZS_Json pos = data.Get("Pos");
			if (!pos || !pos.IsArr() || pos.Count() < 2)
				continue;
			DZS_FxArea k = new DZS_FxArea();
			k.m_Own = true;
			k.m_Kept = true;
			k.m_Elem = kb.GetStr("id");
			k.m_Name = d.GetStr("AreaName");
			k.m_Type = d.GetStr("Type");
			DZS_Json pd = d.Get("PlayerData");
			string around = "";
			if (pd && pd.IsObj())
				around = pd.GetStr("AroundPartName");
			k.m_Preset = DZS_EffectsModel.PresetOf(k.m_Type, around);
			k.m_X = pos.At(0).m_Str.ToFloat();
			k.m_Z = pos.At(pos.Count() - 1).m_Str.ToFloat();
			k.m_R = DZS_KeptBlocks.Num(data, "Radius").ToFloat();
			out_.Insert(k);
		}
	}
}
