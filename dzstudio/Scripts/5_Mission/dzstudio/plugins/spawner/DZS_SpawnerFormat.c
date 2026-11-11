// Object Spawner JSON: read tolerant, keep unknown fields as exact source,
// write in the fixed text form.
class DZS_SpawnerP3D
{
	static int s_NoProbe = -1;

	static bool NoProbe()
	{
		if (s_NoProbe < 0)
		{
			string v;
			s_NoProbe = GetCLIParam("dzsNoP3DProbe", v);
		}
		return s_NoProbe == 1;
	}

	// Origin and degrees the game reports for a model created from file values, as a server would.
	static bool FromFile(string name, vector filePos, vector fileYpr, float scale, out vector origin, out vector yprDeg)
	{
		if (NoProbe())
		{
			origin = filePos;
			yprDeg = fileYpr;
			return true;
		}
		Object o = GetGame().CreateStaticObjectUsingP3D(name, filePos, fileYpr, scale, true);
		if (!o)
			return false;
		origin = o.GetPosition();
		yprDeg = o.GetOrientation();
		GetGame().ObjectDelete(o);
		return true;
	}

	// File position for a wanted origin: create the model at a known point, read the offset, subtract it.
	static bool ToFile(string name, vector origin, vector yprRad, float scale, out vector filePos)
	{
		if (NoProbe())
		{
			filePos = origin;
			return true;
		}
		Object o = GetGame().CreateStaticObjectUsingP3D(name, origin, yprRad, scale, true);
		if (!o)
			return false;
		vector offset = o.GetPosition() - origin;
		GetGame().ObjectDelete(o);
		filePos = origin - offset;
		return true;
	}
}

class DZS_SpawnerFormat : DZS_Format
{
	static const string READ_TYPE = "spawner:read";
	static const string ROOT_TYPE = "spawner:root";
	static const int MAX_TEXT = 1023;
	static const int MAX_FILE = 100000000;

	protected static ref array<string> s_Allowed;

	override string GetId() { return "spawner"; }
	override string GetPluginName() { return "spawner"; }

	override bool HandlesFile(string fileName)
	{
		string low = fileName;
		low.ToLower();
		int n = low.Length();
		return n > 5 && low.Substring(n - 5, 5) == ".json";
	}

	// The game's rule: a console server places models via the Object Spawner only under DZ\plants* and DZ\rocks*.
	// Map suffixes come before the bare root, so a first match is the longest.
	static array<string> AllowedRoots()
	{
		array<string> seps = {"\\", "/"};
		array<string> roots = {"plants", "rocks"};
		array<string> suffixes = {"_bliss", "_sakhal", ""};
		array<string> list = new array<string>();
		foreach (string sep : seps)
		{
			foreach (string root : roots)
			{
				foreach (string suffix : suffixes)
				{
					list.Insert("DZ" + sep + root + suffix);
				}
			}
		}
		return list;
	}

	static bool PathAllowed(string name)
	{
		if (!s_Allowed)
			s_Allowed = AllowedRoots();
		foreach (string p : s_Allowed)
		{
			if (name.Contains(p))
				return true;
		}
		return false;
	}

	static float WorldSize()
	{
		if (!GetGame().GetWorld() || GetGame().GetWorldName() == "")
			return 0;
		return GetGame().GetWorld().GetWorldSize();
	}

	// ---- reading ----

	protected bool IntOutOfRange(string num)
	{
		if (num.Contains(".") || num.Contains("e") || num.Contains("E"))
			return false;
		string d = num;
		if (d.Get(0) == "-")
			d = d.Substring(1, d.Length() - 1);
		if (d.Length() > 10)
			return true;
		if (d.Length() < 10)
			return false;
		return d > "2147483647";
	}

	protected bool BadInts(DZS_SrcNode n)
	{
		if (!n)
			return false;
		if (n.IsNum())
			return IntOutOfRange(n.m_Str);
		if (n.IsArr())
		{
			foreach (DZS_SrcNode it : n.m_Items)
			{
				if (it.IsNum() && IntOutOfRange(it.m_Str))
					return true;
			}
		}
		return false;
	}

	// One number of pos/ypr/scale as float32; non-finite → fallback with a warning.
	protected float Number(DZS_SrcNode n, float fallback, DZS_Issues issues, string what, string field, out bool tolerant)
	{
		float f;
		if (!DZS_F32.FromText(n.m_Str, f))
		{
			string fb = DZS_F32.ToText(fallback);
			issues.Add(DZS_Issue.WARNING, field + ": " + n.m_Str + " ist keine endliche float32-Zahl — ersetzt durch " + fb, field + ": " + n.m_Str + " is not a finite float32 number — replaced by " + fb, what);
			tolerant = true;
			return fallback;
		}
		return f;
	}

	protected bool Vec3(DZS_SrcNode n, out float a, out float b, out float c, DZS_Issues issues, string what, string field, out bool tolerant)
	{
		a = 0;
		b = 0;
		c = 0;
		if (!n)
			return true;
		if (n.IsNull())
		{
			issues.Add(DZS_Issue.WARNING, field + ": null durch den Standardwert ersetzt", field + ": null replaced by the default", what);
			tolerant = true;
			return true;
		}
		if (!n.IsArr() || n.Count() != 3)
			return false;
		for (int i = 0; i < 3; i++)
		{
			if (!n.m_Items[i].IsNum())
				return false;
		}
		a = Number(n.m_Items[0], 0, issues, what, field, tolerant);
		b = Number(n.m_Items[1], 0, issues, what, field, tolerant);
		c = Number(n.m_Items[2], 0, issues, what, field, tolerant);
		return true;
	}

	override DZS_ReadResult Read(array<string> chunks)
	{
		DZS_ReadResult res = new DZS_ReadResult();
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		DZS_SrcNode objs = ReadHead(doc, res);
		if (!objs)
			return res;
		float worldSize = WorldSize();
		int outside = 0;
		for (int i = 0; i < objs.Count(); i++)
		{
			if (!ReadOne(doc, objs, i, worldSize, res, outside))
				return res;
		}
		ReadTail(res, outside);
		return res;
	}

	override DZS_FormatReader BeginRead(array<string> chunks)
	{
		return new DZS_SpawnerReader(this, chunks);
	}

	// Root checks; the "Objects" list to read, or null when the result is already final.
	DZS_SrcNode ReadHead(DZS_SrcDoc doc, DZS_ReadResult res)
	{
		if (!doc.m_Root)
		{
			Unreadable(res, "kein gültiges JSON: " + doc.ErrorText(), "not valid JSON: " + doc.m_Error + " (line " + doc.m_Line + ", column " + doc.m_Col + ")");
			return null;
		}
		DZS_SrcNode root = doc.m_Root;
		if (!root.IsObj())
		{
			Unreadable(res, "die Wurzel ist kein Objekt", "the root is not an object");
			return null;
		}
		DZS_Issues issues = res.m_Issues;
		// unknown fields of the root, the last occurrence of a name at its place
		DZS_Json rootUnknown = DZS_Json.Arr();
		for (int r = 0; r < root.m_Keys.Count(); r++)
		{
			string rk = root.m_Keys[r];
			if (root.CountKey(rk) > 1 && root.Get(rk) != root.m_Items[r])
				continue;
			if (root.CountKey(rk) > 1)
				issues.Add(DZS_Issue.WARNING, "Feld „" + rk + "“ steht mehrfach in der Wurzel — das letzte gilt", "field \"" + rk + "\" appears more than once in the root — the last one counts");
			if (rk == "Objects")
				continue;
			DZS_SrcNode rv = root.m_Items[r];
			DZS_Json pair = DZS_Json.Arr();
			pair.Add(DZS_Json.Str(doc.Slice(rv.m_KeyStart, rv.m_KeyEnd)));
			pair.Add(DZS_Json.Str(doc.Raw(rv)));
			rootUnknown.Add(pair);
		}
		if (rootUnknown.Count() > 0)
		{
			array<string> t196 = rootUnknown.ToChunks();
			res.m_GroupCompanions.Insert(new DZS_Companion(ROOT_TYPE, 1, Join(t196)));
		}
		DZS_SrcNode objs = root.Get("Objects");
		if (!objs || objs.IsNull())
		{
			issues.Add(DZS_Issue.WARNING, "„Objects“ fehlt oder ist null — leere Gruppe", "\"Objects\" is missing or null — empty group");
			res.m_Readable = true;
			return null;
		}
		if (!objs.IsArr())
		{
			Unreadable(res, "„Objects“ ist keine Liste", "\"Objects\" is not a list");
			return null;
		}
		return objs;
	}

	// One entry of the list; false when the file turns out unreadable (the result is final then).
	bool ReadOne(DZS_SrcDoc doc, DZS_SrcNode objs, int i, float worldSize, DZS_ReadResult res, inout int outside)
	{
		DZS_SrcNode e = objs.m_Items[i];
		if (!e.IsObj())
		{
			Unreadable(res, "Eintrag " + (i + 1) + " ist kein Objekt", "entry " + (i + 1) + " is not an object");
			return false;
		}
		DZS_ReadObject ro = ReadEntry(doc, e, i, res.m_Issues, worldSize, res);
		if (!ro)
			return false;
		if (ro.m_Data.m_Fields.Get("outside") == "1")
		{
			outside++;
			ro.m_Data.m_Fields.Remove("outside");
		}
		res.m_Objects.Insert(ro);
		return true;
	}

	void ReadTail(DZS_ReadResult res, int outside)
	{
		if (outside > 10)
		{
			string nOut = outside.ToString();
			res.m_Issues.Add(DZS_Issue.WARNING, nOut + " Objekte liegen außerhalb der Karte — gehört die Datei zu einer anderen Karte?", nOut + " objects lie outside the map — does the file belong to another map?");
		}
		res.m_Readable = true;
	}

	protected DZS_ReadResult Unreadable(DZS_ReadResult res, string de, string en)
	{
		res.m_Readable = false;
		res.m_ErrorDe = de;
		res.m_ErrorEn = en;
		res.m_Objects.Clear();
		DZS_Issue i = res.m_Issues.Add(DZS_Issue.ERROR, "Datei nicht lesbar: " + de, "File not readable: " + en);
		return res;
	}

	static string Join(array<string> chunks)
	{
		string s = "";
		foreach (string c : chunks)
			s += c;
		return s;
	}

	protected DZS_ReadObject ReadEntry(DZS_SrcDoc doc, DZS_SrcNode e, int index, DZS_Issues issues, float worldSize, DZS_ReadResult res)
	{
		string what = "#" + (index + 1);
		bool tolerant = false;
		DZS_Json warns = DZS_Json.Arr();
		array<string> known = {"name", "pos", "ypr", "scale", "enableCEPersistency", "customString"};
		foreach (string kk : known)
		{
			if (e.CountKey(kk) > 1)
				issues.Add(DZS_Issue.WARNING, "Feld „" + kk + "“ steht mehrfach — das letzte gilt", "field \"" + kk + "\" appears more than once — the last one counts", what);
			if (BadInts(e.Get(kk)))
			{
				Unreadable(res, "Ganzzahl außerhalb von 32 Bit in Eintrag " + (index + 1), "integer outside 32 bits in entry " + (index + 1));
				return null;
			}
		}
		int before = issues.Total();
		string name = "";
		DZS_SrcNode nn = e.Get("name");
		if (nn)
		{
			if (nn.IsStr())
				name = nn.m_Str;
			else if (nn.IsNull())
			{
				issues.Add(DZS_Issue.WARNING, "name: null durch den Standardwert ersetzt", "name: null replaced by the default", what);
				tolerant = true;
			}
			else
			{
				Unreadable(res, "„name“ ist kein Text in Eintrag " + (index + 1), "\"name\" is not text in entry " + (index + 1));
				return null;
			}
		}
		if (name != "")
			what = name;
		float px, py, pz, yy, yp, yr;
		DZS_SrcNode epos = e.Get("pos");
		DZS_SrcNode eypr = e.Get("ypr");
		bool okp = Vec3(epos, px, py, pz, issues, what, "pos", tolerant);
		bool oky = false;
		if (okp)
			oky = Vec3(eypr, yy, yp, yr, issues, what, "ypr", tolerant);
		if (!okp || !oky)
		{
			Unreadable(res, "„pos“ oder „ypr“ ist nicht genau 3 Zahlen in Eintrag " + (index + 1), "\"pos\" or \"ypr\" is not exactly 3 numbers in entry " + (index + 1));
			return null;
		}
		float scale = 1;
		DZS_SrcNode sn = e.Get("scale");
		if (sn)
		{
			if (sn.IsNum())
			{
				scale = Number(sn, 1, issues, what, "scale", tolerant);
				if (scale == 0)
					scale = 1;
			}
			else if (sn.IsNull())
			{
				issues.Add(DZS_Issue.WARNING, "scale: null durch den Standardwert ersetzt", "scale: null replaced by the default", what);
				tolerant = true;
			}
			else
			{
				Unreadable(res, "„scale“ ist keine Zahl in Eintrag " + (index + 1), "\"scale\" is not a number in entry " + (index + 1));
				return null;
			}
		}
		string persist = "0";
		DZS_SrcNode pn = e.Get("enableCEPersistency");
		if (pn)
		{
			if (pn.IsBool())
			{
				if (pn.m_Bool)
					persist = "1";
				string readAs = DZS_Str.Bool(pn.m_Bool);
				issues.Add(DZS_Issue.WARNING, "enableCEPersistency wird als " + persist + " geschrieben (gelesen: " + readAs + ")", "enableCEPersistency is written as " + persist + " (read: " + readAs + ")", what);
				tolerant = true;
			}
			else if (pn.IsNum())
			{
				float pv;
				DZS_F32.FromText(pn.m_Str, pv);
				if (pv != 0)
					persist = "1";
				if (pn.m_Str != "0" && pn.m_Str != "1")
				{
					issues.Add(DZS_Issue.WARNING, "enableCEPersistency wird als " + persist + " geschrieben (gelesen: " + pn.m_Str + ")", "enableCEPersistency is written as " + persist + " (read: " + pn.m_Str + ")", what);
					tolerant = true;
				}
			}
			else if (pn.IsNull())
			{
				issues.Add(DZS_Issue.WARNING, "enableCEPersistency: null durch den Standardwert ersetzt", "enableCEPersistency: null replaced by the default", what);
				tolerant = true;
			}
			else
			{
				Unreadable(res, "„enableCEPersistency“ hat den falschen Typ in Eintrag " + (index + 1), "\"enableCEPersistency\" has the wrong type in entry " + (index + 1));
				return null;
			}
		}
		string custom = "";
		DZS_SrcNode cn = e.Get("customString");
		if (cn)
		{
			if (cn.IsStr())
				custom = cn.m_Str;
			else if (cn.IsNull())
			{
				issues.Add(DZS_Issue.WARNING, "customString: null durch den Standardwert ersetzt", "customString: null replaced by the default", what);
				tolerant = true;
			}
			else
			{
				Unreadable(res, "„customString“ ist kein Text in Eintrag " + (index + 1), "\"customString\" is not text in entry " + (index + 1));
				return null;
			}
		}
		for (int w = before; w < issues.Total(); w++)
			warns.Add(DZS_Json.Str(issues.m_Items[w].m_De + "\n" + issues.m_Items[w].m_En));
		DZS_Json unknown = DZS_Json.Arr();
		for (int k = 0; k < e.m_Keys.Count(); k++)
		{
			string key = e.m_Keys[k];
			if (known.Find(key) >= 0)
				continue;
			if (e.CountKey(key) > 1)
			{
				if (e.Get(key) != e.m_Items[k])
					continue;
				issues.Add(DZS_Issue.WARNING, "Feld „" + key + "“ steht mehrfach — das letzte gilt", "field \"" + key + "\" appears more than once — the last one counts", what);
			}
			DZS_SrcNode uv = e.m_Items[k];
			DZS_Json upair = DZS_Json.Arr();
			upair.Add(DZS_Json.Str(doc.Slice(uv.m_KeyStart, uv.m_KeyEnd)));
			upair.Add(DZS_Json.Str(doc.Raw(uv)));
			unknown.Add(upair);
		}
		return FromValues(name, px, py, pz, yy, yp, yr, scale, persist, custom, issues, what, worldSize, unknown, warns);
	}

	// An object from values as read (spawner file or .dze import): checks of name and place, the origin of a model
	// path as the game creates it, and the read values as companion for writing them back unchanged.
	DZS_ReadObject FromValues(string name, float px, float py, float pz, float yy, float yp, float yr, float scale, string persist, string custom, DZS_Issues issues, string what, float worldSize, DZS_Json unknownIn, DZS_Json warnsIn)
	{
		DZS_Json unknown = unknownIn;
		DZS_Json warns = warnsIn;
		// what the object holds
		float ox = px;
		float oy = py;
		float oz = pz;
		float dy = yy;
		float dp = yp;
		float dr = yr;
		bool placeholder = false;
		if (name == "")
		{
			placeholder = true;
			issues.Add(DZS_Issue.WARNING, "Objekt ohne Namen — entsteht auf dem Server nicht", "Object without a name — not created on the server", what);
		}
		else if (DZS_Obj.IsModelPath(name))
		{
			if (!PathAllowed(name))
				issues.Add(DZS_Issue.WARNING, "Modellpfad nicht erlaubt — der Server setzt nur Pflanzen und Felsen aus DZ\\plants… und DZ\\rocks…", "Model path not allowed — the server only places plants and rocks from DZ\\plants… and DZ\\rocks…", what);
			vector origin, deg;
			if (DZS_SpawnerP3D.FromFile(name, Vector(px, py, pz), Vector(yy, yp, yr), scale, origin, deg))
			{
				ox = origin[0];
				oy = origin[1];
				oz = origin[2];
				dy = deg[0];
				dp = deg[1];
				dr = deg[2];
			}
			else
			{
				placeholder = true;
				issues.Add(DZS_Issue.WARNING, "Modell nicht gefunden", "Model not found", what);
			}
		}
		else
		{
			string spelled = DZS_ClassDir.Find(name);
			if (spelled == "")
			{
				placeholder = true;
				issues.Add(DZS_Issue.WARNING, "Klasse unbekannt — entsteht auf dem Server nicht", "Class unknown — not created on the server", what);
			}
			else if (spelled != name)
				issues.Add(DZS_Issue.HINT, "Schreibweise im Spiel: " + spelled, "Spelling in the game: " + spelled, what);
		}
		if (scale < 0)
			issues.Add(DZS_Issue.WARNING, "Größe negativ", "Negative size", what);
		if (name.Length() > MAX_TEXT || custom.Length() > MAX_TEXT)
			issues.Add(DZS_Issue.WARNING, "Text über 1023 Byte — der Server schneidet ihn ab", "Text over 1023 bytes — the server cuts it", what);
		bool outside = worldSize > 0 && (px < 0 || pz < 0 || px > worldSize || pz > worldSize);
		DZS_ElementData d = new DZS_ElementData();
		d.m_Kind = DZS_KIND_OBJECT;
		d.m_Fields.Set(DZS_Obj.NAME, name);
		d.m_Fields.Set(DZS_Obj.POS, DZS_Obj.Vec(ox, oy, oz));
		d.m_Fields.Set(DZS_Obj.YPR, DZS_Obj.Vec(dy, dp, dr));
		d.m_Fields.Set(DZS_Obj.SCALE, DZS_Obj.Num(scale));
		d.m_Fields.Set(DZS_Obj.PERSIST, persist);
		d.m_Fields.Set(DZS_Obj.CUSTOM, custom);
		if (outside)
		{
			issues.Add(DZS_Issue.WARNING, "Objekt außerhalb der Karte", "Object outside the map", what);
			d.m_Fields.Set("outside", "1");
		}
		DZS_Json comp = DZS_Json.Obj();
		DZS_Json t448 = Snapshot(d);
		comp.Set("f", t448);
		comp.Set("p", DZS_Json.Str(DZS_Obj.Vec(px, py, pz)));
		comp.Set("y", DZS_Json.Str(DZS_Obj.Vec(yy, yp, yr)));
		comp.Set("ph", DZS_Json.Boolean(placeholder));
		comp.Set("u", unknown);
		comp.Set("w", warns);
		array<string> t454 = comp.ToChunks();
		d.m_Companions.Set(READ_TYPE, new DZS_Companion(READ_TYPE, 1, Join(t454)));
		DZS_ReadObject ro = new DZS_ReadObject();
		ro.m_Data = d;
		ro.m_FileName = name;
		ro.m_FileX = px;
		ro.m_FileY = py;
		ro.m_FileZ = pz;
		return ro;
	}

	// The fields as they were read, to tell later whether the object changed.
	static DZS_Json Snapshot(DZS_ElementData d)
	{
		DZS_Json f = DZS_Json.Arr();
		array<string> keys = {DZS_Obj.NAME, DZS_Obj.POS, DZS_Obj.YPR, DZS_Obj.SCALE, DZS_Obj.PERSIST, DZS_Obj.CUSTOM};
		foreach (string k : keys)
			f.Add(DZS_Json.Str(d.m_Fields.Get(k)));
		return f;
	}

	static DZS_Json SnapshotOf(DZS_Instance inst, string id)
	{
		DZS_Json f = DZS_Json.Arr();
		array<string> keys = {DZS_Obj.NAME, DZS_Obj.POS, DZS_Obj.YPR, DZS_Obj.SCALE, DZS_Obj.PERSIST, DZS_Obj.CUSTOM};
		foreach (string k : keys)
		{
			string v;
			inst.GetField(id, k, v);
			f.Add(DZS_Json.Str(v));
		}
		return f;
	}

	static bool SameSnapshot(DZS_Json a, DZS_Json b)
	{
		if (!a || !b || !a.IsArr() || !b.IsArr() || a.Count() != b.Count())
			return false;
		for (int i = 0; i < a.Count(); i++)
		{
			if (a.At(i).m_Str != b.At(i).m_Str)
				return false;
		}
		return true;
	}

	// ---- writing ----

	override DZS_WriteResult Write(DZS_Instance inst, string groupId)
	{
		DZS_FormatWriter w = BeginWrite(inst, groupId);
		while (!w.Step(DZS_Job.ALL_AT_ONCE))
		{
		}
		return w.Result();
	}

	override DZS_FormatWriter BeginWrite(DZS_Instance inst, string groupId)
	{
		return new DZS_SpawnerWriter(this, inst, groupId);
	}

	// Tail of the file after the objects: root keys kept from reading, then the checks of the whole file.
	void Finish(DZS_Instance inst, string groupId, DZS_TextWriter w, DZS_WriteResult res, int count, int hidden)
	{
		DZS_Issues issues = res.m_Issues;
		if (count > 0)
			w.Append("\n    ]");
		int vc;
		string rootData;
		if (inst.GetCompanion(groupId, ROOT_TYPE, vc, rootData) && rootData != "")
		{
			string err;
			DZS_Json ru = DZS_Json.ParseText(rootData, err);
			if (ru && ru.IsArr())
			{
				for (int r = 0; r < ru.Count(); r++)
					w.Append(",\n    " + ru.At(r).At(0).m_Str + ": " + ru.At(r).At(1).m_Str);
			}
		}
		w.Append("\n}\n");
		res.m_Chunks = w.Finish();
		if (hidden > 0)
		{
			if (hidden == 1)
				issues.Add(DZS_Issue.HINT, "1 ausgeblendetes Objekt wird mitgeschrieben und erscheint auf dem Server", "1 hidden object is written and appears on the server");
			else
			{
				string nHidden = hidden.ToString();
				issues.Add(DZS_Issue.HINT, nHidden + " ausgeblendete Objekte werden mitgeschrieben und erscheinen auf dem Server", nHidden + " hidden objects are written and appear on the server");
			}
		}
		if (DZS_Files.Length(res.m_Chunks) > MAX_FILE)
		{
			DZS_Issue big = issues.Add(DZS_Issue.ERROR, "Datei über 100 000 000 Byte — der Server lädt sie nicht; teile die Gruppe", "File over 100,000,000 bytes — the server does not load it; split the group");
			big.m_Confirmable = true;
		}
	}

	string Entry(DZS_Instance inst, string id, DZS_Issues issues, float worldSize, DZS_WriteResult res)
	{
		string name, pos, ypr, scale, persist, custom;
		inst.GetField(id, DZS_Obj.NAME, name);
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		inst.GetField(id, DZS_Obj.PERSIST, persist);
		inst.GetField(id, DZS_Obj.CUSTOM, custom);
		if (pos == "")
			pos = "0.0 0.0 0.0";
		if (ypr == "")
			ypr = "0.0 0.0 0.0";
		if (scale == "")
			scale = "1.0";
		if (persist != "1")
			persist = "0";
		string what = name;
		if (what == "")
			what = id;
		DZS_Json read = null;
		int ver;
		string data;
		if (inst.GetCompanion(id, READ_TYPE, ver, data))
		{
			string err;
			read = DZS_Json.ParseText(data, err);
		}
		bool unchanged = read && SameSnapshot(read.Get("f"), SnapshotOf(inst, id));
		bool placeholder = read && read.GetBool("ph");
		string filePos = pos;
		string fileYpr = ypr;
		if (DZS_Obj.IsModelPath(name))
		{
			if (unchanged)
			{
				filePos = read.GetStr("p");
				fileYpr = read.GetStr("y");
			}
			else
			{
				float dy, dp, dr, ox, oy, oz;
				DZS_Obj.ParseVec(ypr, dy, dp, dr);
				DZS_Obj.ParseVec(pos, ox, oy, oz);
				float ry = dy * Math.DEG2RAD;
				float rp = dp * Math.DEG2RAD;
				float rr = dr * Math.DEG2RAD;
				float sc;
				DZS_F32.FromText(scale, sc);
				vector fp;
				if (DZS_SpawnerP3D.ToFile(name, Vector(ox, oy, oz), Vector(ry, rp, rr), sc, fp))
					filePos = DZS_Obj.Vec(fp[0], fp[1], fp[2]);
				else
					issues.Add(DZS_Issue.WARNING, "Platzhalter verschoben — die Lage auf dem Server ist nicht vorhersagbar", "Placeholder moved — its place on the server cannot be predicted", what, id);
				fileYpr = DZS_Obj.Vec(ry, rp, rr);
			}
			if (!PathAllowed(name))
			{
				DZS_Issue bad = issues.Add(DZS_Issue.WARNING, "Modellpfad nicht erlaubt — der Server setzt nur Pflanzen und Felsen aus DZ\\plants… und DZ\\rocks…", "Model path not allowed — the server only places plants and rocks from DZ\\plants… and DZ\\rocks…", what, id);
				string folderFixed = DZS_SpawnerFix.Fixed(name);
				if (folderFixed != "" && id != "")
				{
					DZS_SpawnerFix fix = new DZS_SpawnerFix();
					fix.m_De = "Schreibweise korrigieren";
					fix.m_En = "Correct spelling";
					fix.m_Id = id;
					fix.m_Name = folderFixed;
					bad.m_Fix = fix;
				}
			}
			else if (placeholder && unchanged)
				issues.Add(DZS_Issue.WARNING, "Modell nicht gefunden", "Model not found", what, id);
		}
		else if (name == "")
			issues.Add(DZS_Issue.WARNING, "Objekt ohne Namen — entsteht auf dem Server nicht", "Object without a name — not created on the server", what, id);
		else
		{
			string spelled = DZS_ClassDir.Find(name);
			if (spelled == "")
				issues.Add(DZS_Issue.WARNING, "Klasse unbekannt — entsteht auf dem Server nicht", "Class unknown — not created on the server", what, id);
			else if (spelled != name)
				issues.Add(DZS_Issue.HINT, "Schreibweise im Spiel: " + spelled, "Spelling in the game: " + spelled, what, id);
		}
		if (scale.Get(0) == "-")
			issues.Add(DZS_Issue.WARNING, "Größe negativ", "Negative size", what, id);
		if (name.Length() > MAX_TEXT || custom.Length() > MAX_TEXT)
			issues.Add(DZS_Issue.WARNING, "Text über 1023 Byte — der Server schneidet ihn ab", "Text over 1023 bytes — the server cuts it", what, id);
		vector q = DZS_Obj.QuickVec(filePos);
		if (worldSize > 0 && (q[0] < 0 || q[2] < 0 || q[0] > worldSize || q[2] > worldSize))
			issues.Add(DZS_Issue.WARNING, "Objekt außerhalb der Karte", "Object outside the map", what, id);
		if (unchanged && read.Get("w"))
		{
			DZS_Json warns = read.Get("w");
			for (int k = 0; k < warns.Count(); k++)
			{
				array<string> both = new array<string>();
				warns.At(k).m_Str.Split("\n", both);
				if (both.Count() == 2)
					issues.Add(DZS_Issue.WARNING, both[0], both[1], what, id);
			}
		}
		float fx, fy, fz;
		DZS_Obj.ParseVec(filePos, fx, fy, fz);
		res.m_Names.Insert(name);
		res.m_FileX.Insert(fx);
		res.m_FileY.Insert(fy);
		res.m_FileZ.Insert(fz);
		string s = "        {\n";
		s += "            \"name\": " + DZS_TextForm.Str(name) + ",\n";
		s += "            \"pos\": [" + Commas(filePos) + "],\n";
		s += "            \"ypr\": [" + Commas(fileYpr) + "],\n";
		s += "            \"scale\": " + scale + ",\n";
		s += "            \"enableCEPersistency\": " + persist + ",\n";
		s += "            \"customString\": " + DZS_TextForm.Str(custom);
		if (read && read.Get("u"))
		{
			DZS_Json u = read.Get("u");
			for (int i = 0; i < u.Count(); i++)
				s += ",\n            " + u.At(i).At(0).m_Str + ": " + u.At(i).At(1).m_Str;
		}
		s += "\n        }";
		return s;
	}

	static string Commas(string vec)
	{
		string r = vec;
		r.Replace(" ", ", ");
		return r;
	}
}

// Reading in slices: the text with DZS_SrcJsonSteps (objects of the list read whole), then the entries.
class DZS_SpawnerReader : DZS_FormatReader
{
	protected DZS_SpawnerFormat m_Format;
	protected ref DZS_SrcJsonSteps m_Parse;
	protected ref DZS_SrcDoc m_Doc;
	protected DZS_SrcNode m_Objs;
	protected int m_Next;
	protected int m_Outside;
	protected float m_WorldSize;
	protected bool m_Done;

	void DZS_SpawnerReader(DZS_SpawnerFormat f, array<string> chunks)
	{
		m_Format = f;
		m_Result = new DZS_ReadResult();
		m_Parse = new DZS_SrcJsonSteps(chunks, 2);
		m_WorldSize = DZS_SpawnerFormat.WorldSize();
	}

	override float Progress()
	{
		if (m_Done)
			return 1;
		if (!m_Doc)
			return 0.8 * m_Parse.Progress();
		if (!m_Objs || m_Objs.Count() == 0)
			return 0.8;
		return 0.8 + 0.2 * m_Next / m_Objs.Count();
	}

	override bool Step(float ms)
	{
		if (m_Done)
			return true;
		int t0 = TickCount(0);
		float limit = ms * 10000;
		if (!m_Doc)
		{
			if (!m_Parse.Step(ms))
				return false;
			m_Doc = m_Parse.Result();
			m_Objs = m_Format.ReadHead(m_Doc, m_Result);
			if (!m_Objs)
			{
				m_Done = true;
				return true;
			}
			if (TickCount(0) - t0 >= limit)
				return false;
		}
		while (m_Next < m_Objs.Count())
		{
			if (!m_Format.ReadOne(m_Doc, m_Objs, m_Next, m_WorldSize, m_Result, m_Outside))
			{
				m_Done = true;
				return true;
			}
			m_Next++;
			if ((m_Next & 7) == 0 && TickCount(0) - t0 >= limit)
				return false;
		}
		m_Format.ReadTail(m_Result, m_Outside);
		m_Done = true;
		return true;
	}
}

// Objects written in slices; ids are taken at the start (the caller reads the state as it was then).
class DZS_SpawnerWriter : DZS_FormatWriter
{
	protected DZS_SpawnerFormat m_Format;
	protected DZS_Instance m_Inst;
	protected string m_Group;
	protected ref array<string> m_All;
	protected ref DZS_TextWriter m_W;
	protected int m_Next;
	protected int m_Written;
	protected int m_Hidden;
	protected bool m_GroupHidden;
	protected float m_WorldSize;
	protected bool m_Done;

	void DZS_SpawnerWriter(DZS_SpawnerFormat f, DZS_Instance inst, string groupId)
	{
		m_Format = f;
		m_Inst = inst;
		m_Group = groupId;
		m_Result = new DZS_WriteResult();
		m_Result.m_Ids = new array<string>();
		m_All = new array<string>();
		m_All.Copy(inst.GetChildren(groupId));
		m_W = new DZS_TextWriter(DZS_Files.BLOCK);
		m_WorldSize = DZS_SpawnerFormat.WorldSize();
		m_GroupHidden = inst.IsHidden(groupId);
	}

	override bool Step(float ms)
	{
		if (m_Done)
			return true;
		int t0 = TickCount(0);
		float limit = ms * 10000;
		while (m_Next < m_All.Count())
		{
			string id = m_All[m_Next];
			m_Next++;
			if (!m_Inst.Has(id))
				continue;
			if (m_GroupHidden || m_Inst.IsHidden(id))
				m_Hidden++;
			string entry = m_Format.Entry(m_Inst, id, m_Result.m_Issues, m_WorldSize, m_Result);
			string piece;
			if (m_Written == 0)
				piece = "{\n    \"Objects\": [\n" + entry;
			else
				piece = ",\n" + entry;
			m_W.Append(piece);
			m_Result.m_Ids.Insert(id);
			m_Written++;
			if ((m_Next & 15) == 0 && TickCount(0) - t0 >= limit)
				return false;
		}
		if (m_Written == 0)
			m_W.Append("{\n    \"Objects\": []");
		m_Format.Finish(m_Inst, m_Group, m_W, m_Result, m_Written, m_Hidden);
		m_Done = true;
		return true;
	}
}
