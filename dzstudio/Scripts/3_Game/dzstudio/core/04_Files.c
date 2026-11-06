// File access as the game allows it: create one folder level per call, write
// (truncating), copy (also over an existing file), delete files and empty folders, list. No rename.
// Files are read raw in blocks (CR, BOM and every byte stay as they are).
class DZS_Files
{
	static const int BLOCK = 1000;

	static bool Exists(string path)
	{
		return FileExist(path);
	}

	// Raw content as chunks of at most BLOCK bytes; null if the file cannot be opened.
	static array<string> Read(string path)
	{
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (!fh)
			return null;
		array<string> chunks = new array<string>();
		while (true)
		{
			string part;
			int n = ReadFile(fh, part, BLOCK);
			if (n <= 0)
				break;
			chunks.Insert(part);
		}
		CloseFile(fh);
		return chunks;
	}

	static bool Write(string path, array<string> chunks)
	{
		FileHandle fh = OpenFile(path, FileMode.WRITE);
		if (!fh)
			return false;
		foreach (string c : chunks)
			FPrint(fh, c);
		CloseFile(fh);
		return true;
	}

	static bool Copy(string src, string dst)
	{
		return CopyFile(src, dst);
	}

	static bool Delete(string path)
	{
		return DeleteFile(path);
	}

	static int Length(array<string> chunks)
	{
		int n = 0;
		foreach (string c : chunks)
			n += c.Length();
		return n;
	}

	static bool Same(array<string> a, array<string> b)
	{
		if (!a || !b)
			return false;
		string ja = Join(a);
		string jb = Join(b);
		return ja == jb;
	}

	// Same bytes in both, however the chunks are cut (compares piece by piece, no joined copy).
	static bool SameContent(array<string> a, array<string> b)
	{
		if (!a || !b || Length(a) != Length(b))
			return false;
		int ia = 0;
		int ib = 0;
		int oa = 0;
		int ob = 0;
		while (ia < a.Count() && ib < b.Count())
		{
			string ca = a[ia];
			string cb = b[ib];
			int take = Math.Min(ca.Length() - oa, cb.Length() - ob);
			if (take > 0)
			{
				string pa = ca.Substring(oa, take);
				string pb = cb.Substring(ob, take);
				if (pa != pb)
					return false;
			}
			oa += take;
			ob += take;
			if (oa >= ca.Length())
			{
				ia++;
				oa = 0;
			}
			if (ob >= cb.Length())
			{
				ib++;
				ob = 0;
			}
		}
		return true;
	}

	static string Join(array<string> chunks)
	{
		string s = "";
		foreach (string c : chunks)
			s += c;
		return s;
	}

	// Content checksum (FNV-1a over the bytes, with the length).
	static string Checksum(array<string> chunks)
	{
		if (!chunks)
			return "";
		int h = -2128831035;
		int len = 0;
		foreach (string c : chunks)
		{
			int n = c.Length();
			for (int i = 0; i < n; i++)
			{
				int b = c.Get(i).ToAscii();
				if (b < 0)
					b += 256;
				h = h ^ b;
				h = h * 16777619;
			}
			len += n;
		}
		string lt = len.ToString();
		string ht = h.ToString();
		return lt + ":" + ht;
	}

	// Creates every missing folder of a path below a prefix like "$saves:" (one level per step).
	static void MakeDirs(string path)
	{
		int colon = path.IndexOf(":");
		string done = path.Substring(0, colon + 1);
		string rest = path.Substring(colon + 1, path.Length() - colon - 1);
		rest.Replace("/", "\\");
		array<string> parts = new array<string>();
		rest.Split("\\", parts);
		foreach (string p : parts)
		{
			if (p == "")
				continue;
			if (done.Length() > colon + 1)
				done += "\\";
			done += p;
			MakeDirectory(done);
		}
	}

	// Entries of one folder: names, and whether each is a folder.
	static void List(string dir, out array<string> names, out array<bool> folders)
	{
		names = new array<string>();
		folders = new array<bool>();
		string name;
		FileAttr attr;
		FindFileHandle h = FindFile(dir + "\\*", name, attr, FindFileFlags.DIRECTORIES);
		if (!h)
			return;
		while (true)
		{
			if (name != "" && name != "." && name != "..")
			{
				names.Insert(name);
				folders.Insert((attr & FileAttr.DIRECTORY) != 0);
			}
			if (!FindNextFile(h, name, attr))
				break;
		}
		CloseFindFile(h);
	}

	// Every file below dir, as paths relative to dir with "/" between folders.
	static array<string> ListTree(string dir)
	{
		array<string> result = new array<string>();
		CollectTree(dir, "", result);
		return result;
	}

	protected static void CollectTree(string dir, string rel, array<string> result)
	{
		array<string> names;
		array<bool> folders;
		List(dir, names, folders);
		for (int i = 0; i < names.Count(); i++)
		{
			string r = names[i];
			if (rel != "")
				r = rel + "/" + names[i];
			if (folders[i])
				CollectTree(dir + "\\" + names[i], r, result);
			else
				result.Insert(r);
		}
	}

	static bool IsFolder(string dir)
	{
		int slash = Math.Max(dir.LastIndexOf("\\"), dir.LastIndexOf("/"));
		string parent = dir.Substring(0, slash);
		string leaf = dir.Substring(slash + 1, dir.Length() - slash - 1);
		array<string> names;
		array<bool> folders;
		List(parent, names, folders);
		string want = leaf;
		want.ToLower();
		for (int i = 0; i < names.Count(); i++)
		{
			string n = names[i];
			n.ToLower();
			if (n == want && folders[i])
				return true;
		}
		return false;
	}

	// "Ordner löschen": every file, then the folders from inside out.
	static bool DeleteTree(string dir)
	{
		array<string> names;
		array<bool> folders;
		List(dir, names, folders);
		bool ok = true;
		for (int i = 0; i < names.Count(); i++)
		{
			string p = dir + "\\" + names[i];
			if (folders[i])
				ok = DeleteTree(p) && ok;
			else
				ok = DeleteFile(p) && ok;
		}
		return DeleteFile(dir) && ok;
	}

	// Splits text into chunks of at most BLOCK bytes at UTF-8 boundaries.
	static array<string> Chunk(string s)
	{
		DZS_TextWriter w = new DZS_TextWriter(BLOCK);
		w.Append(s);
		return w.Finish();
	}
}

// DZS_Files.Checksum in slices over several frames: whole chunks until about 'ms' milliseconds have passed.
class DZS_FileSum : Managed
{
	protected int m_H = -2128831035;
	protected int m_Len;
	protected int m_Ci;

	bool Feed(array<string> chunks, int ms)
	{
		int t0 = TickCount(0);
		int h = m_H;
		while (m_Ci < chunks.Count())
		{
			string c = chunks[m_Ci];
			int n = c.Length();
			for (int i = 0; i < n; i++)
			{
				int b = c.Get(i).ToAscii();
				if (b < 0)
					b += 256;
				h = h ^ b;
				h = h * 16777619;
			}
			m_Len += n;
			m_Ci++;
			if ((TickCount(0) - t0) / 10000 >= ms)
				break;
		}
		m_H = h;
		return m_Ci >= chunks.Count();
	}

	string Result()
	{
		string lt = m_Len.ToString();
		string ht = m_H.ToString();
		return lt + ":" + ht;
	}
}
