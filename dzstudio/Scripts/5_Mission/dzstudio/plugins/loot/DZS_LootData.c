// Editable loot entries: what the loot editor changes is kept as an element
// "loot:entry" per type in the project state, its content in a compact text form. Values keep their text from the
// file; values the editor sets are written with six decimals. m_Src ties containers and points to the element they
// came from, so writing touches only what changed.
class DZS_LootEPoint : Managed
{
	string m_Pos;
	string m_Range;
	string m_Height;
	string m_Flags;
	bool m_HasRange;
	bool m_HasHeight;
	int m_Src = -1;
	// an own point's extensions as JSON text ("" = none), see DZS_EntityExts
	string m_Ext;

	DZS_LootEPoint Copy()
	{
		DZS_LootEPoint p = new DZS_LootEPoint();
		p.m_Pos = m_Pos;
		p.m_Range = m_Range;
		p.m_Height = m_Height;
		p.m_Flags = m_Flags;
		p.m_HasRange = m_HasRange;
		p.m_HasHeight = m_HasHeight;
		p.m_Src = m_Src;
		p.m_Ext = m_Ext;
		return p;
	}

	bool Valid()
	{
		float a, b, c;
		return DZS_LootFiles.Three(m_Pos, a, b, c);
	}

	vector P()
	{
		float a, b, c;
		DZS_LootFiles.Three(m_Pos, a, b, c);
		return Vector(a, b, c);
	}

	void SetP(vector p)
	{
		string x = DZS_XmlWriter.Fixed6(p[0]);
		string y = DZS_XmlWriter.Fixed6(p[1]);
		string z = DZS_XmlWriter.Fixed6(p[2]);
		m_Pos = x + " " + y + " " + z;
	}

	float Range()
	{
		if (!m_HasRange)
			return 0.1;
		return m_Range.ToFloat();
	}

	float Height()
	{
		if (!m_HasHeight)
			return 0.1;
		return m_Height.ToFloat();
	}

	void SetRange(float v)
	{
		m_Range = DZS_XmlWriter.Fixed6(v);
		m_HasRange = true;
	}

	void SetHeight(float v)
	{
		m_Height = DZS_XmlWriter.Fixed6(v);
		m_HasHeight = true;
	}
}

class DZS_LootEContainer : Managed
{
	string m_Name;
	string m_Lootmax;
	ref array<string> m_Cats = new array<string>();
	ref array<string> m_Tags = new array<string>();
	ref array<ref DZS_LootEPoint> m_Points = new array<ref DZS_LootEPoint>();
	int m_Src = -1;
	int m_Extra;
	string m_Ext;

	DZS_LootEContainer Copy()
	{
		DZS_LootEContainer c = new DZS_LootEContainer();
		c.m_Name = m_Name;
		c.m_Lootmax = m_Lootmax;
		c.m_Cats.Copy(m_Cats);
		c.m_Tags.Copy(m_Tags);
		foreach (DZS_LootEPoint p : m_Points)
		{
			DZS_LootEPoint pc = p.Copy();
			c.m_Points.Insert(pc);
		}
		c.m_Src = m_Src;
		c.m_Extra = m_Extra;
		c.m_Ext = m_Ext;
		return c;
	}
}

class DZS_LootEEntry : Managed
{
	string m_Name;
	string m_Lootmax;
	ref array<string> m_Usages = new array<string>();
	ref array<string> m_Values = new array<string>();
	ref array<ref DZS_LootEContainer> m_Containers = new array<ref DZS_LootEContainer>();
	int m_Dispatch;
	int m_Extra;
	string m_ExtraSig;
	// an own type's extensions as JSON text
	string m_Ext;

	DZS_LootEEntry Copy()
	{
		DZS_LootEEntry e = new DZS_LootEEntry();
		e.m_Name = m_Name;
		e.m_Lootmax = m_Lootmax;
		e.m_Usages.Copy(m_Usages);
		e.m_Values.Copy(m_Values);
		foreach (DZS_LootEContainer c : m_Containers)
		{
			DZS_LootEContainer cc = c.Copy();
			e.m_Containers.Insert(cc);
		}
		e.m_Dispatch = m_Dispatch;
		e.m_Extra = m_Extra;
		e.m_ExtraSig = m_ExtraSig;
		e.m_Ext = m_Ext;
		return e;
	}

	int CountPoints()
	{
		int n = 0;
		foreach (DZS_LootEContainer c : m_Containers)
			n += c.m_Points.Count();
		return n;
	}

	int CountFlagged()
	{
		int n = 0;
		foreach (DZS_LootEContainer c : m_Containers)
		{
			foreach (DZS_LootEPoint p : c.m_Points)
			{
				if (p.m_Flags != "")
					n++;
			}
		}
		return n;
	}

	// Content as compared: names, order and the raw text of the values, so
	// 0.6 and 0.600000 count as different; the attribute dzstudio does not count.
	string Sig()
	{
		string s = "g " + m_Lootmax;
		foreach (string v : m_Values)
			s += "|v " + v;
		foreach (string u : m_Usages)
			s += "|u " + u;
		foreach (DZS_LootEContainer c : m_Containers)
		{
			s += "|c " + c.m_Name + " " + c.m_Lootmax;
			foreach (string k : c.m_Cats)
				s += "|k " + k;
			foreach (string t : c.m_Tags)
				s += "|t " + t;
			foreach (DZS_LootEPoint p : c.m_Points)
			{
				string r = "-";
				if (p.m_HasRange)
					r = p.m_Range;
				string h = "-";
				if (p.m_HasHeight)
					h = p.m_Height;
				s += "|p " + p.m_Pos + " " + r + " " + h + " " + p.m_Flags;
			}
		}
		s += "|x " + m_ExtraSig;
		return s;
	}

	// ---- text form in the project state ----

	protected static string Esc(string t)
	{
		string s = t;
		s.Replace("%", "%25");
		s.Replace("|", "%7C");
		s.Replace(";", "%3B");
		return s;
	}

	protected static string Unesc(string t)
	{
		if (t.IndexOf("%") < 0)
			return t;
		string s = t;
		s.Replace("%7C", "|");
		s.Replace("%3B", ";");
		s.Replace("%25", "%");
		return s;
	}

	protected static string Opt(bool has, string v)
	{
		if (!has)
			return "~";
		return v;
	}

	string ToText()
	{
		string en = Esc(m_Name);
		string el = Esc(m_Lootmax);
		string ex = Esc(m_ExtraSig);
		string s = "E|" + en + "|" + el + "|" + m_Dispatch + "|" + m_Extra + "|" + ex;
		if (m_Ext != "")
			s += "|" + Esc(m_Ext);
		foreach (string v : m_Values)
			s += ";V|" + Esc(v);
		foreach (string u : m_Usages)
			s += ";U|" + Esc(u);
		foreach (DZS_LootEContainer c : m_Containers)
		{
			string cn = Esc(c.m_Name);
			string cl = Esc(c.m_Lootmax);
			s += ";C|" + c.m_Src + "|" + cn + "|" + cl + "|" + c.m_Extra;
			if (c.m_Ext != "")
				s += "|" + Esc(c.m_Ext);
			foreach (string k : c.m_Cats)
				s += ";K|" + Esc(k);
			foreach (string t : c.m_Tags)
				s += ";T|" + Esc(t);
			foreach (DZS_LootEPoint p : c.m_Points)
			{
				string pr = Opt(p.m_HasRange, p.m_Range);
				string ph = Opt(p.m_HasHeight, p.m_Height);
				string pf = Esc(p.m_Flags);
				s += ";P|" + p.m_Src + "|" + p.m_Pos + "|" + pr + "|" + ph + "|" + pf;
				if (p.m_Ext != "")
					s += "|" + Esc(p.m_Ext);
			}
		}
		return s;
	}

	// Like string.Split, but empty pieces stay (Split drops them, which shifted the fields of the text form).
	static void SplitKeep(string text, string sep, array<string> into)
	{
		int from = 0;
		int n = text.Length();
		while (true)
		{
			int at = text.IndexOfFrom(from, sep);
			if (at < 0)
			{
				string last = text.Substring(from, n - from);
				into.Insert(last);
				return;
			}
			string piece = text.Substring(from, at - from);
			into.Insert(piece);
			from = at + 1;
		}
	}

	static DZS_LootEEntry FromText(string text)
	{
		array<string> lines = new array<string>();
		SplitKeep(text, ";", lines);
		DZS_LootEEntry e = new DZS_LootEEntry();
		DZS_LootEContainer cur = null;
		foreach (string line : lines)
		{
			array<string> f = new array<string>();
			SplitKeep(line, "|", f);
			if (f.Count() == 0)
				continue;
			string k = f[0];
			if (k == "E" && f.Count() >= 6)
			{
				e.m_Name = Unesc(f[1]);
				e.m_Lootmax = Unesc(f[2]);
				e.m_Dispatch = f[3].ToInt();
				e.m_Extra = f[4].ToInt();
				e.m_ExtraSig = Unesc(f[5]);
				if (f.Count() >= 7)
					e.m_Ext = Unesc(f[6]);
			}
			else if (k == "V" && f.Count() >= 2)
			{
				string tv8 = Unesc(f[1]);
				e.m_Values.Insert(tv8);
			}
			else if (k == "U" && f.Count() >= 2)
			{
				string tv9 = Unesc(f[1]);
				e.m_Usages.Insert(tv9);
			}
			else if (k == "C" && f.Count() >= 5)
			{
				cur = new DZS_LootEContainer();
				cur.m_Src = f[1].ToInt();
				cur.m_Name = Unesc(f[2]);
				cur.m_Lootmax = Unesc(f[3]);
				cur.m_Extra = f[4].ToInt();
				if (f.Count() >= 6)
					cur.m_Ext = Unesc(f[5]);
				e.m_Containers.Insert(cur);
			}
			else if (k == "K" && cur && f.Count() >= 2)
			{
				string tv10 = Unesc(f[1]);
				cur.m_Cats.Insert(tv10);
			}
			else if (k == "T" && cur && f.Count() >= 2)
			{
				string tv11 = Unesc(f[1]);
				cur.m_Tags.Insert(tv11);
			}
			else if (k == "P" && cur && f.Count() >= 6)
			{
				DZS_LootEPoint p = new DZS_LootEPoint();
				p.m_Src = f[1].ToInt();
				p.m_Pos = f[2];
				p.m_HasRange = f[3] != "~";
				if (p.m_HasRange)
					p.m_Range = f[3];
				p.m_HasHeight = f[4] != "~";
				if (p.m_HasHeight)
					p.m_Height = f[4];
				p.m_Flags = Unesc(f[5]);
				if (f.Count() >= 7)
					p.m_Ext = Unesc(f[6]);
				cur.m_Points.Insert(p);
			}
		}
		return e;
	}

	// ---- from the file ----

	// The entry of a group element; points and containers are numbered in file order (m_Src).
	static DZS_LootEEntry FromNode(DZS_XmlDoc doc, DZS_XmlNode g)
	{
		DZS_LootEEntry e = new DZS_LootEEntry();
		e.m_Name = g.Get("name");
		e.m_Lootmax = g.Get("lootmax");
		int ci = 0;
		int pi = 0;
		foreach (DZS_XmlNode c : g.m_Children)
		{
			if (c.m_Name == "usage")
			{
				string tv12 = c.Get("name");
				e.m_Usages.Insert(tv12);
			}
			else if (c.m_Name == "value")
			{
				string tv13 = c.Get("name");
				e.m_Values.Insert(tv13);
			}
			else if (c.m_Name == "dispatch")
			{
				e.m_Dispatch += c.m_Children.Count();
				string draw = doc.Raw(c);
				e.m_ExtraSig += "d" + draw.Length();
			}
			else if (c.m_Name == "container")
			{
				DZS_LootEContainer lc = new DZS_LootEContainer();
				lc.m_Src = ci;
				ci++;
				lc.m_Name = c.Get("name");
				lc.m_Lootmax = c.Get("lootmax");
				foreach (DZS_XmlNode k : c.m_Children)
				{
					if (k.m_Name == "category")
					{
						string tv14 = k.Get("name");
						lc.m_Cats.Insert(tv14);
					}
					else if (k.m_Name == "tag")
					{
						string tv15 = k.Get("name");
						lc.m_Tags.Insert(tv15);
					}
					else if (k.m_Name == "point")
					{
						DZS_LootEPoint p = new DZS_LootEPoint();
						p.m_Src = pi;
						pi++;
						p.m_Pos = k.Get("pos");
						p.m_HasRange = k.Has("range");
						p.m_Range = k.Get("range");
						p.m_HasHeight = k.Has("height");
						p.m_Height = k.Get("height");
						p.m_Flags = k.Get("flags");
						lc.m_Points.Insert(p);
					}
					else
					{
						lc.m_Extra++;
						e.m_ExtraSig += "k" + k.m_Name + k.Get("name");
					}
				}
				e.m_Containers.Insert(lc);
			}
			else
			{
				e.m_Extra++;
				e.m_ExtraSig += "e" + c.m_Name + c.Get("name");
			}
		}
		return e;
	}

	// ---- new text ----

	static string Elem(string tag, string name)
	{
		return "<" + tag + " name=\"" + DZS_Xml.Escape(name) + "\" />";
	}

	static string PointText(DZS_LootEPoint p)
	{
		string s = "<point pos=\"" + p.m_Pos + "\"";
		if (p.m_HasRange)
			s += " range=\"" + p.m_Range + "\"";
		if (p.m_HasHeight)
			s += " height=\"" + p.m_Height + "\"";
		return s + " />";
	}

	static string ContainerText(DZS_LootEContainer c, string ind, string unit, string eol)
	{
		string s = "<container name=\"" + DZS_Xml.Escape(c.m_Name) + "\"";
		if (c.m_Lootmax != "")
			s += " lootmax=\"" + c.m_Lootmax + "\"";
		s += ">";
		string in2 = ind + unit;
		foreach (string k : c.m_Cats)
			s += eol + in2 + Elem("category", k);
		foreach (string t : c.m_Tags)
			s += eol + in2 + Elem("tag", t);
		foreach (DZS_LootEPoint p : c.m_Points)
			s += eol + in2 + PointText(p);
		return s + eol + ind + "</container>";
	}

	string GroupText(string ind, string unit, string eol, string mark)
	{
		string s = "<group name=\"" + DZS_Xml.Escape(m_Name) + "\"";
		if (m_Lootmax != "")
			s += " lootmax=\"" + m_Lootmax + "\"";
		if (mark != "")
			s += " dzstudio=\"" + mark + "\"";
		s += ">";
		string in2 = ind + unit;
		foreach (string v : m_Values)
			s += eol + in2 + Elem("value", v);
		foreach (string u : m_Usages)
			s += eol + in2 + Elem("usage", u);
		foreach (DZS_LootEContainer c : m_Containers)
			s += eol + in2 + ContainerText(c, in2, unit, eol);
		return s + eol + ind + "</group>";
	}
}

// Patches that turn a group element of the file into an entry (only what differs is rewritten).
class DZS_LootDiff
{
	protected DZS_XmlDoc m_Doc;
	protected array<ref DZS_XmlPatch> m_Out;
	protected string m_Eol;
	protected string m_Unit;

	void DZS_LootDiff(DZS_XmlDoc doc, array<ref DZS_XmlPatch> into, string unit)
	{
		m_Doc = doc;
		m_Out = into;
		m_Eol = doc.m_Eol;
		m_Unit = unit;
	}

	protected void Patch(int start, int end, string text, DZS_XmlNode parent, DZS_XmlNode tagOf = null)
	{
		DZS_XmlPatch p = new DZS_XmlPatch();
		p.m_Start = start;
		p.m_End = end;
		p.m_Text = text;
		p.m_Parent = parent;
		p.m_TagOf = tagOf;
		m_Out.Insert(p);
	}

	// An attribute set to a text ("" = removed with the space before it); a missing one goes after 'after'.
	void Attr(DZS_XmlNode n, string name, string value, string after)
	{
		DZS_XmlAttr a = n.Attr(name);
		if (a)
		{
			if (value == "")
				Patch(a.m_LeadStart, a.m_ValueEnd + 1, "", null, n);
			else if (a.m_Raw != value)
				Patch(a.m_ValueStart, a.m_ValueEnd, value, null, n);
			return;
		}
		if (value == "")
			return;
		int at = n.m_Start + 1 + n.m_Name.Length();
		DZS_XmlAttr prev = n.Attr(after);
		if (prev)
			at = prev.m_ValueEnd + 1;
		Patch(at, at, " " + name + "=\"" + value + "\"", null, n);
	}

	// The project's mark as the last attribute of the start tag; one of another project takes the new value in place
	// ("" removes it).
	void Mark(DZS_XmlNode n, string mark)
	{
		DZS_XmlAttr a = n.Attr("dzstudio");
		if (a || mark == "")
		{
			Attr(n, "dzstudio", mark, "");
			return;
		}
		int at = n.m_Start + 1 + n.m_Name.Length();
		int last = n.m_Attrs.Count() - 1;
		if (last >= 0)
			at = n.m_Attrs[last].m_ValueEnd + 1;
		Patch(at, at, " dzstudio=\"" + mark + "\"", null, n);
	}

	void Delete(DZS_XmlNode n)
	{
		Patch(n.m_LeadStart, n.m_End, "", null);
	}

	protected string IndentFor(DZS_XmlNode parent)
	{
		if (parent.m_Children.Count() > 0)
			return m_Doc.IndentOf(parent.m_Children[0]);
		return m_Doc.IndentOf(parent) + m_Unit;
	}

	// New element text placed after 'afterPos' (end of an element, or the end of the parent's start tag).
	void InsertAt(DZS_XmlNode parent, int afterPos, string elemText)
	{
		Patch(afterPos, afterPos, m_Eol + IndentFor(parent) + elemText, parent);
	}

	protected static int LastEnd(DZS_XmlNode parent, string kind)
	{
		int at = -1;
		foreach (DZS_XmlNode c : parent.m_Children)
		{
			if (c.m_Name == kind)
				at = c.m_End;
		}
		return at;
	}

	// A list of names (usages, values, categories, tags) brought to 'want'; new names after the last of their kind,
	// else after the last of 'prevKinds' (in order), else at the start of the parent.
	void Names(DZS_XmlNode parent, string kind, array<string> want, array<string> prevKinds)
	{
		array<string> left = new array<string>();
		left.Copy(want);
		foreach (DZS_XmlNode c : parent.m_Children)
		{
			if (c.m_Name != kind)
				continue;
			string nm = c.Get("name");
			int i = left.Find(nm);
			if (i >= 0)
				left.Remove(i);
			else
				Delete(c);
		}
		if (left.Count() == 0)
			return;
		int at = LastEnd(parent, kind);
		if (at < 0)
		{
			foreach (string pk : prevKinds)
			{
				int pe = LastEnd(parent, pk);
				if (pe > at)
					at = pe;
			}
		}
		if (at < 0)
			at = parent.m_OpenEnd;
		foreach (string n : left)
			InsertAt(parent, at, DZS_LootEEntry.Elem(kind, n));
	}

	void OneGroup(DZS_XmlNode g, DZS_LootEEntry e)
	{
		Attr(g, "lootmax", e.m_Lootmax, "name");
		array<string> none = new array<string>();
		Names(g, "value", e.m_Values, none);
		array<string> afterUsage = {"value"};
		Names(g, "usage", e.m_Usages, afterUsage);
		array<DZS_XmlNode> nodes = new array<DZS_XmlNode>();
		array<DZS_XmlNode> pointNodes = new array<DZS_XmlNode>();
		foreach (DZS_XmlNode c : g.m_Children)
		{
			if (c.m_Name != "container")
				continue;
			nodes.Insert(c);
			foreach (DZS_XmlNode k : c.m_Children)
			{
				if (k.m_Name == "point")
					pointNodes.Insert(k);
			}
		}
		map<int, bool> keptC = new map<int, bool>();
		map<int, bool> keptP = new map<int, bool>();
		foreach (DZS_LootEContainer lc : e.m_Containers)
		{
			if (lc.m_Src >= 0 && lc.m_Src < nodes.Count())
				keptC.Set(lc.m_Src, true);
		}
		// containers and points no longer there
		for (int ni = 0; ni < nodes.Count(); ni++)
		{
			if (!keptC.Contains(ni))
				Delete(nodes[ni]);
		}
		int lastC = LastEnd(g, "container");
		int anchor = lastC;
		if (anchor < 0)
		{
			foreach (DZS_XmlNode d : g.m_Children)
			{
				if (d.m_Name == "dispatch")
				{
					anchor = d.m_LeadStart;
					break;
				}
			}
		}
		if (anchor < 0)
		{
			if (g.m_Children.Count() > 0)
				anchor = g.m_Children[g.m_Children.Count() - 1].m_End;
			else
				anchor = g.m_OpenEnd;
		}
		string ind = IndentFor(g);
		foreach (DZS_LootEContainer ec : e.m_Containers)
		{
			if (ec.m_Src < 0 || ec.m_Src >= nodes.Count())
			{
				Patch(anchor, anchor, m_Eol + ind + DZS_LootEEntry.ContainerText(ec, ind, m_Unit, m_Eol), g);
				continue;
			}
			DZS_XmlNode cn = nodes[ec.m_Src];
			OneContainer(cn, ec, pointNodes, keptP);
		}
		foreach (DZS_XmlNode cn2 : nodes)
		{
			foreach (DZS_XmlNode pk : cn2.m_Children)
			{
				if (pk.m_Name != "point")
					continue;
				int idx = pointNodes.Find(pk);
				if (!keptP.Contains(idx) && keptC.Contains(nodes.Find(cn2)))
					Delete(pk);
			}
		}
	}

	protected void OneContainer(DZS_XmlNode cn, DZS_LootEContainer ec, array<DZS_XmlNode> pointNodes, map<int, bool> keptP)
	{
		Attr(cn, "name", ec.m_Name, "");
		Attr(cn, "lootmax", ec.m_Lootmax, "name");
		array<string> none = new array<string>();
		Names(cn, "category", ec.m_Cats, none);
		array<string> afterTag = {"category"};
		Names(cn, "tag", ec.m_Tags, afterTag);
		int lastAt = -1;
		foreach (DZS_XmlNode k0 : cn.m_Children)
		{
			if (k0.m_Name == "category" || k0.m_Name == "tag")
				lastAt = k0.m_End;
		}
		if (lastAt < 0)
			lastAt = cn.m_OpenEnd;
		foreach (DZS_LootEPoint p : ec.m_Points)
		{
			DZS_XmlNode pn = null;
			if (p.m_Src >= 0 && p.m_Src < pointNodes.Count() && cn.m_Children.Find(pointNodes[p.m_Src]) >= 0 && !keptP.Contains(p.m_Src))
				pn = pointNodes[p.m_Src];
			if (!pn)
			{
				InsertAt(cn, lastAt, DZS_LootEEntry.PointText(p));
				continue;
			}
			keptP.Set(p.m_Src, true);
			lastAt = pn.m_End;
			Attr(pn, "pos", p.m_Pos, "");
			string r = "";
			if (p.m_HasRange)
				r = p.m_Range;
			Attr(pn, "range", r, "pos");
			string h = "";
			if (p.m_HasHeight)
				h = p.m_Height;
			string hAfter = "range";
			if (!pn.Has("range"))
				hAfter = "pos";
			Attr(pn, "height", h, hAfter);
		}
	}
}
