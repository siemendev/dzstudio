// Size of a step before it exists: the length of the text form, counted without building it. Strings
// are taken with their quotes; escapes are counted only where the text can hold them, so the count is never below.
class DZS_Est
{
	static int Str(string s)
	{
		int n = s.Length();
		int extra = 0;
		if (n > 24 || s.IndexOf("\"") >= 0 || s.IndexOf("\\") >= 0 || s.IndexOf("\n") >= 0 || s.IndexOf("\t") >= 0 || s.IndexOf("\r") >= 0)
		{
			for (int i = 0; i < n; i++)
			{
				int b = DZS_Utf8.Byte(s, i);
				if (b == 34 || b == 92 || b == 8 || b == 9 || b == 10 || b == 12 || b == 13)
					extra += 1;
				else if (b < 32)
					extra += 5;
			}
		}
		return n + 2 + extra;
	}

	static int Bool(bool b)
	{
		if (b)
			return 4;
		return 5;
	}

	static int Int(int v)
	{
		return v.ToString().Length();
	}

	// "key": with quotes and colon
	static int Key(string k)
	{
		return k.Length() + 3;
	}
}

class DZS_Companion : Managed
{
	string m_Type;
	int m_Version;
	string m_Data;

	void DZS_Companion(string type = "", int version = 1, string data = "")
	{
		m_Type = type;
		m_Version = version;
		m_Data = data;
	}

	DZS_Companion Copy()
	{
		return new DZS_Companion(m_Type, m_Version, m_Data);
	}

	bool Equals(DZS_Companion o)
	{
		return o && o.m_Type == m_Type && o.m_Version == m_Version && o.m_Data == m_Data;
	}

	DZS_Json ToJson()
	{
		DZS_Json j = DZS_Json.Obj();
		j.Set("v", DZS_Json.Int(m_Version));
		j.Set("d", DZS_Json.Str(m_Data));
		return j;
	}

	static int Estimate(int version, string data)
	{
		return 1 + DZS_Est.Key("v") + DZS_Est.Int(version) + 1 + DZS_Est.Key("d") + DZS_Est.Str(data) + 1;
	}

	static DZS_Companion FromJson(string type, DZS_Json j)
	{
		if (!j || !j.IsObj() || !j.Has("v"))
			return null;
		return new DZS_Companion(type, j.GetInt("v", 1), j.GetStr("d"));
	}
}

// Plain data of one element. The project state keeps these privately; everything handed out is a copy.
class DZS_ElementData : Managed
{
	string m_Id;
	string m_Kind;
	string m_Parent;
	string m_After;
	bool m_Hidden;
	bool m_Locked;
	ref map<string, string> m_Fields;
	ref map<string, ref DZS_Companion> m_Companions;
	ref array<string> m_Children;

	void DZS_ElementData()
	{
		m_Fields = new map<string, string>();
		m_Companions = new map<string, ref DZS_Companion>();
		m_Children = new array<string>();
	}

	DZS_ElementData Copy(bool withChildren = true)
	{
		DZS_ElementData c = new DZS_ElementData();
		c.m_Id = m_Id;
		c.m_Kind = m_Kind;
		c.m_Parent = m_Parent;
		c.m_After = m_After;
		c.m_Hidden = m_Hidden;
		c.m_Locked = m_Locked;
		for (int i = 0; i < m_Fields.Count(); i++)
			c.m_Fields.Set(m_Fields.GetKey(i), m_Fields.GetElement(i));
		for (int k = 0; k < m_Companions.Count(); k++)
		{
			DZS_Companion t72 = m_Companions.GetElement(k).Copy();
			c.m_Companions.Set(m_Companions.GetKey(k), t72);
		}
		if (withChildren)
			c.m_Children.Copy(m_Children);
		return c;
	}

	// Same content (fields, flags, companions); ignores children and position.
	bool SameContent(DZS_ElementData o)
	{
		if (o.m_Kind != m_Kind || o.m_Hidden != m_Hidden || o.m_Locked != m_Locked)
			return false;
		if (o.m_Fields.Count() != m_Fields.Count() || o.m_Companions.Count() != m_Companions.Count())
			return false;
		for (int i = 0; i < m_Fields.Count(); i++)
		{
			string v;
			if (!o.m_Fields.Find(m_Fields.GetKey(i), v) || v != m_Fields.GetElement(i))
				return false;
		}
		for (int k = 0; k < m_Companions.Count(); k++)
		{
			DZS_Companion oc;
			if (!o.m_Companions.Find(m_Companions.GetKey(k), oc) || !m_Companions.GetElement(k).Equals(oc))
				return false;
		}
		return true;
	}

	static array<string> SortedKeys(map<string, string> m)
	{
		array<string> keys = new array<string>();
		for (int i = 0; i < m.Count(); i++)
			keys.Insert(m.GetKey(i));
		keys.Sort();
		return keys;
	}

	DZS_Json ToJson()
	{
		DZS_Json j = DZS_Json.Obj();
		j.Set("id", DZS_Json.Str(m_Id));
		j.Set("kind", DZS_Json.Str(m_Kind));
		j.Set("parent", DZS_Json.Str(m_Parent));
		j.Set("after", DZS_Json.Str(m_After));
		j.Set("hidden", DZS_Json.Boolean(m_Hidden));
		j.Set("locked", DZS_Json.Boolean(m_Locked));
		DZS_Json f = DZS_Json.Obj();
		array<string> keys = SortedKeys(m_Fields);
		foreach (string key : keys)
			f.Set(key, DZS_Json.Str(m_Fields.Get(key)));
		j.Set("fields", f);
		DZS_Json c = DZS_Json.Obj();
		array<string> ckeys = new array<string>();
		for (int i = 0; i < m_Companions.Count(); i++)
			ckeys.Insert(m_Companions.GetKey(i));
		ckeys.Sort();
		foreach (string ck : ckeys)
		{
			DZS_Json t129 = m_Companions.Get(ck).ToJson();
			c.Set(ck, t129);
		}
		j.Set("comp", c);
		return j;
	}

	// Bytes of ToJson() in the compact text form, without the closing brace.
	int EstimateOpen()
	{
		int n = 1;
		n += DZS_Est.Key("id") + DZS_Est.Str(m_Id) + 1;
		n += DZS_Est.Key("kind") + DZS_Est.Str(m_Kind) + 1;
		n += DZS_Est.Key("parent") + DZS_Est.Str(m_Parent) + 1;
		n += DZS_Est.Key("after") + DZS_Est.Str(m_After) + 1;
		n += DZS_Est.Key("hidden") + DZS_Est.Bool(m_Hidden) + 1;
		n += DZS_Est.Key("locked") + DZS_Est.Bool(m_Locked) + 1;
		n += DZS_Est.Key("fields") + 2;
		for (int i = 0; i < m_Fields.Count(); i++)
		{
			if (i > 0)
				n += 1;
			n += DZS_Est.Str(m_Fields.GetKey(i)) + 1 + DZS_Est.Str(m_Fields.GetElement(i));
		}
		n += 1 + DZS_Est.Key("comp") + 2;
		for (int k = 0; k < m_Companions.Count(); k++)
		{
			if (k > 0)
				n += 1;
			DZS_Companion c = m_Companions.GetElement(k);
			n += DZS_Est.Str(m_Companions.GetKey(k)) + 1 + DZS_Companion.Estimate(c.m_Version, c.m_Data);
		}
		return n;
	}

	static DZS_ElementData FromJson(DZS_Json j, out string error)
	{
		if (!j || !j.IsObj() || !j.Has("id") || !j.Has("kind"))
		{
			error = "element without id or kind";
			return null;
		}
		DZS_ElementData e = new DZS_ElementData();
		e.m_Id = j.GetStr("id");
		e.m_Kind = j.GetStr("kind");
		e.m_Parent = j.GetStr("parent");
		e.m_After = j.GetStr("after");
		e.m_Hidden = j.GetBool("hidden");
		e.m_Locked = j.GetBool("locked");
		DZS_Json f = j.Get("fields");
		if (f && f.IsObj())
		{
			for (int i = 0; i < f.Count(); i++)
			{
				DZS_Json fv = f.Get(f.m_Keys[i]);
				if (!fv.IsStr())
				{
					error = "field value is not text";
					return null;
				}
				e.m_Fields.Set(f.m_Keys[i], fv.m_Str);
			}
		}
		DZS_Json c = j.Get("comp");
		if (c && c.IsObj())
		{
			for (int k = 0; k < c.Count(); k++)
			{
				DZS_Companion comp = DZS_Companion.FromJson(c.m_Keys[k], c.Get(c.m_Keys[k]));
				if (!comp)
				{
					error = "bad companion data";
					return null;
				}
				e.m_Companions.Set(comp.m_Type, comp);
			}
		}
		return e;
	}
}

// One primitive change with what it expects to find (precondition) and what it sets.
class DZS_Change : Managed
{
	protected DZS_ChangeKind m_Kind;
	protected string m_Id;
	protected ref DZS_ElementData m_Snap;
	protected string m_Key;
	protected bool m_HasOld;
	protected bool m_HasNew;
	protected string m_Old;
	protected string m_New;
	protected int m_OldVer;
	protected int m_NewVer;
	protected bool m_OldFlag;
	protected bool m_NewFlag;
	protected string m_FromParent;
	protected string m_FromAfter;
	protected string m_ToParent;
	protected string m_ToAfter;

	DZS_ChangeKind GetKind() { return m_Kind; }
	string GetId() { return m_Id; }
	string GetKey() { return m_Key; }
	bool HasOld() { return m_HasOld; }
	bool HasNew() { return m_HasNew; }
	string GetOld() { return m_Old; }
	string GetNew() { return m_New; }
	int GetOldVersion() { return m_OldVer; }
	int GetNewVersion() { return m_NewVer; }
	bool GetOldFlag() { return m_OldFlag; }
	bool GetNewFlag() { return m_NewFlag; }
	string GetFromParent() { return m_FromParent; }
	string GetFromAfter() { return m_FromAfter; }
	string GetToParent() { return m_ToParent; }
	string GetToAfter() { return m_ToAfter; }

	DZS_ElementData GetSnapshot()
	{
		if (!m_Snap)
			return null;
		return m_Snap.Copy(false);
	}

	string GetSnapshotKind()
	{
		if (!m_Snap)
			return "";
		return m_Snap.m_Kind;
	}

	static DZS_Change Create(DZS_ElementData snap)
	{
		DZS_Change c = new DZS_Change();
		c.m_Kind = DZS_ChangeKind.CREATE;
		c.m_Id = snap.m_Id;
		c.m_Snap = snap.Copy(false);
		return c;
	}

	static DZS_Change Delete(DZS_ElementData snap)
	{
		DZS_Change c = Create(snap);
		c.m_Kind = DZS_ChangeKind.DELETE;
		return c;
	}

	static DZS_Change SetField(string id, string key, bool hasOld, string oldValue, bool hasNew, string newValue)
	{
		DZS_Change c = new DZS_Change();
		c.m_Kind = DZS_ChangeKind.SET_FIELD;
		c.m_Id = id;
		c.m_Key = key;
		c.m_HasOld = hasOld;
		c.m_Old = oldValue;
		c.m_HasNew = hasNew;
		c.m_New = newValue;
		return c;
	}

	static DZS_Change SetFlag(DZS_ChangeKind kind, string id, bool oldFlag, bool newFlag)
	{
		DZS_Change c = new DZS_Change();
		c.m_Kind = kind;
		c.m_Id = id;
		c.m_OldFlag = oldFlag;
		c.m_NewFlag = newFlag;
		return c;
	}

	static DZS_Change Move(string id, string fromParent, string fromAfter, string toParent, string toAfter)
	{
		DZS_Change c = new DZS_Change();
		c.m_Kind = DZS_ChangeKind.MOVE;
		c.m_Id = id;
		c.m_FromParent = fromParent;
		c.m_FromAfter = fromAfter;
		c.m_ToParent = toParent;
		c.m_ToAfter = toAfter;
		return c;
	}

	static DZS_Change SetCompanion(string id, string type, DZS_Companion oldC, DZS_Companion newC)
	{
		DZS_Change c = new DZS_Change();
		c.m_Kind = DZS_ChangeKind.SET_COMPANION;
		c.m_Id = id;
		c.m_Key = type;
		if (oldC)
		{
			c.m_HasOld = true;
			c.m_OldVer = oldC.m_Version;
			c.m_Old = oldC.m_Data;
		}
		if (newC)
		{
			c.m_HasNew = true;
			c.m_NewVer = newC.m_Version;
			c.m_New = newC.m_Data;
		}
		return c;
	}

	DZS_Change Copy()
	{
		DZS_Change c = new DZS_Change();
		c.m_Kind = m_Kind;
		c.m_Id = m_Id;
		if (m_Snap)
			c.m_Snap = m_Snap.Copy(false);
		c.m_Key = m_Key;
		c.m_HasOld = m_HasOld;
		c.m_HasNew = m_HasNew;
		c.m_Old = m_Old;
		c.m_New = m_New;
		c.m_OldVer = m_OldVer;
		c.m_NewVer = m_NewVer;
		c.m_OldFlag = m_OldFlag;
		c.m_NewFlag = m_NewFlag;
		c.m_FromParent = m_FromParent;
		c.m_FromAfter = m_FromAfter;
		c.m_ToParent = m_ToParent;
		c.m_ToAfter = m_ToAfter;
		return c;
	}

	// The counter-change, built from this change alone.
	DZS_Change Inverse()
	{
		DZS_Change c = Copy();
		if (m_Kind == DZS_ChangeKind.CREATE)
			c.m_Kind = DZS_ChangeKind.DELETE;
		else if (m_Kind == DZS_ChangeKind.DELETE)
			c.m_Kind = DZS_ChangeKind.CREATE;
		else if (m_Kind == DZS_ChangeKind.SET_FIELD || m_Kind == DZS_ChangeKind.SET_COMPANION)
		{
			c.m_HasOld = m_HasNew;
			c.m_Old = m_New;
			c.m_OldVer = m_NewVer;
			c.m_HasNew = m_HasOld;
			c.m_New = m_Old;
			c.m_NewVer = m_OldVer;
		}
		else if (m_Kind == DZS_ChangeKind.SET_HIDDEN || m_Kind == DZS_ChangeKind.SET_LOCKED)
		{
			c.m_OldFlag = m_NewFlag;
			c.m_NewFlag = m_OldFlag;
		}
		else if (m_Kind == DZS_ChangeKind.MOVE)
		{
			c.m_FromParent = m_ToParent;
			c.m_FromAfter = m_ToAfter;
			c.m_ToParent = m_FromParent;
			c.m_ToAfter = m_FromAfter;
		}
		return c;
	}

	static string KindName(DZS_ChangeKind k)
	{
		if (k == DZS_ChangeKind.CREATE) return "create";
		if (k == DZS_ChangeKind.DELETE) return "delete";
		if (k == DZS_ChangeKind.SET_FIELD) return "field";
		if (k == DZS_ChangeKind.SET_HIDDEN) return "hidden";
		if (k == DZS_ChangeKind.SET_LOCKED) return "locked";
		if (k == DZS_ChangeKind.MOVE) return "move";
		return "comp";
	}

	DZS_Json ToJson()
	{
		DZS_Json j;
		if (m_Kind == DZS_ChangeKind.CREATE || m_Kind == DZS_ChangeKind.DELETE)
			j = m_Snap.ToJson();
		else
		{
			j = DZS_Json.Obj();
			j.Set("id", DZS_Json.Str(m_Id));
		}
		j.Set("k", DZS_Json.Str(KindName(m_Kind)));
		if (m_Kind == DZS_ChangeKind.SET_FIELD)
		{
			j.Set("key", DZS_Json.Str(m_Key));
			if (m_HasOld)
				j.Set("old", DZS_Json.Str(m_Old));
			if (m_HasNew)
				j.Set("new", DZS_Json.Str(m_New));
		}
		else if (m_Kind == DZS_ChangeKind.SET_COMPANION)
		{
			j.Set("type", DZS_Json.Str(m_Key));
			if (m_HasOld)
			{
				DZS_Companion oc = new DZS_Companion(m_Key, m_OldVer, m_Old);
				DZS_Json t393 = oc.ToJson();
				j.Set("old", t393);
			}
			if (m_HasNew)
			{
				DZS_Companion nc = new DZS_Companion(m_Key, m_NewVer, m_New);
				DZS_Json t398 = nc.ToJson();
				j.Set("new", t398);
			}
		}
		else if (m_Kind == DZS_ChangeKind.SET_HIDDEN || m_Kind == DZS_ChangeKind.SET_LOCKED)
		{
			j.Set("old", DZS_Json.Boolean(m_OldFlag));
			j.Set("new", DZS_Json.Boolean(m_NewFlag));
		}
		else if (m_Kind == DZS_ChangeKind.MOVE)
		{
			j.Set("from", DZS_Json.Str(m_FromParent));
			j.Set("fromAfter", DZS_Json.Str(m_FromAfter));
			j.Set("to", DZS_Json.Str(m_ToParent));
			j.Set("toAfter", DZS_Json.Str(m_ToAfter));
		}
		return j;
	}

	int Estimate()
	{
		int n;
		if (m_Kind == DZS_ChangeKind.CREATE || m_Kind == DZS_ChangeKind.DELETE)
			n = m_Snap.EstimateOpen() + 1;
		else
			n = 1 + DZS_Est.Key("id") + DZS_Est.Str(m_Id) + 1;
		n += DZS_Est.Key("k") + DZS_Est.Str(KindName(m_Kind));
		if (m_Kind == DZS_ChangeKind.SET_FIELD)
		{
			n += 1 + DZS_Est.Key("key") + DZS_Est.Str(m_Key);
			if (m_HasOld)
				n += 1 + DZS_Est.Key("old") + DZS_Est.Str(m_Old);
			if (m_HasNew)
				n += 1 + DZS_Est.Key("new") + DZS_Est.Str(m_New);
		}
		else if (m_Kind == DZS_ChangeKind.SET_COMPANION)
		{
			n += 1 + DZS_Est.Key("type") + DZS_Est.Str(m_Key);
			if (m_HasOld)
				n += 1 + DZS_Est.Key("old") + DZS_Companion.Estimate(m_OldVer, m_Old);
			if (m_HasNew)
				n += 1 + DZS_Est.Key("new") + DZS_Companion.Estimate(m_NewVer, m_New);
		}
		else if (m_Kind == DZS_ChangeKind.SET_HIDDEN || m_Kind == DZS_ChangeKind.SET_LOCKED)
			n += 1 + DZS_Est.Key("old") + DZS_Est.Bool(m_OldFlag) + 1 + DZS_Est.Key("new") + DZS_Est.Bool(m_NewFlag);
		else if (m_Kind == DZS_ChangeKind.MOVE)
		{
			n += 1 + DZS_Est.Key("from") + DZS_Est.Str(m_FromParent) + 1 + DZS_Est.Key("fromAfter") + DZS_Est.Str(m_FromAfter);
			n += 1 + DZS_Est.Key("to") + DZS_Est.Str(m_ToParent) + 1 + DZS_Est.Key("toAfter") + DZS_Est.Str(m_ToAfter);
		}
		return n + 1;
	}

	static DZS_Change FromJson(DZS_Json j, out string error)
	{
		if (!j || !j.IsObj())
		{
			error = "change is not an object";
			return null;
		}
		string k = j.GetStr("k");
		DZS_Change c = new DZS_Change();
		c.m_Id = j.GetStr("id");
		if (k == "create" || k == "delete")
		{
			DZS_ElementData snap = DZS_ElementData.FromJson(j, error);
			if (!snap)
				return null;
			c.m_Snap = snap;
			if (k == "create")
				c.m_Kind = DZS_ChangeKind.CREATE;
			else
				c.m_Kind = DZS_ChangeKind.DELETE;
		}
		else if (k == "field")
		{
			c.m_Kind = DZS_ChangeKind.SET_FIELD;
			c.m_Key = j.GetStr("key");
			c.m_HasOld = j.Has("old");
			c.m_Old = j.GetStr("old");
			c.m_HasNew = j.Has("new");
			c.m_New = j.GetStr("new");
		}
		else if (k == "comp")
		{
			c.m_Kind = DZS_ChangeKind.SET_COMPANION;
			c.m_Key = j.GetStr("type");
			DZS_Companion o = DZS_Companion.FromJson(c.m_Key, j.Get("old"));
			DZS_Companion n = DZS_Companion.FromJson(c.m_Key, j.Get("new"));
			if (o)
			{
				c.m_HasOld = true;
				c.m_OldVer = o.m_Version;
				c.m_Old = o.m_Data;
			}
			if (n)
			{
				c.m_HasNew = true;
				c.m_NewVer = n.m_Version;
				c.m_New = n.m_Data;
			}
		}
		else if (k == "hidden" || k == "locked")
		{
			if (k == "hidden")
				c.m_Kind = DZS_ChangeKind.SET_HIDDEN;
			else
				c.m_Kind = DZS_ChangeKind.SET_LOCKED;
			c.m_OldFlag = j.GetBool("old");
			c.m_NewFlag = j.GetBool("new");
		}
		else if (k == "move")
		{
			c.m_Kind = DZS_ChangeKind.MOVE;
			c.m_FromParent = j.GetStr("from");
			c.m_FromAfter = j.GetStr("fromAfter");
			c.m_ToParent = j.GetStr("to");
			c.m_ToAfter = j.GetStr("toAfter");
		}
		else
		{
			error = "unknown change kind '" + k + "'";
			return null;
		}
		return c;
	}
}

// A record of one change of the project state: type, id, author, origin and its primitive changes.
class DZS_Operation : Managed
{
	protected string m_OpId;
	protected string m_Type;
	protected int m_TypeVersion;
	protected string m_Author;
	protected string m_AuthorName;
	protected DZS_Origin m_Origin;
	protected ref array<ref DZS_Change> m_Changes;
	protected bool m_Sealed;

	void DZS_Operation(string type = "", int typeVersion = 1)
	{
		m_Type = type;
		m_TypeVersion = typeVersion;
		m_Changes = new array<ref DZS_Change>();
	}

	string GetId() { return m_OpId; }
	string GetType() { return m_Type; }
	int GetTypeVersion() { return m_TypeVersion; }
	string GetAuthor() { return m_Author; }
	string GetAuthorName() { return m_AuthorName; }
	DZS_Origin GetOrigin() { return m_Origin; }
	int CountChanges() { return m_Changes.Count(); }
	bool IsSealed() { return m_Sealed; }

	DZS_Change GetChange(int i)
	{
		DZS_Change c = m_Changes[i];
		return c.Copy();
	}

	// The stored change itself, for reading only (no copy).
	DZS_Change PeekChange(int i)
	{
		return m_Changes[i];
	}

	bool AddChange(DZS_Change c)
	{
		if (m_Sealed || !c)
			return false;
		DZS_Change t528 = c.Copy();
		m_Changes.Insert(t528);
		return true;
	}

	// Identity and origin are fixed once, by the kernel, when the operation enters an instance.
	void Seal(string opId, string author, string authorName, DZS_Origin origin)
	{
		if (m_Sealed)
			return;
		m_OpId = opId;
		m_Author = author;
		m_AuthorName = authorName;
		m_Origin = origin;
		m_Sealed = true;
	}

	DZS_Operation CopyUnsealed()
	{
		DZS_Operation o = new DZS_Operation(m_Type, m_TypeVersion);
		foreach (DZS_Change c : m_Changes)
		{
			DZS_Change t548 = c.Copy();
			o.m_Changes.Insert(t548);
		}
		return o;
	}

	DZS_Json ToJson()
	{
		DZS_Json j = DZS_Json.Obj();
		j.Set("id", DZS_Json.Str(m_OpId));
		j.Set("type", DZS_Json.Str(m_Type));
		j.Set("tv", DZS_Json.Int(m_TypeVersion));
		j.Set("author", DZS_Json.Str(m_Author));
		j.Set("authorName", DZS_Json.Str(m_AuthorName));
		j.Set("origin", DZS_Json.Str(DZS_Str.OriginName(m_Origin)));
		DZS_Json arr = DZS_Json.Arr();
		foreach (DZS_Change c : m_Changes)
		{
			DZS_Json t563 = c.ToJson();
			arr.Add(t563);
		}
		j.Set("changes", arr);
		return j;
	}

	// Bytes of ToText() without building it.
	int EstimateText()
	{
		int n = 1 + DZS_Est.Key("textform") + DZS_Est.Int(DZS_TEXTFORM_VERSION) + 1 + DZS_Est.Key("op");
		n += 1 + DZS_Est.Key("id") + DZS_Est.Str(m_OpId) + 1 + DZS_Est.Key("type") + DZS_Est.Str(m_Type);
		n += 1 + DZS_Est.Key("tv") + DZS_Est.Int(m_TypeVersion) + 1 + DZS_Est.Key("author") + DZS_Est.Str(m_Author);
		n += 1 + DZS_Est.Key("authorName") + DZS_Est.Str(m_AuthorName) + 1 + DZS_Est.Key("origin") + DZS_Est.Str(DZS_Str.OriginName(m_Origin));
		n += 1 + DZS_Est.Key("changes") + 2;
		for (int i = 0; i < m_Changes.Count(); i++)
		{
			if (i > 0)
				n += 1;
			n += m_Changes[i].Estimate();
		}
		return n + 2;
	}

	array<string> ToText()
	{
		DZS_Json root = DZS_Json.Obj();
		root.Set("textform", DZS_Json.Int(DZS_TEXTFORM_VERSION));
		DZS_Json t572 = ToJson();
		root.Set("op", t572);
		return root.ToChunks();
	}

	static DZS_Operation FromText(array<string> chunks, out string error)
	{
		DZS_Json root = DZS_Json.Parse(chunks, error);
		if (!root)
			return null;
		if (!root.IsObj() || !root.Has("textform"))
		{
			error = "no text form";
			return null;
		}
		int tf = root.GetInt("textform");
		if (tf > DZS_TEXTFORM_VERSION)
		{
			error = "text form version " + tf + " is newer than " + DZS_TEXTFORM_VERSION;
			return null;
		}
		return FromJson(root.Get("op"), error);
	}

	static DZS_Operation FromJson(DZS_Json j, out string error)
	{
		if (!j || !j.IsObj())
		{
			error = "operation missing";
			return null;
		}
		DZS_Operation o = new DZS_Operation(j.GetStr("type"), j.GetInt("tv", 1));
		DZS_Origin origin;
		if (!DZS_Str.OriginFromName(j.GetStr("origin"), origin))
		{
			error = "bad origin";
			return null;
		}
		DZS_Json arr = j.Get("changes");
		if (!arr || !arr.IsArr())
		{
			error = "changes missing";
			return null;
		}
		for (int i = 0; i < arr.Count(); i++)
		{
			DZS_Change c = DZS_Change.FromJson(arr.At(i), error);
			if (!c)
				return null;
			o.m_Changes.Insert(c);
		}
		string id = j.GetStr("id");
		string author = j.GetStr("author");
		string authorName = j.GetStr("authorName");
		o.Seal(id, author, authorName, origin);
		return o;
	}
}
