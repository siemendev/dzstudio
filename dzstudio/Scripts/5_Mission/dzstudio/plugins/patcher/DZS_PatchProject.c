// A patch project (format "dzstudio-project", version 2): own blocks and changes to blocks that already exist in a
// mission's files, each addressed by its identity key. Values stay JSON trees so numbers keep their text.
class DZS_PatchBlock : Managed
{
	string m_Id;
	string m_Kind;
	// a key into the base ({"event": "…"}), or null; m_ParentId names an own block instead
	ref DZS_Json m_Parent;
	string m_ParentId;
	ref DZS_Json m_Data;
}

class DZS_PatchChange : Managed
{
	string m_Op;
	string m_Block;
	ref DZS_Json m_Key;
	string m_Field;
	ref DZS_Json m_From;
	ref DZS_Json m_To;
	bool m_HasFrom;
}

class DZS_PatchProject : Managed
{
	static const string FORMAT = "dzstudio-project";
	static const int VERSION = 2;

	string m_Map;
	string m_Slug;
	ref array<ref DZS_PatchBlock> m_Blocks = new array<ref DZS_PatchBlock>();
	ref array<ref DZS_PatchChange> m_Changes = new array<ref DZS_PatchChange>();

	// Reads the project file; null and 'error' set when it is not a version 2 project.
	static DZS_PatchProject Read(array<string> chunks, out string error)
	{
		error = "";
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (doc.m_Error != "")
		{
			error = doc.ErrorText();
			return null;
		}
		DZS_Json root = doc.m_Root.ToJson();
		return FromJson(root, error);
	}

	static DZS_PatchProject FromJson(DZS_Json root, out string error)
	{
		if (!root.IsObj() || root.GetStr("format") != FORMAT)
		{
			error = "not a dzstudio-project file";
			return null;
		}
		if (root.GetInt("version") != VERSION)
		{
			error = "format version " + root.GetInt("version") + " (this patcher reads " + VERSION + ")";
			return null;
		}
		DZS_PatchProject p = new DZS_PatchProject();
		p.m_Map = root.GetStr("map");
		p.m_Slug = root.GetStr("slug");
		DZS_Json blocks = root.Get("blocks");
		if (blocks && blocks.IsArr())
		{
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json bj = blocks.At(i);
				if (!bj.IsObj())
				{
					error = "block " + i + " is not an object";
					return null;
				}
				DZS_PatchBlock b = new DZS_PatchBlock();
				b.m_Id = bj.GetStr("id");
				b.m_Kind = bj.GetStr("kind");
				DZS_Json par = bj.Get("parent");
				if (par && par.IsStr())
					b.m_ParentId = par.m_Str;
				else if (par && par.IsObj())
					b.m_Parent = par;
				// extensions of the block and of anything inside its data are not the game's: never written into a file
				b.m_Data = WithoutExtensions(bj.Get("data"));
				if (!b.m_Data)
					b.m_Data = DZS_Json.Obj();
				p.m_Blocks.Insert(b);
			}
		}
		DZS_Json changes = root.Get("changes");
		if (changes && changes.IsArr())
		{
			for (int k = 0; k < changes.Count(); k++)
			{
				DZS_Json cj = changes.At(k);
				if (!cj.IsObj())
				{
					error = "change " + k + " is not an object";
					return null;
				}
				DZS_PatchChange c = new DZS_PatchChange();
				c.m_Op = cj.GetStr("op");
				c.m_Block = cj.GetStr("block");
				c.m_Key = cj.Get("key");
				if (!c.m_Key)
					c.m_Key = DZS_Json.Obj();
				c.m_Field = cj.GetStr("field");
				c.m_HasFrom = cj.Has("from");
				// a value set into a file (an object, a list) never takes "extensions" members along either
				c.m_From = WithoutExtensions(cj.Get("from"));
				c.m_To = WithoutExtensions(cj.Get("to"));
				p.m_Changes.Insert(c);
			}
		}
		return p;
	}

	// A value without "extensions" members at any depth: the same value when it has none, else a copy.
	static DZS_Json WithoutExtensions(DZS_Json v)
	{
		if (!v || !HasExtensions(v))
			return v;
		DZS_Json copy = v.Clone();
		StripExtensions(copy);
		return copy;
	}

	protected static bool HasExtensions(DZS_Json v)
	{
		if (v.IsObj())
		{
			if (v.Has(DZS_EntityExts.MEMBER))
				return true;
			for (int i = 0; i < v.m_Keys.Count(); i++)
			{
				DZS_Json member = v.Get(v.m_Keys[i]);
				if (member && HasExtensions(member))
					return true;
			}
			return false;
		}
		if (v.IsArr())
		{
			for (int k = 0; k < v.Count(); k++)
			{
				DZS_Json item = v.At(k);
				if (item && HasExtensions(item))
					return true;
			}
		}
		return false;
	}

	protected static void StripExtensions(DZS_Json v)
	{
		if (v.IsObj())
		{
			v.Remove(DZS_EntityExts.MEMBER);
			for (int i = 0; i < v.m_Keys.Count(); i++)
			{
				DZS_Json member = v.Get(v.m_Keys[i]);
				if (member)
					StripExtensions(member);
			}
			return;
		}
		if (!v.IsArr())
			return;
		for (int k = 0; k < v.Count(); k++)
		{
			DZS_Json item = v.At(k);
			if (item)
				StripExtensions(item);
		}
	}

	DZS_Json ToJson()
	{
		DZS_Json root = DZS_Json.Obj();
		DZS_Json fmt = DZS_Json.Str(FORMAT);
		root.Set("format", fmt);
		DZS_Json ver = DZS_Json.Int(VERSION);
		root.Set("version", ver);
		DZS_Json mapName = DZS_Json.Str(m_Map);
		root.Set("map", mapName);
		if (m_Slug != "")
		{
			DZS_Json slug = DZS_Json.Str(m_Slug);
			root.Set("slug", slug);
		}
		DZS_Json blocks = DZS_Json.Arr();
		foreach (DZS_PatchBlock b : m_Blocks)
		{
			DZS_Json bj = DZS_Json.Obj();
			DZS_Json id = DZS_Json.Str(b.m_Id);
			bj.Set("id", id);
			DZS_Json kind = DZS_Json.Str(b.m_Kind);
			bj.Set("kind", kind);
			DZS_Json par;
			if (b.m_ParentId != "")
				par = DZS_Json.Str(b.m_ParentId);
			else if (b.m_Parent)
				par = b.m_Parent;
			else
				par = DZS_Json.Null();
			bj.Set("parent", par);
			bj.Set("data", b.m_Data);
			blocks.Add(bj);
		}
		root.Set("blocks", blocks);
		DZS_Json changes = DZS_Json.Arr();
		foreach (DZS_PatchChange c : m_Changes)
		{
			DZS_Json cj = DZS_Json.Obj();
			DZS_Json op = DZS_Json.Str(c.m_Op);
			cj.Set("op", op);
			DZS_Json blk = DZS_Json.Str(c.m_Block);
			cj.Set("block", blk);
			cj.Set("key", c.m_Key);
			if (c.m_Field != "")
			{
				DZS_Json fld = DZS_Json.Str(c.m_Field);
				cj.Set("field", fld);
			}
			if (c.m_HasFrom)
			{
				DZS_Json fromV = c.m_From;
				if (!fromV)
					fromV = DZS_Json.Null();
				cj.Set("from", fromV);
			}
			if (c.m_Op == "set")
			{
				DZS_Json toV = c.m_To;
				if (!toV)
					toV = DZS_Json.Null();
				cj.Set("to", toV);
			}
			changes.Add(cj);
		}
		root.Set("changes", changes);
		return root;
	}

	// The project file in the editor's fixed text form.
	array<string> Write()
	{
		DZS_TextWriter w = new DZS_TextWriter();
		DZS_Json root = ToJson();
		DZS_TextForm.Value(w, root, 0);
		w.Append("\n");
		return w.Finish();
	}
}
