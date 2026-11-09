// Extensions of single entities of the open project (docs/extensions/README.md): every block, change and element of a
// block's lists may carry "extensions": {"<spec url>": {…}}. They live on the entity in dzStudio's model, as JSON text in
// a field of the element the entity is built from, so they take part in undo, collab state, backups and duplicates like
// any other value of that element:
// - a spawner group and each of its objects: field "ext"; the loot row of an object with loot: "ext:lootRow";
// - layer elements (Spawns, Effects, Territories): "ext" for the block an element makes ("ext:child" for an own event's
//   child), ChangeSlot ("ext:<field>", "ext:remove") for each change an element makes;
// - loot: an own block on its part of the loot entry's data (DZS_LootEEntry/Container/Point.m_Ext), a change in the
//   loot:entry element's field "ext" (an object of extensions by change ref — the key of a base entry, never an index).
// Opening hands the file's extensions to the model once: layers take theirs while restoring their elements (by block id
// and change ref of the same file), Distribute the rest (spawner groups by block id, objects by their index in the
// opened group, loot changes by ref, loot blocks by their content). DZS_ProjectV2.Build reads them from the model and
// records where each entity's extensions live (DZS_ExtOwner), so the editors address them by the block ids, element
// indexes and change refs of the current state.
class DZS_ExtOwner : Managed
{
	string m_Elem;
	// the element's field holding the extensions as JSON text
	string m_Slot;
	// "": the field holds the entity's extensions object; else the field holds an object of them and this is the key
	string m_Sub;
	// a loot own block: the part of the loot entry in the field "data" ("t", "c2", "c2p5")
	string m_Path;
	// the block's kind, for blocks
	string m_Kind;

	static DZS_ExtOwner Make(string elem, string slot, string sub = "", string path = "")
	{
		DZS_ExtOwner o = new DZS_ExtOwner();
		o.m_Elem = elem;
		o.m_Slot = slot;
		o.m_Sub = sub;
		o.m_Path = path;
		return o;
	}
}

// Where the entities of one state of a project keep their extensions, as DZS_ProjectV2.Build made them.
class DZS_ExtIndex : Managed
{
	DZS_Instance m_Inst;
	int m_Gen = -1;
	ref map<string, ref DZS_ExtOwner> m_Blocks = new map<string, ref DZS_ExtOwner>();
	ref array<string> m_BlockIds = new array<string>();
	ref map<string, ref DZS_ExtOwner> m_Elements = new map<string, ref DZS_ExtOwner>();
	ref array<string> m_ElementKeys = new array<string>();
	ref map<string, ref DZS_ExtOwner> m_Changes = new map<string, ref DZS_ExtOwner>();
	ref array<string> m_ChangeRefs = new array<string>();
	// spawner group element -> block id
	ref map<string, string> m_GroupBlock = new map<string, string>();
	// block id -> the element ids of its "objects" in the order Build wrote them
	ref map<string, ref array<string>> m_Objects = new map<string, ref array<string>>();

	array<ref DZS_ExtOwner> All()
	{
		array<ref DZS_ExtOwner> r = new array<ref DZS_ExtOwner>();
		foreach (string bid : m_BlockIds)
			r.Insert(m_Blocks.Get(bid));
		foreach (string ek : m_ElementKeys)
			r.Insert(m_Elements.Get(ek));
		foreach (string cref : m_ChangeRefs)
			r.Insert(m_Changes.Get(cref));
		return r;
	}
}

class DZS_EntityExts
{
	static const string MEMBER = "extensions";
	static const string SLOT = "ext";

	// being recorded by DZS_ProjectV2.Build
	protected static ref DZS_ExtIndex s_Rec;
	// of the last complete Build
	protected static ref DZS_ExtIndex s_Index;
	// the opened file's entity extensions until the model took them: block id / element key / change ref -> extensions
	protected static ref map<string, ref DZS_Json> s_FileBlocks;
	protected static ref map<string, ref DZS_Json> s_FileElements;
	protected static ref map<string, ref DZS_Json> s_FileChanges;
	// the file's blocks that carry extensions (loot blocks are found by their content)
	protected static ref array<ref DZS_Json> s_FileBlockList;
	// the blocks of the last Build by id, while the opened file's loot blocks are matched by content
	protected static ref map<string, ref DZS_Json> s_Built;
	// edits not committed yet: "<element>\n<field>" -> the field's new text ("" removes the field)
	protected static ref map<string, string> s_Batch;
	protected static ref array<string> s_BatchOrder;
	protected static DZS_Instance s_BatchInst;

	static void Reset()
	{
		s_FileBlocks = new map<string, ref DZS_Json>();
		s_FileElements = new map<string, ref DZS_Json>();
		s_FileChanges = new map<string, ref DZS_Json>();
		s_FileBlockList = new array<ref DZS_Json>();
		s_Batch = new map<string, string>();
		s_BatchOrder = new array<string>();
		s_BatchInst = null;
		s_Index = null;
		s_Rec = null;
		s_Built = null;
	}

	protected static void Ensure()
	{
		if (!s_FileBlocks)
			Reset();
	}

	static string ElemKey(string blockId, string list, int index)
	{
		string k = blockId + "\n" + list;
		return k + "\n" + index.ToString();
	}

	// The field of the extensions of a change an element makes: "ext:<field>", "ext:remove" for a removal.
	static string ChangeSlot(DZS_Json change)
	{
		if (change.GetStr("op") == "remove")
			return SLOT + ":remove";
		return SLOT + ":" + change.GetStr("field");
	}

	// The extensions object of an entity, null when it has none.
	static DZS_Json Of(DZS_Json entity)
	{
		if (!entity || !entity.IsObj())
			return null;
		DZS_Json e = entity.Get(MEMBER);
		if (!e || !e.IsObj() || e.Count() == 0)
			return null;
		return e;
	}

	static string TextOf(DZS_Json v)
	{
		array<string> chunks = v.ToChunks();
		return DZS_Files.Join(chunks);
	}

	protected static DZS_Json Parse(string text)
	{
		if (text == "")
			return null;
		string err;
		DZS_Json j = DZS_Json.ParseText(text, err);
		if (!j || !j.IsObj())
			return null;
		return j;
	}

	// ---- opening ----

	// The project file was read: every entity's extensions, until the model takes them.
	static void Load(DZS_Json root)
	{
		Reset();
		if (!root || !root.IsObj())
			return;
		DZS_Json blocks = root.Get("blocks");
		if (blocks && blocks.IsArr())
		{
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json b = blocks.At(i);
				if (!b || !b.IsObj())
					continue;
				string bid = b.GetStr("id");
				if (bid == "")
					continue;
				DZS_Json be = Of(b);
				if (be)
				{
					DZS_Json beCopy = be.Clone();
					s_FileBlocks.Set(bid, beCopy);
					DZS_Json whole = b.Clone();
					s_FileBlockList.Insert(whole);
				}
				DZS_Json data = b.Get("data");
				if (!data || !data.IsObj())
					continue;
				for (int k = 0; k < data.m_Keys.Count(); k++)
				{
					string list = data.m_Keys[k];
					DZS_Json items = data.Get(list);
					if (!items || !items.IsArr())
						continue;
					for (int n = 0; n < items.Count(); n++)
					{
						DZS_Json ie = Of(items.At(n));
						if (!ie)
							continue;
						DZS_Json ieCopy = ie.Clone();
						s_FileElements.Set(ElemKey(bid, list, n), ieCopy);
					}
				}
			}
		}
		DZS_Json changes = root.Get("changes");
		if (changes && changes.IsArr())
		{
			for (int c = 0; c < changes.Count(); c++)
			{
				DZS_Json ch = changes.At(c);
				DZS_Json ce = Of(ch);
				if (!ce)
					continue;
				DZS_Json ceCopy = ce.Clone();
				s_FileChanges.Set(DZS_ProjectRefs.ChangeRef(ch), ceCopy);
			}
		}
	}

	// A layer restoring its elements from its part of the opened file: the extensions of that block or change of the
	// file go into the element's field 'slot' (taken: nobody else gets them).
	static void TakeInto(map<string, string> fields, string slot, DZS_Json entity)
	{
		Ensure();
		if (!entity || !entity.IsObj())
			return;
		DZS_Json e = null;
		if (entity.Has("op"))
		{
			string cref = DZS_ProjectRefs.ChangeRef(entity);
			if (s_FileChanges.Find(cref, e))
				s_FileChanges.Remove(cref);
		}
		else
		{
			string bid = entity.GetStr("id");
			if (s_FileBlocks.Find(bid, e))
				s_FileBlocks.Remove(bid);
		}
		if (e && e.Count() > 0)
			fields.Set(slot, TextOf(e));
	}

	// The opened project's model is complete: what the layers did not take goes to its entity, then the file's data
	// is let go (an entity the model has not got loses its extensions, as at saving).
	static void Distribute(DZS_Instance inst)
	{
		Ensure();
		if (!inst)
			return;
		int pending = s_FileBlocks.Count() + s_FileElements.Count() + s_FileChanges.Count();
		if (pending == 0)
		{
			Reset();
			return;
		}
		DZS_ExtIndex idx = Index(inst);
		int moved = 0;
		if (idx)
		{
			array<bool> used = new array<bool>();
			for (int u = 0; u < s_FileBlockList.Count(); u++)
				used.Insert(false);
			foreach (string bid : idx.m_BlockIds)
			{
				DZS_ExtOwner bo = idx.m_Blocks.Get(bid);
				DZS_Json be = null;
				if (bo.m_Kind == "spawnerGroup")
				{
					if (s_FileBlocks.Find(bid, be))
						s_FileBlocks.Remove(bid);
				}
				else if (bo.m_Kind.IndexOf("loot") == 0)
					be = ByContent(inst, bid, used);
				if (!be || Exts(inst, bo))
					continue;
				PutExts(inst, bo, be);
				moved++;
			}
			foreach (string ek : idx.m_ElementKeys)
			{
				DZS_Json ee;
				if (!s_FileElements.Find(ek, ee))
					continue;
				s_FileElements.Remove(ek);
				PutExts(inst, idx.m_Elements.Get(ek), ee);
				moved++;
			}
			foreach (string cref : idx.m_ChangeRefs)
			{
				DZS_Json ce;
				if (!s_FileChanges.Find(cref, ce))
					continue;
				s_FileChanges.Remove(cref);
				DZS_ExtOwner co = idx.m_Changes.Get(cref);
				if (Exts(inst, co))
					continue;
				PutExts(inst, co, ce);
				moved++;
			}
		}
		Commit(inst, "", "", true);
		int lost = s_FileBlocks.Count() + s_FileElements.Count() + s_FileChanges.Count();
		DZS_Log.Info("extensions: " + moved + " entity extension(s) put on the model at opening, " + lost + " without an entity");
		Reset();
	}

	// A built loot block's extensions from the file: the file's block of the same kind, parent and data (numbers
	// compared as numbers).
	protected static DZS_Json ByContent(DZS_Instance inst, string bid, array<bool> used)
	{
		DZS_Json built = BuiltBlock(inst, bid);
		if (!built)
			return null;
		string want = ContentOf(built);
		for (int i = 0; i < s_FileBlockList.Count(); i++)
		{
			if (used[i])
				continue;
			DZS_Json fb = s_FileBlockList[i];
			if (ContentOf(fb) != want)
				continue;
			used[i] = true;
			s_FileBlocks.Remove(fb.GetStr("id"));
			return Of(fb);
		}
		return null;
	}

	// The block of the last Build with that id (kept while Distribute runs).
	protected static DZS_Json BuiltBlock(DZS_Instance inst, string bid)
	{
		if (!s_Built)
			return null;
		return s_Built.Get(bid);
	}

	protected static string ContentOf(DZS_Json block)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		w.Append(block.GetStr("kind"));
		w.Append("\n");
		DZS_Json par = block.Get("parent");
		if (par)
			DZS_ProjectRefs.Canon(w, par);
		w.Append("\n");
		DZS_Json data = block.Get("data");
		if (data)
		{
			DZS_Json plain = DZS_PatchProject.WithoutExtensions(data);
			DZS_ProjectRefs.Canon(w, plain);
		}
		array<string> parts = w.Finish();
		return DZS_Files.Join(parts);
	}

	// ---- the index (DZS_ProjectV2.Build records it) ----

	static void BeginRecord(DZS_Instance inst)
	{
		s_Rec = new DZS_ExtIndex();
		s_Rec.m_Inst = inst;
		if (inst)
			s_Rec.m_Gen = inst.GetGen();
	}

	// complete: false when the build left something out (loot), the index is not kept then
	static void EndRecord(DZS_Json root, bool complete)
	{
		if (complete && s_Rec)
		{
			s_Index = s_Rec;
			if (s_FileBlockList && s_FileBlockList.Count() > 0)
			{
				s_Built = new map<string, ref DZS_Json>();
				DZS_Json blocks = root.Get("blocks");
				for (int i = 0; i < blocks.Count(); i++)
				{
					DZS_Json b = blocks.At(i);
					s_Built.Set(b.GetStr("id"), b);
				}
			}
			else
				s_Built = null;
		}
		s_Rec = null;
	}

	// The index of the state as it is now (built again after every change).
	static DZS_ExtIndex Index(DZS_Instance inst)
	{
		if (!inst)
			return null;
		if (s_Index && s_Index.m_Inst == inst && s_Index.m_Gen == inst.GetGen())
			return s_Index;
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return null;
		DZS_ProjectV2.Build(k, inst, "", "");
		if (s_Index && s_Index.m_Inst == inst)
			return s_Index;
		return null;
	}

	// Build: the entity's extensions from where they live, and that place remembered.
	static void Attach(DZS_Json entity, DZS_Instance inst, string elem, string slot)
	{
		DZS_ExtOwner o = DZS_ExtOwner.Make(elem, slot);
		Bind(entity, inst, o, true);
	}

	// read false: the entity carries its extensions already (loot own blocks), only the place is remembered.
	static void Bind(DZS_Json entity, DZS_Instance inst, DZS_ExtOwner o, bool read)
	{
		if (!entity || !entity.IsObj() || !o || o.m_Elem == "")
			return;
		if (read)
		{
			DZS_Json e = Exts(inst, o);
			if (e)
				entity.Set(MEMBER, e);
			else
				entity.Remove(MEMBER);
		}
		if (!s_Rec)
			return;
		if (entity.Has("op"))
		{
			string cref = DZS_ProjectRefs.ChangeRef(entity);
			if (!s_Rec.m_Changes.Contains(cref))
				s_Rec.m_ChangeRefs.Insert(cref);
			s_Rec.m_Changes.Set(cref, o);
			return;
		}
		string bid = entity.GetStr("id");
		o.m_Kind = entity.GetStr("kind");
		if (!s_Rec.m_Blocks.Contains(bid))
			s_Rec.m_BlockIds.Insert(bid);
		s_Rec.m_Blocks.Set(bid, o);
		if (o.m_Kind == "spawnerGroup" && o.m_Path == "" && o.m_Sub == "")
			s_Rec.m_GroupBlock.Set(o.m_Elem, bid);
	}

	// Build: an element of a block's list (a spawner group's object).
	static void AttachElement(DZS_Json item, DZS_Instance inst, string blockId, string list, int index, string elem, string slot)
	{
		DZS_ExtOwner o = DZS_ExtOwner.Make(elem, slot);
		DZS_Json e = Exts(inst, o);
		if (e)
			item.Set(MEMBER, e);
		if (!s_Rec)
			return;
		string key = ElemKey(blockId, list, index);
		if (!s_Rec.m_Elements.Contains(key))
			s_Rec.m_ElementKeys.Insert(key);
		s_Rec.m_Elements.Set(key, o);
		if (list != "objects")
			return;
		array<string> objs = s_Rec.m_Objects.Get(blockId);
		if (!objs)
		{
			objs = new array<string>();
			s_Rec.m_Objects.Set(blockId, objs);
		}
		objs.Insert(elem);
	}

	// ---- reading and writing the model ----

	// A field's text, edits not committed yet included.
	protected static string Raw(DZS_Instance inst, string elem, string slot)
	{
		string k = elem + "\n" + slot;
		string v = "";
		if (s_Batch && s_BatchInst == inst && s_Batch.Find(k, v))
			return v;
		string f = "";
		inst.GetField(elem, slot, f);
		return f;
	}

	// The entity's extensions object (a copy), null when it has none.
	static DZS_Json Exts(DZS_Instance inst, DZS_ExtOwner o)
	{
		if (!inst || !o || o.m_Elem == "")
			return null;
		string text = Raw(inst, o.m_Elem, o.m_Slot);
		if (text == "")
			return null;
		if (o.m_Path != "")
		{
			DZS_LootEEntry entry = DZS_LootEEntry.FromText(text);
			text = PartExt(entry, o.m_Path);
		}
		DZS_Json all = Parse(text);
		if (!all)
			return null;
		DZS_Json r = all;
		if (o.m_Sub != "")
			r = all.Get(o.m_Sub);
		if (!r || !r.IsObj() || r.Count() == 0)
			return null;
		return r;
	}

	// Replaces the entity's extensions object (null or empty: none); committed with Commit.
	static void PutExts(DZS_Instance inst, DZS_ExtOwner o, DZS_Json exts)
	{
		if (!inst || !o || o.m_Elem == "")
			return;
		string text = "";
		if (exts && exts.IsObj() && exts.Count() > 0)
			text = TextOf(exts);
		string cur = Raw(inst, o.m_Elem, o.m_Slot);
		string next = text;
		if (o.m_Path != "")
		{
			if (cur == "")
				return;
			DZS_LootEEntry entry = DZS_LootEEntry.FromText(cur);
			if (!SetPartExt(entry, o.m_Path, text))
				return;
			next = entry.ToText();
		}
		else if (o.m_Sub != "")
		{
			DZS_Json all = Parse(cur);
			if (!all)
				all = DZS_Json.Obj();
			if (text == "")
				all.Remove(o.m_Sub);
			else
			{
				DZS_Json copy = exts.Clone();
				all.Set(o.m_Sub, copy);
			}
			next = "";
			if (all.Count() > 0)
				next = TextOf(all);
		}
		if (next == cur)
			return;
		Stage(inst, o.m_Elem, o.m_Slot, next);
	}

	// One extension's value on the entity (null when it has none).
	static DZS_Json Value(DZS_Instance inst, DZS_ExtOwner o, string url)
	{
		DZS_Json e = Exts(inst, o);
		if (!e)
			return null;
		return e.Get(url);
	}

	// One extension's value replaced (null removes it); committed with Commit.
	static void SetValue(DZS_Instance inst, DZS_ExtOwner o, string url, DZS_Json value)
	{
		DZS_Json work = Exts(inst, o);
		if (!work)
		{
			if (!value)
				return;
			work = DZS_Json.Obj();
		}
		if (value)
		{
			DZS_Json copy = value.Clone();
			work.Set(url, copy);
		}
		else
			work.Remove(url);
		PutExts(inst, o, work);
	}

	protected static void Stage(DZS_Instance inst, string elem, string slot, string text)
	{
		if (!s_Batch || s_BatchInst != inst)
		{
			s_Batch = new map<string, string>();
			s_BatchOrder = new array<string>();
			s_BatchInst = inst;
		}
		string k = elem + "\n" + slot;
		if (!s_Batch.Contains(k))
			s_BatchOrder.Insert(k);
		s_Batch.Set(k, text);
	}

	// The staged edits as one step of the history (noStep: a system change, as part of opening); false when nothing
	// was written.
	static bool Commit(DZS_Instance inst, string nameDe, string nameEn, bool noStep = false)
	{
		if (!inst || !s_Batch || s_BatchInst != inst || s_BatchOrder.Count() == 0)
		{
			s_Batch = null;
			s_BatchOrder = null;
			s_BatchInst = null;
			return false;
		}
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		int n = 0;
		foreach (string k : s_BatchOrder)
		{
			int nl = k.IndexOf("\n");
			string elem = k.Substring(0, nl);
			string slot = k.Substring(nl + 1, k.Length() - nl - 1);
			string text = s_Batch.Get(k);
			bool did;
			if (text == "")
				did = b.ClearField(elem, slot);
			else
				did = b.SetField(elem, slot, text);
			if (did)
				n++;
		}
		s_Batch = null;
		s_BatchOrder = null;
		s_BatchInst = null;
		if (n == 0)
			return false;
		DZS_SubmitResult r;
		DZS_Operation op = b.GetOperation();
		if (noStep)
			r = inst.SubmitSystem(op);
		else
			r = b.Commit(nameDe, nameEn);
		if (!r || !r.Ok())
		{
			DZS_Log.Warn("extensions: " + n + " edit(s) not applied");
			return false;
		}
		return true;
	}

	// ---- loot entry parts ----

	// "c2p5" -> 2, 5; "c2" -> 2, -1; "t" -> -1, -1
	protected static void PathParts(string path, out int ci, out int pi)
	{
		ci = -1;
		pi = -1;
		if (path == "" || path.Get(0) != "c")
			return;
		int pAt = path.IndexOf("p");
		if (pAt < 0)
		{
			ci = path.Substring(1, path.Length() - 1).ToInt();
			return;
		}
		ci = path.Substring(1, pAt - 1).ToInt();
		pi = path.Substring(pAt + 1, path.Length() - pAt - 1).ToInt();
	}

	protected static string PartExt(DZS_LootEEntry e, string path)
	{
		if (path == "t")
			return e.m_Ext;
		int ci;
		int pi;
		PathParts(path, ci, pi);
		if (ci < 0 || ci >= e.m_Containers.Count())
			return "";
		DZS_LootEContainer c = e.m_Containers[ci];
		if (pi < 0)
			return c.m_Ext;
		if (pi >= c.m_Points.Count())
			return "";
		return c.m_Points[pi].m_Ext;
	}

	protected static bool SetPartExt(DZS_LootEEntry e, string path, string text)
	{
		if (path == "t")
		{
			e.m_Ext = text;
			return true;
		}
		int ci;
		int pi;
		PathParts(path, ci, pi);
		if (ci < 0 || ci >= e.m_Containers.Count())
			return false;
		DZS_LootEContainer c = e.m_Containers[ci];
		if (pi < 0)
		{
			c.m_Ext = text;
			return true;
		}
		if (pi >= c.m_Points.Count())
			return false;
		c.m_Points[pi].m_Ext = text;
		return true;
	}

	// ---- by the addresses of the project file (block id, element index, change ref) ----

	static DZS_ExtOwner BlockOwner(DZS_Instance inst, string blockId)
	{
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return null;
		return idx.m_Blocks.Get(blockId);
	}

	static DZS_ExtOwner ElementOwner(DZS_Instance inst, string blockId, string list, int index)
	{
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return null;
		return idx.m_Elements.Get(ElemKey(blockId, list, index));
	}

	static DZS_ExtOwner ChangeOwner(DZS_Instance inst, string cref)
	{
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return null;
		return idx.m_Changes.Get(cref);
	}

	static DZS_Json Block(DZS_Instance inst, string blockId, string url)
	{
		return Value(inst, BlockOwner(inst, blockId), url);
	}

	static void SetBlock(DZS_Instance inst, string blockId, string url, DZS_Json value)
	{
		DZS_ExtOwner o = BlockOwner(inst, blockId);
		if (!o)
		{
			DZS_Log.Warn("extensions: block " + blockId + " not in the project, value of " + url + " not stored");
			return;
		}
		SetValue(inst, o, url, value);
	}

	static DZS_Json Element(DZS_Instance inst, string blockId, string list, int index, string url)
	{
		return Value(inst, ElementOwner(inst, blockId, list, index), url);
	}

	static void SetElement(DZS_Instance inst, string blockId, string list, int index, string url, DZS_Json value)
	{
		DZS_ExtOwner o = ElementOwner(inst, blockId, list, index);
		if (!o)
		{
			string where = ElemKey(blockId, list, index);
			DZS_Log.Warn("extensions: element " + where + " not in the project, " + url + " not stored");
			return;
		}
		SetValue(inst, o, url, value);
	}

	static DZS_Json Change(DZS_Instance inst, DZS_Json change, string url)
	{
		return ChangeByRef(inst, DZS_ProjectRefs.ChangeRef(change), url);
	}

	static void SetChange(DZS_Instance inst, DZS_Json change, string url, DZS_Json value)
	{
		SetChangeByRef(inst, DZS_ProjectRefs.ChangeRef(change), url, value);
	}

	static DZS_Json ChangeByRef(DZS_Instance inst, string cref, string url)
	{
		return Value(inst, ChangeOwner(inst, cref), url);
	}

	static void SetChangeByRef(DZS_Instance inst, string cref, string url, DZS_Json value)
	{
		DZS_ExtOwner o = ChangeOwner(inst, cref);
		if (!o)
		{
			DZS_Log.Warn("extensions: change " + cref + " not in the project (or not editable), value of " + url + " not stored");
			return;
		}
		SetValue(inst, o, url, value);
	}

	// The values of the extension on a block's list elements by "<list>[<index>]".
	static map<string, ref DZS_Json> ElementsOf(DZS_Instance inst, string blockId, string url)
	{
		map<string, ref DZS_Json> r = new map<string, ref DZS_Json>();
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return r;
		string prefix = blockId + "\n";
		foreach (string key : idx.m_ElementKeys)
		{
			if (key.IndexOf(prefix) != 0)
				continue;
			DZS_Json v = Value(inst, idx.m_Elements.Get(key), url);
			if (!v)
				continue;
			string rest = key.Substring(prefix.Length(), key.Length() - prefix.Length());
			int nl = rest.IndexOf("\n");
			if (nl <= 0)
				continue;
			string list = rest.Substring(0, nl);
			string at = rest.Substring(nl + 1, rest.Length() - nl - 1);
			r.Set(list + "[" + at + "]", v);
		}
		return r;
	}

	// Ids of blocks that hold a value of the extension.
	static array<string> BlockIds(DZS_Instance inst, string url)
	{
		array<string> r = new array<string>();
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return r;
		foreach (string bid : idx.m_BlockIds)
		{
			if (Value(inst, idx.m_Blocks.Get(bid), url))
				r.Insert(bid);
		}
		return r;
	}

	// Refs of changes that hold a value of the extension.
	static array<string> ChangeRefs(DZS_Instance inst, string url)
	{
		array<string> r = new array<string>();
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return r;
		foreach (string cref : idx.m_ChangeRefs)
		{
			if (Value(inst, idx.m_Changes.Get(cref), url))
				r.Insert(cref);
		}
		return r;
	}

	// The scene group of a spawner group block, "" when the block is no spawner group.
	static string GroupOf(DZS_Instance inst, string blockId)
	{
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return "";
		for (int i = 0; i < idx.m_GroupBlock.Count(); i++)
		{
			if (idx.m_GroupBlock.GetElement(i) == blockId)
				return idx.m_GroupBlock.GetKey(i);
		}
		return "";
	}

	// The objects of a group in the order the project file writes them.
	static array<string> Objects(DZS_Instance inst, string gid)
	{
		array<string> r = new array<string>();
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return r;
		string bid;
		if (!idx.m_GroupBlock.Find(gid, bid))
			return r;
		array<string> objs = idx.m_Objects.Get(bid);
		if (objs)
			r.Copy(objs);
		return r;
	}

	// Whether a spawner group or one of its objects holds a value of the extension (read from the elements directly:
	// marks in the project bar must not build the project after every change).
	static bool GroupHas(DZS_Instance inst, string gid, string url)
	{
		if (!inst)
			return false;
		if (HasIn(inst, gid, url))
			return true;
		array<string> kids = inst.GetChildren(gid);
		foreach (string oid : kids)
		{
			if (inst.GetKind(oid) == DZS_KIND_OBJECT && HasIn(inst, oid, url))
				return true;
		}
		return false;
	}

	protected static bool HasIn(DZS_Instance inst, string elem, string url)
	{
		string text = Raw(inst, elem, SLOT);
		if (text == "" || text.IndexOf(url) < 0)
			return false;
		DZS_ExtOwner o = DZS_ExtOwner.Make(elem, SLOT);
		return Value(inst, o, url) != null;
	}

	// Every URL an entity of the open project holds, in first-seen order (the opened file's not taken yet included).
	static array<string> Urls(DZS_Instance inst = null)
	{
		Ensure();
		array<string> r = new array<string>();
		DZS_Instance use = inst;
		if (!use && DZS_Kernel.Get())
			use = DZS_Kernel.Get().GetShown();
		DZS_ExtIndex idx = null;
		if (use)
			idx = Index(use);
		if (idx)
		{
			array<ref DZS_ExtOwner> all = idx.All();
			foreach (DZS_ExtOwner o : all)
			{
				DZS_Json e = Exts(use, o);
				if (e)
					AddKeys(e, r);
			}
		}
		CollectUrls(s_FileBlocks, r);
		CollectUrls(s_FileElements, r);
		CollectUrls(s_FileChanges, r);
		return r;
	}

	// The URLs on entities of the opened file, before the model took them.
	static array<string> FileUrls()
	{
		Ensure();
		array<string> r = new array<string>();
		CollectUrls(s_FileBlocks, r);
		CollectUrls(s_FileElements, r);
		CollectUrls(s_FileChanges, r);
		return r;
	}

	protected static void AddKeys(DZS_Json e, array<string> into)
	{
		foreach (string u : e.m_Keys)
		{
			if (into.Find(u) < 0)
				into.Insert(u);
		}
	}

	protected static void CollectUrls(map<string, ref DZS_Json> from, array<string> into)
	{
		for (int i = 0; i < from.Count(); i++)
		{
			DZS_Json e = from.GetElement(i);
			if (e)
				AddKeys(e, into);
		}
	}

	// Removing an extension from the project: its values on every entity, as one step.
	static void RemoveUrl(DZS_Instance inst, string url)
	{
		Ensure();
		DZS_ExtIndex idx = Index(inst);
		if (idx)
		{
			array<ref DZS_ExtOwner> all = idx.All();
			foreach (DZS_ExtOwner o : all)
				SetValue(inst, o, url, null);
		}
		Commit(inst, "Erweiterung entfernen", "Remove extension");
	}

	// Every owner of an entity that holds a value of the extension (renaming a name the values refer to).
	static array<ref DZS_ExtOwner> OwnersWith(DZS_Instance inst, string url)
	{
		array<ref DZS_ExtOwner> r = new array<ref DZS_ExtOwner>();
		DZS_ExtIndex idx = Index(inst);
		if (!idx)
			return r;
		array<ref DZS_ExtOwner> all = idx.All();
		foreach (DZS_ExtOwner o : all)
		{
			if (Value(inst, o, url))
				r.Insert(o);
		}
		return r;
	}
}
