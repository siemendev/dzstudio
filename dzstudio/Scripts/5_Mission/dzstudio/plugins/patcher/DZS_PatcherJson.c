// The JSON block kinds: effect areas and safe positions (cfgeffectarea.json), gameplay settings (cfggameplay.json),
// own object spawner groups, spawn gear presets and restricted areas and their registration. Every JSON file written is checked with the engine's rules.
class DZS_PatcherJson : DZS_PatcherXml
{
	static const string AREA_FILE = "cfgeffectarea.json";
	static const string GAMEPLAY_FILE = "cfggameplay.json";
	static const string SPAWNERS = "WorldsData.objectSpawnersArr";
	static const string GEAR = "PlayerData.spawnGearPresetFiles";
	static const string PRA = "WorldsData.playerRestrictedAreaFiles";
	// Sakhal's own restricted area: the map default a list made new must keep
	static const string SAKHAL_PRA = "pra/warheadstorage.json";
	static const int GEAR_SET = 0;
	static const int GEAR_CHILD = 1;
	static const int GEAR_CARGO = 2;

	protected static ref array<ref DZS_PField> s_AreaFields;

	static array<ref DZS_PField> AreaFields()
	{
		if (s_AreaFields)
			return s_AreaFields;
		s_AreaFields = new array<ref DZS_PField>();
		A("AreaName", DZS_PField.STR);
		A("Type", DZS_PField.STR);
		A("TriggerType", DZS_PField.STR);
		A("Data/Pos", DZS_PField.VEC3);
		A("Data/Radius", DZS_PField.NUM);
		A("Data/PosHeight", DZS_PField.NUM);
		A("Data/NegHeight", DZS_PField.NUM);
		A("Data/InnerRingCount", DZS_PField.INT);
		A("Data/InnerPartDist", DZS_PField.INT);
		A("Data/OuterRingToggle", DZS_PField.BOOL);
		A("Data/OuterPartDist", DZS_PField.INT);
		A("Data/OuterOffset", DZS_PField.INT);
		A("Data/VerticalLayers", DZS_PField.INT);
		A("Data/VerticalOffset", DZS_PField.INT);
		A("Data/ParticleName", DZS_PField.STR);
		A("Data/EffectInterval", DZS_PField.INT);
		A("Data/EffectDuration", DZS_PField.INT);
		A("Data/EffectModifier", DZS_PField.BOOL);
		A("PlayerData/AroundPartName", DZS_PField.STR);
		A("PlayerData/TinyPartName", DZS_PField.STR);
		A("PlayerData/PPERequesterType", DZS_PField.STR);
		return s_AreaFields;
	}

	protected static void A(string path, int fmt)
	{
		DZS_PField f = new DZS_PField(path, fmt, true);
		s_AreaFields.Insert(f);
	}

	static DZS_PField AreaField(string name)
	{
		array<ref DZS_PField> all = AreaFields();
		foreach (DZS_PField f : all)
		{
			if (f.m_Name == name)
				return f;
		}
		return null;
	}

	// Integer fields the engine refuses with a decimal point: the whole file is then lost.
	static bool IsIntPath(string file, string path)
	{
		array<string> gameplay = {"version", "PlayerData.WeaponObstructionData.staticMode", "PlayerData.WeaponObstructionData.dynamicMode", "WorldsData.lightingConfig", "UIData.HitIndicationData.hitDirectionBehaviour", "UIData.HitIndicationData.hitDirectionStyle", "ExternalLockData.protectionCountStageOne", "ExternalLockData.protectionCountStageTwo"};
		array<string> area = {"Data.InnerRingCount", "Data.InnerPartDist", "Data.OuterPartDist", "Data.OuterOffset", "Data.VerticalLayers", "Data.VerticalOffset", "Data.EffectInterval", "Data.EffectDuration"};
		if (file == AREA_FILE)
			return area.Find(path) >= 0;
		if (file == GAMEPLAY_FILE)
			return gameplay.Find(path) >= 0;
		return false;
	}

	// Engine check of a JSON text: strict syntax, no decimal in an integer field. "" when it loads.
	static string EngineCheck(string file, array<string> chunks)
	{
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (doc.m_Error != "")
			return "parse";
		return IntCheck(file, doc.m_Root);
	}

	static string IntCheck(string file, DZS_SrcNode root)
	{
		if (!root.IsObj())
			return "";
		if (file == AREA_FILE)
		{
			DZS_SrcNode areas = root.Get("Areas");
			if (!areas || !areas.IsArr())
				return "";
			foreach (DZS_SrcNode a : areas.m_Items)
			{
				if (!a.IsObj())
					continue;
				DZS_SrcNode data = a.Get("Data");
				if (!data || !data.IsObj())
					continue;
				for (int i = 0; i < data.m_Keys.Count(); i++)
				{
					DZS_SrcNode v = data.m_Items[i];
					if (v.IsNum() && IsIntPath(file, "Data." + data.m_Keys[i]) && !DZS_PatchNum.IsInt(v.m_Str))
						return "int-field";
				}
			}
			return "";
		}
		if (file == GAMEPLAY_FILE)
			return IntWalk(file, root, "");
		return "";
	}

	protected static string IntWalk(string file, DZS_SrcNode n, string path)
	{
		for (int i = 0; i < n.m_Keys.Count(); i++)
		{
			string p = n.m_Keys[i];
			if (path != "")
				p = path + "." + n.m_Keys[i];
			DZS_SrcNode v = n.m_Items[i];
			if (v.IsObj())
			{
				string r = IntWalk(file, v, p);
				if (r != "")
					return r;
			}
			else if (v.IsNum() && IsIntPath(file, p) && !DZS_PatchNum.IsInt(v.m_Str))
				return "int-field";
		}
		return "";
	}

	// ---- JSON values ----

	// A project value as a JSON value for a field; numbers already in their written form. null and 'err' when it
	// does not fit. Integer fields keep the given number text: the engine check refuses a decimal later.
	static DZS_Json JsonValue(DZS_PField f, DZS_Json v, out string err)
	{
		err = "";
		if (!v || v.m_Type == DZS_Json.NUL)
		{
			err = "bad-value";
			return null;
		}
		int fmt = f.m_Fmt;
		if (fmt == DZS_PField.INT || fmt == DZS_PField.NUM)
		{
			if (!v.IsNum() || !DZS_PatchNum.IsNumber(v.m_Str))
			{
				err = "bad-value";
				return null;
			}
			return DZS_Json.Num(DZS_PatchNum.Canon(v.m_Str));
		}
		if (fmt == DZS_PField.FIX6)
		{
			if (!v.IsNum() || !DZS_PatchNum.IsNumber(v.m_Str))
			{
				err = "bad-value";
				return null;
			}
			return DZS_Json.Num(DZS_PatchNum.Fixed6(v.m_Str));
		}
		if (fmt == DZS_PField.BOOL)
		{
			int b = BoolOf(v);
			if (b < 0)
			{
				err = "bad-value";
				return null;
			}
			return DZS_Json.Boolean(b == 1);
		}
		if (fmt == DZS_PField.VEC3)
		{
			if (!v.IsArr() || v.Count() != 3)
			{
				err = "bad-value";
				return null;
			}
			DZS_Json arr = DZS_Json.Arr();
			for (int i = 0; i < 3; i++)
			{
				DZS_Json c = v.At(i);
				if (!c.IsNum() || !DZS_PatchNum.IsNumber(c.m_Str))
				{
					err = "bad-value";
					return null;
				}
				DZS_Json cn = DZS_Json.Num(DZS_PatchNum.Fixed6(c.m_Str));
				arr.Add(cn);
			}
			return arr;
		}
		if (!v.IsStr())
		{
			err = "bad-value";
			return null;
		}
		return DZS_Json.Str(v.m_Str);
	}

	// Whether a file value equals a project value (numbers numerically, arrays member by member).
	static bool JsonEqual(DZS_SrcNode n, DZS_Json v)
	{
		if (!v || v.m_Type == DZS_Json.NUL)
			return !n || n.IsNull();
		if (!n)
			return false;
		if (n.IsNum())
			return v.IsNum() && DZS_PatchNum.Same(n.m_Str, v.m_Str);
		if (n.IsBool())
		{
			int b = BoolOf(v);
			if (n.m_Bool)
				return b == 1;
			return b == 0;
		}
		if (n.IsStr())
			return v.IsStr() && v.m_Str == n.m_Str;
		if (n.IsArr())
		{
			if (!v.IsArr() || v.Count() != n.Count())
				return false;
			for (int i = 0; i < n.Count(); i++)
			{
				if (!JsonEqual(n.m_Items[i], v.At(i)))
					return false;
			}
			return true;
		}
		return false;
	}

	// Last member of an object by name, or null.
	static int MemberIndex(DZS_SrcNode obj, string name)
	{
		if (!obj || !obj.IsObj())
			return -1;
		for (int i = obj.m_Keys.Count() - 1; i >= 0; i--)
		{
			if (obj.m_Keys[i] == name)
				return i;
		}
		return -1;
	}

	// The node at a dotted path below 'obj' (members removed in this run count as absent), or null.
	DZS_SrcNode JsonAt(DZS_PatchFile f, DZS_SrcNode obj, string path)
	{
		array<string> parts = new array<string>();
		path.Split(".", parts);
		DZS_SrcNode cur = obj;
		foreach (string p : parts)
		{
			int i = MemberIndex(cur, p);
			if (i < 0 || f.ItemRemoved(cur, i))
				return null;
			cur = cur.m_Items[i];
		}
		return cur;
	}

	// Sets a value at a dotted path, creating missing objects on the way. 'v' is the JSON value to write.
	void JsonSet(DZS_PatchFile f, DZS_SrcNode obj, string path, DZS_Json v)
	{
		array<string> parts = new array<string>();
		path.Split(".", parts);
		DZS_SrcNode cur = obj;
		for (int i = 0; i < parts.Count(); i++)
		{
			string p = parts[i];
			int idx = MemberIndex(cur, p);
			if (idx >= 0 && !f.ItemRemoved(cur, idx))
			{
				DZS_SrcNode next = cur.m_Items[idx];
				if (i == parts.Count() - 1)
				{
					string unit = DZS_PatchFile.UnitOf(f.LineIndent(next.m_Start));
					string text = DZS_PatchText.JsonText(v, "", unit, "\n");
					f.SetRaw(next, text);
					return;
				}
				if (!next.IsObj())
					return;
				cur = next;
				continue;
			}
			DZS_Json pending = f.Pending(cur, p);
			if (i == parts.Count() - 1)
			{
				f.AppendItem(cur, p, v);
				return;
			}
			if (!pending)
			{
				pending = DZS_Json.Obj();
				f.AppendItem(cur, p, pending);
			}
			for (int k = i + 1; k < parts.Count() - 1; k++)
			{
				string q = parts[k];
				DZS_Json sub = pending.Get(q);
				if (!sub)
				{
					sub = DZS_Json.Obj();
					pending.Set(q, sub);
				}
				pending = sub;
			}
			pending.Set(parts[parts.Count() - 1], v);
			return;
		}
	}

	// ---- effect areas ----

	// Indices of the areas with this name (not removed).
	array<int> AreasNamed(DZS_PatchFile f, DZS_SrcNode areas, string name)
	{
		array<int> r = new array<int>();
		if (!areas || !areas.IsArr())
			return r;
		for (int i = 0; i < areas.Count(); i++)
		{
			DZS_SrcNode a = areas.m_Items[i];
			if (f.ItemRemoved(areas, i) || !a.IsObj())
				continue;
			DZS_SrcNode n = a.Get("AreaName");
			if (n && n.IsStr() && n.m_Str == name)
				r.Insert(i);
		}
		return r;
	}

	void ChangeArea(DZS_PatchEntry e, DZS_PatchChange c)
	{
		DZS_PatchFile f = File(AREA_FILE);
		if (!Usable(e, f))
			return;
		string name = KeyStr(c.m_Key, "name");
		if (name == "")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-key";
			return;
		}
		DZS_SrcNode root = f.m_Json.m_Root;
		DZS_SrcNode areas = JsonAt(f, root, "Areas");
		array<int> hits = AreasNamed(f, areas, name);
		if (c.m_Op == "remove")
		{
			if (hits.Count() == 0)
			{
				e.m_Status = "gone";
				return;
			}
			if (hits.Count() > 1)
			{
				e.m_Status = "ambiguous";
				return;
			}
			f.RemoveItem(areas, hits[0]);
			e.m_Status = "applied";
			Depends(e, f);
			return;
		}
		if (c.m_Op != "set")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-op";
			return;
		}
		DZS_PField fld = AreaField(c.m_Field);
		if (!fld || fld.m_Name == "AreaName")
		{
			e.m_Status = "invalid";
			e.m_Detail = "unknown-field";
			return;
		}
		string err;
		DZS_Json v = JsonValue(fld, c.m_To, err);
		if (err != "")
		{
			e.m_Status = "invalid";
			e.m_Detail = err;
			return;
		}
		if (hits.Count() == 0)
		{
			e.m_Status = "orphaned";
			return;
		}
		if (hits.Count() > 1)
		{
			e.m_Status = "ambiguous";
			return;
		}
		DZS_SrcNode area = areas.m_Items[hits[0]];
		JsonApply(e, c, f, area, fld.m_Name, v);
	}

	// Compares and writes one JSON leaf (applied / already / overrode).
	void JsonApply(DZS_PatchEntry e, DZS_PatchChange c, DZS_PatchFile f, DZS_SrcNode obj, string path, DZS_Json v)
	{
		DZS_SrcNode cur = JsonAt(f, obj, path);
		DZS_Json curOver = null;
		if (cur)
		{
			string raw = f.Raw(cur);
			string perr;
			curOver = DZS_Json.ParseText(raw, perr);
		}
		if (JsonEqualJ(curOver, v))
		{
			e.m_Status = "already";
			return;
		}
		e.m_Status = "applied";
		if (c.m_HasFrom && !JsonEqualJ(curOver, c.m_From))
		{
			e.m_Status = "overrode";
			if (cur)
				e.m_Current = DZS_Json.Str(f.Raw(cur));
		}
		JsonSet(f, obj, path, v);
		Depends(e, f);
	}

	static bool JsonEqualJ(DZS_Json a, DZS_Json b)
	{
		bool an = !a || a.m_Type == DZS_Json.NUL;
		bool bn = !b || b.m_Type == DZS_Json.NUL;
		if (an || bn)
			return an && bn;
		if (a.IsNum())
			return b.IsNum() && DZS_PatchNum.Same(a.m_Str, b.m_Str);
		if (a.IsBool())
		{
			int bb = BoolOf(b);
			if (a.m_Bool)
				return bb == 1;
			return bb == 0;
		}
		if (a.IsStr())
			return b.IsStr() && a.m_Str == b.m_Str;
		if (a.IsArr())
		{
			if (!b.IsArr() || a.Count() != b.Count())
				return false;
			for (int i = 0; i < a.Count(); i++)
			{
				if (!JsonEqualJ(a.At(i), b.At(i)))
					return false;
			}
			return true;
		}
		return false;
	}

	void OwnArea(DZS_PatchEntry e, DZS_PatchBlock b, array<string> ownNames)
	{
		DZS_PatchFile f = File(AREA_FILE);
		if (!Usable(e, f))
			return;
		DZS_Json obj = DZS_Json.Obj();
		array<ref DZS_PField> fields = AreaFields();
		foreach (DZS_PField fld : fields)
		{
			DZS_Json given = JsonPath(b.m_Data, fld.m_Name);
			string err;
			DZS_Json v = JsonValue(fld, given, err);
			if (err != "")
			{
				e.m_Status = "invalid";
				e.m_Detail = "incomplete";
				if (given)
					e.m_Detail = err;
				return;
			}
			string top = fld.m_Elems[0];
			if (fld.m_Elems.Count() == 1)
				obj.Set(top, v);
			else
			{
				DZS_Json sub = obj.Get(top);
				if (!sub)
				{
					sub = DZS_Json.Obj();
					obj.Set(top, sub);
				}
				sub.Set(fld.m_Elems[1], v);
			}
		}
		string name = obj.GetStr("AreaName");
		DZS_SrcNode root = f.m_Json.m_Root;
		DZS_SrcNode areas = JsonAt(f, root, "Areas");
		array<int> hits = AreasNamed(f, areas, name);
		if (hits.Count() > 0 || ownNames.Find(name) >= 0)
		{
			e.m_Status = "collision";
			return;
		}
		ownNames.Insert(name);
		if (areas && areas.IsArr())
			f.AppendItem(areas, "", obj);
		else
		{
			DZS_Json list = DZS_Json.Arr();
			list.Add(obj);
			JsonSet(f, root, "Areas", list);
		}
		if (obj.GetStr("Type") == "ContaminatedArea_Static" && !JsonAt(f, root, "SafePositions") && !f.Pending(root, "SafePositions"))
		{
			DZS_Json none = DZS_Json.Arr();
			f.AppendItem(root, "SafePositions", none);
		}
		e.m_Status = "applied";
		Depends(e, f);
	}

	// A value at a dotted path of a project object (nested objects or dotted member names), or null.
	static DZS_Json JsonPath(DZS_Json obj, string path)
	{
		if (!obj || !obj.IsObj())
			return null;
		if (obj.Has(path))
			return obj.Get(path);
		int dot = path.IndexOf(".");
		if (dot < 0)
			return null;
		string head = path.Substring(0, dot);
		string rest = path.Substring(dot + 1, path.Length() - dot - 1);
		return JsonPath(obj.Get(head), rest);
	}

	// ---- safe positions ----

	array<int> SafeAt(DZS_PatchFile f, DZS_SrcNode list, string x, string z)
	{
		array<int> r = new array<int>();
		if (!list || !list.IsArr())
			return r;
		for (int i = 0; i < list.Count(); i++)
		{
			DZS_SrcNode p = list.m_Items[i];
			if (f.ItemRemoved(list, i) || !p.IsArr() || p.Count() != 2)
				continue;
			if (DZS_PatchNum.Same(p.m_Items[0].m_Str, x) && DZS_PatchNum.Same(p.m_Items[1].m_Str, z))
				r.Insert(i);
		}
		return r;
	}

	void ChangeSafe(DZS_PatchEntry e, DZS_PatchChange c)
	{
		DZS_PatchFile f = File(AREA_FILE);
		if (!Usable(e, f))
			return;
		string x = KeyStr(c.m_Key, "x");
		string z = KeyStr(c.m_Key, "z");
		if (!DZS_PatchNum.IsNumber(x) || !DZS_PatchNum.IsNumber(z))
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-key";
			return;
		}
		DZS_SrcNode list = JsonAt(f, f.m_Json.m_Root, "SafePositions");
		array<int> hits = SafeAt(f, list, x, z);
		if (c.m_Op == "remove")
		{
			if (hits.Count() == 0)
			{
				e.m_Status = "gone";
				return;
			}
			foreach (int h : hits)
				f.RemoveItem(list, h);
			e.m_Status = "applied";
			Depends(e, f);
			return;
		}
		e.m_Status = "invalid";
		e.m_Detail = "bad-op";
	}

	void OwnSafe(DZS_PatchEntry e, DZS_PatchBlock b, array<string> ownKeys)
	{
		DZS_PatchFile f = File(AREA_FILE);
		if (!Usable(e, f))
			return;
		DZS_Json x = b.m_Data.Get("x");
		DZS_Json z = b.m_Data.Get("z");
		if (!x || !z || !x.IsNum() || !z.IsNum() || !DZS_PatchNum.IsNumber(x.m_Str) || !DZS_PatchNum.IsNumber(z.m_Str) || b.m_Data.Count() != 2)
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-value";
			return;
		}
		DZS_SrcNode root = f.m_Json.m_Root;
		DZS_SrcNode list = JsonAt(f, root, "SafePositions");
		string k = DZS_PatchNum.Fixed6(x.m_Str) + "," + DZS_PatchNum.Fixed6(z.m_Str);
		array<int> hits = SafeAt(f, list, x.m_Str, z.m_Str);
		if (hits.Count() > 0 || ownKeys.Find(k) >= 0)
		{
			e.m_Status = "collision";
			return;
		}
		ownKeys.Insert(k);
		DZS_Json pos = DZS_Json.Arr();
		DZS_Json px = DZS_Json.Num(DZS_PatchNum.Fixed6(x.m_Str));
		DZS_Json pz = DZS_Json.Num(DZS_PatchNum.Fixed6(z.m_Str));
		pos.Add(px);
		pos.Add(pz);
		if (list && list.IsArr())
			f.AppendItem(list, "", pos);
		else
		{
			DZS_Json pend = f.Pending(root, "SafePositions");
			if (pend)
				pend.Add(pos);
			else
			{
				DZS_Json fresh = DZS_Json.Arr();
				fresh.Add(pos);
				f.AppendItem(root, "SafePositions", fresh);
			}
		}
		e.m_Status = "applied";
		Depends(e, f);
	}

	// ---- gameplay settings ----

	static bool IsRegistrationList(string path)
	{
		return path == SPAWNERS || path == GEAR || path == PRA;
	}

	void ChangeGameplay(DZS_PatchEntry e, DZS_PatchChange c)
	{
		DZS_PatchFile f = File(GAMEPLAY_FILE);
		if (!Usable(e, f))
			return;
		string path = KeyStr(c.m_Key, "path");
		if (path == "" || (c.m_Field != "" && c.m_Field != "value"))
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-key";
			return;
		}
		if (IsRegistrationList(path))
		{
			e.m_Status = "invalid";
			e.m_Detail = "registration-list";
			return;
		}
		DZS_SrcNode root = f.m_Json.m_Root;
		DZS_SrcNode cur = JsonAt(f, root, path);
		if (c.m_Op == "remove")
		{
			if (!cur)
			{
				e.m_Status = "gone";
				return;
			}
			int dot = path.LastIndexOf(".");
			DZS_SrcNode parent = root;
			string leaf = path;
			if (dot >= 0)
			{
				parent = JsonAt(f, root, path.Substring(0, dot));
				leaf = path.Substring(dot + 1, path.Length() - dot - 1);
			}
			f.RemoveItem(parent, MemberIndex(parent, leaf));
			e.m_Status = "applied";
			Depends(e, f);
			return;
		}
		if (c.m_Op != "set")
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-op";
			return;
		}
		DZS_Json v = GameplayValue(c.m_To);
		if (!v)
		{
			e.m_Status = "invalid";
			e.m_Detail = "bad-value";
			return;
		}
		if (cur && cur.IsObj())
		{
			e.m_Status = "invalid";
			e.m_Detail = "not-a-leaf";
			return;
		}
		JsonApply(e, c, f, root, path, v);
	}

	// A setting value as written: numbers in their shortest decimal form, scalars and arrays of scalars.
	static DZS_Json GameplayValue(DZS_Json v)
	{
		if (!v)
			return null;
		if (v.IsNum())
		{
			if (!DZS_PatchNum.IsNumber(v.m_Str))
				return null;
			return DZS_Json.Num(DZS_PatchNum.Canon(v.m_Str));
		}
		if (v.IsBool())
			return DZS_Json.Boolean(v.m_Bool);
		if (v.IsStr())
			return DZS_Json.Str(v.m_Str);
		if (v.IsArr())
		{
			DZS_Json a = DZS_Json.Arr();
			for (int i = 0; i < v.Count(); i++)
			{
				DZS_Json it = GameplayValue(v.At(i));
				if (!it || it.IsArr())
					return null;
				a.Add(it);
			}
			return a;
		}
		return null;
	}

	// ---- own object spawner groups ----

	static bool SafeName(string s, bool lowerOnly)
	{
		if (s == "")
			return false;
		string ok = "abcdefghijklmnopqrstuvwxyz0123456789-_";
		if (!lowerOnly)
			ok += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		for (int i = 0; i < s.Length(); i++)
		{
			if (!ok.Contains(s.Get(i)))
				return false;
		}
		return true;
	}

	// The spawner file of a group: "dzstudio/<slug>/<block id>.json" — the same on every run, whatever the group is called.
	string SpawnerPath(DZS_PatchBlock b)
	{
		return "dzstudio/" + m_Project.m_Slug + "/" + b.m_Id + ".json";
	}

	// The file text of an own spawner group, or null with 'err' set.
	static array<string> SpawnerText(DZS_PatchBlock b, out string err)
	{
		err = "";
		DZS_Json objs = b.m_Data.Get("objects");
		if (!objs || !objs.IsArr())
		{
			err = "incomplete";
			return null;
		}
		DZS_Json list = DZS_Json.Arr();
		for (int i = 0; i < objs.Count(); i++)
		{
			DZS_Json o = objs.At(i);
			if (!o.IsObj())
			{
				err = "bad-value";
				return null;
			}
			DZS_Json name = o.Get("name");
			if (!name || !name.IsStr() || name.m_Str == "")
			{
				err = "incomplete";
				return null;
			}
			DZS_PField vec = new DZS_PField("pos", DZS_PField.VEC3, true);
			DZS_Json pos = JsonValue(vec, o.Get("pos"), err);
			if (err != "")
				return null;
			DZS_Json yprIn = o.Get("ypr");
			DZS_Json ypr;
			if (yprIn)
				ypr = JsonValue(vec, yprIn, err);
			else
				ypr = DZS_Json.ParseText("[0.000000, 0.000000, 0.000000]", err);
			if (err != "")
				return null;
			DZS_Json scale = DZS_Json.Num("1");
			DZS_Json scaleIn = o.Get("scale");
			if (scaleIn)
			{
				if (!scaleIn.IsNum() || !DZS_PatchNum.IsNumber(scaleIn.m_Str))
				{
					err = "bad-value";
					return null;
				}
				scale = DZS_Json.Num(DZS_PatchNum.Canon(scaleIn.m_Str));
			}
			bool persist = false;
			DZS_Json persistIn = o.Get("enableCEPersistency");
			if (persistIn)
			{
				int pb = BoolOf(persistIn);
				if (pb < 0)
				{
					err = "bad-value";
					return null;
				}
				persist = pb == 1;
			}
			string custom = "";
			DZS_Json customIn = o.Get("customString");
			if (customIn)
			{
				if (!customIn.IsStr())
				{
					err = "bad-value";
					return null;
				}
				custom = customIn.m_Str;
			}
			DZS_Json e = DZS_Json.Obj();
			DZS_Json nameOut = DZS_Json.Str(name.m_Str);
			e.Set("name", nameOut);
			e.Set("pos", pos);
			e.Set("ypr", ypr);
			e.Set("scale", scale);
			DZS_Json persistOut = DZS_Json.Boolean(persist);
			e.Set("enableCEPersistency", persistOut);
			DZS_Json customOut = DZS_Json.Str(custom);
			e.Set("customString", customOut);
			list.Add(e);
		}
		DZS_Json root = DZS_Json.Obj();
		root.Set("Objects", list);
		string text = DZS_PatchText.JsonText(root, "", "    ", "\n") + "\n";
		return DZS_Files.Chunk(text);
	}

	// ---- own spawn gear presets ----

	// The file text of an own spawn gear preset, written complete (a missing quickBarSlot would bind the item to slot
	// 0, missing attributes drop the variant), or null with 'err' set.
	static array<string> GearText(DZS_PatchBlock b, out string err)
	{
		err = "";
		DZS_Json d = b.m_Data;
		array<string> top = {"spawnWeight", "name", "characterTypes", "attachmentSlotItemSets", "discreteUnsortedItemSets"};
		if (!OnlyKeys(d, top, err))
			return null;
		DZS_Json res = DZS_Json.Obj();
		if (!GearInt(d, "spawnWeight", "", res, err))
			return null;
		if (!GearStr(d, "name", true, false, res, err))
			return null;
		// without character types the player's own choice is kept; an empty list is left out for the same reason
		DZS_Json chars = DZS_Json.Obj();
		if (!GearNames(d, "characterTypes", chars, err))
			return null;
		DZS_Json types = chars.Get("characterTypes");
		if (types && types.Count() > 0)
			res.Set("characterTypes", types);
		DZS_Json slots = DZS_Json.Arr();
		DZS_Json slotsIn = d.Get("attachmentSlotItemSets");
		if (slotsIn)
		{
			if (!slotsIn.IsArr())
			{
				err = "bad-value";
				return null;
			}
			array<string> slotKeys = {"slotName", "discreteItemSets"};
			for (int i = 0; i < slotsIn.Count(); i++)
			{
				DZS_Json slotIn = slotsIn.At(i);
				if (!OnlyKeys(slotIn, slotKeys, err))
					return null;
				DZS_Json slot = DZS_Json.Obj();
				if (!GearStr(slotIn, "slotName", false, true, slot, err))
					return null;
				DZS_Json sets = GearItems(slotIn, "discreteItemSets", GEAR_SET, err);
				if (!sets)
					return null;
				if (sets.Count() == 0)
				{
					err = "incomplete";
					return null;
				}
				slot.Set("discreteItemSets", sets);
				slots.Add(slot);
			}
		}
		res.Set("attachmentSlotItemSets", slots);
		DZS_Json cargo = GearItems(d, "discreteUnsortedItemSets", GEAR_CARGO, err);
		if (!cargo)
			return null;
		res.Set("discreteUnsortedItemSets", cargo);
		string text = DZS_PatchText.JsonText(res, "", "    ", "\n") + "\n";
		return DZS_Files.Chunk(text);
	}

	static bool OnlyKeys(DZS_Json o, array<string> allowed, out string err)
	{
		if (!o || !o.IsObj())
		{
			err = "bad-value";
			return false;
		}
		foreach (string key : o.m_Keys)
		{
			if (allowed.Find(key) < 0)
			{
				err = "unknown-field";
				return false;
			}
		}
		return true;
	}

	// An integer ≥ 1, or the default when absent ("" = required).
	static bool GearInt(DZS_Json o, string name, string def, DZS_Json res, out string err)
	{
		DZS_Json v = o.Get(name);
		if (!v)
		{
			if (def == "")
			{
				err = "incomplete";
				return false;
			}
			DZS_Json dv = DZS_Json.Num(def);
			res.Set(name, dv);
			return true;
		}
		if (!v.IsNum() || !DZS_PatchNum.IsInt(v.m_Str) || (def == "" && v.m_Str.ToInt() < 1))
		{
			err = "bad-value";
			return false;
		}
		DZS_Json nv = DZS_Json.Num(v.m_Str);
		res.Set(name, nv);
		return true;
	}

	static bool GearStr(DZS_Json o, string name, bool opt, bool nonEmpty, DZS_Json res, out string err)
	{
		DZS_Json v = o.Get(name);
		if (!v)
		{
			if (!opt)
			{
				err = "incomplete";
				return false;
			}
			DZS_Json none = DZS_Json.Str("");
			res.Set(name, none);
			return true;
		}
		if (!v.IsStr() || (nonEmpty && v.m_Str == ""))
		{
			err = "bad-value";
			return false;
		}
		DZS_Json sv = DZS_Json.Str(v.m_Str);
		res.Set(name, sv);
		return true;
	}

	// A list of class names, [] when absent.
	static bool GearNames(DZS_Json o, string name, DZS_Json res, out string err)
	{
		DZS_Json names = DZS_Json.Arr();
		DZS_Json v = o.Get(name);
		if (v)
		{
			if (!v.IsArr())
			{
				err = "bad-value";
				return false;
			}
			for (int i = 0; i < v.Count(); i++)
			{
				DZS_Json it = v.At(i);
				if (!it.IsStr() || it.m_Str == "")
				{
					err = "bad-value";
					return false;
				}
				DZS_Json nj = DZS_Json.Str(it.m_Str);
				names.Add(nj);
			}
		}
		res.Set(name, names);
		return true;
	}

	// Item sets of one kind (slot alternatives, complex children, cargo sets), [] when absent. Parameters are read into
	// locals first: Enforce can lose a caller's parameters across a recursive call.
	static DZS_Json GearItems(DZS_Json o, string name, int kind, out string err)
	{
		DZS_Json src = o;
		int kd = kind;
		DZS_Json list = DZS_Json.Arr();
		DZS_Json v = src.Get(name);
		if (!v)
			return list;
		if (!v.IsArr())
		{
			err = "bad-value";
			return null;
		}
		for (int i = 0; i < v.Count(); i++)
		{
			DZS_Json it = v.At(i);
			DZS_Json item = GearItem(it, kd, err);
			if (!item)
				return null;
			list.Add(item);
		}
		return list;
	}

	static DZS_Json GearItem(DZS_Json o, int kind, out string err)
	{
		DZS_Json src = o;
		int kd = kind;
		array<string> allowed = {"attributes", "complexChildrenTypes", "simpleChildrenTypes", "simpleChildrenUseDefaultAttributes"};
		if (kd == GEAR_CARGO)
			allowed.Insert("name");
		else
		{
			allowed.Insert("itemType");
			allowed.Insert("quickBarSlot");
		}
		if (kd != GEAR_CHILD)
			allowed.Insert("spawnWeight");
		if (!OnlyKeys(src, allowed, err))
			return null;
		DZS_Json res = DZS_Json.Obj();
		if (kd == GEAR_CARGO)
		{
			if (!GearStr(src, "name", true, false, res, err))
				return null;
		}
		else if (!GearStr(src, "itemType", false, kd == GEAR_CHILD, res, err))
			return null;
		if (kd != GEAR_CHILD && !GearInt(src, "spawnWeight", "", res, err))
			return null;
		DZS_Json attrs = src.Get("attributes");
		if (!attrs)
		{
			err = "incomplete";
			return null;
		}
		array<string> attrNames = {"healthMin", "healthMax", "quantityMin", "quantityMax"};
		if (!OnlyKeys(attrs, attrNames, err))
			return null;
		DZS_Json a = DZS_Json.Obj();
		foreach (string an : attrNames)
		{
			DZS_Json av = attrs.Get(an);
			if (!av)
			{
				err = "incomplete";
				return null;
			}
			if (!av.IsNum() || !DZS_PatchNum.IsNumber(av.m_Str))
			{
				err = "bad-value";
				return null;
			}
			DZS_Json anum = DZS_Json.Num(DZS_PatchNum.Canon(av.m_Str));
			a.Set(an, anum);
		}
		res.Set("attributes", a);
		if (kd != GEAR_CARGO && !GearInt(src, "quickBarSlot", "-1", res, err))
			return null;
		DZS_Json kids = GearItems(src, "complexChildrenTypes", GEAR_CHILD, err);
		if (!kids)
			return null;
		res.Set("complexChildrenTypes", kids);
		if (!GearNames(src, "simpleChildrenTypes", res, err))
			return null;
		DZS_Json useIn = src.Get("simpleChildrenUseDefaultAttributes");
		bool useDef = false;
		if (useIn)
		{
			int db = BoolOf(useIn);
			if (db < 0)
			{
				err = "bad-value";
				return null;
			}
			useDef = db == 1;
		}
		DZS_Json useOut = DZS_Json.Boolean(useDef);
		res.Set("simpleChildrenUseDefaultAttributes", useOut);
		return res;
	}

	// ---- own restricted areas ----

	// The file text of an own restricted area: a name, at least one box or polygon and at least one safe position
	// (an area without one sends a player nowhere), or null with 'err' set.
	static array<string> PraText(DZS_PatchBlock b, out string err)
	{
		err = "";
		DZS_Json d = b.m_Data;
		array<string> keys = {"areaName", "PRABoxes", "PRAPolygons", "safePositions3D", "safePositions2D"};
		if (!OnlyKeys(d, keys, err))
			return null;
		DZS_Json res = DZS_Json.Obj();
		if (!GearStr(d, "areaName", true, false, res, err))
			return null;
		int shapes = 0;
		int safe = 0;
		array<string> names = {"PRABoxes", "PRAPolygons", "safePositions2D", "safePositions3D"};
		for (int i = 0; i < names.Count(); i++)
		{
			DZS_Json list = PraList(d, names[i], i, err);
			if (!list)
				return null;
			if (i < 2)
				shapes += list.Count();
			else
				safe += list.Count();
			if (list.Count() > 0)
				res.Set(names[i], list);
		}
		if (shapes == 0 || safe == 0)
		{
			err = "incomplete";
			return null;
		}
		string text = DZS_PatchText.JsonText(res, "", "    ", "\n") + "\n";
		return DZS_Files.Chunk(text);
	}

	// One list of a restricted area: shape 0 boxes ([size], [yaw, pitch, roll], [centre]), 1 polygons (≥ 3 [x, z]),
	// 2 points [x, z], 3 points [x, y, z]. Positions are written Fixed6, sizes and angles shortest. [] when absent.
	static DZS_Json PraList(DZS_Json o, string name, int shape, out string err)
	{
		DZS_Json list = DZS_Json.Arr();
		DZS_Json v = o.Get(name);
		if (!v)
			return list;
		if (!v.IsArr())
		{
			err = "bad-value";
			return null;
		}
		for (int i = 0; i < v.Count(); i++)
		{
			DZS_Json it = v.At(i);
			DZS_Json item = null;
			if (shape == 0 && it.IsArr() && it.Count() == 3)
			{
				DZS_Json sz = NumArr(it.At(0), 3, false);
				DZS_Json rot = NumArr(it.At(1), 3, false);
				DZS_Json centre = NumArr(it.At(2), 3, true);
				if (sz && rot && centre)
				{
					item = DZS_Json.Arr();
					item.Add(sz);
					item.Add(rot);
					item.Add(centre);
				}
			}
			else if (shape == 1 && it.IsArr() && it.Count() >= 3)
			{
				item = DZS_Json.Arr();
				for (int k = 0; k < it.Count(); k++)
				{
					DZS_Json pt = NumArr(it.At(k), 2, true);
					if (!pt)
					{
						item = null;
						break;
					}
					item.Add(pt);
				}
			}
			else if (shape == 2 || shape == 3)
				item = NumArr(it, shape, true);
			if (!item)
			{
				err = "bad-value";
				return null;
			}
			list.Add(item);
		}
		return list;
	}

	// An array of exactly n numbers, Fixed6 or shortest; null when it is none.
	static DZS_Json NumArr(DZS_Json v, int n, bool fix6)
	{
		if (!v.IsArr() || v.Count() != n)
			return null;
		DZS_Json r = DZS_Json.Arr();
		for (int i = 0; i < n; i++)
		{
			DZS_Json c = v.At(i);
			if (!c.IsNum() || !DZS_PatchNum.IsNumber(c.m_Str))
				return null;
			string t = DZS_PatchNum.Canon(c.m_Str);
			if (fix6)
				t = DZS_PatchNum.Fixed6(c.m_Str);
			DZS_Json cj = DZS_Json.Num(t);
			r.Add(cj);
		}
		return r;
	}

	// ---- files ----

	// A base file by its relative path, with the engine check of JSON files applied once.
	DZS_PatchFile File(string rel)
	{
		DZS_PatchFile f = m_Files.Get(rel);
		if (f.m_Exists && f.m_IsJson && f.m_Error == "" && !f.m_Created && !f.m_Checked)
		{
			f.m_Checked = true;
			string low = DZS_PatchText.Lower(f.m_Path);
			string bad = IntCheck(low, f.m_Json.m_Root);
			if (bad != "")
				f.m_Error = bad;
		}
		return f;
	}
}
