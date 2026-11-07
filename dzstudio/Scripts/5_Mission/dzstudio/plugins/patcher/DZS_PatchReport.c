// What a patch run did: one entry per change, own block and registration, the files it touched and notes. The JSON
// form is part of the conformance tests, so it holds codes, never free text.
class DZS_PatchEntry : Managed
{
	string m_What;
	int m_Index;
	string m_Op;
	string m_Kind;
	string m_Id;
	ref DZS_Json m_Key;
	string m_Field;
	string m_Status;
	string m_File;
	string m_Detail;
	ref DZS_Json m_Current;
	string m_Value;

	DZS_Json ToJson()
	{
		DZS_Json o = DZS_Json.Obj();
		Put(o, "what", m_What);
		if (m_What != "registration")
		{
			DZS_Json idx = DZS_Json.Int(m_Index);
			o.Set("index", idx);
		}
		Put(o, "op", m_Op);
		Put(o, "kind", m_Kind);
		Put(o, "id", m_Id);
		if (m_Key)
			o.Set("key", m_Key);
		Put(o, "field", m_Field);
		Put(o, "value", m_Value);
		Put(o, "status", m_Status);
		Put(o, "file", m_File);
		Put(o, "detail", m_Detail);
		if (m_Current)
			o.Set("current", m_Current);
		return o;
	}

	protected static void Put(DZS_Json o, string name, string v)
	{
		if (v == "")
			return;
		DZS_Json s = DZS_Json.Str(v);
		o.Set(name, s);
	}

	string LogLine()
	{
		string s = m_Status + " " + m_What;
		if (m_Op != "")
			s += " " + m_Op;
		if (m_Kind != "")
			s += " " + m_Kind;
		if (m_Id != "")
			s += " " + m_Id;
		if (m_Key)
		{
			array<string> one = m_Key.ToChunks(false);
			s += " " + DZS_Files.Join(one);
		}
		if (m_Field != "")
			s += " ." + m_Field;
		if (m_Value != "")
			s += " " + m_Value;
		if (m_File != "")
			s += " (" + m_File + ")";
		if (m_Detail != "")
			s += " [" + m_Detail + "]";
		return s;
	}
}

class DZS_PatchFileNote : Managed
{
	string m_Path;
	string m_Status;
	string m_Detail;
}

class DZS_PatchReport : Managed
{
	static const string FORMAT = "dzstudio-patch-report";

	ref array<ref DZS_PatchEntry> m_Entries = new array<ref DZS_PatchEntry>();
	ref array<ref DZS_PatchFileNote> m_Files = new array<ref DZS_PatchFileNote>();
	ref array<string> m_Notes = new array<string>();

	DZS_PatchEntry Add(string what, int index, string status)
	{
		DZS_PatchEntry e = new DZS_PatchEntry();
		e.m_What = what;
		e.m_Index = index;
		e.m_Status = status;
		m_Entries.Insert(e);
		return e;
	}

	void FileNote(string path, string status, string detail)
	{
		foreach (DZS_PatchFileNote f0 : m_Files)
		{
			if (f0.m_Path == path)
			{
				f0.m_Status = status;
				f0.m_Detail = detail;
				return;
			}
		}
		DZS_PatchFileNote f = new DZS_PatchFileNote();
		f.m_Path = path;
		f.m_Status = status;
		f.m_Detail = detail;
		m_Files.Insert(f);
	}

	// Files in byte order of their paths.
	array<DZS_PatchFileNote> SortedFiles()
	{
		array<string> paths = new array<string>();
		foreach (DZS_PatchFileNote f0 : m_Files)
			paths.Insert(f0.m_Path);
		paths.Sort();
		array<DZS_PatchFileNote> r = new array<DZS_PatchFileNote>();
		foreach (string p : paths)
		{
			foreach (DZS_PatchFileNote f1 : m_Files)
			{
				if (f1.m_Path == p)
					r.Insert(f1);
			}
		}
		return r;
	}

	void Note(string code)
	{
		if (m_Notes.Find(code) < 0)
			m_Notes.Insert(code);
	}

	int Count(string status)
	{
		int n = 0;
		foreach (DZS_PatchEntry e : m_Entries)
		{
			if (e.m_Status == status)
				n++;
		}
		return n;
	}

	DZS_Json ToJson()
	{
		DZS_Json root = DZS_Json.Obj();
		DZS_Json fmt = DZS_Json.Str(FORMAT);
		root.Set("format", fmt);
		DZS_Json ver = DZS_Json.Int(1);
		root.Set("version", ver);
		DZS_Json entries = DZS_Json.Arr();
		foreach (DZS_PatchEntry e : m_Entries)
		{
			DZS_Json ej = e.ToJson();
			entries.Add(ej);
		}
		root.Set("entries", entries);
		DZS_Json files = DZS_Json.Arr();
		array<DZS_PatchFileNote> sorted = SortedFiles();
		foreach (DZS_PatchFileNote f : sorted)
		{
			DZS_Json fj = DZS_Json.Obj();
			DZS_Json p = DZS_Json.Str(f.m_Path);
			fj.Set("path", p);
			DZS_Json st = DZS_Json.Str(f.m_Status);
			fj.Set("status", st);
			if (f.m_Detail != "")
			{
				DZS_Json dt = DZS_Json.Str(f.m_Detail);
				fj.Set("detail", dt);
			}
			files.Add(fj);
		}
		root.Set("files", files);
		DZS_Json notes = DZS_Json.Arr();
		foreach (string n : m_Notes)
		{
			DZS_Json nj = DZS_Json.Str(n);
			notes.Add(nj);
		}
		root.Set("notes", notes);
		return root;
	}

	array<string> Write()
	{
		DZS_TextWriter w = new DZS_TextWriter();
		DZS_Json root = ToJson();
		DZS_TextForm.Value(w, root, 0);
		w.Append("\n");
		return w.Finish();
	}

	void Log()
	{
		foreach (DZS_PatchEntry e : m_Entries)
			DZS_Log.Info("patch: " + e.LogLine());
		array<DZS_PatchFileNote> sorted = SortedFiles();
		foreach (DZS_PatchFileNote f : sorted)
		{
			string d = "";
			if (f.m_Detail != "")
				d = " [" + f.m_Detail + "]";
			DZS_Log.Info("patch: file " + f.m_Status + " " + f.m_Path + d);
		}
		foreach (string n : m_Notes)
			DZS_Log.Info("patch: note " + n);
	}
}
