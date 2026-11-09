// What a field of a spec is set on: the project, a block, a change or an element of a block's list.
class DZS_ExtTarget : Managed
{
	// "project", "block", "change", "element"
	string m_Attach;
	// a block's or element's block kind, a change's "block"
	string m_Kind;
	string m_BlockId;
	string m_List;
	int m_Index;
	// the block, change or element as DZS_ProjectV2.Build made it
	ref DZS_Json m_Json;
	string m_Label;
	// the scene group of a spawner group block, "" otherwise
	string m_GroupId;
	bool m_Selected;
	// where the entity keeps its extensions in the model, taken when the target is first read
	ref DZS_ExtOwner m_Owner;

	static DZS_ExtTarget Project()
	{
		DZS_ExtTarget t = new DZS_ExtTarget();
		t.m_Attach = "project";
		t.m_Label = DZS_Texts.Pick("Projekt", "Project");
		return t;
	}

	static DZS_ExtTarget OfBlock(DZS_Json block)
	{
		DZS_ExtTarget t = new DZS_ExtTarget();
		t.m_Attach = "block";
		t.m_Kind = block.GetStr("kind");
		t.m_BlockId = block.GetStr("id");
		t.m_Json = block;
		t.m_Label = DZS_ProjectRefs.BlockLabel(block);
		return t;
	}

	static DZS_ExtTarget OfChange(DZS_Json change)
	{
		DZS_ExtTarget t = new DZS_ExtTarget();
		t.m_Attach = "change";
		t.m_Kind = change.GetStr("block");
		t.m_Json = change;
		t.m_Label = DZS_ProjectRefs.ChangeLabel(change);
		return t;
	}

	static DZS_ExtTarget OfElement(DZS_ExtTarget block, string list, int index, DZS_Json element)
	{
		DZS_ExtTarget t = new DZS_ExtTarget();
		t.m_Attach = "element";
		t.m_Kind = block.m_Kind;
		t.m_BlockId = block.m_BlockId;
		t.m_GroupId = block.m_GroupId;
		t.m_List = list;
		t.m_Index = index;
		t.m_Json = element;
		string name = "";
		if (element && element.IsObj())
		{
			name = element.GetStr("name");
			if (name == "")
				name = element.GetStr("type");
		}
		string label = list + "[" + index.ToString() + "]";
		if (name != "")
			label += " · " + name;
		t.m_Label = label;
		return t;
	}
}

// The handler of an extension no plugin reads: keeps the project's entry as it was read (written back unchanged until
// a field is edited) and edits the values of the fields its spec file offers — on the project inside "data", on blocks,
// changes and elements inside their own "extensions", on their elements in the model (DZS_EntityExts; an edit is a
// step of the history). Members nobody edits stay as they are.
class DZS_ExtGeneric : DZS_ProjectExtension
{
	protected string m_Url;
	protected bool m_Required;
	protected ref DZS_Json m_Data;
	protected bool m_Changed;
	// bumped on every edit, for the marks in the project bar
	protected int m_Gen;
	protected int m_MarkGen = -1;
	protected int m_MarkStateGen = -1;
	protected int m_MarkTick;
	protected ref map<string, string> m_Marks = new map<string, string>();

	void DZS_ExtGeneric(string url, bool required, DZS_Json data)
	{
		m_Url = url;
		m_Required = required;
		m_Data = data;
	}

	override string Url() { return m_Url; }
	override bool Required() { return m_Required; }

	override bool IsActive()
	{
		DZS_ExtSpec s = Spec();
		return s && s.m_Fields.Count() > 0;
	}

	override void Load(DZS_Json data)
	{
		m_Data = data;
	}

	override DZS_Json Data()
	{
		if (!m_Data)
			return DZS_Json.Obj();
		return m_Data.Clone();
	}

	DZS_ExtSpec Spec()
	{
		return DZS_ExtSpecs.Get(m_Url);
	}

	string Title()
	{
		DZS_ExtSpec s = Spec();
		if (s)
			return s.Title();
		return m_Url;
	}

	bool IsChanged() { return m_Changed; }

	void MarkChanged()
	{
		m_Changed = true;
		m_Gen++;
	}

	// ---- paths ----

	// The value at the keys below 'root', null when there is none.
	static DZS_Json At(DZS_Json root, array<string> path)
	{
		DZS_Json cur = root;
		foreach (string k : path)
		{
			if (!cur || !cur.IsObj())
				return null;
			cur = cur.Get(k);
		}
		return cur;
	}

	// Whether a value can be written there: every key on the way is an object or missing.
	static bool Writable(DZS_Json root, array<string> path)
	{
		DZS_Json cur = root;
		for (int i = 0; i < path.Count() - 1; i++)
		{
			if (!cur)
				return true;
			if (!cur.IsObj())
				return false;
			cur = cur.Get(path[i]);
		}
		return !cur || cur.IsObj();
	}

	// Sets the value (null removes it, and objects it leaves empty on the way); false when something else is in the way.
	static bool SetAt(DZS_Json root, array<string> path, DZS_Json value)
	{
		if (!root || !root.IsObj() || !Writable(root, path))
			return false;
		array<DZS_Json> chain = new array<DZS_Json>();
		DZS_Json cur = root;
		int last = path.Count() - 1;
		for (int i = 0; i < last; i++)
		{
			chain.Insert(cur);
			DZS_Json next = cur.Get(path[i]);
			if (!next)
			{
				if (!value)
					return true;
				next = DZS_Json.Obj();
				cur.Set(path[i], next);
			}
			cur = next;
		}
		if (value)
		{
			cur.Set(path[last], value);
			return true;
		}
		cur.Remove(path[last]);
		for (int up = last - 1; up >= 0; up--)
		{
			DZS_Json holder = chain[up];
			DZS_Json child = holder.Get(path[up]);
			if (!child || !child.IsObj() || child.Count() > 0)
				break;
			holder.Remove(path[up]);
		}
		return true;
	}

	// ---- values of a target ----

	// Where an entity target keeps its extensions (found once, by the addresses of the state it was listed from).
	static DZS_ExtOwner OwnerOf(DZS_Instance inst, DZS_ExtTarget t)
	{
		if (t.m_Owner)
			return t.m_Owner;
		if (t.m_Attach == "block")
			t.m_Owner = DZS_EntityExts.BlockOwner(inst, t.m_BlockId);
		else if (t.m_Attach == "change")
			t.m_Owner = DZS_EntityExts.ChangeOwner(inst, DZS_ProjectRefs.ChangeRef(t.m_Json));
		else if (t.m_Attach == "element")
			t.m_Owner = DZS_EntityExts.ElementOwner(inst, t.m_BlockId, t.m_List, t.m_Index);
		return t.m_Owner;
	}

	// The extension's object of the target (the project: its data); null when it has none.
	DZS_Json Holder(DZS_Instance inst, DZS_ExtTarget t)
	{
		if (t.m_Attach == "project")
			return m_Data;
		return DZS_EntityExts.Value(inst, OwnerOf(inst, t), m_Url);
	}

	DZS_Json Value(DZS_Instance inst, DZS_ExtTarget t, DZS_ExtField f)
	{
		DZS_Json h = Holder(inst, t);
		if (!h)
			return null;
		return At(h, f.m_Path);
	}

	// Stores the target's extension object as edited (an empty one removes the entity's entry).
	void Store(DZS_Instance inst, DZS_ExtTarget t, DZS_Json work)
	{
		DZS_Json copy = work.Clone();
		if (t.m_Attach == "project")
		{
			m_Data = copy;
			MarkChanged();
			DZS_ProjectExtensions.Changed(inst);
			return;
		}
		DZS_Json stored = null;
		if (copy.Count() > 0)
			stored = copy;
		DZS_ExtOwner o = OwnerOf(inst, t);
		if (!o)
		{
			DZS_Log.Warn("extensions: " + t.m_Label + " is not in the project any more, " + m_Url + " not stored");
			return;
		}
		DZS_EntityExts.SetValue(inst, o, m_Url, stored);
		DZS_EntityExts.Commit(inst, "Erweiterung bearbeiten", "Edit extension");
		m_Gen++;
	}

	// How many of the fields have a value on the target.
	int CountSet(DZS_Instance inst, DZS_ExtTarget t, array<DZS_ExtField> fields)
	{
		DZS_Json h = Holder(inst, t);
		if (!h)
			return 0;
		int n = 0;
		foreach (DZS_ExtField f : fields)
		{
			if (At(h, f.m_Path))
				n++;
		}
		return n;
	}

	// ---- the project bar ----

	override string GroupMark(DZS_Instance inst, string groupId)
	{
		DZS_ExtSpec s = Spec();
		if (!s)
			return "";
		int now = TickCount(0);
		if (m_MarkGen != m_Gen || m_MarkStateGen != inst.GetGen() || now - m_MarkTick > 20000000 || now < m_MarkTick)
		{
			m_Marks.Clear();
			m_MarkGen = m_Gen;
			m_MarkStateGen = inst.GetGen();
			m_MarkTick = now;
			map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
			for (int i = 0; i < ids.Count(); i++)
			{
				string gid = ids.GetKey(i);
				if (DZS_EntityExts.GroupHas(inst, gid, m_Url))
				{
					DZS_Json title = DZS_Json.Str(s.Title());
					m_Marks.Set(gid, DZS_ProjectRefs.Short(title, 24));
				}
			}
		}
		string mark;
		if (m_Marks.Find(groupId, mark))
			return mark;
		return "";
	}

	override void GroupMenu(DZS_Instance inst, string groupId, array<ref DZS_MenuItem> items)
	{
		DZS_ExtSpec s = Spec();
		if (!s || !s.TouchesBlock("spawnerGroup"))
			return;
		DZS_MenuItem it = new DZS_MenuItem();
		it.m_Tag = "fields";
		it.m_Text = s.Title() + " …";
		items.Insert(it);
	}

	override void GroupMenuChosen(DZS_Instance inst, string groupId, string tag)
	{
		if (tag != "fields")
			return;
		DZS_ExtFieldsDialog d = new DZS_ExtFieldsDialog(this, inst, groupId);
		d.Open();
	}
}
