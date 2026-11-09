// How the project file addresses its own blocks and changes from extensions (docs/extensions/README.md): a block by
// its id, a change by its ref — exactly as dzbot does (ProjectRules::ref) — and short labels for both.
class DZS_ProjectRefs
{
	// block, the key as compact JSON (numbers as PHP writes them after reading), field; trimmed
	static string ChangeRef(DZS_Json change)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		w.Append(change.GetStr("block"));
		w.Append(" ");
		DZS_Json key = change.Get("key");
		if (!key || key.m_Type == DZS_Json.NUL)
			w.Append("{}");
		else
			Canon(w, key);
		w.Append(" ");
		w.Append(change.GetStr("field"));
		array<string> parts = w.Finish();
		string r = "";
		foreach (string p : parts)
			r += p;
		r.TrimInPlace();
		return r;
	}

	static void Canon(DZS_TextWriter w, DZS_Json v)
	{
		if (v.m_Type == DZS_Json.NUM)
		{
			w.Append(PhpNumber(v.m_Str));
			return;
		}
		if (v.m_Type == DZS_Json.ARR)
		{
			w.Append("[");
			for (int i = 0; i < v.m_Items.Count(); i++)
			{
				if (i > 0)
					w.Append(",");
				DZS_Json item = v.m_Items[i];
				Canon(w, item);
			}
			w.Append("]");
			return;
		}
		if (v.m_Type == DZS_Json.OBJ)
		{
			w.Append("{");
			for (int k = 0; k < v.m_Keys.Count(); k++)
			{
				if (k > 0)
					w.Append(",");
				string name = v.m_Keys[k];
				DZS_Json.WriteString(w, name);
				w.Append(":");
				DZS_Json member = v.m_Map.Get(name);
				Canon(w, member);
			}
			w.Append("}");
			return;
		}
		v.Write(w, false, 0);
	}

	// A JSON number as PHP's json_encode writes it after json_decode: 5.0 → 5, 1.50 → 1.5, 1e2 → 100, 0.00001 → 1.0e-5,
	// 1e17 → 1.0e+17. More than 17 significant digits are not rounded.
	static string PhpNumber(string raw)
	{
		string s = raw;
		bool neg = false;
		if (s.Length() > 0 && s.Get(0) == "-")
		{
			neg = true;
			s = s.Substring(1, s.Length() - 1);
		}
		int exp = 0;
		string low = s;
		low.ToLower();
		int e = low.IndexOf("e");
		string mant = s;
		if (e >= 0)
		{
			string es = s.Substring(e + 1, s.Length() - e - 1);
			bool eneg = false;
			if (es.Length() > 0 && (es.Get(0) == "+" || es.Get(0) == "-"))
			{
				eneg = es.Get(0) == "-";
				es = es.Substring(1, es.Length() - 1);
			}
			exp = es.ToInt();
			if (eneg)
				exp = -exp;
			mant = s.Substring(0, e);
		}
		int dot = mant.IndexOf(".");
		string ip = mant;
		string fp = "";
		if (dot >= 0)
		{
			ip = mant.Substring(0, dot);
			fp = mant.Substring(dot + 1, mant.Length() - dot - 1);
		}
		string digits = ip + fp;
		int decpt = ip.Length() + exp;
		while (digits.Length() > 0 && digits.Get(0) == "0")
		{
			digits = digits.Substring(1, digits.Length() - 1);
			decpt--;
		}
		if (e < 0 && dot < 0)
		{
			if (digits == "")
				return "0";
			if (neg)
				return "-" + digits;
			return digits;
		}
		while (digits.Length() > 0 && digits.Get(digits.Length() - 1) == "0")
			digits = digits.Substring(0, digits.Length() - 1);
		if (digits == "")
			return "0";
		string sign = "";
		if (neg)
			sign = "-";
		int n = digits.Length();
		if (decpt >= n && decpt <= 17)
		{
			string whole = digits;
			for (int z = n; z < decpt; z++)
				whole += "0";
			return sign + whole;
		}
		if (decpt < -3 || decpt > 17)
		{
			string rest = "0";
			if (n > 1)
				rest = digits.Substring(1, n - 1);
			int pexp = decpt - 1;
			string esign = "+";
			if (pexp < 0)
			{
				esign = "-";
				pexp = -pexp;
			}
			return sign + digits.Get(0) + "." + rest + "e" + esign + pexp.ToString();
		}
		if (decpt <= 0)
		{
			string lead = "0.";
			for (int q = 0; q < -decpt; q++)
				lead += "0";
			return sign + lead + digits;
		}
		return sign + digits.Substring(0, decpt) + "." + digits.Substring(decpt, n - decpt);
	}

	// ---- labels of what the project holds (DZS_ProjectV2.Build) ----

	static string KindLabel(string kind)
	{
		if (kind == "spawnerGroup")
			return DZS_Texts.Pick("Objektgruppe", "Object group");
		if (kind == "lootRow")
			return DZS_Texts.Pick("Loot-Objekt", "Loot object");
		if (kind == "lootType" || kind == "itemType")
			return DZS_Texts.Pick("Item", "Item");
		if (kind == "lootContainer")
			return DZS_Texts.Pick("Loot-Behälter", "Loot container");
		if (kind == "lootPoint")
			return DZS_Texts.Pick("Loot-Punkt", "Loot point");
		if (kind == "event")
			return "Event";
		if (kind == "eventChild")
			return DZS_Texts.Pick("Event-Fahrzeug/-Tier", "Event child");
		if (kind == "spawnPosition")
			return DZS_Texts.Pick("Spawnpunkt", "Spawn position");
		if (kind == "effectArea")
			return DZS_Texts.Pick("Effektbereich", "Effect area");
		if (kind == "territory" || kind == "territoryZone")
			return DZS_Texts.Pick("Territorium", "Territory");
		if (kind == "weather")
			return DZS_Texts.Pick("Wetter", "Weather");
		if (kind == "gameplay")
			return "cfggameplay.json";
		return kind;
	}

	static string BlockLabel(DZS_Json block)
	{
		string label = KindLabel(block.GetStr("kind"));
		DZS_Json data = block.Get("data");
		string name = "";
		if (data && data.IsObj())
		{
			name = data.GetStr("name");
			if (name == "")
				name = data.GetStr("type");
		}
		if (name != "")
			label += " · " + name;
		return label + " · " + block.GetStr("id");
	}

	static string ChangeLabel(DZS_Json change)
	{
		string label = KindLabel(change.GetStr("block"));
		DZS_Json key = change.Get("key");
		if (key && key.IsObj())
		{
			for (int i = 0; i < key.m_Keys.Count(); i++)
			{
				string kn = key.m_Keys[i];
				DZS_Json kv = key.Get(kn);
				label += " · " + Short(kv, 40);
			}
		}
		string field = change.GetStr("field");
		if (field != "")
			label += " · " + field;
		if (change.GetStr("op") == "remove")
			label += DZS_Texts.Pick(" · entfernen", " · remove");
		else
		{
			DZS_Json to = change.Get("to");
			if (to)
				label += " → " + Short(to, 30);
		}
		return label;
	}

	static string Short(DZS_Json v, int max)
	{
		string t;
		if (v.IsStr())
			t = v.m_Str;
		else if (v.IsNum())
			t = v.m_Str;
		else
		{
			array<string> chunks = v.ToChunks(false);
			t = "";
			foreach (string c : chunks)
			{
				t += c;
				if (t.Length() > max)
					break;
			}
		}
		if (t.Length() > max)
			t = DZS_Utf8.Part(t, 0, max - 1) + "…";
		return t;
	}
}
