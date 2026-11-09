// One mission file during a patch run: its source, the parsed form (XML or strict JSON) and the edits as patches on
// the source, so everything outside a changed span stays byte for byte. Values set earlier in the run are kept in an
// overlay: a later change reads them as the current value.
class DZS_PInsert : Managed
{
	int m_Pos;
	string m_Head;
	string m_Tail;
	ref array<string> m_Items = new array<string>();
	DZS_XmlPatch m_Patch;
}

// A list value set in this run: one insertion holding all its members, rewritten when the list is set again.
class DZS_PList : Managed
{
	DZS_XmlPatch m_Patch;
	string m_Indent;
	bool m_Before;
	ref array<string> m_Tokens = new array<string>();
}

// Deferred edits of one JSON array or object: separators depend on what stays, so they are made at the end.
class DZS_PJCont : Managed
{
	DZS_SrcNode m_Node;
	ref array<int> m_Removed = new array<int>();
	ref array<string> m_Keys = new array<string>();
	ref array<ref DZS_Json> m_Values = new array<ref DZS_Json>();
}

class DZS_PatchFile : Managed
{
	string m_Path;
	bool m_Exists;
	bool m_IsJson;
	bool m_Created;
	// an empty file made for own blocks (a messages.xml the base lacks): reported as created
	bool m_Fresh;
	ref array<string> m_Chunks;
	ref DZS_XmlDoc m_Xml;
	ref DZS_SrcDoc m_Json;
	string m_Error;
	string m_ErrorText;
	string m_Eol = "\n";
	string m_Unit = "    ";
	ref array<ref DZS_XmlPatch> m_Patches = new array<ref DZS_XmlPatch>();
	ref map<string, ref DZS_XmlPatch> m_ByTarget = new map<string, ref DZS_XmlPatch>();
	ref map<string, string> m_Over = new map<string, string>();
	ref map<string, bool> m_OverGone = new map<string, bool>();
	ref map<int, bool> m_Removed = new map<int, bool>();
	ref map<int, ref DZS_PInsert> m_Inserts = new map<int, ref DZS_PInsert>();
	ref array<ref DZS_PJCont> m_Conts = new array<ref DZS_PJCont>();
	ref map<string, ref DZS_PList> m_Lists = new map<string, ref DZS_PList>();
	// children of an element by lower-cased attribute value, built once per element, name and attribute
	ref map<string, ref map<string, ref array<DZS_XmlNode>>> m_Index = new map<string, ref map<string, ref array<DZS_XmlNode>>>();
	// entries of the report that depend on this file, set to invalid when it cannot be written
	ref array<DZS_PatchEntry> m_Entries = new array<DZS_PatchEntry>();
	bool m_Changed;
	bool m_Checked;
	ref array<string> m_Out;

	bool Ok()
	{
		return m_Exists && m_Error == "";
	}

	void Load(array<string> chunks, bool json)
	{
		m_Chunks = chunks;
		m_IsJson = json;
		m_Xml = new DZS_XmlDoc();
		if (json)
		{
			m_Xml.SetSource(chunks);
			m_Json = DZS_SrcJson.Parse(chunks);
			if (m_Json.m_Error != "")
			{
				m_Error = "parse";
				m_ErrorText = m_Json.ErrorText();
			}
		}
		else
		{
			m_Xml = DZS_Xml.Parse(chunks);
			if (m_Xml.m_Error != "")
			{
				m_Error = "parse";
				m_ErrorText = m_Xml.ErrorText();
			}
		}
		Measure();
	}

	// An XML file read over several frames (DZS_XmlSteps): the same as Load.
	void LoadXml(array<string> chunks, DZS_XmlDoc doc)
	{
		m_Chunks = chunks;
		m_IsJson = false;
		m_Xml = doc;
		if (m_Xml.m_Error != "")
		{
			m_Error = "parse";
			m_ErrorText = m_Xml.ErrorText();
		}
		Measure();
	}

	// Line ends and the indent unit of the source.
	protected void Measure()
	{
		int nl = m_Xml.Find(0, "\n");
		if (nl > 0 && m_Xml.CharAt(nl - 1) == "\r")
			m_Eol = "\r\n";
		while (nl >= 0)
		{
			string c = m_Xml.CharAt(nl + 1);
			if (c == "\t" || c == " ")
			{
				string ind = LineIndent(nl + 1);
				if (ind.Length() < 9)
					m_Unit = ind;
				break;
			}
			nl = m_Xml.Find(nl + 1, "\n");
		}
	}

	// ---- patches ----

	DZS_XmlPatch Patch(string target, int start, int end, string text)
	{
		DZS_XmlPatch p = m_ByTarget.Get(target);
		if (p)
		{
			p.m_Text = text;
			return p;
		}
		p = new DZS_XmlPatch();
		p.m_Start = start;
		p.m_End = end;
		p.m_Text = text;
		m_Patches.Insert(p);
		m_ByTarget.Set(target, p);
		m_Changed = true;
		return p;
	}

	DZS_XmlPatch Insertion(int at, string text)
	{
		DZS_XmlPatch p = new DZS_XmlPatch();
		p.m_Start = at;
		p.m_End = at;
		p.m_Text = text;
		m_Patches.Insert(p);
		m_Changed = true;
		return p;
	}

	// Leading spaces and tabs of the line 'at' stands on.
	string LineIndent(int at)
	{
		int i = at;
		while (i > 0 && m_Xml.CharAt(i - 1) != "\n")
			i--;
		int j = i;
		while (j < m_Xml.Length())
		{
			string c = m_Xml.CharAt(j);
			if (c != " " && c != "\t")
				break;
			j++;
		}
		return m_Xml.Slice(i, j);
	}

	// True when only spaces and tabs stand before 'at' on its line.
	bool StartsLine(int at)
	{
		int i = at;
		while (i > 0)
		{
			string c = m_Xml.CharAt(i - 1);
			if (c == "\n")
				return true;
			if (c != " " && c != "\t")
				return false;
			i--;
		}
		return true;
	}

	// Indents every line after the first of a multi-line text by 'indent'.
	static string Reindent(string text, string indent, string eol)
	{
		if (text.IndexOf("\n") < 0)
			return text;
		array<string> lines = new array<string>();
		DZS_PatchText.Lines(text, lines);
		string r = lines[0];
		for (int i = 1; i < lines.Count(); i++)
			r += eol + indent + lines[i];
		return r;
	}

	// ---- XML ----

	// The three numbers of an "x y z" attribute as written in the base, parsed once; null when it holds no vector.
	protected ref map<int, ref array<string>> m_Vecs = new map<int, ref array<string>>();

	array<string> VecAt(DZS_XmlNode n, string attr)
	{
		if (m_Vecs.Contains(n.m_Start))
			return m_Vecs.Get(n.m_Start);
		array<string> parts = new array<string>();
		if (!DZS_PatcherBase.VecText(n.Get(attr), parts))
			parts = null;
		m_Vecs.Set(n.m_Start, parts);
		return parts;
	}

	array<DZS_XmlNode> Named(DZS_XmlNode parent, string elem, string attr, string low)
	{
		string ik = parent.m_Start.ToString() + ":" + elem + "@" + attr;
		map<string, ref array<DZS_XmlNode>> idx = m_Index.Get(ik);
		if (!idx)
		{
			idx = new map<string, ref array<DZS_XmlNode>>();
			m_Index.Set(ik, idx);
			foreach (DZS_XmlNode c : parent.m_Children)
			{
				if (c.m_Name != elem)
					continue;
				string v = DZS_PatchText.Lower(c.Get(attr));
				array<DZS_XmlNode> list = idx.Get(v);
				if (!list)
				{
					list = new array<DZS_XmlNode>();
					idx.Set(v, list);
				}
				list.Insert(c);
			}
		}
		array<DZS_XmlNode> hit = idx.Get(low);
		if (!hit)
			hit = new array<DZS_XmlNode>();
		return hit;
	}

	bool IsRemoved(DZS_XmlNode n)
	{
		return m_Removed.Contains(n.m_Start);
	}

	string AttrKey(DZS_XmlNode n, string name)
	{
		return "a" + n.m_Start + ":" + name;
	}

	// Value of an attribute after the edits so far; has = false when it is absent.
	string Attr(DZS_XmlNode n, string name, out bool has)
	{
		string k = AttrKey(n, name);
		if (m_OverGone.Contains(k))
		{
			has = false;
			return "";
		}
		if (m_Over.Contains(k))
		{
			has = true;
			return m_Over.Get(k);
		}
		DZS_XmlAttr a = n.Attr(name);
		has = a != null;
		if (!a)
			return "";
		return a.Value();
	}

	// Sets an attribute; a new one goes after the last present attribute that comes before it in 'order'.
	void SetAttr(DZS_XmlNode n, string name, string v, array<string> order)
	{
		string k = AttrKey(n, name);
		m_Over.Set(k, v);
		m_OverGone.Remove(k);
		string esc = DZS_Xml.Escape(v);
		DZS_XmlAttr a = n.Attr(name);
		if (a)
		{
			Patch(k, a.m_ValueStart, a.m_ValueEnd, esc);
			return;
		}
		int at = n.m_Start + 1 + n.m_Name.Length();
		int mine = -1;
		if (order)
			mine = order.Find(name);
		if (mine >= 0)
		{
			foreach (DZS_XmlAttr b : n.m_Attrs)
			{
				int bo = order.Find(b.m_Name);
				if (bo >= 0 && bo < mine)
					at = b.m_ValueEnd + 1;
			}
		}
		else if (n.m_Attrs.Count() > 0)
			at = n.m_Attrs[n.m_Attrs.Count() - 1].m_ValueEnd + 1;
		Patch(k, at, at, " " + name + "=\"" + esc + "\"");
	}

	void RemoveAttr(DZS_XmlNode n, string name)
	{
		string k = AttrKey(n, name);
		m_OverGone.Set(k, true);
		m_Over.Remove(k);
		DZS_XmlAttr a = n.Attr(name);
		if (a)
			Patch(k, a.m_LeadStart, a.m_ValueEnd + 1, "");
		else if (m_ByTarget.Contains(k))
			m_ByTarget.Get(k).m_Text = "";
	}

	// Start of the end tag of an element that is not self-closing.
	int EndTagStart(DZS_XmlNode n)
	{
		int i = n.m_End - 1;
		while (i > n.m_OpenEnd && m_Xml.CharAt(i) != "<")
			i--;
		return i;
	}

	string Text(DZS_XmlNode n)
	{
		string k = "t" + n.m_Start;
		if (m_Over.Contains(k))
			return m_Over.Get(k);
		if (n.m_SelfClose)
			return "";
		string raw = m_Xml.Slice(n.m_OpenEnd, EndTagStart(n));
		raw.TrimInPlace();
		return DZS_Xml.Unescape(raw);
	}

	// The text of an element as the base has it, without values set in this run.
	string BaseText(DZS_XmlNode n)
	{
		if (n.m_SelfClose)
			return "";
		string raw = m_Xml.Slice(n.m_OpenEnd, EndTagStart(n));
		raw.TrimInPlace();
		return DZS_Xml.Unescape(raw);
	}

	void SetText(DZS_XmlNode n, string v)
	{
		string k = "t" + n.m_Start;
		m_Over.Set(k, v);
		string esc = DZS_Xml.Escape(v);
		if (n.m_SelfClose)
			Patch(k, n.m_Start, n.m_End, "<" + n.m_Name + ">" + esc + "</" + n.m_Name + ">");
		else
			Patch(k, n.m_OpenEnd, EndTagStart(n), esc);
	}

	// Removes an element; when it stands alone on its line, the whole line goes.
	void RemoveNode(DZS_XmlNode n)
	{
		if (IsRemoved(n))
			return;
		m_Removed.Set(n.m_Start, true);
		int s = n.m_Start;
		int e = n.m_End;
		if (StartsLine(s))
		{
			int j = e;
			while (j < m_Xml.Length() && (m_Xml.CharAt(j) == " " || m_Xml.CharAt(j) == "\t"))
				j++;
			string c = m_Xml.CharAt(j);
			if (c == "\r" && m_Xml.CharAt(j + 1) == "\n")
				j += 2;
			else if (c == "\n")
				j += 1;
			else
				j = -1;
			if (j >= 0)
			{
				s = LineStart(s);
				e = j;
			}
		}
		Patch("r" + n.m_Start, s, e, "");
	}

	int LineStart(int at)
	{
		int i = at;
		while (i > 0 && m_Xml.CharAt(i - 1) != "\n")
			i--;
		return i;
	}

	// Indent for a new child of 'parent': that of its last child on a line of its own, else the parent's plus one unit.
	string ChildIndent(DZS_XmlNode parent)
	{
		for (int i = parent.m_Children.Count() - 1; i >= 0; i--)
		{
			DZS_XmlNode c = parent.m_Children[i];
			if (StartsLine(c.m_Start))
				return LineIndent(c.m_Start);
		}
		return LineIndent(parent.m_Start) + m_Unit;
	}

	// Appends an element (its text without the first line's indent) as the last child of 'parent'.
	void AppendChild(DZS_XmlNode parent, string text)
	{
		string indent = ChildIndent(parent);
		string body = Reindent(text, indent, m_Eol);
		string pind = LineIndent(parent.m_Start);
		if (parent.m_SelfClose)
		{
			string head = m_Xml.Slice(parent.m_Start, parent.m_OpenEnd);
			head = head.Substring(0, head.Length() - 2);
			head = DZS_PatchText.TrimRight(head) + ">";
			AddToInsert(parent.m_Start, parent.m_Start, parent.m_End, head, m_Eol + pind + "</" + parent.m_Name + ">", m_Eol + indent + body);
			return;
		}
		int et = EndTagStart(parent);
		if (StartsLine(et))
		{
			Insertion(LineStart(et), indent + body + m_Eol);
			return;
		}
		AddToInsert(et, et, et, "", m_Eol + pind, m_Eol + indent + body);
	}

	protected void AddToInsert(int key, int start, int end, string head, string tail, string item)
	{
		DZS_PInsert ins = m_Inserts.Get(key);
		if (!ins)
		{
			ins = new DZS_PInsert();
			ins.m_Pos = start;
			ins.m_Head = head;
			ins.m_Tail = tail;
			m_Inserts.Set(key, ins);
			ins.m_Patch = Patch("i" + key, start, end, "");
		}
		ins.m_Items.Insert(item);
		string t = ins.m_Head;
		foreach (string it : ins.m_Items)
			t += it;
		t += ins.m_Tail;
		ins.m_Patch.m_Text = t;
	}

	// Inserts an element right after 'sibling', on a line of its own with the sibling's indent.
	void InsertAfter(DZS_XmlNode sibling, string text)
	{
		string indent = LineIndent(sibling.m_Start);
		Insertion(sibling.m_End, m_Eol + indent + Reindent(text, indent, m_Eol));
	}

	// Inserts an element as the first child of 'parent'.
	void InsertFirst(DZS_XmlNode parent, string text)
	{
		if (parent.m_SelfClose || parent.m_Children.Count() == 0)
		{
			AppendChild(parent, text);
			return;
		}
		string indent = ChildIndent(parent);
		Insertion(parent.m_OpenEnd, m_Eol + indent + Reindent(text, indent, m_Eol));
	}

	// ---- JSON ----

	string Raw(DZS_SrcNode n)
	{
		string k = "j" + n.m_Start;
		if (m_Over.Contains(k))
			return m_Over.Get(k);
		return m_Json.Raw(n);
	}

	void SetRaw(DZS_SrcNode n, string text)
	{
		string k = "j" + n.m_Start;
		m_Over.Set(k, text);
		string body = Reindent(text, LineIndent(n.m_Start), m_Eol);
		Patch(k, n.m_Start, n.m_End, body);
	}

	DZS_PJCont Cont(DZS_SrcNode n)
	{
		foreach (DZS_PJCont c : m_Conts)
		{
			if (c.m_Node == n)
				return c;
		}
		DZS_PJCont nc = new DZS_PJCont();
		nc.m_Node = n;
		m_Conts.Insert(nc);
		m_Changed = true;
		return nc;
	}

	bool ItemRemoved(DZS_SrcNode container, int i)
	{
		foreach (DZS_PJCont c : m_Conts)
		{
			if (c.m_Node == container)
				return c.m_Removed.Find(i) >= 0;
		}
		return false;
	}

	void RemoveItem(DZS_SrcNode container, int i)
	{
		DZS_PJCont c = Cont(container);
		if (c.m_Removed.Find(i) < 0)
			c.m_Removed.Insert(i);
	}

	// Appends a value to an array (name "") or a member to an object.
	void AppendItem(DZS_SrcNode container, string name, DZS_Json v)
	{
		DZS_PJCont c = Cont(container);
		c.m_Keys.Insert(name);
		c.m_Values.Insert(v);
	}

	// A member appended earlier in this run (to create a missing object path).
	DZS_Json Pending(DZS_SrcNode container, string name)
	{
		foreach (DZS_PJCont c : m_Conts)
		{
			if (c.m_Node != container)
				continue;
			for (int i = 0; i < c.m_Keys.Count(); i++)
			{
				if (c.m_Keys[i] == name)
					return c.m_Values[i];
			}
		}
		return null;
	}

	protected int ItemStart(DZS_SrcNode container, int i)
	{
		DZS_SrcNode it = container.m_Items[i];
		if (container.IsObj())
			return it.m_KeyStart;
		return it.m_Start;
	}

	protected void FinishConts()
	{
		foreach (DZS_PJCont c : m_Conts)
		{
			DZS_SrcNode n = c.m_Node;
			int count = n.Count();
			bool kept = false;
			for (int i = 0; i < count; i++)
			{
				if (c.m_Removed.Find(i) >= 0)
				{
					int s;
					int e = n.m_Items[i].m_End;
					if (kept)
						s = n.m_Items[i - 1].m_End;
					else if (i + 1 < count)
					{
						s = ItemStart(n, i);
						e = ItemStart(n, i + 1);
					}
					else
						s = ItemStart(n, i);
					DZS_XmlPatch rp = Insertion(s, "");
					rp.m_End = e;
				}
				else
					kept = true;
			}
			if (c.m_Values.Count() == 0)
				continue;
			string pind = LineIndent(n.m_Start);
			if (OneLine(n, c))
			{
				string flat = "";
				for (int f = 0; f < c.m_Values.Count(); f++)
				{
					if (f > 0 || (count > 0 && kept))
						flat += ", ";
					flat += Member(c.m_Keys[f], c.m_Values[f], "", "");
				}
				if (count == 0)
				{
					DZS_XmlPatch fp = Insertion(n.m_Start + 1, flat);
					fp.m_End = n.m_End - 1;
				}
				else
					Insertion(n.m_Items[count - 1].m_End, flat);
				continue;
			}
			if (count == 0)
			{
				string ind0 = pind + UnitOf(pind);
				string t0 = "";
				if (n.IsObj())
					t0 = "{";
				else
					t0 = "[";
				for (int k = 0; k < c.m_Values.Count(); k++)
				{
					if (k > 0)
						t0 += ",";
					t0 += m_Eol + ind0 + Member(c.m_Keys[k], c.m_Values[k], ind0, UnitOf(ind0));
				}
				if (n.IsObj())
					t0 += m_Eol + pind + "}";
				else
					t0 += m_Eol + pind + "]";
				DZS_XmlPatch ep = Insertion(n.m_Start, t0);
				ep.m_End = n.m_End;
				continue;
			}
			string ind = LineIndent(ItemStart(n, count - 1));
			string t = "";
			for (int m = 0; m < c.m_Values.Count(); m++)
			{
				if (kept || m > 0)
					t += ",";
				t += m_Eol + ind + Member(c.m_Keys[m], c.m_Values[m], ind, UnitOf(ind));
			}
			Insertion(n.m_Items[count - 1].m_End, t);
		}
	}

	// An array written on one line in the base gets scalar values appended on that line.
	protected bool OneLine(DZS_SrcNode n, DZS_PJCont c)
	{
		if (!n.IsArr())
			return false;
		int nl = m_Xml.Find(n.m_Start, "\n");
		if (nl >= 0 && nl < n.m_End)
			return false;
		foreach (DZS_Json v : c.m_Values)
		{
			if (v.IsArr() || v.IsObj())
				return false;
		}
		return true;
	}

	// JSON files mix tabs and spaces; new text indents like the line it follows.
	static string UnitOf(string indent)
	{
		if (indent.IndexOf("\t") >= 0)
			return "\t";
		return "    ";
	}

	protected string Member(string name, DZS_Json v, string indent, string unit)
	{
		string body = DZS_PatchText.JsonText(v, indent, unit, m_Eol);
		if (name == "")
			return body;
		return DZS_TextForm.Str(name) + ": " + body;
	}

	// ---- output ----

	// Applies every edit; m_Out holds the new bytes. False when nothing changed.
	bool Finish()
	{
		if (m_Created)
			return true;
		if (!m_Changed)
			return false;
		if (m_IsJson)
			FinishConts();
		m_Out = DZS_XmlWriter.Apply(m_Xml, m_Patches);
		if (DZS_Files.SameContent(m_Out, m_Chunks))
			return false;
		return true;
	}
}

// Text helpers of the patcher.
class DZS_PatchText
{
	static void Lines(string text, array<string> lines)
	{
		int start = 0;
		int n = text.Length();
		int at = text.IndexOf("\n");
		while (at >= 0)
		{
			lines.Insert(DZS_Utf8.Part(text, start, at - start));
			start = at + 1;
			at = text.IndexOfFrom(start, "\n");
		}
		lines.Insert(DZS_Utf8.Part(text, start, n - start));
	}

	static string TrimRight(string s)
	{
		int n = s.Length();
		while (n > 0)
		{
			string c = s.Get(n - 1);
			if (c != " " && c != "\t" && c != "\r" && c != "\n")
				break;
			n--;
		}
		return s.Substring(0, n);
	}

	static string Lower(string s)
	{
		string l = s;
		l.ToLower();
		return l;
	}

	// JSON value as text: scalar arrays on one line, objects and other arrays one member per line. Lines after the
	// first are indented by 'indent'; '\n' is replaced by 'eol' later by the caller's Reindent where needed.
	static string JsonText(DZS_Json v, string indent, string unit, string eol)
	{
		if (v.m_Type == DZS_Json.STR)
			return DZS_TextForm.Str(v.m_Str);
		if (v.m_Type == DZS_Json.NUM)
			return v.m_Str;
		if (v.m_Type == DZS_Json.BOOL)
			return DZS_Str.Bool(v.m_Bool);
		if (v.m_Type == DZS_Json.NUL)
			return "null";
		if (v.m_Type == DZS_Json.ARR)
		{
			if (v.Count() == 0)
				return "[]";
			bool flat = true;
			for (int i = 0; i < v.Count(); i++)
			{
				DZS_Json it = v.At(i);
				if (it.IsArr() || it.IsObj())
					flat = false;
			}
			string r = "[";
			for (int k = 0; k < v.Count(); k++)
			{
				if (k > 0)
					r += ",";
				if (flat)
				{
					if (k > 0)
						r += " ";
				}
				else
					r += eol + indent + unit;
				r += JsonText(v.At(k), indent + unit, unit, eol);
			}
			if (!flat)
				r += eol + indent;
			return r + "]";
		}
		if (v.Count() == 0)
			return "{}";
		string o = "{";
		for (int m = 0; m < v.m_Keys.Count(); m++)
		{
			if (m > 0)
				o += ",";
			string key = v.m_Keys[m];
			DZS_Json mv = v.Get(key);
			o += eol + indent + unit + DZS_TextForm.Str(key) + ": " + JsonText(mv, indent + unit, unit, eol);
		}
		return o + eol + indent + "}";
	}
}

// The files of one mission folder, found regardless of case and read once.
class DZS_PatchFileRead : Managed
{
	string m_Low;
	ref DZS_PatchFile m_File;
	ref array<string> m_Chunks;
	ref DZS_XmlSteps m_Parse;
}

class DZS_PatchFiles : Managed
{
	string m_Base;
	ref map<string, ref DZS_PatchFile> m_Files = new map<string, ref DZS_PatchFile>();
	ref array<string> m_Order = new array<string>();
	ref map<string, ref array<string>> m_Dirs = new map<string, ref array<string>>();
	// files read from elsewhere, by lower-case path: the original a project kept when it first changed the file;
	// "" hides a file the project itself created
	ref map<string, string> m_Over = new map<string, string>();
	// files read ahead (BeginRead), by lower-case path, until the run asks for them
	ref map<string, ref DZS_PatchFile> m_Ready = new map<string, ref DZS_PatchFile>();

	void DZS_PatchFiles(string baseDir)
	{
		m_Base = baseDir;
	}

	// The base's own spelling of a relative path, or "" when it does not exist.
	string Resolve(string rel)
	{
		array<string> parts = new array<string>();
		rel.Split("/", parts);
		string dir = m_Base;
		string done = "";
		for (int i = 0; i < parts.Count(); i++)
		{
			string want = DZS_PatchText.Lower(parts[i]);
			array<string> names = m_Dirs.Get(dir);
			if (!names)
			{
				names = new array<string>();
				array<string> listed;
				array<bool> folders;
				DZS_Files.List(dir, listed, folders);
				names.Copy(listed);
				m_Dirs.Set(dir, names);
			}
			string found = "";
			foreach (string nm : names)
			{
				if (DZS_PatchText.Lower(nm) == want)
				{
					found = nm;
					break;
				}
			}
			if (found == "")
				return "";
			dir = dir + "\\" + found;
			if (done != "")
				done += "/";
			done += found;
		}
		return done;
	}

	// The file at a relative path (any case); m_Exists false when the base has none.
	DZS_PatchFile Get(string rel)
	{
		string low = DZS_PatchText.Lower(rel);
		DZS_PatchFile f = m_Files.Get(low);
		if (f)
			return f;
		f = m_Ready.Get(low);
		if (!f)
			f = Open(rel, null);
		m_Files.Set(low, f);
		m_Ready.Remove(low);
		m_Order.Insert(low);
		return f;
	}

	// Reading a file ahead over several frames (DZS_Patcher.Step): an XML file of the base is parsed in steps and kept
	// until the run asks for it, so a file the run never asks for leaves no trace in the result. Null when there is
	// nothing to read in steps (read already, missing, JSON: those are read when asked for).
	DZS_PatchFileRead BeginRead(string rel)
	{
		string low = DZS_PatchText.Lower(rel);
		if (m_Files.Contains(low) || m_Ready.Contains(low))
			return null;
		DZS_PatchFileRead r = new DZS_PatchFileRead();
		r.m_Low = low;
		DZS_PatchFile f = Open(rel, r);
		r.m_File = f;
		if (!r.m_Parse)
			return null;
		return r;
	}

	// One slice of a read ahead; true when the file is read and waits for the run.
	bool StepRead(DZS_PatchFileRead r, int t0, float ms)
	{
		if (!r.m_Parse.Step(t0, ms))
			return false;
		r.m_File.LoadXml(r.m_Chunks, r.m_Parse.Result());
		m_Ready.Set(r.m_Low, r.m_File);
		r.m_Parse = null;
		return true;
	}

	// The file as Get reads it; with 'steps' an XML file of the base is not parsed here but handed to steps.m_Parse.
	protected DZS_PatchFile Open(string rel, DZS_PatchFileRead steps)
	{
		string low = DZS_PatchText.Lower(rel);
		DZS_PatchFile f = new DZS_PatchFile();
		string real = Resolve(rel);
		bool json = low.IndexOf(".json") == low.Length() - 5;
		string alt = "";
		bool over = m_Over.Find(low, alt);
		if (over && alt == "")
			real = "";
		if (over && alt != "")
		{
			if (real == "")
				real = rel;
			f.m_Path = real;
			array<string> kept = DZS_Files.Read(alt);
			if (!kept)
			{
				f.m_Exists = false;
				f.m_Error = "unreadable";
			}
			else
			{
				f.m_Exists = true;
				if (steps && !json)
				{
					steps.m_Chunks = kept;
					steps.m_Parse = new DZS_XmlSteps(kept);
				}
				else
					f.Load(kept, json);
			}
		}
		else if (real == "")
		{
			f.m_Path = rel;
			f.m_Exists = false;
			f.m_IsJson = json;
		}
		else
		{
			f.m_Path = real;
			string full = m_Base + "\\" + real;
			full.Replace("/", "\\");
			array<string> chunks = DZS_Files.Read(full);
			if (!chunks)
			{
				f.m_Exists = false;
				f.m_Error = "unreadable";
			}
			else
			{
				f.m_Exists = true;
				if (steps && !json)
				{
					steps.m_Chunks = chunks;
					steps.m_Parse = new DZS_XmlSteps(chunks);
				}
				else
					f.Load(chunks, json);
			}
		}
		return f;
	}

	// A file the run creates (own spawner files).
	DZS_PatchFile Create(string rel, array<string> chunks)
	{
		string low = DZS_PatchText.Lower(rel);
		DZS_PatchFile f = new DZS_PatchFile();
		f.m_Path = rel;
		f.m_Exists = true;
		f.m_Created = true;
		f.m_IsJson = true;
		f.m_Out = chunks;
		f.m_Changed = true;
		m_Files.Set(low, f);
		m_Order.Insert(low);
		return f;
	}

	array<DZS_PatchFile> All()
	{
		array<DZS_PatchFile> r = new array<DZS_PatchFile>();
		foreach (string k : m_Order)
			r.Insert(m_Files.Get(k));
		return r;
	}
}
