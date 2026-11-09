// Extensions of single entities of the open project (docs/extensions/README.md): every block, change and element of a
// block's lists may carry "extensions": {"<spec url>": {…}}. dzStudio rebuilds blocks and changes from the scene on
// every save (DZS_ProjectV2.Build), so the store keeps these from opening and puts them back on the rebuilt entities:
// spawner groups and their objects by their scene ids (an object keeps its data when it is moved or the list
// changes), other blocks by id, their list elements by list and index, changes by their ref (DZS_ProjectRefs).
// An entity that is gone at saving loses its data.
class DZS_EntityExts
{
	static const string MEMBER = "extensions";

	// group id in the scene -> extensions object
	protected static ref map<string, ref DZS_Json> s_Groups;
	// object id in the scene -> extensions object
	protected static ref map<string, ref DZS_Json> s_Objects;
	// block id -> extensions object (blocks other than spawner groups, and groups not found in the scene)
	protected static ref map<string, ref DZS_Json> s_Blocks;
	// "<block id>\n<list>\n<index>" -> extensions object
	protected static ref map<string, ref DZS_Json> s_Elements;
	// change ref -> extensions object
	protected static ref map<string, ref DZS_Json> s_Changes;
	// spawner group blocks of the file, still to be found in the scene
	protected static ref array<string> s_Spawners;
	protected static bool s_Pending;

	static void Reset()
	{
		s_Groups = new map<string, ref DZS_Json>();
		s_Objects = new map<string, ref DZS_Json>();
		s_Blocks = new map<string, ref DZS_Json>();
		s_Elements = new map<string, ref DZS_Json>();
		s_Changes = new map<string, ref DZS_Json>();
		s_Spawners = new array<string>();
		s_Pending = false;
	}

	protected static void Ensure()
	{
		if (!s_Groups)
			Reset();
	}

	static string ElemKey(string blockId, string list, int index)
	{
		string k = blockId + "\n" + list;
		return k + "\n" + index.ToString();
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

	// Opening a project file: every entity's extensions.
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
				if (b.GetStr("kind") == "spawnerGroup")
					s_Spawners.Insert(bid);
				DZS_Json be = Of(b);
				if (be)
				{
					DZS_Json beCopy = be.Clone();
					s_Blocks.Set(bid, beCopy);
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
						DZS_Json item = items.At(n);
						DZS_Json ie = Of(item);
						if (!ie)
							continue;
						DZS_Json ieCopy = ie.Clone();
						s_Elements.Set(ElemKey(bid, list, n), ieCopy);
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
				s_Changes.Set(DZS_ProjectRefs.ChangeRef(ch), ceCopy);
			}
		}
		s_Pending = s_Spawners.Count() > 0;
	}

	// The opened project's scene is shown: spawner groups and their objects take their data by scene id.
	static void Resolve(DZS_Instance inst)
	{
		Ensure();
		if (!s_Pending || !inst)
			return;
		s_Pending = false;
		map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
		int moved = 0;
		for (int i = 0; i < ids.Count(); i++)
		{
			string gid = ids.GetKey(i);
			string bid = ids.GetElement(i);
			if (s_Spawners.Find(bid) < 0)
				continue;
			DZS_Json ge;
			if (s_Blocks.Find(bid, ge))
			{
				s_Groups.Set(gid, ge);
				s_Blocks.Remove(bid);
				moved++;
			}
			array<string> objs = Objects(inst, gid);
			for (int n = 0; n < objs.Count(); n++)
			{
				string key = ElemKey(bid, "objects", n);
				DZS_Json oe;
				if (!s_Elements.Find(key, oe))
					continue;
				s_Objects.Set(objs[n], oe);
				s_Elements.Remove(key);
				moved++;
			}
		}
		DZS_Log.Info("extensions: " + moved + " group/object extension(s) bound to the scene");
	}

	// The objects of a group in the order the project file writes them.
	static array<string> Objects(DZS_Instance inst, string gid)
	{
		array<string> r = new array<string>();
		array<string> kids = inst.GetChildren(gid);
		foreach (string oid : kids)
		{
			if (inst.GetKind(oid) == DZS_KIND_OBJECT)
				r.Insert(oid);
		}
		return r;
	}

	// The scene group of a spawner group block, "" when the block is no spawner group.
	static string GroupOf(DZS_Instance inst, string blockId)
	{
		if (!inst)
			return "";
		map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
		for (int i = 0; i < ids.Count(); i++)
		{
			if (ids.GetElement(i) == blockId)
				return ids.GetKey(i);
		}
		return "";
	}

	// ---- reading and writing one extension's value of an entity ----

	protected static DZS_Json Value(map<string, ref DZS_Json> from, string key, string url)
	{
		DZS_Json e;
		if (!from.Find(key, e) || !e)
			return null;
		return e.Get(url);
	}

	// value null removes the extension's entry of the entity
	protected static void Put(map<string, ref DZS_Json> into, string key, string url, DZS_Json value)
	{
		DZS_Json e;
		into.Find(key, e);
		if (!value)
		{
			if (!e)
				return;
			e.Remove(url);
			if (e.Count() == 0)
				into.Remove(key);
			return;
		}
		if (!e)
		{
			e = DZS_Json.Obj();
			into.Set(key, e);
		}
		e.Set(url, value);
	}

	static DZS_Json Block(DZS_Instance inst, string blockId, string url)
	{
		Ensure();
		Resolve(inst);
		string gid = GroupOf(inst, blockId);
		if (gid != "")
			return Value(s_Groups, gid, url);
		return Value(s_Blocks, blockId, url);
	}

	static void SetBlock(DZS_Instance inst, string blockId, string url, DZS_Json value)
	{
		Ensure();
		Resolve(inst);
		string gid = GroupOf(inst, blockId);
		if (gid != "")
			Put(s_Groups, gid, url, value);
		else
			Put(s_Blocks, blockId, url, value);
	}

	// An element of a block's list; a spawner group's objects by their scene id.
	static DZS_Json Element(DZS_Instance inst, string blockId, string list, int index, string url)
	{
		Ensure();
		Resolve(inst);
		string oid = ObjectOf(inst, blockId, list, index);
		if (oid != "")
			return Value(s_Objects, oid, url);
		return Value(s_Elements, ElemKey(blockId, list, index), url);
	}

	static void SetElement(DZS_Instance inst, string blockId, string list, int index, string url, DZS_Json value)
	{
		Ensure();
		Resolve(inst);
		string oid = ObjectOf(inst, blockId, list, index);
		if (oid != "")
			Put(s_Objects, oid, url, value);
		else
			Put(s_Elements, ElemKey(blockId, list, index), url, value);
	}

	protected static string ObjectOf(DZS_Instance inst, string blockId, string list, int index)
	{
		if (list != "objects")
			return "";
		string gid = GroupOf(inst, blockId);
		if (gid == "")
			return "";
		array<string> objs = Objects(inst, gid);
		if (index < 0 || index >= objs.Count())
			return "";
		return objs[index];
	}

	static DZS_Json Change(DZS_Json change, string url)
	{
		Ensure();
		return Value(s_Changes, DZS_ProjectRefs.ChangeRef(change), url);
	}

	static void SetChange(DZS_Json change, string url, DZS_Json value)
	{
		Ensure();
		Put(s_Changes, DZS_ProjectRefs.ChangeRef(change), url, value);
	}

	static DZS_Json ChangeByRef(string cref, string url)
	{
		Ensure();
		return Value(s_Changes, cref, url);
	}

	static void SetChangeByRef(string cref, string url, DZS_Json value)
	{
		Ensure();
		Put(s_Changes, cref, url, value);
	}

	// The values of the extension on a block's list elements by "<list>[<index>]".
	static map<string, ref DZS_Json> ElementsOf(DZS_Instance inst, string blockId, string url)
	{
		Ensure();
		Resolve(inst);
		map<string, ref DZS_Json> r = new map<string, ref DZS_Json>();
		string gid = GroupOf(inst, blockId);
		if (gid != "")
		{
			array<string> objs = Objects(inst, gid);
			for (int n = 0; n < objs.Count(); n++)
			{
				DZS_Json ov = Value(s_Objects, objs[n], url);
				if (ov)
					r.Set("objects[" + n.ToString() + "]", ov);
			}
		}
		string prefix = blockId + "\n";
		for (int i = 0; i < s_Elements.Count(); i++)
		{
			string key = s_Elements.GetKey(i);
			if (key.IndexOf(prefix) != 0)
				continue;
			DZS_Json e = s_Elements.GetElement(i);
			DZS_Json ev = null;
			if (e)
				ev = e.Get(url);
			if (!ev)
				continue;
			string rest = key.Substring(prefix.Length(), key.Length() - prefix.Length());
			int nl = rest.IndexOf("\n");
			if (nl <= 0)
				continue;
			string list = rest.Substring(0, nl);
			string idx = rest.Substring(nl + 1, rest.Length() - nl - 1);
			r.Set(list + "[" + idx + "]", ev);
		}
		return r;
	}

	// Ids of blocks that hold a value of the extension (groups by their current block id).
	static array<string> BlockIds(DZS_Instance inst, string url)
	{
		Ensure();
		Resolve(inst);
		array<string> r = new array<string>();
		if (inst)
		{
			map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
			for (int i = 0; i < ids.Count(); i++)
			{
				if (Value(s_Groups, ids.GetKey(i), url))
					r.Insert(ids.GetElement(i));
			}
		}
		for (int k = 0; k < s_Blocks.Count(); k++)
		{
			DZS_Json e = s_Blocks.GetElement(k);
			if (e && e.Has(url) && r.Find(s_Blocks.GetKey(k)) < 0)
				r.Insert(s_Blocks.GetKey(k));
		}
		return r;
	}

	// Every value of the extension on any entity, to change in place (renaming a name they refer to).
	static array<ref DZS_Json> Values(string url)
	{
		Ensure();
		array<ref DZS_Json> r = new array<ref DZS_Json>();
		CollectValues(s_Groups, url, r);
		CollectValues(s_Objects, url, r);
		CollectValues(s_Blocks, url, r);
		CollectValues(s_Elements, url, r);
		CollectValues(s_Changes, url, r);
		return r;
	}

	protected static void CollectValues(map<string, ref DZS_Json> from, string url, array<ref DZS_Json> into)
	{
		for (int i = 0; i < from.Count(); i++)
		{
			DZS_Json e = from.GetElement(i);
			if (!e)
				continue;
			DZS_Json v = e.Get(url);
			if (v)
				into.Insert(v);
		}
	}

	// Change refs that hold a value of the extension (also of changes the project no longer has).
	static array<string> ChangeRefs(string url)
	{
		Ensure();
		array<string> r = new array<string>();
		for (int i = 0; i < s_Changes.Count(); i++)
		{
			DZS_Json e = s_Changes.GetElement(i);
			if (e && e.Has(url))
				r.Insert(s_Changes.GetKey(i));
		}
		return r;
	}

	static void DropChange(string cref, string url)
	{
		Ensure();
		Put(s_Changes, cref, url, null);
	}

	// Removing an extension from the project: its values on every entity.
	static void RemoveUrl(string url)
	{
		Ensure();
		DropAll(s_Groups, url);
		DropAll(s_Objects, url);
		DropAll(s_Blocks, url);
		DropAll(s_Elements, url);
		DropAll(s_Changes, url);
	}

	protected static void DropAll(map<string, ref DZS_Json> from, string url)
	{
		array<string> keys = new array<string>();
		for (int i = 0; i < from.Count(); i++)
			keys.Insert(from.GetKey(i));
		foreach (string k : keys)
			Put(from, k, url, null);
	}

	// Every URL an entity of the open project holds, in first-seen order.
	static array<string> Urls()
	{
		Ensure();
		array<string> r = new array<string>();
		CollectUrls(s_Groups, r);
		CollectUrls(s_Blocks, r);
		CollectUrls(s_Objects, r);
		CollectUrls(s_Elements, r);
		CollectUrls(s_Changes, r);
		return r;
	}

	protected static void CollectUrls(map<string, ref DZS_Json> from, array<string> into)
	{
		for (int i = 0; i < from.Count(); i++)
		{
			DZS_Json e = from.GetElement(i);
			if (!e)
				continue;
			foreach (string u : e.m_Keys)
			{
				if (into.Find(u) < 0)
					into.Insert(u);
			}
		}
	}

	// ---- onto the rebuilt project (DZS_ProjectV2.Build) ----

	static void PutOn(DZS_Json entity, DZS_Json exts)
	{
		if (!entity || !entity.IsObj() || !exts || exts.Count() == 0)
			return;
		DZS_Json copy = exts.Clone();
		entity.Set(MEMBER, copy);
	}

	static DZS_Json OfGroup(string gid)
	{
		Ensure();
		DZS_Json e;
		s_Groups.Find(gid, e);
		return e;
	}

	static DZS_Json OfObject(string oid)
	{
		Ensure();
		DZS_Json e;
		s_Objects.Find(oid, e);
		return e;
	}

	// Blocks other than spawner groups (those carry theirs from DZS_ProjectV2.Groups), their list elements, changes.
	static void Attach(DZS_Json root)
	{
		Ensure();
		DZS_Json blocks = root.Get("blocks");
		if (blocks && blocks.IsArr())
		{
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json b = blocks.At(i);
				if (!b || !b.IsObj() || b.GetStr("kind") == "spawnerGroup")
					continue;
				string bid = b.GetStr("id");
				DZS_Json be;
				if (s_Blocks.Find(bid, be))
					PutOn(b, be);
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
						DZS_Json ie;
						if (!s_Elements.Find(ElemKey(bid, list, n), ie))
							continue;
						DZS_Json item = items.At(n);
						PutOn(item, ie);
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
				DZS_Json ce;
				if (s_Changes.Find(DZS_ProjectRefs.ChangeRef(ch), ce))
					PutOn(ch, ce);
			}
		}
	}

	// A block's data without the extensions of its list elements, for the patcher (a copy only when there are some).
	static DZS_Json Plain(DZS_Json data)
	{
		if (!data || !data.IsObj())
			return data;
		bool any = false;
		for (int k = 0; k < data.m_Keys.Count() && !any; k++)
		{
			DZS_Json items = data.Get(data.m_Keys[k]);
			if (!items || !items.IsArr())
				continue;
			for (int n = 0; n < items.Count(); n++)
			{
				DZS_Json item = items.At(n);
				if (item && item.IsObj() && item.Has(MEMBER))
				{
					any = true;
					break;
				}
			}
		}
		if (!any)
			return data;
		DZS_Json copy = data.Clone();
		for (int j = 0; j < copy.m_Keys.Count(); j++)
		{
			DZS_Json list = copy.Get(copy.m_Keys[j]);
			if (!list || !list.IsArr())
				continue;
			for (int m = 0; m < list.Count(); m++)
			{
				DZS_Json el = list.At(m);
				if (el && el.IsObj())
					el.Remove(MEMBER);
			}
		}
		return copy;
	}
}
