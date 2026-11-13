// Shared state and value rules of a patch run: rendering project values in the field's format, comparing them with
// what a file holds, and the report entries.
class DZS_PatcherBase : Managed
{
	ref DZS_PatchProject m_Project;
	ref DZS_PatchFiles m_Files;
	ref DZS_PatchReport m_Report = new DZS_PatchReport();
	// The ground for positions given above it; null when the run knows none (they then stand on height 0).
	ref DZS_PatchGround m_Ground;

	// ---- values ----

	// A project value as the text a file field gets; 'err' set when the value does not fit the format.
	static string Render(DZS_PField f, DZS_Json v, out string err)
	{
		err = "";
		if (!v || v.m_Type == DZS_Json.NUL)
		{
			err = "bad-value";
			return "";
		}
		int fmt = f.m_Fmt;
		if (fmt == DZS_PField.INT)
		{
			if (v.IsNum() && DZS_PatchNum.IsInt(v.m_Str))
				return v.m_Str;
			err = "bad-value";
			return "";
		}
		if (fmt == DZS_PField.NUM || fmt == DZS_PField.FIX6)
		{
			if (!v.IsNum() || !DZS_PatchNum.IsNumber(v.m_Str))
			{
				err = "bad-value";
				return "";
			}
			if (fmt == DZS_PField.FIX6)
				return DZS_PatchNum.Fixed6(v.m_Str);
			return DZS_PatchNum.Canon(v.m_Str);
		}
		if (fmt == DZS_PField.VEC3)
		{
			array<string> parts = new array<string>();
			if (!VecOf(v, parts))
			{
				err = "bad-value";
				return "";
			}
			return DZS_PatchNum.Fixed6(parts[0]) + " " + DZS_PatchNum.Fixed6(parts[1]) + " " + DZS_PatchNum.Fixed6(parts[2]);
		}
		if (fmt == DZS_PField.BOOL01 || fmt == DZS_PField.FLAG)
		{
			int b = BoolOf(v);
			if (b < 0)
			{
				err = "bad-value";
				return "";
			}
			return b.ToString();
		}
		if (fmt == DZS_PField.STR || fmt == DZS_PField.ENUMS)
		{
			if (!v.IsStr())
			{
				err = "bad-value";
				return "";
			}
			if (fmt == DZS_PField.ENUMS && f.m_Enum.Find(v.m_Str) < 0)
			{
				err = "bad-value";
				return "";
			}
			return v.m_Str;
		}
		err = "bad-value";
		return "";
	}

	// 1, 0, or -1 when the value is no boolean (true/false, 0/1).
	static int BoolOf(DZS_Json v)
	{
		if (!v)
			return -1;
		if (v.IsBool())
		{
			if (v.m_Bool)
				return 1;
			return 0;
		}
		if (v.IsNum())
		{
			if (DZS_PatchNum.Same(v.m_Str, "1"))
				return 1;
			if (DZS_PatchNum.Same(v.m_Str, "0"))
				return 0;
		}
		return -1;
	}

	// 1, 0 or -1 for a boolean written in a file (0/1, true/false, yes/no).
	static int BoolText(string t)
	{
		string l = DZS_PatchText.Lower(t);
		l.TrimInPlace();
		if (l == "1" || l == "true" || l == "yes")
			return 1;
		if (l == "0" || l == "false" || l == "no")
			return 0;
		return -1;
	}

	// Whether a file value (has = present) equals a project value of a scalar field.
	static bool Equal(DZS_PField f, string cur, bool has, DZS_Json v)
	{
		if (f.m_Fmt == DZS_PField.FLAG)
		{
			int wantFlag = 0;
			if (v && v.m_Type != DZS_Json.NUL)
				wantFlag = BoolOf(v);
			int hasFlag = 0;
			if (has)
				hasFlag = 1;
			return wantFlag == hasFlag;
		}
		if (!v || v.m_Type == DZS_Json.NUL)
			return !has;
		if (!has)
			return false;
		int fmt = f.m_Fmt;
		if (fmt == DZS_PField.INT || fmt == DZS_PField.NUM || fmt == DZS_PField.FIX6)
			return v.IsNum() && DZS_PatchNum.Same(cur, v.m_Str);
		if (fmt == DZS_PField.VEC3)
		{
			array<string> want = new array<string>();
			array<string> have = new array<string>();
			if (!VecOf(v, want) || !VecText(cur, have))
				return false;
			for (int i = 0; i < 3; i++)
			{
				if (!DZS_PatchNum.Same(want[i], have[i]))
					return false;
			}
			return true;
		}
		if (fmt == DZS_PField.BOOL01)
		{
			int bv = BoolOf(v);
			return bv >= 0 && bv == BoolText(cur);
		}
		return v.IsStr() && v.m_Str == cur;
	}

	// List values: members as "name=…" / "user=…", a chance and an item's quantity appended as "\tchance=…",
	// "\tquantmin=…", "\tquantmax=…"; a project list holds names, {"user": …} or {"name": …, "chance": …}. An item may also carry what the game reads inside it — "damage"
	// {"min", "max"} and lists of "attachments" and "cargo" blocks ({"chance", "preset", "item"}) — as further lines
	// of its member (ItemLines).
	static bool ListTokens(DZS_Json v, array<string> tokens)
	{
		if (!v || !v.IsArr())
			return false;
		for (int i = 0; i < v.Count(); i++)
		{
			DZS_Json it = v.At(i);
			if (it.IsObj() && it.GetStr("user") != "")
			{
				tokens.Insert("user=" + it.GetStr("user"));
				continue;
			}
			string tok;
			if (!ItemToken(it, 0, tok))
				return false;
			tokens.Insert(tok);
		}
		return true;
	}

	// An item of a project list as its member: "name=…", "\tchance=…" and the lines of its content one level in.
	protected static bool ItemToken(DZS_Json it, int level, out string tok)
	{
		tok = "";
		if (it.IsStr() && it.m_Str != "")
		{
			tok = "name=" + it.m_Str;
			return true;
		}
		if (!it.IsObj() || it.GetStr("name") == "")
			return false;
		string r = "name=" + it.GetStr("name");
		array<string> numbers = {"chance", "quantmin", "quantmax"};
		foreach (string a : numbers)
		{
			DZS_Json n = it.Get(a);
			if (!n)
				continue;
			if (!n.IsNum() || !DZS_PatchNum.IsNumber(n.m_Str))
				return false;
			r += "\t" + a + "=" + DZS_PatchNum.Canon(n.m_Str);
		}
		array<string> lines = new array<string>();
		if (!ItemLines(it, level, lines))
			return false;
		foreach (string line : lines)
			r += "\n" + line;
		tok = r;
		return true;
	}

	// What a project item holds inside it, as the canonical lines of its XML: the damage, then the attachments and
	// cargo blocks in their order, each line indented by tabs from 'level' on. False when a value is no number or list.
	protected static bool ItemLines(DZS_Json it, int level, array<string> lines)
	{
		string tabs = Tabs(level);
		DZS_Json damage = it.Get("damage");
		if (damage && damage.m_Type != DZS_Json.NUL)
		{
			if (!damage.IsObj())
				return false;
			array<string> dn = new array<string>();
			array<string> dv = new array<string>();
			array<string> ends = {"min", "max"};
			foreach (string side : ends)
			{
				DZS_Json n = damage.Get(side);
				if (!n)
					continue;
				if (!n.IsNum() || !DZS_PatchNum.IsNumber(n.m_Str))
					return false;
				dn.Insert(side);
				dv.Insert(DZS_PatchNum.Canon(n.m_Str));
			}
			lines.Insert(tabs + Tag("damage", dn, dv, true));
		}
		array<string> elems = {"attachments", "cargo"};
		foreach (string elem : elems)
		{
			DZS_Json blocks = it.Get(elem);
			if (!blocks || blocks.m_Type == DZS_Json.NUL)
				continue;
			if (!blocks.IsArr())
				return false;
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json b = blocks.At(i);
				if (!b.IsObj())
					return false;
				array<string> an = new array<string>();
				array<string> av = new array<string>();
				DZS_Json chance = b.Get("chance");
				if (chance)
				{
					if (!chance.IsNum() || !DZS_PatchNum.IsNumber(chance.m_Str))
						return false;
					an.Insert("chance");
					av.Insert(DZS_PatchNum.Canon(chance.m_Str));
				}
				if (b.GetStr("preset") != "")
				{
					an.Insert("preset");
					av.Insert(b.GetStr("preset"));
				}
				array<string> inner = new array<string>();
				DZS_Json items = b.Get("item");
				if (items && items.m_Type != DZS_Json.NUL)
				{
					if (!items.IsArr())
						return false;
					for (int j = 0; j < items.Count(); j++)
					{
						string sub;
						if (!ItemToken(items.At(j), level + 2, sub))
							return false;
						ItemXml(sub, tabs + "\t", inner);
					}
				}
				if (inner.Count() == 0)
				{
					lines.Insert(tabs + Tag(elem, an, av, true));
					continue;
				}
				lines.Insert(tabs + Tag(elem, an, av, false));
				foreach (string il : inner)
					lines.Insert(il);
				lines.Insert(tabs + "</" + elem + ">");
			}
		}
		return true;
	}

	// A member's XML lines: its element (indented by 'tabs'), and the lines of its content, which carry their own
	// indentation.
	static void ItemXml(string token, string tabs, array<string> dest)
	{
		array<string> lines = new array<string>();
		DZS_PatchText.Lines(token, lines);
		array<string> parts = new array<string>();
		lines[0].Split("\t", parts);
		array<string> an = new array<string>();
		array<string> av = new array<string>();
		foreach (string part : parts)
		{
			int eq = part.IndexOf("=");
			an.Insert(part.Substring(0, eq));
			av.Insert(part.Substring(eq + 1, part.Length() - eq - 1));
		}
		if (lines.Count() == 1)
		{
			dest.Insert(tabs + Tag("item", an, av, true));
			return;
		}
		dest.Insert(tabs + Tag("item", an, av, false));
		for (int i = 1; i < lines.Count(); i++)
			dest.Insert(lines[i]);
		dest.Insert(tabs + "</item>");
	}

	static string Tag(string elem, array<string> names, array<string> values, bool close)
	{
		string r = "<" + elem;
		for (int i = 0; i < names.Count(); i++)
			r += " " + names[i] + "=\"" + DZS_Xml.Escape(values[i]) + "\"";
		if (close)
			return r + " />";
		return r + ">";
	}

	static string Tabs(int n)
	{
		string r = "";
		for (int i = 0; i < n; i++)
			r += "\t";
		return r;
	}

	// "sha256:" and the hash of the members joined by line feeds.
	static string Fingerprint(array<string> tokens)
	{
		string text = "";
		for (int i = 0; i < tokens.Count(); i++)
		{
			if (i > 0)
				text += "\n";
			text += tokens[i];
		}
		return "sha256:" + DZS_Sha256.OfText(text);
	}

	static bool SameList(array<string> cur, DZS_Json v)
	{
		if (!v)
			return false;
		if (v.IsStr())
			return v.m_Str == Fingerprint(cur);
		array<string> want = new array<string>();
		if (!ListTokens(v, want) || want.Count() != cur.Count())
			return false;
		for (int i = 0; i < want.Count(); i++)
		{
			if (want[i] != cur[i])
				return false;
		}
		return true;
	}

	// Three numbers from a project value: [x, y, z] or "x y z".
	static bool VecOf(DZS_Json v, array<string> parts)
	{
		parts.Clear();
		if (!v)
			return false;
		if (v.IsStr())
			return VecText(v.m_Str, parts);
		if (!v.IsArr() || v.Count() != 3)
			return false;
		for (int i = 0; i < 3; i++)
		{
			DZS_Json c = v.At(i);
			if (!c.IsNum() || !DZS_PatchNum.IsNumber(c.m_Str))
				return false;
			parts.Insert(c.m_Str);
		}
		return true;
	}

	// Three numbers from file text "x y z" (any spaces or tabs between them).
	static bool VecText(string t, array<string> parts)
	{
		parts.Clear();
		string cur = "";
		for (int i = 0; i <= t.Length(); i++)
		{
			string c = " ";
			if (i < t.Length())
				c = t.Get(i);
			if (c == " " || c == "\t" || c == "\r" || c == "\n")
			{
				if (cur != "")
				{
					parts.Insert(cur);
					cur = "";
				}
			}
			else
				cur += c;
		}
		if (parts.Count() != 3)
			return false;
		foreach (string p : parts)
		{
			if (!DZS_PatchNum.IsNumber(p))
				return false;
		}
		return true;
	}

	static bool NearVec(array<string> a, array<string> b, int tol)
	{
		for (int i = 0; i < 3; i++)
		{
			if (!DZS_PatchNum.Near(a[i], b[i], tol))
				return false;
		}
		return true;
	}

	static string KeyStr(DZS_Json key, string name)
	{
		if (!key || !key.IsObj())
			return "";
		DZS_Json v = key.Get(name);
		if (!v)
			return "";
		if (v.IsStr() || v.IsNum())
			return v.m_Str;
		return "";
	}

	// ---- report ----

	DZS_PatchEntry ChangeEntry(int i, DZS_PatchChange c)
	{
		DZS_PatchEntry e = m_Report.Add("change", i, "");
		e.m_Op = c.m_Op;
		e.m_Kind = c.m_Block;
		e.m_Key = c.m_Key;
		e.m_Field = c.m_Field;
		return e;
	}

	DZS_PatchEntry BlockEntry(int i, DZS_PatchBlock b)
	{
		DZS_PatchEntry e = m_Report.Add("block", i, "");
		e.m_Kind = b.m_Kind;
		e.m_Id = b.m_Id;
		return e;
	}

	// Marks an entry as depending on a file: when the file cannot be written, the entry becomes invalid.
	void Depends(DZS_PatchEntry e, DZS_PatchFile f)
	{
		e.m_File = f.m_Path;
		if (f.m_Entries.Find(e) < 0)
			f.m_Entries.Insert(e);
	}

	// The file is usable; otherwise the entry is set invalid with the reason.
	bool Usable(DZS_PatchEntry e, DZS_PatchFile f)
	{
		e.m_File = f.m_Path;
		if (!f.m_Exists && f.m_Error == "")
		{
			e.m_Status = "invalid";
			e.m_Detail = "missing-file";
			m_Report.FileNote(f.m_Path, "missing", "");
			return false;
		}
		if (f.m_Error != "")
		{
			e.m_Status = "invalid";
			e.m_Detail = "base-invalid";
			return false;
		}
		return true;
	}
}
