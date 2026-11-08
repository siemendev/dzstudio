// What a key found: the elements, or none / ambiguous, and whether a name only matched in another case.
class DZS_PTarget : Managed
{
	ref array<DZS_XmlNode> m_Nodes = new array<DZS_XmlNode>();
	bool m_Ambiguous;
	bool m_Case;
	bool m_BadKey;
}

// An own XML block waiting to be written: its values and the texts of own children that go inside it.
class DZS_POwn : Managed
{
	DZS_PatchBlock m_Block;
	DZS_PatchEntry m_Entry;
	DZS_PKind m_Kind;
	ref map<string, ref DZS_Json> m_Values;
	string m_Name;
	ref array<ref DZS_POwn> m_Kids = new array<ref DZS_POwn>();
	// the base element it goes into; null at the top level of its file or inside another own block
	DZS_XmlNode m_Into;
	// the file it goes into (a kind's own file, or the territory file its key names)
	string m_File;
	bool m_Ok;
}

// The XML block kinds: resolving keys, reading and setting fields, and writing own blocks complete.
class DZS_PatcherXml : DZS_PatcherBase
{
	// ---- keys ----

	// Elements of a kind in a file that match a key; 'all' keeps every match (remove), else the per-file rule picks.
	DZS_PTarget Resolve(DZS_PatchFile f, DZS_PKind k, DZS_Json key, bool all)
	{
		DZS_PTarget t = new DZS_PTarget();
		array<DZS_XmlNode> pool = new array<DZS_XmlNode>();
		DZS_XmlNode root = f.m_Xml.m_Root;
		if (!root)
			return t;
		if (k.m_Singleton)
		{
			if (root.m_Name == k.m_Elem)
				t.m_Nodes.Insert(root);
			return t;
		}
		if (k.m_ParentKind == "")
		{
			if (root.m_Name != k.m_Root)
				return t;
			if (k.m_KeyAttr != "")
			{
				string top = KeyStr(key, k.m_KeyAttr);
				if (top == "")
				{
					t.m_BadKey = true;
					return t;
				}
				pool.Copy(f.Named(root, k.m_Elem, k.m_KeyAttr, DZS_PatchText.Lower(top)));
			}
			else
			{
				foreach (DZS_XmlNode c : root.m_Children)
				{
					if (c.m_Name == k.m_Elem)
						pool.Insert(c);
				}
			}
		}
		else
		{
			DZS_PKind pk = DZS_PKinds.Get(k.m_ParentKind);
			string pname;
			DZS_Json pkey = ParentKeyOf(k, pk, key, pname);
			if (pname == "" && k.m_AnyParent)
			{
				if (root.m_Name != pk.m_Root)
					return t;
				foreach (DZS_XmlNode anyParent : root.m_Children)
				{
					if (anyParent.m_Name == pk.m_Elem)
						Members(f, anyParent, k, pool, true);
				}
				Filter(f, k, key, pool, t);
				return Narrow(f, k, key, t, all);
			}
			if (pname == "")
			{
				t.m_BadKey = true;
				return t;
			}
			DZS_PTarget pt = Resolve(f, pk, pkey, pk.m_Dups == DZS_PKind.MERGE);
			t.m_Case = pt.m_Case;
			if (pt.m_BadKey)
			{
				t.m_BadKey = true;
				return t;
			}
			if (pt.m_Ambiguous)
			{
				t.m_Ambiguous = true;
				return t;
			}
			foreach (DZS_XmlNode p : pt.m_Nodes)
				Members(f, p, k, pool, true);
		}
		Filter(f, k, key, pool, t);
		return Narrow(f, k, key, t, all);
	}

	// The matches by ordinal, without removed elements, and by the per-file rule for equal keys.
	protected DZS_PTarget Narrow(DZS_PatchFile f, DZS_PKind k, DZS_Json key, DZS_PTarget t, bool all)
	{
		if (t.m_BadKey)
			return t;
		// ordinals count the base's elements, also those an earlier change removed
		string ord = KeyStr(key, "ordinal");
		if (ord != "")
		{
			int n = ord.ToInt();
			DZS_XmlNode pick = null;
			if (n >= 1 && n <= t.m_Nodes.Count())
				pick = t.m_Nodes[n - 1];
			t.m_Nodes.Clear();
			if (pick && !f.IsRemoved(pick))
				t.m_Nodes.Insert(pick);
			return t;
		}
		for (int r = t.m_Nodes.Count() - 1; r >= 0; r--)
		{
			if (f.IsRemoved(t.m_Nodes[r]))
				t.m_Nodes.Remove(r);
		}
		if (t.m_Nodes.Count() <= 1)
			return t;
		if (k.m_Dups == DZS_PKind.MERGE || (all && (k.m_Dups == DZS_PKind.LAST || k.m_Dups == DZS_PKind.FIRST || k.m_RemoveAll)))
			return t;
		if (k.m_Dups == DZS_PKind.LAST)
		{
			DZS_XmlNode last = t.m_Nodes[t.m_Nodes.Count() - 1];
			t.m_Nodes.Clear();
			t.m_Nodes.Insert(last);
			return t;
		}
		if (k.m_Dups == DZS_PKind.FIRST)
		{
			DZS_XmlNode first = t.m_Nodes[0];
			t.m_Nodes.Clear();
			t.m_Nodes.Insert(first);
			return t;
		}
		t.m_Ambiguous = true;
		return t;
	}

	// The key of the parent block named in a key of kind k (or in a block's parent key); 'first' is "" when the key
	// names no parent.
	static DZS_Json ParentKeyOf(DZS_PKind k, DZS_PKind pk, DZS_Json key, out string first)
	{
		first = "";
		DZS_Json pkey = DZS_Json.Obj();
		array<string> pairs = new array<string>();
		pairs.Copy(k.m_ParentMap);
		if (pairs.Count() == 0)
			pairs.Insert(k.m_ParentKey + "=" + pk.m_KeyAttr);
		for (int i = 0; i < pairs.Count(); i++)
		{
			string pair = pairs[i];
			int eq = pair.IndexOf("=");
			string from = pair.Substring(0, eq);
			string to = pair.Substring(eq + 1, pair.Length() - eq - 1);
			string v = KeyStr(key, from);
			if (i == 0)
				first = v;
			if (v == "")
				continue;
			DZS_Json vj = DZS_Json.Str(v);
			pkey.Set(to, vj);
		}
		return pkey;
	}

	// Elements of kind k directly inside a parent element (through its container element, if the kind has one).
	void Members(DZS_PatchFile f, DZS_XmlNode parent, DZS_PKind k, array<DZS_XmlNode> pool, bool withRemoved = false)
	{
		foreach (DZS_XmlNode c : parent.m_Children)
		{
			if (k.m_Container == "")
			{
				if (c.m_Name == k.m_Elem && (withRemoved || !f.IsRemoved(c)))
					pool.Insert(c);
			}
			else if (c.m_Name == k.m_Container && !f.IsRemoved(c))
			{
				foreach (DZS_XmlNode cc : c.m_Children)
				{
					if (cc.m_Name == k.m_Elem && (withRemoved || !f.IsRemoved(cc)))
						pool.Insert(cc);
				}
			}
		}
	}

	protected void Filter(DZS_PatchFile f, DZS_PKind k, DZS_Json key, array<DZS_XmlNode> pool, DZS_PTarget t)
	{
		string name = "";
		bool useName = k.m_KeyAttr != "";
		if (useName)
		{
			name = KeyStr(key, k.m_KeyAttr);
			if (name == "")
			{
				t.m_BadKey = true;
				return;
			}
		}
		string kx = "";
		string kz = "";
		if (k.IsCoord())
		{
			kx = KeyStr(key, "x");
			kz = KeyStr(key, "z");
			if (!DZS_PatchNum.IsNumber(kx) || !DZS_PatchNum.IsNumber(kz))
			{
				t.m_BadKey = true;
				return;
			}
		}
		array<string> kv = new array<string>();
		if (k.IsVec())
		{
			DZS_Json kvj = null;
			if (key && key.IsObj())
				kvj = key.Get(k.m_Vec);
			if (!VecOf(kvj, kv))
			{
				t.m_BadKey = true;
				return;
			}
		}
		string low = DZS_PatchText.Lower(name);
		bool caseOnly = false;
		foreach (DZS_XmlNode n : pool)
		{
			if (useName)
			{
				string v = n.Get(k.m_KeyAttr);
				if (k.m_KeyCi)
				{
					if (DZS_PatchText.Lower(v) != low)
						continue;
				}
				else if (v != name)
				{
					if (DZS_PatchText.Lower(v) == low)
						caseOnly = true;
					continue;
				}
			}
			if (k.IsCoord())
			{
				string nx = n.Get("x");
				string nz = n.Get("z");
				if (!DZS_PatchNum.Near(nx, kx, k.m_Tol) || !DZS_PatchNum.Near(nz, kz, k.m_Tol))
					continue;
			}
			if (k.IsVec())
			{
				array<string> nv = f.VecAt(n, k.m_Vec);
				if (!nv || !NearVec(nv, kv, k.m_Tol))
					continue;
			}
			t.m_Nodes.Insert(n);
		}
		if (t.m_Nodes.Count() == 0 && caseOnly)
			t.m_Case = true;
	}

	// ---- fields ----

	// The element at a field's path below n (first one not removed), down to 'depth' elements; null when missing.
	DZS_XmlNode Walk(DZS_PatchFile f, DZS_XmlNode n, DZS_PField fld, int depth)
	{
		DZS_XmlNode cur = n;
		for (int i = 0; i < depth; i++)
		{
			DZS_XmlNode next = null;
			foreach (DZS_XmlNode c : cur.m_Children)
			{
				if (c.m_Name == fld.m_Elems[i] && !f.IsRemoved(c))
				{
					next = c;
					break;
				}
			}
			if (!next)
				return null;
			cur = next;
		}
		return cur;
	}

	// Current value of a scalar field; has = false when absent.
	string Current(DZS_PatchFile f, DZS_XmlNode n, DZS_PField fld, out bool has)
	{
		has = false;
		DZS_XmlNode at = Walk(f, n, fld, fld.m_Elems.Count());
		if (!at)
			return "";
		if (fld.m_Attr != "")
			return f.Attr(at, fld.m_Attr, has);
		has = true;
		return f.Text(at);
	}

	// Current members of a list field.
	void CurrentList(DZS_PatchFile f, DZS_XmlNode n, DZS_PField fld, array<string> tokens)
	{
		DZS_XmlNode parent = Walk(f, n, fld, fld.m_Elems.Count() - 1);
		if (!parent)
			return;
		DZS_PList listed = f.m_Lists.Get(ListKey(n, fld));
		if (listed)
		{
			tokens.Copy(listed.m_Tokens);
			return;
		}
		string elem = fld.m_Elems[fld.m_Elems.Count() - 1];
		foreach (DZS_XmlNode c : parent.m_Children)
		{
			if (c.m_Name != elem || f.IsRemoved(c))
				continue;
			if (c.Has("user"))
				tokens.Insert("user=" + c.Get("user"));
			else
				tokens.Insert("name=" + c.Get("name"));
		}
	}

	// Attribute order for an element at a field path: the declared order of the fields that sit on it.
	array<string> AttrOrder(DZS_PKind k, DZS_PField fld)
	{
		if (fld.m_Elems.Count() == 0)
			return k.m_AttrOrder;
		array<string> r = new array<string>();
		string here = fld.ElemPath(99);
		foreach (DZS_PField o : k.m_Fields)
		{
			if (o.m_Attr != "" && o.ElemPath(99) == here)
				r.Insert(o.m_Attr);
		}
		return r;
	}

	// Inserts a new child element where the kind's order puts it among the existing children.
	void InsertOrdered(DZS_PatchFile f, DZS_PKind k, DZS_XmlNode parent, string name, string text)
	{
		int mine = k.m_Order.Find(name);
		DZS_XmlNode after = null;
		bool any = false;
		foreach (DZS_XmlNode c : parent.m_Children)
		{
			if (f.IsRemoved(c))
				continue;
			any = true;
			int o = k.m_Order.Find(c.m_Name);
			if (mine >= 0 && o >= 0 && o <= mine)
				after = c;
		}
		if (after)
			f.InsertAfter(after, text);
		else if (any && mine >= 0)
			f.InsertFirst(parent, text);
		else
			f.AppendChild(parent, text);
	}

	// Sets one field of an element. 'values' (by field name) fill an element that has to be created; 'done' collects
	// created element paths, so later fields under them are already written. Returns "" or a detail code.
	string SetField(DZS_PatchFile f, DZS_PKind k, DZS_XmlNode n, DZS_PField fld, DZS_Json v, map<string, ref DZS_Json> values, array<string> done)
	{
		bool remove = !v || v.m_Type == DZS_Json.NUL;
		if (remove && fld.m_Req)
			return "bad-value";
		if (fld.m_List)
			return SetList(f, k, n, fld, v);
		string text = "";
		if (!remove)
		{
			string err;
			text = Render(fld, v, err);
			if (err != "")
				return err;
		}
		DZS_XmlNode cur = n;
		for (int i = 0; i < fld.m_Elems.Count(); i++)
		{
			string prefix = fld.ElemPath(i + 1);
			if (done && done.Find(prefix) >= 0)
				return "";
			DZS_XmlNode next = null;
			foreach (DZS_XmlNode c : cur.m_Children)
			{
				if (c.m_Name == fld.m_Elems[i] && !f.IsRemoved(c))
				{
					next = c;
					break;
				}
			}
			if (!next)
			{
				if (remove)
					return "";
				map<string, ref DZS_Json> only = values;
				if (!only)
				{
					only = new map<string, ref DZS_Json>();
					only.Set(fld.m_Name, v);
				}
				string made;
				string last = fld.m_Elems[i];
				if (i == fld.m_Elems.Count() - 1 && fld.m_Attr == "")
					made = "<" + last + ">" + DZS_Xml.Escape(text) + "</" + last + ">";
				else
				{
					array<string> path = new array<string>();
					for (int pi = 0; pi < i; pi++)
						path.Insert(fld.m_Elems[pi]);
					made = Gen(k, path, last, only, null, f.m_Unit);
				}
				InsertOrdered(f, k, cur, fld.m_Elems[i], made);
				if (done)
					done.Insert(prefix);
				return "";
			}
			cur = next;
		}
		if (fld.m_Attr != "")
		{
			if (remove)
				f.RemoveAttr(cur, fld.m_Attr);
			else
			{
				array<string> order = AttrOrder(k, fld);
				f.SetAttr(cur, fld.m_Attr, text, order);
			}
		}
		else if (remove)
			f.RemoveNode(cur);
		else
			f.SetText(cur, text);
		return "";
	}

	static string ListKey(DZS_XmlNode n, DZS_PField fld)
	{
		return "l" + n.m_Start + ":" + fld.m_Name;
	}

	// Replaces a list value: the old members go, the new ones are one insertion where the first old member stood (or
	// where the kind's order puts them), so setting the list again later in the run rewrites that same insertion.
	string SetList(DZS_PatchFile f, DZS_PKind k, DZS_XmlNode n, DZS_PField fld, DZS_Json v)
	{
		array<string> tokens = new array<string>();
		if (v && v.m_Type != DZS_Json.NUL && !ListTokens(v, tokens))
			return "bad-value";
		DZS_XmlNode parent = Walk(f, n, fld, fld.m_Elems.Count() - 1);
		if (!parent)
			return "bad-value";
		string elem = fld.m_Elems[fld.m_Elems.Count() - 1];
		string key = ListKey(n, fld);
		DZS_PList pl = f.m_Lists.Get(key);
		if (!pl)
		{
			array<DZS_XmlNode> old = new array<DZS_XmlNode>();
			foreach (DZS_XmlNode c : parent.m_Children)
			{
				if (c.m_Name == elem && !f.IsRemoved(c))
					old.Insert(c);
			}
			pl = ListAnchor(f, k, parent, elem, old);
			if (!pl)
			{
				string joined = "";
				foreach (string tok0 : tokens)
				{
					if (joined != "")
						joined += "\n";
					joined += ListItem(elem, tok0, k.m_SelfClose);
				}
				if (joined != "")
					InsertOrdered(f, k, parent, elem, joined);
				foreach (DZS_XmlNode o0 : old)
					f.RemoveNode(o0);
				return "";
			}
			foreach (DZS_XmlNode o : old)
				f.RemoveNode(o);
			f.m_Lists.Set(key, pl);
		}
		pl.m_Tokens.Clear();
		pl.m_Tokens.Copy(tokens);
		string text = "";
		foreach (string tok : tokens)
		{
			string item = ListItem(elem, tok, k.m_SelfClose);
			if (pl.m_Before)
				text += pl.m_Indent + item + f.m_Eol;
			else
				text += f.m_Eol + pl.m_Indent + item;
		}
		pl.m_Patch.m_Text = text;
		return "";
	}

	// Where new list members go, as an empty insertion; null when the parent has no line of its own to put them on.
	protected DZS_PList ListAnchor(DZS_PatchFile f, DZS_PKind k, DZS_XmlNode parent, string elem, array<DZS_XmlNode> old)
	{
		DZS_PList pl = new DZS_PList();
		if (old.Count() > 0 && f.StartsLine(old[0].m_Start))
		{
			pl.m_Before = true;
			pl.m_Indent = f.LineIndent(old[0].m_Start);
			pl.m_Patch = f.Insertion(f.LineStart(old[0].m_Start), "");
			return pl;
		}
		if (parent.m_SelfClose)
			return null;
		int mine = k.m_Order.Find(elem);
		DZS_XmlNode after = null;
		foreach (DZS_XmlNode c : parent.m_Children)
		{
			if (f.IsRemoved(c))
				continue;
			int ord = k.m_Order.Find(c.m_Name);
			if (mine >= 0 && ord >= 0 && ord < mine)
				after = c;
		}
		if (after && f.StartsLine(after.m_Start))
		{
			pl.m_Indent = f.LineIndent(after.m_Start);
			pl.m_Patch = f.Insertion(after.m_End, "");
			return pl;
		}
		if (!after)
		{
			foreach (DZS_XmlNode first : parent.m_Children)
			{
				if (f.IsRemoved(first) || k.m_Order.Find(first.m_Name) < 0)
					continue;
				if (!f.StartsLine(first.m_Start))
					return null;
				pl.m_Before = true;
				pl.m_Indent = f.LineIndent(first.m_Start);
				pl.m_Patch = f.Insertion(f.LineStart(first.m_Start), "");
				return pl;
			}
		}
		int et = f.EndTagStart(parent);
		if (after || !f.StartsLine(et))
			return null;
		pl.m_Before = true;
		pl.m_Indent = f.ChildIndent(parent);
		pl.m_Patch = f.Insertion(f.LineStart(et), "");
		return pl;
	}

	static string ListItem(string elem, string token, string selfClose)
	{
		int eq = token.IndexOf("=");
		string attr = token.Substring(0, eq);
		string val = token.Substring(eq + 1, token.Length() - eq - 1);
		return "<" + elem + " " + attr + "=\"" + DZS_Xml.Escape(val) + "\"" + selfClose;
	}

	// ---- writing own blocks ----

	protected static bool ElemsAre(DZS_PField f, array<string> path)
	{
		if (f.m_Elems.Count() != path.Count())
			return false;
		for (int i = 0; i < path.Count(); i++)
		{
			if (f.m_Elems[i] != path[i])
				return false;
		}
		return true;
	}

	protected static bool ElemsUnder(DZS_PField f, array<string> path)
	{
		if (f.m_Elems.Count() <= path.Count())
			return false;
		for (int i = 0; i < path.Count(); i++)
		{
			if (f.m_Elems[i] != path[i])
				return false;
		}
		return true;
	}

	// An element of a kind written from values: 'path' leads from the block's element to this one's parent (empty
	// for the block itself). 'nested' are texts of own children that go into the container element (or directly
	// into the block's element when the kind has none). Lines after the first are indented with 'unit' per level.
	string Gen(DZS_PKind k, array<string> path, string name, map<string, ref DZS_Json> values, array<string> nested, string unit)
	{
		array<string> here = new array<string>();
		bool isBlock = name == k.m_Elem && path.Count() == 0;
		if (!isBlock)
		{
			here.Copy(path);
			here.Insert(name);
		}
		string head = "<" + name;
		array<string> kids = new array<string>();
		// copied first: the recursive calls below pass null and Enforce then loses this call's parameter
		array<string> given = new array<string>();
		if (nested)
			given.Copy(nested);
		int nestedCount = given.Count();
		foreach (DZS_PField f : k.m_Fields)
		{
			if (!ElemsAre(f, here))
				continue;
			DZS_Json v = values.Get(f.m_Name);
			if (!v || v.m_Type == DZS_Json.NUL)
				continue;
			if (f.m_Attr != "")
			{
				string err;
				string av = Render(f, v, err);
				head += " " + f.m_Attr + "=\"" + DZS_Xml.Escape(av) + "\"";
			}
		}
		foreach (string cn : k.m_Order)
		{
			array<string> sub = new array<string>();
			sub.Copy(here);
			sub.Insert(cn);
			bool deeper = false;
			foreach (DZS_PField g : k.m_Fields)
			{
				DZS_Json gv = values.Get(g.m_Name);
				if (!gv || gv.m_Type == DZS_Json.NUL)
					continue;
				if (ElemsAre(g, sub))
				{
					if (g.m_List)
					{
						array<string> tokens = new array<string>();
						ListTokens(gv, tokens);
						foreach (string tok : tokens)
							kids.Insert(ListItem(cn, tok, k.m_SelfClose));
					}
					else if (g.m_Attr == "")
					{
						string err2;
						string tv = Render(g, gv, err2);
						kids.Insert("<" + cn + ">" + DZS_Xml.Escape(tv) + "</" + cn + ">");
					}
					else
						deeper = true;
				}
				else if (ElemsUnder(g, sub))
					deeper = true;
			}
			if (deeper)
			{
				string inner = Gen(k, here, cn, values, null, unit);
				kids.Insert(inner);
			}
			bool box = isBlock && cn == ContainerOf(k);
			if (box && nestedCount == 0)
				kids.Insert("<" + cn + k.m_SelfClose);
			else if (box)
			{
				string boxed = "<" + cn + ">";
				foreach (string nt : given)
					boxed += "\n" + unit + DZS_PatchFile.Reindent(nt, unit, "\n");
				boxed += "\n</" + cn + ">";
				kids.Insert(boxed);
			}
		}
		if (isBlock && nestedCount > 0 && ContainerOf(k) == "")
		{
			foreach (string nt2 : given)
				kids.Insert(nt2);
		}
		if (kids.Count() == 0)
			return head + k.m_SelfClose;
		string r = head + ">";
		foreach (string kt : kids)
			r += "\n" + unit + DZS_PatchFile.Reindent(kt, unit, "\n");
		r += "\n</" + name + ">";
		return r;
	}

	// The element own children of this kind go into ("children" for events, "" when they sit directly inside).
	static string ContainerOf(DZS_PKind k)
	{
		if (k.m_Name == "event")
			return "children";
		return "";
	}

	// Flattens a block's data into field values: nested objects are read as dotted names. 'err' names the problem.
	static map<string, ref DZS_Json> Values(DZS_PKind k, DZS_Json data, out string err)
	{
		map<string, ref DZS_Json> r = new map<string, ref DZS_Json>();
		err = "";
		Flatten(k, data, "", r, err);
		return r;
	}

	protected static void Flatten(DZS_PKind k, DZS_Json obj, string prefix, map<string, ref DZS_Json> r, out string err)
	{
		if (!obj || !obj.IsObj())
		{
			err = "bad-data";
			return;
		}
		foreach (string key : obj.m_Keys)
		{
			if (prefix == "" && key == "file" && k.m_FileInKey)
				continue;
			string name = prefix + key;
			DZS_Json v = obj.Get(key);
			if (k.Field(name))
				r.Set(name, v);
			else if (v.IsObj())
				Flatten(k, v, name + ".", r, err);
			else
			{
				err = "unknown-field";
				return;
			}
		}
	}

	// Checks an own block's values: every required field present, every value in its format. "" when fine.
	static string CheckValues(DZS_PKind k, map<string, ref DZS_Json> values)
	{
		foreach (DZS_PField f : k.m_Fields)
		{
			DZS_Json v = values.Get(f.m_Name);
			if (!v || v.m_Type == DZS_Json.NUL)
			{
				if (f.m_Req)
					return "incomplete";
				continue;
			}
			if (f.m_List)
			{
				array<string> tokens = new array<string>();
				if (!ListTokens(v, tokens))
					return "bad-value";
				continue;
			}
			string err;
			Render(f, v, err);
			if (err != "")
				return err;
		}
		return k.CheckOwn(values);
	}
}
