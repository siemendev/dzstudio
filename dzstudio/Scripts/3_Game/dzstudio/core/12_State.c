const string DZS_KIND_PROJECT = "core:project";
const string DZS_KIND_GROUP = "core:group";
const string DZS_KIND_OBJECT = "core:object";
const string DZS_END = "<end>";

class DZS_ApplyFailure : Managed
{
	string m_Id;
	string m_Kind;
	string m_Code;

	void Set(string id, string kind, string code)
	{
		m_Id = id;
		m_Kind = kind;
		m_Code = code;
	}
}

// The state at a moment, kept while a save reads it over several frames: an element is
// copied before its first change; one created later counts as absent.
class DZS_StateFreeze : Managed
{
	protected ref map<string, ref DZS_ElementData> m_Copy = new map<string, ref DZS_ElementData>();
	protected ref map<string, bool> m_Absent = new map<string, bool>();

	void Touch(string id, DZS_ElementData live)
	{
		if (m_Copy.Contains(id) || m_Absent.Contains(id))
			return;
		if (live)
			m_Copy.Set(id, live.Copy(true));
		else
			m_Absent.Set(id, true);
	}

	bool Find(string id, out DZS_ElementData e)
	{
		if (m_Absent.Contains(id))
		{
			e = null;
			return true;
		}
		return m_Copy.Find(id, e);
	}

	int Count() { return m_Copy.Count() + m_Absent.Count(); }
}

// The text form of a state built over several frames: order, after and
// kinds are taken at the start; an element a later operation changes is copied before the change, so the result is
// exactly the state at the start.
class DZS_StateSlicer : Managed
{
	protected map<string, ref DZS_ElementData> m_E;
	protected ref array<string> m_Order = new array<string>();
	protected ref map<string, int> m_Index = new map<string, int>();
	protected ref map<string, string> m_After = new map<string, string>();
	protected ref map<string, ref DZS_ElementData> m_Frozen = new map<string, ref DZS_ElementData>();
	protected ref DZS_TextWriter m_W = new DZS_TextWriter();
	protected int m_Next;
	protected bool m_Done;
	ref array<string> m_Chunks;
	// Kinds of the elements, sorted (the backup head names them).
	ref array<string> m_Kinds = new array<string>();

	void DZS_StateSlicer(map<string, ref DZS_ElementData> elements, string rootId)
	{
		m_E = elements;
		map<string, bool> kinds = new map<string, bool>();
		if (m_E.Contains(rootId))
		{
			array<string> stack = {rootId};
			while (stack.Count() > 0)
			{
				string id = stack[stack.Count() - 1];
				stack.Remove(stack.Count() - 1);
				DZS_ElementData e = m_E.Get(id);
				m_Index.Set(id, m_Order.Count());
				m_Order.Insert(id);
				kinds.Set(e.m_Kind, true);
				string prev = "";
				foreach (string c : e.m_Children)
				{
					m_After.Set(c, prev);
					prev = c;
				}
				for (int k = e.m_Children.Count() - 1; k >= 0; k--)
					stack.Insert(e.m_Children[k]);
			}
		}
		array<string> names = m_Kinds;
		for (int i = 0; i < kinds.Count(); i++)
			names.Insert(kinds.GetKey(i));
		names.Sort();
		m_W.Append("{\"textform\":" + DZS_TEXTFORM_VERSION + ",\"state\":{\"kinds\":{");
		for (int n = 0; n < names.Count(); n++)
		{
			if (n > 0)
				m_W.Append(",");
			m_W.Append("\"" + names[n] + "\":1");
		}
		m_W.Append("},\"elements\":[");
	}

	int Count() { return m_Order.Count(); }
	int Written() { return m_Next; }
	bool IsDone() { return m_Done; }

	// Called before an element changes while slices are still being written.
	void Touch(string id)
	{
		if (m_Done || m_Frozen.Contains(id))
			return;
		int at;
		if (!m_Index.Find(id, at) || at < m_Next)
			return;
		DZS_ElementData e = m_E.Get(id);
		if (e)
			m_Frozen.Set(id, e.Copy(false));
	}

	// Writes elements until the budget (ms) is used; true when the text is complete.
	bool Step(float budgetMs)
	{
		if (m_Done)
			return true;
		int t0 = TickCount(0);
		while (m_Next < m_Order.Count())
		{
			string id = m_Order[m_Next];
			DZS_ElementData src = m_Frozen.Get(id);
			if (!src)
				src = m_E.Get(id);
			DZS_ElementData e = src.Copy(false);
			e.m_After = m_After.Get(id);
			if (m_Next > 0)
				m_W.Append(",");
			DZS_Json j = e.ToJson();
			j.Write(m_W, false, 0);
			m_Next++;
			if ((m_Next & 15) == 0 && (TickCount(0) - t0) / 10000.0 >= budgetMs)
				return false;
		}
		m_W.Append("]}}");
		m_Chunks = m_W.Finish();
		m_Frozen.Clear();
		m_Done = true;
		return true;
	}
}

// The project state of one instance. Read access is public; changes happen only through
// the protected apply path, which DZS_Instance drives from its single inlet.
class DZS_ProjectState : Managed
{
	protected ref map<string, ref DZS_ElementData> m_Elements;
	protected ref DZS_TypeRegistry m_Types;
	protected ref array<ref DZS_StateSlicer> m_Slicers;
	protected ref array<ref DZS_StateFreeze> m_Freezes;
	protected DZS_StateFreeze m_View;
	// counts applied operations: derived data (DZS_EntityExts' index) is built again when it changed
	protected int m_Gen;

	int GetGen() { return m_Gen; }

	DZS_StateFreeze BeginFreeze()
	{
		if (!m_Freezes)
			m_Freezes = new array<ref DZS_StateFreeze>();
		DZS_StateFreeze fz = new DZS_StateFreeze();
		m_Freezes.Insert(fz);
		return fz;
	}

	void EndFreeze(DZS_StateFreeze fz)
	{
		if (m_View == fz)
			m_View = null;
		if (!m_Freezes)
			return;
		int i = m_Freezes.Find(fz);
		if (i >= 0)
			m_Freezes.Remove(i);
	}

	// While set, the read methods answer from the frozen state (null = the live state).
	void SetView(DZS_StateFreeze fz)
	{
		m_View = fz;
	}

	protected DZS_ElementData El(string id)
	{
		if (m_View)
		{
			DZS_ElementData f;
			if (m_View.Find(id, f))
				return f;
		}
		return m_Elements.Get(id);
	}

	// Starts the text form of the current state in slices; the caller steps it and ends it.
	DZS_StateSlicer BeginSlices()
	{
		if (!m_Slicers)
			m_Slicers = new array<ref DZS_StateSlicer>();
		DZS_StateSlicer sl = new DZS_StateSlicer(m_Elements, DZS_PROJECT_ID);
		m_Slicers.Insert(sl);
		return sl;
	}

	void EndSlices(DZS_StateSlicer sl)
	{
		if (!m_Slicers)
			return;
		int i = m_Slicers.Find(sl);
		if (i >= 0)
			m_Slicers.Remove(i);
	}

	void DZS_ProjectState()
	{
		m_Elements = new map<string, ref DZS_ElementData>();
	}

	// ---- reading ----

	bool IsEmpty() { return m_Elements.Count() == 0; }
	int CountElements() { return m_Elements.Count(); }
	bool Has(string id) { return El(id) != null; }

	string GetKind(string id)
	{
		DZS_ElementData e = El(id);
		if (!e)
			return "";
		return e.m_Kind;
	}

	string GetParent(string id)
	{
		DZS_ElementData e = El(id);
		if (!e)
			return "";
		return e.m_Parent;
	}

	array<string> GetChildren(string id)
	{
		array<string> r = new array<string>();
		DZS_ElementData e = El(id);
		if (e)
			r.Copy(e.m_Children);
		return r;
	}

	array<string> GetGroupIds()
	{
		return GetChildren(DZS_PROJECT_ID);
	}

	bool GetField(string id, string key, out string value)
	{
		DZS_ElementData e = El(id);
		if (!e)
			return false;
		return e.m_Fields.Find(key, value);
	}

	array<string> GetFieldKeys(string id)
	{
		DZS_ElementData e = El(id);
		if (!e)
			return new array<string>();
		return DZS_ElementData.SortedKeys(e.m_Fields);
	}

	bool IsHidden(string id)
	{
		DZS_ElementData e = El(id);
		return e && e.m_Hidden;
	}

	bool IsLocked(string id)
	{
		DZS_ElementData e = El(id);
		return e && e.m_Locked;
	}

	bool GetCompanion(string id, string type, out int version, out string data)
	{
		DZS_ElementData e = El(id);
		if (!e)
			return false;
		DZS_Companion c = e.m_Companions.Get(type);
		if (!c)
			return false;
		version = c.m_Version;
		data = c.m_Data;
		return true;
	}

	array<string> GetCompanionTypes(string id)
	{
		array<string> r = new array<string>();
		DZS_ElementData e = El(id);
		if (e)
		{
			for (int i = 0; i < e.m_Companions.Count(); i++)
				r.Insert(e.m_Companions.GetKey(i));
		}
		r.Sort();
		return r;
	}

	DZS_ElementData GetElementCopy(string id)
	{
		DZS_ElementData e = m_Elements.Get(id);
		if (!e)
			return null;
		DZS_ElementData c = e.Copy(true);
		c.m_After = PreviousSibling(id);
		return c;
	}

	string PreviousSibling(string id)
	{
		DZS_ElementData e = m_Elements.Get(id);
		if (!e || e.m_Parent == "")
			return "";
		DZS_ElementData p = m_Elements.Get(e.m_Parent);
		if (!p)
			return "";
		int idx = p.m_Children.Find(id);
		if (idx <= 0)
			return "";
		return p.m_Children[idx - 1];
	}

	// Nearest enclosing element of the given kind, or "" (the element itself counts).
	string FindAncestorOfKind(string id, string kind)
	{
		string cur = id;
		int guard = 0;
		while (cur != "" && guard < 64)
		{
			DZS_ElementData e = m_Elements.Get(cur);
			if (!e)
				return "";
			if (e.m_Kind == kind)
				return cur;
			cur = e.m_Parent;
			guard++;
		}
		return "";
	}

	// Whole state as text form: elements depth-first, parents before children, in group order.
	array<string> ExportState()
	{
		DZS_Json root = DZS_Json.Obj();
		root.Set("textform", DZS_Json.Int(DZS_TEXTFORM_VERSION));
		DZS_Json st = DZS_Json.Obj();
		DZS_Json elems = DZS_Json.Arr();
		map<string, int> kinds = new map<string, int>();
		if (Has(DZS_PROJECT_ID))
			ExportElement(DZS_PROJECT_ID, elems, kinds);
		DZS_Json kj = DZS_Json.Obj();
		array<string> kindNames = new array<string>();
		for (int i = 0; i < kinds.Count(); i++)
			kindNames.Insert(kinds.GetKey(i));
		kindNames.Sort();
		foreach (string kn : kindNames)
			kj.Set(kn, DZS_Json.Int(kinds.Get(kn)));
		st.Set("kinds", kj);
		st.Set("elements", elems);
		root.Set("state", st);
		return root.ToChunks();
	}

	protected void ExportElement(string id, DZS_Json elems, map<string, int> kinds)
	{
		DZS_ElementData e = GetElementCopy(id);
		DZS_Json t190 = e.ToJson();
		elems.Add(t190);
		kinds.Set(e.m_Kind, 1);
		foreach (string child : e.m_Children)
			ExportElement(child, elems, kinds);
	}

	// ---- applying ----

	protected bool ApplyChanges(DZS_Operation op, DZS_ApplyFailure fail)
	{
		array<ref DZS_Change> done = new array<ref DZS_Change>();
		int n = op.CountChanges();
		for (int i = 0; i < n; i++)
		{
			DZS_Change c = op.PeekChange(i);
			if (!ApplyChange(c, fail))
			{
				for (int k = done.Count() - 1; k >= 0; k--)
				{
					DZS_ApplyFailure ignored = new DZS_ApplyFailure();
					DZS_Change t210 = done[k].Inverse();
					if (!ApplyChange(t210, ignored))
						DZS_Log.Error("rollback failed at " + ignored.m_Id + " (" + ignored.m_Code + ")");
				}
				return false;
			}
			done.Insert(c);
		}
		m_Gen++;
		return true;
	}

	protected bool ApplyChange(DZS_Change c, DZS_ApplyFailure fail)
	{
		string id = c.GetId();
		if (m_Slicers)
		{
			foreach (DZS_StateSlicer sl : m_Slicers)
				sl.Touch(id);
		}
		if (m_Freezes)
		{
			DZS_ElementData live = m_Elements.Get(id);
			foreach (DZS_StateFreeze fz : m_Freezes)
				fz.Touch(id, live);
		}
		DZS_ChangeKind kind = c.GetKind();
		DZS_ElementData e = m_Elements.Get(id);

		if (kind == DZS_ChangeKind.CREATE)
		{
			DZS_ElementData snap = c.GetSnapshot();
			if (id == "" || e)
			{
				fail.Set(id, snap.m_Kind, "exists");
				return false;
			}
			if (snap.m_Parent == "")
			{
				if (id != DZS_PROJECT_ID || snap.m_Kind != DZS_KIND_PROJECT || !IsEmpty())
				{
					fail.Set(id, snap.m_Kind, "invalid");
					return false;
				}
				m_Elements.Set(id, snap);
				return true;
			}
			DZS_ElementData parent = m_Elements.Get(snap.m_Parent);
			if (!parent)
			{
				fail.Set(snap.m_Parent, DZS_KIND_GROUP, "missing");
				return false;
			}
			if (!m_Types.AllowsParent(snap.m_Kind, parent.m_Kind))
			{
				fail.Set(id, snap.m_Kind, "invalid");
				return false;
			}
			InsertChild(parent, id, snap.m_After);
			snap.m_After = "";
			m_Elements.Set(id, snap);
			return true;
		}

		if (!e)
		{
			fail.Set(id, c.GetSnapshotKind(), "missing");
			return false;
		}

		if (kind == DZS_ChangeKind.DELETE)
		{
			DZS_ElementData was = c.GetSnapshot();
			if (e.m_Children.Count() > 0 || e.m_Parent != was.m_Parent || PreviousSibling(id) != was.m_After || !e.SameContent(was))
			{
				fail.Set(id, e.m_Kind, "changed");
				return false;
			}
			if (e.m_Parent != "")
			{
				DZS_ElementData p = m_Elements.Get(e.m_Parent);
				p.m_Children.RemoveOrdered(p.m_Children.Find(id));
			}
			m_Elements.Remove(id);
			return true;
		}

		if (kind == DZS_ChangeKind.SET_FIELD)
		{
			string cur;
			bool has = e.m_Fields.Find(c.GetKey(), cur);
			if (has != c.HasOld() || (has && cur != c.GetOld()))
			{
				fail.Set(id, e.m_Kind, "changed");
				return false;
			}
			if (c.HasNew())
				e.m_Fields.Set(c.GetKey(), c.GetNew());
			else
				e.m_Fields.Remove(c.GetKey());
			return true;
		}

		if (kind == DZS_ChangeKind.SET_HIDDEN || kind == DZS_ChangeKind.SET_LOCKED)
		{
			bool curFlag = e.m_Locked;
			if (kind == DZS_ChangeKind.SET_HIDDEN)
				curFlag = e.m_Hidden;
			if (curFlag != c.GetOldFlag())
			{
				fail.Set(id, e.m_Kind, "changed");
				return false;
			}
			if (kind == DZS_ChangeKind.SET_HIDDEN)
				e.m_Hidden = c.GetNewFlag();
			else
				e.m_Locked = c.GetNewFlag();
			return true;
		}

		if (kind == DZS_ChangeKind.MOVE)
		{
			if (e.m_Parent != c.GetFromParent() || PreviousSibling(id) != c.GetFromAfter())
			{
				fail.Set(id, e.m_Kind, "changed");
				return false;
			}
			DZS_ElementData target = m_Elements.Get(c.GetToParent());
			if (!target)
			{
				fail.Set(c.GetToParent(), DZS_KIND_GROUP, "missing");
				return false;
			}
			if (!m_Types.AllowsParent(e.m_Kind, target.m_Kind) || FindAncestorOfKindId(c.GetToParent(), id))
			{
				fail.Set(id, e.m_Kind, "invalid");
				return false;
			}
			DZS_ElementData from = m_Elements.Get(e.m_Parent);
			from.m_Children.RemoveOrdered(from.m_Children.Find(id));
			string toAfter = c.GetToAfter();
			if (toAfter == id)
				toAfter = DZS_END;
			InsertChild(target, id, toAfter);
			e.m_Parent = c.GetToParent();
			return true;
		}

		if (kind == DZS_ChangeKind.SET_COMPANION)
		{
			DZS_Companion curC = e.m_Companions.Get(c.GetKey());
			bool hasC = curC != null;
			if (hasC != c.HasOld() || (hasC && (curC.m_Version != c.GetOldVersion() || curC.m_Data != c.GetOld())))
			{
				fail.Set(id, e.m_Kind, "changed");
				return false;
			}
			if (c.HasNew())
				e.m_Companions.Set(c.GetKey(), new DZS_Companion(c.GetKey(), c.GetNewVersion(), c.GetNew()));
			else
				e.m_Companions.Remove(c.GetKey());
			return true;
		}

		fail.Set(id, e.m_Kind, "invalid");
		return false;
	}

	// True if 'ancestor' is 'start' or encloses it.
	protected bool FindAncestorOfKindId(string start, string ancestor)
	{
		string cur = start;
		int guard = 0;
		while (cur != "" && guard < 64)
		{
			if (cur == ancestor)
				return true;
			DZS_ElementData e = m_Elements.Get(cur);
			if (!e)
				return false;
			cur = e.m_Parent;
			guard++;
		}
		return false;
	}

	// "" = at the start; a missing neighbour (or DZS_END) = at the end of the group.
	protected void InsertChild(DZS_ElementData parent, string id, string after)
	{
		if (after == "")
		{
			if (parent.m_Children.Count() == 0)
				parent.m_Children.Insert(id);
			else
				parent.m_Children.InsertAt(id, 0);
			return;
		}
		int last = parent.m_Children.Count() - 1;
		if (last >= 0 && parent.m_Children[last] == after)
		{
			parent.m_Children.Insert(id);
			return;
		}
		int idx = parent.m_Children.Find(after);
		if (idx < 0 || idx + 1 >= parent.m_Children.Count())
			parent.m_Children.Insert(id);
		else
			parent.m_Children.InsertAt(id, idx + 1);
	}
}
