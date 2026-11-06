// Format plugins: a format reads a file into the objects of one group and writes a group back.
// Results of checks before writing and of reading go into one collected list.
class DZS_IssueFix : Managed
{
	string m_De;
	string m_En;

	// true = the cause is gone (the list drops the row)
	bool Apply() { return false; }
}

class DZS_Issue : Managed
{
	static const string ERROR = "error";
	static const string WARNING = "warning";
	static const string HINT = "hint";

	string m_Severity;
	string m_Group;
	string m_File;
	string m_ObjectId;
	string m_What;
	string m_De;
	string m_En;
	bool m_Blocking;
	bool m_Confirmable;
	// a fix the list offers as a knob (e.g. „Schreibweise korrigieren“)
	ref DZS_IssueFix m_Fix;

	int Rank()
	{
		if (m_Severity == ERROR)
			return 0;
		if (m_Severity == WARNING)
			return 1;
		return 2;
	}
}

class DZS_Issues : Managed
{
	ref array<ref DZS_Issue> m_Items;

	void DZS_Issues()
	{
		m_Items = new array<ref DZS_Issue>();
	}

	DZS_Issue Add(string severity, string de, string en, string what = "", string objectId = "")
	{
		DZS_Issue i = new DZS_Issue();
		i.m_Severity = severity;
		i.m_De = de;
		i.m_En = en;
		i.m_What = what;
		i.m_ObjectId = objectId;
		i.m_Blocking = severity == DZS_Issue.ERROR;
		m_Items.Insert(i);
		return i;
	}

	void Append(DZS_Issues other, string group = "", string file = "")
	{
		if (!other)
			return;
		foreach (DZS_Issue i : other.m_Items)
		{
			if (group != "" && i.m_Group == "")
				i.m_Group = group;
			if (file != "" && i.m_File == "")
				i.m_File = file;
			m_Items.Insert(i);
		}
	}

	int Count(string severity)
	{
		int n = 0;
		foreach (DZS_Issue i : m_Items)
		{
			if (i.m_Severity == severity)
				n++;
		}
		return n;
	}

	int Total() { return m_Items.Count(); }

	bool HasBlocking()
	{
		foreach (DZS_Issue i : m_Items)
		{
			if (i.m_Severity == DZS_Issue.ERROR && i.m_Blocking && !i.m_Confirmable)
				return true;
		}
		return false;
	}

	bool HasConfirmable()
	{
		foreach (DZS_Issue i : m_Items)
		{
			if (i.m_Severity == DZS_Issue.ERROR && i.m_Confirmable)
				return true;
		}
		return false;
	}

	// Ordered by severity, then group (stable inside).
	array<DZS_Issue> Sorted()
	{
		array<DZS_Issue> r = new array<DZS_Issue>();
		for (int rank = 0; rank < 3; rank++)
		{
			array<string> groups = new array<string>();
			foreach (DZS_Issue a : m_Items)
			{
				if (a.Rank() == rank && groups.Find(a.m_Group) < 0)
					groups.Insert(a.m_Group);
			}
			foreach (string g : groups)
			{
				foreach (DZS_Issue b : m_Items)
				{
					if (b.Rank() == rank && b.m_Group == g)
						r.Insert(b);
				}
			}
		}
		return r;
	}

	// "1 Fehler · 3 Warnungen · 2 Hinweise"
	string Title()
	{
		array<string> parts = new array<string>();
		int e = Count(DZS_Issue.ERROR);
		int w = Count(DZS_Issue.WARNING);
		int h = Count(DZS_Issue.HINT);
		string de;
		string en;
		if (e > 0)
		{
			en = e.ToString() + Plural(e, " error", " errors");
			parts.Insert(DZS_Texts.Pick(e.ToString() + " Fehler", en));
		}
		if (w > 0)
		{
			de = w.ToString() + Plural(w, " Warnung", " Warnungen");
			en = w.ToString() + Plural(w, " warning", " warnings");
			parts.Insert(DZS_Texts.Pick(de, en));
		}
		if (h > 0)
		{
			de = h.ToString() + Plural(h, " Hinweis", " Hinweise");
			en = h.ToString() + Plural(h, " hint", " hints");
			parts.Insert(DZS_Texts.Pick(de, en));
		}
		string s = "";
		for (int i = 0; i < parts.Count(); i++)
		{
			if (i > 0)
				s += " · ";
			s += parts[i];
		}
		return s;
	}

	static string Plural(int n, string one, string many)
	{
		string r;
		if (n == 1)
			r = one + "";
		else
			r = many + "";
		return r;
	}
}

// One object as a format read it: element data (fields, flags, companions) and its name and position as
// they stand in the file (for the manifest's object entries).
class DZS_ReadObject : Managed
{
	ref DZS_ElementData m_Data;
	string m_FileName;
	float m_FileX;
	float m_FileY;
	float m_FileZ;
}

class DZS_ReadResult : Managed
{
	bool m_Readable;
	string m_ErrorDe;
	string m_ErrorEn;
	ref array<ref DZS_ReadObject> m_Objects;
	ref array<ref DZS_Companion> m_GroupCompanions;
	ref DZS_Issues m_Issues;

	void DZS_ReadResult()
	{
		m_Objects = new array<ref DZS_ReadObject>();
		m_GroupCompanions = new array<ref DZS_Companion>();
		m_Issues = new DZS_Issues();
	}
}

class DZS_WriteResult : Managed
{
	ref array<string> m_Chunks;
	// ids in the order written, when a sliced writer skipped elements deleted meanwhile
	ref array<string> m_Ids;
	ref array<string> m_Names;
	ref array<float> m_FileX;
	ref array<float> m_FileY;
	ref array<float> m_FileZ;
	ref DZS_Issues m_Issues;

	void DZS_WriteResult()
	{
		m_Names = new array<string>();
		m_FileX = new array<float>();
		m_FileY = new array<float>();
		m_FileZ = new array<float>();
		m_Issues = new DZS_Issues();
	}
}

class DZS_Format : Managed
{
	// Fixed id, kept in the group's binding ("spawner").
	string GetId() { return ""; }
	string GetPluginName() { return ""; }
	bool HandlesFile(string fileName) { return false; }
	DZS_ReadResult Read(array<string> chunks) { return null; }
	// Checks and text of one group; null chunks if nothing may be written.
	DZS_WriteResult Write(DZS_Instance inst, string groupId) { return null; }
	// The same in slices (about 4 ms per frame); formats without their own run in one step.
	DZS_FormatWriter BeginWrite(DZS_Instance inst, string groupId) { return new DZS_FormatWriterOnce(this, inst, groupId); }
	// Reading in slices (no frame over 50 ms); formats without their own run read in one step.
	DZS_FormatReader BeginRead(array<string> chunks) { return new DZS_FormatReaderOnce(this, chunks); }
}

class DZS_FormatReader : Managed
{
	protected ref DZS_ReadResult m_Result;
	// Works for about ms milliseconds; true once the result is there.
	bool Step(float ms) { return true; }
	// Share done, 0 … 1.
	float Progress() { return 0; }
	DZS_ReadResult Result() { return m_Result; }
}

class DZS_FormatReaderOnce : DZS_FormatReader
{
	protected DZS_Format m_Format;
	protected ref array<string> m_Chunks;

	void DZS_FormatReaderOnce(DZS_Format f, array<string> chunks)
	{
		m_Format = f;
		m_Chunks = chunks;
	}

	override bool Step(float ms)
	{
		if (!m_Result)
			m_Result = m_Format.Read(m_Chunks);
		return true;
	}

	override float Progress()
	{
		if (m_Result)
			return 1;
		return 0;
	}
}

class DZS_FormatWriter : Managed
{
	protected ref DZS_WriteResult m_Result;
	// Works for about ms milliseconds; true once the result is there.
	bool Step(float ms) { return true; }
	DZS_WriteResult Result() { return m_Result; }
}

class DZS_FormatWriterOnce : DZS_FormatWriter
{
	protected DZS_Format m_Format;
	protected DZS_Instance m_Inst;
	protected string m_Group;

	void DZS_FormatWriterOnce(DZS_Format f, DZS_Instance inst, string groupId)
	{
		m_Format = f;
		m_Inst = inst;
		m_Group = groupId;
	}

	override bool Step(float ms)
	{
		if (!m_Result)
			m_Result = m_Format.Write(m_Inst, m_Group);
		return true;
	}
}

// Files a plugin adds to a save beside the group files (e.g. the loot files).
// Only changed files are handed in; m_Types are the loot entry keys the project has written.
class DZS_SaveFileOut : Managed
{
	string m_Rel;
	ref array<string> m_Chunks;
	// checksum of the file as the plugin read it (DZS_Files.Checksum), for "changed outside"
	string m_ReadSum;
}

class DZS_SaveContributor : Managed
{
	// writtenTypes and originals (original text per type, "" = none before the project) are updated in place.
	void Contribute(DZS_Instance inst, string folder, array<ref DZS_SaveFileOut> files, DZS_Issues issues, array<string> writtenTypes, array<string> originals) {}
	// Called after the files were written and read back.
	void Saved(DZS_Instance inst, string folder) {}
	// The open project became a copy written without this contributor's files: drop its part as a save result.
	void Dropped(DZS_Instance inst) {}
}
