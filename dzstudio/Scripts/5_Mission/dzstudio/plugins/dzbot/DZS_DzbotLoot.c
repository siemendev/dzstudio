// Loot files of a dzbot mod: the plugin's own delta per
// target path (at most one delete and one merge_add), the base before it, the working state the editor edits
// (base minus the delete plus the merge_add), and the comparison form of the composed state for merged (Format ›
// comparison form).

// A file of the mod after the own delta on the same path.
class DZS_DzbotAfter : Managed
{
	string m_Key;
	int m_Rev;
	string m_Type;
	bool m_Template;
	ref array<string> m_Chunks;
}

// SHA-256 of a composed XML list in the comparison form, in slices: every element under the
// root on its own as Canonical XML without comments and without whitespace-only text, sorted by bytes, duplicates once,
// joined by \n. The files of the mod after the own delta are applied on the formed entries (merge_add appends, delete
// removes by its attributes, replace starts over); a template or a merge_match leaves the path without expectation.
class DZS_XmlCanon : Managed
{
	ref DZS_XmlDoc m_Doc;
	string m_Name;
	protected ref array<string> m_Entries = new array<string>();
	protected ref array<ref DZS_XmlNode> m_Nodes = new array<ref DZS_XmlNode>();
	protected ref array<ref DZS_XmlDoc> m_Docs = new array<ref DZS_XmlDoc>();
	protected ref array<bool> m_Gone = new array<bool>();
	protected ref map<string, ref array<int>> m_ByName = new map<string, ref array<int>>();
	protected ref array<ref DZS_DzbotAfter> m_After = new array<ref DZS_DzbotAfter>();
	protected int m_At;
	protected int m_Phase;
	protected ref array<string> m_Feed;
	protected ref DZS_Sha256 m_Hash;
	protected ref array<string> m_ReplaceChunks;
	string m_Sum;
	string m_Error;
	// why no expectation goes to dzbot for this path ("" = it does)
	string m_NoExpect;
	int m_Ms;

	// the file is parsed in the first steps (3 MB of loot files at a save would stop the game for seconds)
	protected ref DZS_XmlSteps m_Parse;

	void DZS_XmlCanon(string name, array<string> chunks)
	{
		m_Name = name;
		m_Parse = new DZS_XmlSteps(chunks);
	}

	void AddAfter(DZS_DzbotAfter a)
	{
		m_After.Insert(a);
		if (a.m_Template)
			m_NoExpect = "template";
		else if (a.m_Type == "merge_match")
			m_NoExpect = "merge_match";
	}

	bool Done() { return m_Sum != "" || m_Error != ""; }

	static bool IsBlank(string s)
	{
		int n = s.Length();
		for (int i = 0; i < n; i++)
		{
			string c = s.Get(i);
			if (c != " " && c != "\t" && c != "\r" && c != "\n")
				return false;
		}
		return true;
	}

	static string AttrText(string raw)
	{
		string v = raw;
		v.Replace("\r\n", " ");
		v.Replace("\t", " ");
		v.Replace("\r", " ");
		v.Replace("\n", " ");
		v = DZS_Xml.Unescape(v);
		v.Replace("&", "&amp;");
		v.Replace("<", "&lt;");
		v.Replace("\"", "&quot;");
		return v;
	}

	static string TextOut(string t, bool cdata)
	{
		if (IsBlank(t))
			return "";
		string v = t;
		v.Replace("\r\n", "\n");
		v.Replace("\r", "\n");
		if (!cdata)
			v = DZS_Xml.Unescape(v);
		v.Replace("&", "&amp;");
		v.Replace("<", "&lt;");
		v.Replace(">", "&gt;");
		return v;
	}

	// Text and CDATA between from and to of a document, comments and processing instructions left out.
	static string TextIn(DZS_XmlDoc d, int from, int to)
	{
		string s = "";
		int at = from;
		while (at < to)
		{
			int lt = d.Find(at, "<");
			if (lt < 0 || lt > to)
				lt = to;
			if (lt > at)
			{
				string seg = d.Slice(at, lt);
				s += TextOut(seg, false);
			}
			if (lt >= to)
				break;
			if (d.StartsAt(lt, "<!--"))
			{
				int ce = d.FindStr(lt + 4, "-->");
				if (ce < 0)
					break;
				at = ce + 3;
			}
			else if (d.StartsAt(lt, "<![CDATA["))
			{
				int de = d.FindStr(lt + 9, "]]>");
				if (de < 0)
					break;
				string cd = d.Slice(lt + 9, de);
				s += TextOut(cd, true);
				at = de + 3;
			}
			else if (d.StartsAt(lt, "<?"))
			{
				int pe = d.FindStr(lt + 2, "?>");
				if (pe < 0)
					break;
				at = pe + 2;
			}
			else
				break;
		}
		return s;
	}

	static int CloseStart(DZS_XmlDoc d, DZS_XmlNode n)
	{
		int i = n.m_End - 1;
		while (i > n.m_OpenEnd)
		{
			if (d.CharAt(i) == "<")
				return i;
			i--;
		}
		return n.m_OpenEnd;
	}

	static string Form(DZS_XmlDoc d, DZS_XmlNode n)
	{
		array<string> names = new array<string>();
		map<string, string> vals = new map<string, string>();
		foreach (DZS_XmlAttr a : n.m_Attrs)
		{
			names.Insert(a.m_Name);
			string av = AttrText(a.m_Raw);
			vals.Set(a.m_Name, av);
		}
		names.Sort();
		string s = "<" + n.m_Name;
		foreach (string an : names)
		{
			string v = vals.Get(an);
			s += " " + an + "=\"" + v + "\"";
		}
		s += ">";
		if (!n.m_SelfClose)
		{
			int from = n.m_OpenEnd;
			foreach (DZS_XmlNode c : n.m_Children)
			{
				s += TextIn(d, from, c.m_Start);
				string cf = Form(d, c);
				s += cf;
				from = c.m_End;
			}
			int close = CloseStart(d, n);
			s += TextIn(d, from, close);
		}
		s += "</" + n.m_Name + ">";
		return s;
	}

	protected void Take(DZS_XmlDoc d, DZS_XmlNode e)
	{
		string f = Form(d, e);
		int idx = m_Entries.Count();
		m_Entries.Insert(f);
		m_Nodes.Insert(e);
		m_Gone.Insert(false);
		string nm = e.Get("name");
		array<int> list = m_ByName.Get(nm);
		if (!list)
		{
			list = new array<int>();
			m_ByName.Set(nm, list);
		}
		list.Insert(idx);
	}

	protected void Clear()
	{
		m_Entries.Clear();
		m_Nodes.Clear();
		m_Gone.Clear();
		m_ByName.Clear();
	}

	protected static bool Matches(DZS_XmlNode e, DZS_XmlNode d)
	{
		if (e.m_Name != d.m_Name)
			return false;
		foreach (DZS_XmlAttr a : d.m_Attrs)
		{
			DZS_XmlAttr ea = e.Attr(a.m_Name);
			if (!ea || ea.Value() != a.Value())
				return false;
		}
		return true;
	}

	protected void ApplyAfter()
	{
		m_ReplaceChunks = null;
		foreach (DZS_DzbotAfter a : m_After)
		{
			m_ReplaceChunks = null;
			bool blank = true;
			foreach (string c : a.m_Chunks)
			{
				if (!IsBlank(c))
					blank = false;
			}
			if (a.m_Type == "delete" && blank)
			{
				Clear();
				continue;
			}
			if (blank)
			{
				if (a.m_Type == "replace")
					Clear();
				continue;
			}
			DZS_XmlDoc d = DZS_Xml.Parse(a.m_Chunks);
			if (d.m_Error != "" || !d.m_Root)
			{
				m_NoExpect = "after " + a.m_Key + ": " + d.ErrorText();
				return;
			}
			m_Docs.Insert(d);
			if (a.m_Type == "replace")
			{
				Clear();
				foreach (DZS_XmlNode r : d.m_Root.m_Children)
					Take(d, r);
				m_ReplaceChunks = a.m_Chunks;
			}
			else if (a.m_Type == "merge_add")
			{
				foreach (DZS_XmlNode m : d.m_Root.m_Children)
					Take(d, m);
			}
			else if (a.m_Type == "delete")
			{
				foreach (DZS_XmlNode x : d.m_Root.m_Children)
				{
					string xn = x.Get("name");
					array<int> cand = m_ByName.Get(xn);
					if (!cand)
						continue;
					foreach (int ci : cand)
					{
						if (!m_Gone[ci] && Matches(m_Nodes[ci], x))
							m_Gone[ci] = true;
					}
				}
			}
		}
	}

	// Some work within ms milliseconds; true when finished.
	bool Step(int ms)
	{
		if (Done())
			return true;
		int t0 = TickCount(0);
		if (m_Parse)
		{
			bool parsed = m_Parse.Step(t0, ms);
			m_Ms += (TickCount(0) - t0) / 10000;
			if (!parsed)
				return false;
			m_Doc = m_Parse.Result();
			m_Parse = null;
			DZS_Log.Info("dzbot: " + m_Name + " parsed in steps in " + m_Ms + " ms");
			if (m_Doc.m_Error != "")
			{
				m_Error = m_Doc.ErrorText();
				return true;
			}
			return false;
		}
		if (m_Phase == 0)
		{
			array<ref DZS_XmlNode> top = m_Doc.m_Root.m_Children;
			while (m_At < top.Count())
			{
				DZS_XmlNode e = top[m_At];
				Take(m_Doc, e);
				m_At++;
				if ((TickCount(0) - t0) / 10000 >= ms)
					return false;
			}
			ApplyAfter();
			m_Hash = new DZS_Sha256();
			m_Feed = new array<string>();
			m_Phase = 1;
			if (m_ReplaceChunks)
			{
				foreach (string rc : m_ReplaceChunks)
					m_Feed.Insert(rc);
				Clear();
				return false;
			}
			array<string> live = new array<string>();
			for (int i = 0; i < m_Entries.Count(); i++)
			{
				if (!m_Gone[i])
					live.Insert(m_Entries[i]);
			}
			Clear();
			live.Sort();
			string last = "";
			bool any = false;
			foreach (string en : live)
			{
				if (any && en == last)
					continue;
				if (any)
					m_Feed.Insert("\n");
				Pieces(en, m_Feed);
				last = en;
				any = true;
			}
			return false;
		}
		int left = ms - (TickCount(0) - t0) / 10000;
		if (left < 1)
			left = 1;
		if (!m_Hash.Feed(m_Feed, left))
			return false;
		m_Sum = m_Hash.Finish();
		m_Feed = null;
		m_Docs.Clear();
		return true;
	}

	static void Pieces(string s, array<string> into)
	{
		int n = s.Length();
		int at = 0;
		while (at < n)
		{
			int take = Math.Min(DZS_DzbotSection.PIECE, n - at);
			string p = s.Substring(at, take);
			into.Insert(p);
			at += take;
		}
	}

	// Developer check (-dzsShaTest): sort order and forms against values computed outside the game.
	static void SelfTest()
	{
		array<string> s = {"b", "B", "a", "_", "Z", "aa", "a b"};
		s.Sort();
		string joined = "";
		foreach (string x : s)
			joined += "[" + x + "]";
		DZS_Log.Info("canon: sort " + joined + " (bytes: [B][Z][_][a][a b][aa][b])");
		array<string> c = {"<?xml version=\"1.0\"?>\n<prototype>\n\t<group x=\"1\" name=\"A\">\n\t\t<usage name=\"F\" />\n\t\t<!-- c -->\n\t</group>\n\t<group name=\"B\"/>\n\t<t a=\"1\"> x &amp; y </t>\n\t<t><![CDATA[ \n ]]></t>\n</prototype>\n"};
		DZS_XmlCanon k = new DZS_XmlCanon("test", c);
		while (!k.Step(50))
		{
		}
		DZS_Log.Info("canon: test " + k.m_Sum + " (want c536e72e15228906e6ba6cdf39fb98b07ed37373ef80100ea98cb834565d5d81)");
		string big = "";
		for (int b = 0; b < 100; b++)
			big += "\t\t\t\t\t\t<point pos=\"1.000000 2.000000 3.000000\" range=\"0.5\" height=\"1.2\" />\r\n";
		int before = big.Length();
		string viaReplace = big;
		viaReplace.Replace("\r\n", "\n");
		string viaLf = DZS_DzbotLoot.Lf(big);
		DZS_Log.Info("canon: replace on " + before + " bytes -> " + viaReplace.Length() + " (want " + (before - 100) + "), Lf -> " + viaLf.Length());
	}
}

// One own delta file of the plugin on a loot path.
class DZS_DzbotLootFile : Managed
{
	string m_Key;
	string m_Path;
	string m_Type;
	int m_Rev;
}

// What the plugin holds of the loot of an open mod.
class DZS_DzbotLootState : Managed
{
	ref DZS_LootOrigin m_Origin = new DZS_LootOrigin();
	// files of the mod after the own delta, per path
	ref map<string, ref array<ref DZS_DzbotAfter>> m_After = new map<string, ref array<ref DZS_DzbotAfter>>();
	ref array<ref DZS_DzbotLootFile> m_Files = new array<ref DZS_DzbotLootFile>();
	// own rows of mapgrouppos.xml: key "name\npos" -> row text, in order
	ref array<string> m_RowKeys = new array<string>();
	ref map<string, string> m_RowText = new map<string, string>();
	// rows of the base the delta removes: key -> name, pos (raw)
	ref array<string> m_DelKeys = new array<string>();
	// pending map of a save, taken over when dzbot wrote it
	ref array<string> m_NextRowKeys;
	ref map<string, string> m_NextRowText;
	ref array<string> m_NextDelKeys;
	ref array<string> m_NextTypes;
	ref array<string> m_NextOrigs;

	DZS_DzbotLootFile Own(string path, string type)
	{
		foreach (DZS_DzbotLootFile f : m_Files)
		{
			if (f.m_Path == path && f.m_Type == type)
				return f;
		}
		return null;
	}

	DZS_DzbotLootFile ByKey(string key)
	{
		foreach (DZS_DzbotLootFile f : m_Files)
		{
			if (f.m_Key == key)
				return f;
		}
		return null;
	}

	// The key handed to "Fetch base": the first own file on the path (the delete stands before the merge_add).
	string BaseKey(string path)
	{
		DZS_DzbotLootFile d = Own(path, "delete");
		if (d)
			return d.m_Key;
		DZS_DzbotLootFile a = Own(path, "merge_add");
		if (a)
			return a.m_Key;
		return "";
	}
}

class DZS_DzbotLoot
{
	static const string HEAD = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

	static string RowKey(string nameRaw, string posRaw)
	{
		return nameRaw + "\n" + posRaw;
	}

	// name and pos of a single row text, raw (an element of mapgrouppos.xml).
	static bool RowOfText(string text, out string nameRaw, out string posRaw)
	{
		nameRaw = "";
		posRaw = "";
		array<string> one = {text};
		DZS_XmlDoc d = DZS_Xml.Parse(one);
		if (d.m_Error != "" || !d.m_Root)
			return false;
		DZS_XmlAttr an = d.m_Root.Attr("name");
		DZS_XmlAttr ap = d.m_Root.Attr("pos");
		if (!an || !ap)
			return false;
		nameRaw = an.m_Raw;
		posRaw = ap.m_Raw;
		return true;
	}

	static string Lf(string text)
	{
		return DZS_Xml.ToLf(text);
	}

	static string Crlf(string lfText)
	{
		return DZS_Xml.ToCrlf(lfText);
	}

	// Working state of mapgroupproto.xml: the base without the entries of the own delete, with the entries of the own
	// merge_add appended; the own types with their text in the base ("" = none there).
	static array<string> ComposeProto(array<string> baseChunks, array<string> delChunks, array<string> addChunks, DZS_DzbotLootState st)
	{
		st.m_Origin.m_OwnTypes.Clear();
		st.m_Origin.m_Origs.Clear();
		if (!delChunks && !addChunks)
			return baseChunks;
		DZS_XmlDoc doc = DZS_Xml.Parse(baseChunks);
		if (doc.m_Error != "")
			return null;
		map<string, string> baseText = new map<string, string>();
		foreach (DZS_XmlNode g : doc.m_Root.m_Children)
		{
			string gn = g.Get("name");
			if (g.m_Name == "group" && gn != "" && !baseText.Contains(gn))
			{
				string raw = doc.Raw(g);
				baseText.Set(gn, raw);
			}
		}
		map<string, bool> gone = new map<string, bool>();
		if (delChunks)
		{
			DZS_XmlDoc dd = DZS_Xml.Parse(delChunks);
			if (dd.m_Error == "")
			{
				foreach (DZS_XmlNode d : dd.m_Root.m_Children)
				{
					string dn = d.Get("name");
					gone.Set(dn, true);
				}
			}
		}
		array<ref DZS_XmlPatch> patches = new array<ref DZS_XmlPatch>();
		string ind = "\t";
		bool indSet = false;
		foreach (DZS_XmlNode g2 : doc.m_Root.m_Children)
		{
			if (g2.m_Name == "group" && !indSet)
			{
				ind = doc.IndentOf(g2);
				indSet = true;
			}
			string g2n = g2.Get("name");
			if (g2.m_Name == "group" && gone.Contains(g2n))
				DZS_XmlWriter.Add(patches, g2.m_LeadStart, g2.m_End, "");
		}
		string added = "";
		if (addChunks)
		{
			DZS_XmlDoc ad = DZS_Xml.Parse(addChunks);
			if (ad.m_Error == "")
			{
				foreach (DZS_XmlNode a : ad.m_Root.m_Children)
				{
					string an = a.Get("name");
					if (a.m_Name != "group" || an == "")
						continue;
					string at = ad.Raw(a);
					string text = Lf(at);
					if (doc.m_Eol == "\r\n")
						text = Crlf(text);
					added += ind + text + doc.m_Eol;
					string og = "";
					baseText.Find(an, og);
					st.m_Origin.m_OwnTypes.Insert(an);
					st.m_Origin.m_Origs.Insert(og);
				}
			}
		}
		if (added != "")
		{
			string closeTag = "</" + doc.m_Root.m_Name + ">";
			int endAt = doc.LineStart(doc.m_Root.m_End - closeTag.Length());
			DZS_XmlWriter.Add(patches, endAt, endAt, added);
		}
		return DZS_XmlWriter.Apply(doc, patches);
	}

	// Working state of mapgrouppos.xml: base rows without those the own delete names (name + pos), own rows appended.
	static array<string> ComposePos(array<string> baseChunks, array<string> delChunks, array<string> addChunks, DZS_DzbotLootState st)
	{
		st.m_Origin.m_OwnRows.Clear();
		st.m_RowKeys.Clear();
		st.m_RowText.Clear();
		st.m_DelKeys.Clear();
		if (!delChunks && !addChunks)
			return baseChunks;
		DZS_XmlDoc doc = DZS_Xml.Parse(baseChunks);
		if (doc.m_Error != "")
			return null;
		map<string, bool> gone = new map<string, bool>();
		if (delChunks)
		{
			DZS_XmlDoc dd = DZS_Xml.Parse(delChunks);
			if (dd.m_Error == "")
			{
				foreach (DZS_XmlNode d : dd.m_Root.m_Children)
				{
					DZS_XmlAttr dn = d.Attr("name");
					DZS_XmlAttr dp = d.Attr("pos");
					if (!dn || !dp)
						continue;
					string dk = RowKey(dn.m_Raw, dp.m_Raw);
					if (!gone.Contains(dk))
						st.m_DelKeys.Insert(dk);
					gone.Set(dk, true);
				}
			}
		}
		array<ref DZS_XmlPatch> patches = new array<ref DZS_XmlPatch>();
		string ind = "    ";
		bool indSet = false;
		foreach (DZS_XmlNode g : doc.m_Root.m_Children)
		{
			if (!indSet)
			{
				ind = doc.IndentOf(g);
				indSet = true;
			}
			DZS_XmlAttr gn = g.Attr("name");
			DZS_XmlAttr gp = g.Attr("pos");
			if (!gn || !gp)
				continue;
			string gk = RowKey(gn.m_Raw, gp.m_Raw);
			if (gone.Contains(gk))
				DZS_XmlWriter.Add(patches, g.m_LeadStart, g.m_End, "");
		}
		string added = "";
		if (addChunks)
		{
			DZS_XmlDoc ad = DZS_Xml.Parse(addChunks);
			if (ad.m_Error == "")
			{
				foreach (DZS_XmlNode a : ad.m_Root.m_Children)
				{
					DZS_XmlAttr an = a.Attr("name");
					DZS_XmlAttr ap = a.Attr("pos");
					if (!an || !ap)
						continue;
					string text = ad.Raw(a);
					string ak = RowKey(an.m_Raw, ap.m_Raw);
					added += ind + text + doc.m_Eol;
					if (!st.m_RowText.Contains(ak))
						st.m_RowKeys.Insert(ak);
					st.m_RowText.Set(ak, text);
					st.m_Origin.m_OwnRows.Set(ak, true);
				}
			}
		}
		if (added != "")
		{
			string closeTag = "</" + doc.m_Root.m_Name + ">";
			int endAt = doc.LineStart(doc.m_Root.m_End - closeTag.Length());
			DZS_XmlWriter.Add(patches, endAt, endAt, added);
		}
		return DZS_XmlWriter.Apply(doc, patches);
	}

	// The delta files of a save. proto: the written types (names, originals "" = none in the base) with their text in the
	// written working state; pos: the own rows and the base rows removed, after the change list of the loot plugin.
	static void ProtoDelta(DZS_XmlDoc written, array<string> names, array<string> origs, out array<string> del, out array<string> add)
	{
		DZS_TextWriter wd = new DZS_TextWriter();
		DZS_TextWriter wa = new DZS_TextWriter();
		int nd = 0;
		int na = 0;
		map<string, string> text = new map<string, string>();
		foreach (DZS_XmlNode g : written.m_Root.m_Children)
		{
			string gn = g.Get("name");
			if (g.m_Name == "group" && gn != "")
			{
				string low = gn;
				low.ToLower();
				if (!text.Contains(low))
				{
					string raw = written.Raw(g);
					text.Set(low, raw);
				}
			}
		}
		for (int i = 0; i < names.Count(); i++)
		{
			string n = names[i];
			string k = n;
			k.ToLower();
			string t;
			if (!text.Find(k, t))
				continue;
			if (origs[i] != "")
			{
				if (nd == 0)
					wd.Append(HEAD + "<prototype>\n");
				string qn = DZS_Xml.Escape(n);
				wd.Append("\t<group name=\"" + qn + "\"/>\n");
				nd++;
			}
			if (na == 0)
				wa.Append(HEAD + "<prototype>\n");
			string lt = Lf(t);
			wa.Append("\t" + lt + "\n");
			na++;
		}
		del = null;
		add = null;
		if (nd > 0)
		{
			wd.Append("</prototype>\n");
			del = wd.Finish();
		}
		if (na > 0)
		{
			wa.Append("</prototype>\n");
			add = wa.Finish();
		}
	}

	static void PosDelta(DZS_DzbotLootState st, array<ref DZS_LootChange> changes, out array<string> del, out array<string> add)
	{
		array<string> keys = new array<string>();
		map<string, string> rows = new map<string, string>();
		foreach (string k0 : st.m_RowKeys)
		{
			keys.Insert(k0);
			string t0 = st.m_RowText.Get(k0);
			rows.Set(k0, t0);
		}
		array<string> dels = new array<string>();
		foreach (string d0 : st.m_DelKeys)
			dels.Insert(d0);
		foreach (DZS_LootChange c : changes)
		{
			if (c.m_File != DZS_LootFiles.POS)
				continue;
			string nm, ps;
			if (!RowOfText(c.m_Text, nm, ps))
				continue;
			string key = RowKey(nm, ps);
			if (c.m_Kind == "removed")
			{
				if (rows.Contains(key))
				{
					rows.Remove(key);
					keys.RemoveItem(key);
				}
				else if (dels.Find(key) < 0)
					dels.Insert(key);
			}
			else if (c.m_Kind == "added")
			{
				if (!rows.Contains(key))
					keys.Insert(key);
				string lt = Lf(c.m_Text);
				rows.Set(key, lt);
			}
		}
		st.m_NextRowKeys = keys;
		st.m_NextRowText = rows;
		st.m_NextDelKeys = dels;
		del = null;
		add = null;
		if (dels.Count() > 0)
		{
			DZS_TextWriter wd = new DZS_TextWriter();
			wd.Append(HEAD + "<map>\n");
			foreach (string dk : dels)
			{
				int nl = dk.IndexOf("\n");
				string dn = dk.Substring(0, nl);
				string dp = dk.Substring(nl + 1, dk.Length() - nl - 1);
				wd.Append("\t<group name=\"" + dn + "\" pos=\"" + dp + "\"/>\n");
			}
			wd.Append("</map>\n");
			del = wd.Finish();
		}
		if (keys.Count() > 0)
		{
			DZS_TextWriter wa = new DZS_TextWriter();
			wa.Append(HEAD + "<map>\n");
			foreach (string ak : keys)
			{
				string at = rows.Get(ak);
				wa.Append("\t" + at + "\n");
			}
			wa.Append("</map>\n");
			add = wa.Finish();
		}
	}
}
