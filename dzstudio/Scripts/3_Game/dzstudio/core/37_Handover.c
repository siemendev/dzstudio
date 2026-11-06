// What to do after the game has loaded another world (script modules of the mission are reloaded then).
class DZS_Handover
{
	// "open", "new", "restore", "home" or ""
	static string s_Kind;
	static string s_Folder;
	static string s_MapKey;
	static string s_Backup;
	static bool s_AutoDone;
	// "shared": a project from the text form of a session; name, plate and the state travel here.
	static string s_Name;
	static string s_Plate;
	static ref array<string> s_Chunks;
	// the collaboration plugin's own carry-over (plain texts, the mission module is reloaded)
	static ref map<string, string> s_Carry;

	static void SetShared(string name, string mapKey, array<string> chunks, string plate, string folder = "")
	{
		Set("shared", folder, mapKey);
		s_Name = name;
		s_Plate = plate;
		s_Chunks = new array<string>();
		foreach (string c : chunks)
			s_Chunks.Insert(c);
	}

	static map<string, string> Carry()
	{
		if (!s_Carry)
			s_Carry = new map<string, string>();
		return s_Carry;
	}

	static void Set(string kind, string folder, string mapKey, string backup = "")
	{
		s_Kind = kind;
		s_Folder = folder;
		s_MapKey = mapKey;
		s_Backup = backup;
	}

	static void Clear()
	{
		s_Kind = "";
		s_Folder = "";
		s_MapKey = "";
		s_Backup = "";
		s_Name = "";
		s_Plate = "";
		s_Chunks = null;
	}
}
