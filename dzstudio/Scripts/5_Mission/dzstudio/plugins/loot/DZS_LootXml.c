// XML for the loot files: every element keeps where it stands in
// the source (start, end of its start tag, end, the space before it) and every attribute its raw text and place, so
// writing can replace exactly what changed and copy everything else byte for byte.
class DZS_XmlAttr : Managed
{
	string m_Name;
	string m_Raw;
	int m_LeadStart;
	int m_ValueStart;
	int m_ValueEnd;

	string Value()
	{
		return DZS_Xml.Unescape(m_Raw);
	}
}

class DZS_XmlNode : Managed
{
	string m_Name;
	int m_LeadStart;
	int m_Start;
	int m_OpenEnd;
	int m_End;
	int m_Line;
	bool m_SelfClose;
	ref array<ref DZS_XmlAttr> m_Attrs = new array<ref DZS_XmlAttr>();
	ref array<ref DZS_XmlNode> m_Children = new array<ref DZS_XmlNode>();

	DZS_XmlAttr Attr(string name)
	{
		foreach (DZS_XmlAttr a : m_Attrs)
		{
			if (a.m_Name == name)
				return a;
		}
		return null;
	}

	bool Has(string name)
	{
		return Attr(name) != null;
	}

	string Get(string name)
	{
		DZS_XmlAttr a = Attr(name);
		if (!a)
			return "";
		return a.Value();
	}

	array<DZS_XmlNode> Kids(string name)
	{
		array<DZS_XmlNode> r = new array<DZS_XmlNode>();
		foreach (DZS_XmlNode c : m_Children)
		{
			if (c.m_Name == name)
				r.Insert(c);
		}
		return r;
	}
}

// The source in pieces of exactly PIECE bytes (the last one shorter): Enforce string access on texts of a
// megabyte is slow, on pieces it is not.
class DZS_XmlDoc : Managed
{
	static const int PIECE = 1000;
	ref array<string> m_C = new array<string>();
	int m_N;
	ref DZS_XmlNode m_Root;
	bool m_Bom;
	string m_Eol = "\n";
	string m_Error;
	int m_ErrLine;
	int m_ErrCol;

	void SetSource(array<string> chunks)
	{
		m_C.Clear();
		bool even = true;
		for (int i = 0; i < chunks.Count() - 1; i++)
		{
			string ci = chunks[i];
			if (ci.Length() != PIECE)
				even = false;
		}
		if (even)
		{
			foreach (string c : chunks)
			{
				int len = c.Length();
				if (len > 0)
				{
					string copy = c.Substring(0, len);
					m_C.Insert(copy);
				}
			}
		}
		else
		{
			string buf = "";
			foreach (string c2 : chunks)
			{
				buf += c2;
				while (buf.Length() >= PIECE)
				{
					string piece = buf.Substring(0, PIECE);
					m_C.Insert(piece);
					string rest = buf.Substring(PIECE, buf.Length() - PIECE);
					buf = rest;
				}
			}
			if (buf != "")
				m_C.Insert(buf);
		}
		m_N = 0;
		foreach (string p : m_C)
			m_N += p.Length();
	}

	int Length() { return m_N; }

	string CharAt(int off)
	{
		if (off < 0 || off >= m_N)
			return "";
		string piece = m_C[off / PIECE];
		return piece.Get(off % PIECE);
	}

	int ByteAt(int off)
	{
		string c = CharAt(off);
		if (c == "")
			return -1;
		int b = c.ToAscii();
		if (b < 0)
			b += 256;
		return b;
	}

	// Offset of the next occurrence of one character at or after 'from', or -1.
	int Find(int from, string ch)
	{
		if (from < 0)
			from = 0;
		int idx = from / PIECE;
		int pos = from % PIECE;
		while (idx < m_C.Count())
		{
			string piece = m_C[idx];
			int i = piece.IndexOfFrom(pos, ch);
			if (i >= 0)
				return idx * PIECE + i;
			idx++;
			pos = 0;
		}
		return -1;
	}

	bool StartsAt(int off, string s)
	{
		int len = s.Length();
		if (off + len > m_N)
			return false;
		for (int i = 0; i < len; i++)
		{
			string a = CharAt(off + i);
			string b = s.Get(i);
			if (a != b)
				return false;
		}
		return true;
	}

	int FindStr(int from, string s)
	{
		string first = s.Get(0);
		int f = Find(from, first);
		while (f >= 0)
		{
			if (StartsAt(f, s))
				return f;
			f = Find(f + 1, first);
		}
		return -1;
	}

	string Slice(int start, int end)
	{
		string r = "";
		if (end > m_N)
			end = m_N;
		int at = start;
		while (at < end)
		{
			int idx = at / PIECE;
			int pos = at % PIECE;
			string piece = m_C[idx];
			int take = Math.Min(piece.Length() - pos, end - at);
			string part = piece.Substring(pos, take);
			r += part;
			at += take;
		}
		return r;
	}

	string Raw(DZS_XmlNode n)
	{
		return Slice(n.m_Start, n.m_End);
	}

	// Leading whitespace of the line an element starts on (its indent).
	string IndentOf(DZS_XmlNode n)
	{
		int i = n.m_Start;
		while (i > 0)
		{
			string c = CharAt(i - 1);
			if (c != " " && c != "\t")
				break;
			i--;
		}
		return Slice(i, n.m_Start);
	}

	// Start of the line of 'at' when only spaces and tabs stand before it on that line, else 'at'.
	int LineStart(int at)
	{
		int i = at;
		while (i > 0)
		{
			string c = CharAt(i - 1);
			if (c == "\n")
				return i;
			if (c != " " && c != "\t")
				return at;
			i--;
		}
		return i;
	}

	// After writing 'patches' (as DZS_XmlWriter.Apply did) the document follows the new text without reading it
	// again: positions move, elements inside replaced ranges go, elements in the new texts are parsed alone and
	// placed under their parent, start tags with changed attributes are read again.
	void Follow(array<ref DZS_XmlPatch> patches, array<string> newChunks)
	{
		array<DZS_XmlPatch> sorted = new array<DZS_XmlPatch>();
		foreach (DZS_XmlPatch p0 : patches)
		{
			if (p0.m_NewStart >= 0)
				sorted.Insert(p0);
		}
		m_PS = new array<int>();
		m_PE = new array<int>();
		m_PD = new array<int>();
		int cum = 0;
		foreach (DZS_XmlPatch p1 : sorted)
		{
			m_PS.Insert(p1.m_Start);
			m_PE.Insert(p1.m_End);
			cum += p1.m_Text.Length() - (p1.m_End - p1.m_Start);
			m_PD.Insert(cum);
		}
		if (m_Root)
			Shift(m_Root);
		m_PS = null;
		m_PE = null;
		m_PD = null;
		SetSource(newChunks);
		foreach (DZS_XmlPatch p : sorted)
		{
			if (p.m_TagOf)
				DZS_Xml.ReadTag(this, p.m_TagOf);
			if (!p.m_Parent || p.m_Text.IndexOf("<") < 0)
				continue;
			array<ref DZS_XmlNode> made = DZS_Xml.Fragment(this, p.m_NewStart, p.m_NewStart + p.m_Text.Length());
			p.m_Made = new array<DZS_XmlNode>();
			foreach (DZS_XmlNode n : made)
			{
				Place(p.m_Parent, n);
				p.m_Made.Insert(n);
			}
		}
	}

	protected ref array<int> m_PS;
	protected ref array<int> m_PE;
	protected ref array<int> m_PD;

	// Delta for a position: starts move with every patch that ends at or before them, ends only with those before.
	protected int Moved(int x, bool isEnd)
	{
		int lo = 0;
		int hi = m_PS.Count() - 1;
		int best = -1;
		while (lo <= hi)
		{
			int mid = (lo + hi) / 2;
			bool before = m_PE[mid] <= x;
			if (isEnd && m_PE[mid] == x && m_PS[mid] == m_PE[mid])
				before = false;
			if (before)
			{
				best = mid;
				lo = mid + 1;
			}
			else
				hi = mid - 1;
		}
		if (best < 0)
			return x;
		return x + m_PD[best];
	}

	protected bool Inside(DZS_XmlNode n)
	{
		for (int i = 0; i < m_PS.Count(); i++)
		{
			if (m_PS[i] >= n.m_End)
				return false;
			if (m_PE[i] > m_PS[i] && n.m_Start >= m_PS[i] && n.m_End <= m_PE[i])
				return true;
		}
		return false;
	}

	protected void Shift(DZS_XmlNode n)
	{
		int s0 = n.m_Start;
		n.m_LeadStart = Moved(n.m_LeadStart, true);
		n.m_Start = Moved(n.m_Start, false);
		n.m_OpenEnd = Moved(n.m_OpenEnd, true);
		n.m_End = Moved(n.m_End, true);
		foreach (DZS_XmlAttr a : n.m_Attrs)
		{
			a.m_LeadStart = Moved(a.m_LeadStart, false);
			a.m_ValueStart = Moved(a.m_ValueStart, false);
			a.m_ValueEnd = Moved(a.m_ValueEnd, true);
		}
		if (m_PS.Count() > 0 && n.m_Children.Count() > 0)
		{
			for (int i = n.m_Children.Count() - 1; i >= 0; i--)
			{
				DZS_XmlNode c = n.m_Children[i];
				if (Inside(c))
					n.m_Children.Remove(i);
			}
		}
		foreach (DZS_XmlNode k : n.m_Children)
			Shift(k);
	}

	protected void Place(DZS_XmlNode parent, DZS_XmlNode n)
	{
		int at = parent.m_Children.Count();
		while (at > 0 && parent.m_Children[at - 1].m_Start > n.m_Start)
			at--;
		if (at == parent.m_Children.Count())
			parent.m_Children.Insert(n);
		else
			parent.m_Children.InsertAt(n, at);
		// whitespace before a new element reaches back to the end of what precedes it, as when parsed
		int lead = n.m_LeadStart;
		while (lead > 0)
		{
			string c = CharAt(lead - 1);
			if (c != " " && c != "\t" && c != "\r" && c != "\n")
				break;
			lead--;
		}
		n.m_LeadStart = lead;
	}

	string ErrorText()
	{
		return m_Error + " (Zeile " + m_ErrLine + ", Spalte " + m_ErrCol + ")";
	}
}

class DZS_Xml
{
	protected int m_P;
	protected int m_N;
	protected DZS_XmlDoc m_Doc;

	static DZS_XmlDoc Parse(array<string> chunks)
	{
		DZS_XmlDoc doc = new DZS_XmlDoc();
		doc.SetSource(chunks);
		DZS_Xml p = new DZS_Xml();
		p.Run(doc);
		return doc;
	}

	protected void Run(DZS_XmlDoc doc)
	{
		m_Doc = doc;
		m_N = doc.Length();
		m_P = 0;
		if (m_N >= 3 && doc.ByteAt(0) == 239 && doc.ByteAt(1) == 187 && doc.ByteAt(2) == 191)
		{
			doc.m_Bom = true;
			m_P = 3;
		}
		int nl = doc.Find(0, "\n");
		if (nl > 0 && doc.CharAt(nl - 1) == "\r")
			doc.m_Eol = "\r\n";
		DZS_XmlNode top = new DZS_XmlNode();
		top.m_Name = "";
		if (!Content(top, m_P))
			return;
		foreach (DZS_XmlNode c : top.m_Children)
		{
			if (!doc.m_Root)
				doc.m_Root = c;
			else
			{
				Fail("mehr als ein Wurzelelement", c.m_Start);
				return;
			}
		}
		if (!doc.m_Root)
			Fail("kein Wurzelelement", m_N);
	}

	protected string Ch(int i)
	{
		return m_Doc.CharAt(i);
	}

	protected void Fail(string msg, int at)
	{
		if (m_Doc.m_Error != "")
			return;
		m_Doc.m_Error = msg;
		int line = 1;
		int lineStart = 0;
		int i = m_Doc.Find(0, "\n");
		while (i >= 0 && i < at)
		{
			line++;
			lineStart = i + 1;
			i = m_Doc.Find(i + 1, "\n");
		}
		m_Doc.m_ErrLine = line;
		m_Doc.m_ErrCol = at - lineStart + 1;
	}

	protected static bool IsSpace(string c)
	{
		return c == " " || c == "\t" || c == "\n" || c == "\r";
	}

	protected static bool IsNameEnd(string c)
	{
		return c == "" || IsSpace(c) || c == "=" || c == ">" || c == "/" || c == "?";
	}

	// Children of 'parent' until its end tag (or the end of the file for the top level).
	protected bool Content(DZS_XmlNode parent, int lead)
	{
		int lastEnd = lead;
		while (true)
		{
			int lt = m_Doc.Find(m_P, "<");
			if (lt < 0)
			{
				if (parent.m_Name != "")
				{
					Fail("</" + parent.m_Name + "> fehlt", m_N);
					return false;
				}
				m_P = m_N;
				return true;
			}
			m_P = lt;
			if (m_Doc.StartsAt(m_P, "<!--"))
			{
				int ce = m_Doc.FindStr(m_P + 4, "-->");
				if (ce < 0)
				{
					Fail("Kommentar ohne Ende", m_P);
					return false;
				}
				m_P = ce + 3;
				lastEnd = m_P;
				continue;
			}
			if (m_Doc.StartsAt(m_P, "<![CDATA["))
			{
				int de = m_Doc.FindStr(m_P, "]]>");
				if (de < 0)
				{
					Fail("CDATA ohne Ende", m_P);
					return false;
				}
				m_P = de + 3;
				continue;
			}
			string second = Ch(m_P + 1);
			if (second == "?" || second == "!")
			{
				int pe = m_Doc.Find(m_P, ">");
				if (pe < 0)
				{
					Fail("unvollständige Angabe", m_P);
					return false;
				}
				m_P = pe + 1;
				lastEnd = m_P;
				continue;
			}
			if (second == "/")
			{
				int ee = m_Doc.Find(m_P, ">");
				if (ee < 0)
				{
					Fail("unvollständiges End-Tag", m_P);
					return false;
				}
				string endName = m_Doc.Slice(m_P + 2, ee);
				endName.TrimInPlace();
				if (parent.m_Name == "" || endName != parent.m_Name)
				{
					Fail("</" + endName + "> passt nicht", m_P);
					return false;
				}
				m_P = ee + 1;
				parent.m_End = m_P;
				return true;
			}
			DZS_XmlNode n = Element(lastEnd);
			if (!n)
				return false;
			parent.m_Children.Insert(n);
			lastEnd = n.m_End;
		}
		return false;
	}

	protected DZS_XmlNode Element(int lead)
	{
		DZS_XmlNode n = new DZS_XmlNode();
		n.m_LeadStart = lead;
		n.m_Start = m_P;
		int i = m_P + 1;
		while (i < m_N && !IsNameEnd(Ch(i)))
			i++;
		n.m_Name = m_Doc.Slice(m_P + 1, i);
		if (n.m_Name == "")
		{
			Fail("Elementname fehlt", m_P);
			return null;
		}
		m_P = i;
		while (true)
		{
			int ws = m_P;
			while (m_P < m_N && IsSpace(Ch(m_P)))
				m_P++;
			if (m_P >= m_N)
			{
				Fail("Tag ohne Ende", n.m_Start);
				return null;
			}
			string c = Ch(m_P);
			if (c == ">")
			{
				m_P++;
				n.m_OpenEnd = m_P;
				if (!Content(n, m_P))
					return null;
				return n;
			}
			if (c == "/")
			{
				if (Ch(m_P + 1) != ">")
				{
					Fail("„/>“ erwartet", m_P);
					return null;
				}
				m_P += 2;
				n.m_OpenEnd = m_P;
				n.m_End = m_P;
				n.m_SelfClose = true;
				return n;
			}
			if (ws == m_P)
			{
				Fail("Leerzeichen vor dem Attribut erwartet", m_P);
				return null;
			}
			int ns = m_P;
			while (m_P < m_N && !IsNameEnd(Ch(m_P)))
				m_P++;
			DZS_XmlAttr a = new DZS_XmlAttr();
			a.m_Name = m_Doc.Slice(ns, m_P);
			a.m_LeadStart = ws;
			while (m_P < m_N && IsSpace(Ch(m_P)))
				m_P++;
			if (Ch(m_P) != "=")
			{
				Fail("„=“ erwartet", m_P);
				return null;
			}
			m_P++;
			while (m_P < m_N && IsSpace(Ch(m_P)))
				m_P++;
			string q = Ch(m_P);
			if (q != "\"" && q != "'")
			{
				Fail("Anführungszeichen erwartet", m_P);
				return null;
			}
			int qe = m_Doc.Find(m_P + 1, q);
			if (qe < 0)
			{
				Fail("Wert ohne Ende", m_P);
				return null;
			}
			a.m_ValueStart = m_P + 1;
			a.m_ValueEnd = qe;
			a.m_Raw = m_Doc.Slice(m_P + 1, qe);
			m_P = qe + 1;
			n.m_Attrs.Insert(a);
		}
		return null;
	}

	// Elements in [from, to) of a document (a text the editor wrote), with their positions in that document.
	static array<ref DZS_XmlNode> Fragment(DZS_XmlDoc doc, int from, int to)
	{
		DZS_Xml p = new DZS_Xml();
		p.m_Doc = doc;
		p.m_N = to;
		p.m_P = from;
		DZS_XmlNode top = new DZS_XmlNode();
		top.m_Name = "";
		string keepErr = doc.m_Error;
		p.Content(top, from);
		doc.m_Error = keepErr;
		array<ref DZS_XmlNode> r = new array<ref DZS_XmlNode>();
		foreach (DZS_XmlNode c : top.m_Children)
			r.Insert(c);
		return r;
	}

	// The attributes of an element's start tag read again (after its attributes changed).
	static void ReadTag(DZS_XmlDoc doc, DZS_XmlNode n)
	{
		DZS_Xml p = new DZS_Xml();
		p.m_Doc = doc;
		p.m_N = doc.Length();
		p.m_P = n.m_Start + 1 + n.m_Name.Length();
		n.m_Attrs.Clear();
		while (true)
		{
			int ws = p.m_P;
			while (p.m_P < p.m_N && IsSpace(p.Ch(p.m_P)))
				p.m_P++;
			string c = p.Ch(p.m_P);
			if (c == ">" || c == "/" || c == "")
			{
				if (c == ">")
					n.m_OpenEnd = p.m_P + 1;
				else if (c == "/")
					n.m_OpenEnd = p.m_P + 2;
				if (n.m_SelfClose)
					n.m_End = n.m_OpenEnd;
				return;
			}
			int ns = p.m_P;
			while (p.m_P < p.m_N && !IsNameEnd(p.Ch(p.m_P)))
				p.m_P++;
			DZS_XmlAttr a = new DZS_XmlAttr();
			a.m_Name = doc.Slice(ns, p.m_P);
			a.m_LeadStart = ws;
			int q = doc.Find(p.m_P, "\"");
			int qe = doc.Find(q + 1, "\"");
			if (q < 0 || qe < 0)
				return;
			a.m_ValueStart = q + 1;
			a.m_ValueEnd = qe;
			a.m_Raw = doc.Slice(q + 1, qe);
			p.m_P = qe + 1;
			n.m_Attrs.Insert(a);
		}
	}

	static string Unescape(string raw)
	{
		if (raw.IndexOf("&") < 0)
			return raw;
		string s = raw;
		s.Replace("&lt;", "<");
		s.Replace("&gt;", ">");
		s.Replace("&quot;", "\"");
		s.Replace("&apos;", "'");
		s.Replace("&amp;", "&");
		return s;
	}

	// CRLF to LF in slices: Replace on a text of 7500 bytes came back with 5106 (DayZ 1.30).
	static string ToLf(string text)
	{
		int n = text.Length();
		string res = "";
		int at = 0;
		while (at < n)
		{
			int take = Math.Min(1000, n - at);
			string part = text.Substring(at, take);
			if (at + take < n)
			{
				int lastIdx = take - 1;
				string last = part.Substring(lastIdx, 1);
				if (last == "\r")
				{
					take++;
					part = text.Substring(at, take);
				}
			}
			part.Replace("\r\n", "\n");
			res += part;
			at += take;
		}
		return res;
	}

	static string ToCrlf(string lfText)
	{
		int n = lfText.Length();
		string res = "";
		int at = 0;
		while (at < n)
		{
			int take = Math.Min(1000, n - at);
			string part = lfText.Substring(at, take);
			part.Replace("\n", "\r\n");
			res += part;
			at += take;
		}
		return res;
	}

	static string Escape(string text)
	{
		string s = text;
		s.Replace("&", "&amp;");
		s.Replace("<", "&lt;");
		s.Replace(">", "&gt;");
		s.Replace("\"", "&quot;");
		return s;
	}
}

// Replacements on a source text: [start, end) becomes text; everything between stays byte for byte.
class DZS_XmlPatch : Managed
{
	int m_Start;
	int m_End;
	string m_Text;
	// for following the document after writing: parent of the elements in m_Text, element whose start tag changes
	DZS_XmlNode m_Parent;
	DZS_XmlNode m_TagOf;
	string m_Tag;
	int m_NewStart;
	ref array<DZS_XmlNode> m_Made;
}

class DZS_XmlWriter
{
	static void Add(array<ref DZS_XmlPatch> list, int start, int end, string text)
	{
		DZS_XmlPatch p = new DZS_XmlPatch();
		p.m_Start = start;
		p.m_End = end;
		p.m_Text = text;
		list.Insert(p);
	}

	// The source with the patches applied, in chunks of at most 1000 bytes. Patches must not overlap; patches at the
	// same place keep their order.
	static array<string> Apply(DZS_XmlDoc src, array<ref DZS_XmlPatch> patches)
	{
		array<DZS_XmlPatch> sorted = Sorted(patches);
		DZS_TextWriter w = new DZS_TextWriter();
		int pos = 0;
		int shift = 0;
		foreach (DZS_XmlPatch p : sorted)
		{
			if (p.m_Start < pos)
			{
				p.m_NewStart = -1;
				continue;
			}
			Copy(w, src, pos, p.m_Start);
			p.m_NewStart = p.m_Start + shift;
			w.Append(p.m_Text);
			shift += p.m_Text.Length() - (p.m_End - p.m_Start);
			pos = p.m_End;
		}
		Copy(w, src, pos, src.Length());
		return w.Finish();
	}

	// By start; at the same place insertions before replacements, otherwise in the order given.
	static array<DZS_XmlPatch> Sorted(array<ref DZS_XmlPatch> patches)
	{
		array<DZS_XmlPatch> sorted = new array<DZS_XmlPatch>();
		foreach (DZS_XmlPatch p0 : patches)
		{
			int at = sorted.Count();
			while (at > 0 && After(sorted[at - 1], p0))
				at--;
			if (at == sorted.Count())
				sorted.Insert(p0);
			else
				sorted.InsertAt(p0, at);
		}
		return sorted;
	}

	protected static bool After(DZS_XmlPatch a, DZS_XmlPatch b)
	{
		if (a.m_Start != b.m_Start)
			return a.m_Start > b.m_Start;
		return a.m_End > a.m_Start && b.m_End == b.m_Start;
	}

	protected static void Copy(DZS_TextWriter w, DZS_XmlDoc src, int from, int to)
	{
		int at = from;
		while (at < to)
		{
			int take = Math.Min(1000, to - at);
			string piece = src.Slice(at, at + take);
			w.Append(piece);
			at += take;
		}
	}

	// float32 value as written by the editor: fixed point with six decimals, no exponent, no "-0.000000".
	static string Fixed6(float v)
	{
		string t = DZS_F32.ToText(v);
		bool neg = false;
		if (t.IndexOf("-") == 0)
		{
			neg = true;
			t = t.Substring(1, t.Length() - 1);
		}
		int dot = t.IndexOf(".");
		string ip = t;
		string fp = "";
		if (dot >= 0)
		{
			ip = t.Substring(0, dot);
			fp = t.Substring(dot + 1, t.Length() - dot - 1);
		}
		if (ip == "")
			ip = "0";
		if (fp.Length() <= 6)
		{
			while (fp.Length() < 6)
				fp += "0";
		}
		else
		{
			bool up = fp.Get(6).ToInt() >= 5;
			fp = fp.Substring(0, 6);
			if (up)
			{
				string digits = ip + fp;
				digits = AddOne(digits);
				ip = digits.Substring(0, digits.Length() - 6);
				fp = digits.Substring(digits.Length() - 6, 6);
			}
		}
		string r = ip + "." + fp;
		if (neg && r != "0.000000")
			r = "-" + r;
		return r;
	}

	protected static string AddOne(string digits)
	{
		string r = "";
		int carry = 1;
		for (int i = digits.Length() - 1; i >= 0; i--)
		{
			int d = digits.Get(i).ToInt() + carry;
			carry = 0;
			if (d == 10)
			{
				d = 0;
				carry = 1;
			}
			r = d.ToString() + r;
		}
		if (carry == 1)
			r = "1" + r;
		return r;
	}
}
