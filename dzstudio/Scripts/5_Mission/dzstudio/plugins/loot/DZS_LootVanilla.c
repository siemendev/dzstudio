// Vanilla state from Bohemia's DayZ-Central-Economy repository (build plan, answers to section 9 (1)): only on
// "Fetch vanilla state …", fetched with the game's RestApi (GET; the whole answer arrives as one string, 1.2 MB in
// 0.2 s on the test PC; FILE() wrote empty files in DayZ 1.30) and written to $profile:dzstudio\vanilla\<map>\
// with a small companion file (date, source). Not part of the project.
class DZS_LootVanillaCall : RestCallback
{
	DZS_LootVanilla m_V;
	string m_File;

	override void OnError(int errorCode)
	{
		m_V.Done(m_File, false, "error " + errorCode, 0);
	}

	override void OnTimeout()
	{
		m_V.Done(m_File, false, "timeout", 0);
	}

	override void OnSuccess(string data, int dataSize)
	{
		m_V.Text(m_File, data, dataSize);
	}

	override void OnFileCreated(string fileName, int dataSize)
	{
		m_V.Done(m_File, false, "file not created", dataSize);
	}

}

class DZS_LootVanilla : Managed
{
	static const string HOST = "https://raw.githubusercontent.com/";
	static const string REPO = "BohemiaInteractive/DayZ-Central-Economy/master/";
	static const string SOURCE = "https://github.com/BohemiaInteractive/DayZ-Central-Economy";
	static const string SOURCE_FILE = "source.json";

	string m_Map;
	// "" for the loot files next to the world folder, "mission" for every file a patch base needs
	string m_Sub;
	ref array<string> m_Files = new array<string>();
	ref array<ref DZS_LootVanillaCall> m_Calls = new array<ref DZS_LootVanillaCall>();
	int m_Left;
	bool m_Ok = true;
	string m_Why;
	float m_Started;
	ref ScriptInvoker m_OnDone = new ScriptInvoker();

	static string MissionOf(string mapKey)
	{
		if (mapKey == "chernarus")
			return "dayzOffline.chernarusplus";
		if (mapKey == "livonia")
			return "dayzOffline.enoch";
		if (mapKey == "sakhal")
			return "dayzOffline.sakhal";
		if (mapKey == "badlands")
			return "dayzOffline.nasdara";
		return "";
	}

	// Fetched vanilla state: the folder is named after the world (ChernarusPlus); a complete state
	// of earlier builds under the map key moves there at the first read.
	static string Dir(string mapKey)
	{
		string fresh = NewDir(mapKey);
		string old = "$profile:dzstudio\\vanilla\\" + mapKey;
		if (old != fresh && !Complete(fresh) && Complete(old))
			MoveOld(old, fresh);
		return fresh;
	}

	protected static void MoveOld(string old, string fresh)
	{
		DZS_Files.MakeDirs(fresh);
		array<string> names;
		array<bool> folders;
		DZS_Files.List(old, names, folders);
		int moved = 0;
		for (int i = 0; i < names.Count(); i++)
		{
			if (folders[i])
				continue;
			string nm = names[i];
			if (!DZS_Files.Copy(old + "\\" + nm, fresh + "\\" + nm))
			{
				DZS_Log.Warn("loot: vanilla state not moved, copying " + nm + " failed");
				return;
			}
			moved++;
		}
		if (Complete(fresh))
			DZS_Files.DeleteTree(old);
		DZS_Log.Info("loot: vanilla state moved from " + old + " to " + fresh + ", " + moved + " file(s)");
	}

	static string NewDir(string mapKey)
	{
		string world = DZS_Maps.WorldOf(mapKey);
		if (world == "")
			world = mapKey;
		return "$profile:dzstudio\\vanilla\\" + world;
	}

	protected static bool Complete(string dir)
	{
		return FileExist(dir + "\\" + DZS_LootFiles.PROTO) && ValidSource(dir + "\\" + SOURCE_FILE);
	}

	static string NewPathOf(string mapKey, string file)
	{
		return NewDir(mapKey) + "\\" + file;
	}

	// Vanilla mission files a building-block project is applied to, as Bohemia names them.
	static array<string> MissionFiles(string mapKey)
	{
		string area = "cfgeffectarea.json";
		if (mapKey == "chernarus" || mapKey == "livonia")
			area = "cfgEffectArea.json";
		array<string> r = {"db/events.xml", "db/types.xml", "cfgeventspawns.xml", "cfgeventgroups.xml", area, "cfgweather.xml", "cfggameplay.json", "mapgroupproto.xml", "mapgrouppos.xml", "cfglimitsdefinition.xml"};
		return r;
	}

	// The vanilla mission folder of a map below the fetched state: a patch base without a server's files.
	static string MissionDir(string mapKey)
	{
		return NewDir(mapKey) + "\\mission";
	}

	static bool HaveMission(string mapKey)
	{
		string dir = MissionDir(mapKey);
		if (!ValidSource(dir + "\\" + SOURCE_FILE))
			return false;
		array<string> files = MissionFiles(mapKey);
		foreach (string f : files)
		{
			string full = dir + "\\" + f;
			full.Replace("/", "\\");
			if (!FileExist(full))
				return false;
		}
		return true;
	}

	// Starts fetching the whole vanilla mission (MissionFiles) into MissionDir; m_OnDone follows.
	bool StartMission(string mapKey)
	{
		m_Sub = "mission";
		array<string> files = MissionFiles(mapKey);
		m_Files.Copy(files);
		return Start(mapKey);
	}

	protected string Target(string file)
	{
		string dir = NewDir(m_Map);
		if (m_Sub != "")
			dir += "\\" + m_Sub;
		string full = dir + "\\" + file;
		full.Replace("/", "\\");
		return full;
	}

	static string PathOf(string mapKey, string file)
	{
		return Dir(mapKey) + "\\" + file;
	}

	// Files of a complete earlier download, else false.
	static bool Have(string mapKey)
	{
		string a = PathOf(mapKey, DZS_LootFiles.PROTO);
		string b = PathOf(mapKey, SOURCE_FILE);
		return FileExist(a) && ValidSource(b);
	}

	// Starts the downloads; m_OnDone(DZS_LootVanilla) follows once all have answered.
	bool Start(string mapKey)
	{
		m_Map = mapKey;
		string mission = MissionOf(mapKey);
		if (mission == "")
			return false;
		DZS_Files.MakeDirs(NewDir(mapKey));
		RestApi api = GetRestApi();
		if (!api)
			api = CreateRestApi();
		if (!api)
			return false;
		api.SetOption(ERestOption.ERESTOPTION_READOPERATION, 60);
		api.SetOption(ERestOption.ERESTOPTION_CONNECTION, 15);
		RestContext ctx = api.GetRestContext(HOST);
		if (!ctx)
			return false;
		if (m_Files.Count() == 0)
		{
			m_Files.Insert(DZS_LootFiles.PROTO);
			m_Files.Insert(DZS_LootFiles.LIMITS);
		}
		foreach (string mk : m_Files)
		{
			string tf = Target(mk);
			DZS_Files.MakeDirs(tf.Substring(0, tf.LastIndexOf("\\")));
		}
		m_Left = m_Files.Count();
		m_Started = GetGame().GetTickTime();
		foreach (string f : m_Files)
		{
			DZS_LootVanillaCall c = new DZS_LootVanillaCall();
			c.m_V = this;
			c.m_File = f;
			m_Calls.Insert(c);
			string req = REPO + mission + "/" + f;
			int r = ctx.GET(c, req);
			DZS_Log.Info("loot: vanilla request " + req + " -> " + r);
		}
		return true;
	}

	// The answer as text: written to <file>.tmp in pieces, checked against the size, then put in place.
	void Text(string file, string data, int size)
	{
		int len = data.Length();
		DZS_Log.Info("loot: vanilla answer " + file + " size " + size + " string length " + len);
		bool ok = len == size && len > 100 && (data.IndexOf("<") >= 0 || data.IndexOf("{") >= 0);
		if (ok)
		{
			array<string> chunks = DZS_Files.Chunk(data);
			ok = DZS_Files.Write(Target(file + ".tmp"), chunks);
		}
		string why = "";
		if (!ok)
			why = "answer incomplete (" + len + " of " + size + " bytes)";
		Done(file, ok, why, size);
	}

	void Done(string file, bool ok, string why, int size)
	{
		float secs = GetGame().GetTickTime() - m_Started;
		string tmp = Target(file + ".tmp");
		int have = 0;
		if (ok)
		{
			array<string> back = DZS_Files.Read(tmp);
			if (back)
				have = DZS_Files.Length(back);
			if (have < 100 || (size > 0 && have != size))
			{
				ok = false;
				why = "file incomplete (" + have + " of " + size + " bytes)";
			}
		}
		DZS_Log.Info("loot: vanilla " + file + " ok " + DZS_Str.Bool(ok) + " size " + size + " on disk " + have + " after " + secs + " s " + why);
		if (ok)
		{
			string dst = Target(file);
			DeleteFile(dst);
			ok = CopyFile(tmp, dst);
			DeleteFile(tmp);
		}
		else
		{
			DeleteFile(tmp);
			m_Ok = false;
			if (m_Why == "")
				m_Why = why;
		}
		m_Left--;
		if (m_Left > 0)
			return;
		if (m_Ok)
			WriteSource();
		m_OnDone.Invoke(this);
	}

	protected void WriteSource()
	{
		int y, mo, d, h, mi, sec;
		GetYearMonthDayUTC(y, mo, d);
		GetHourMinuteSecondUTC(h, mi, sec);
		string at = y.ToString() + "-" + mo.ToStringLen(2) + "-" + d.ToStringLen(2) + "T" + h.ToStringLen(2) + ":" + mi.ToStringLen(2) + ":" + sec.ToStringLen(2) + "Z";
		string mission = MissionOf(m_Map);
		string text = "{\n    \"version\": 1,\n    \"source\": [\n";
		for (int i = 0; i < m_Files.Count(); i++)
		{
			text += "        \"" + HOST + REPO + mission + "/" + m_Files[i] + "\"";
			if (i < m_Files.Count() - 1)
				text += ",";
			text += "\n";
		}
		text += "    ],\n    \"fetchedAt\": \"" + at + "\"\n}\n";
		array<string> chunks = DZS_Files.Chunk(text);
		DZS_Files.Write(Target(SOURCE_FILE), chunks);
	}

	// Source of a fetched vanilla state: version (1), source (addresses), fetchedAt (UTC, ISO 8601).
	static DZS_SrcNode Source(string path)
	{
		array<string> c = DZS_Files.Read(path);
		if (!c)
			return null;
		DZS_SrcDoc doc = DZS_SrcJson.Parse(c);
		if (!doc || doc.m_Error != "" || !doc.m_Root || !doc.m_Root.IsObj())
			return null;
		DZS_SrcNode v = doc.m_Root.Get("version");
		DZS_SrcNode src = doc.m_Root.Get("source");
		DZS_SrcNode at = doc.m_Root.Get("fetchedAt");
		if (!at && !v)
		{
			at = doc.m_Root.Get("fetched");
			v = at;
		}
		if (!v || !src || !at || !at.IsStr() || at.m_Str.Length() < 16)
			return null;
		if (v != at && !v.IsNum())
			return null;
		if (!src.IsStr() && !src.IsArr())
			return null;
		return doc.m_Root;
	}

	static bool ValidSource(string path)
	{
		return Source(path) != null;
	}

	// "YYYY-MM-DD hh:mm" of fetchedAt (UTC), "" without a valid companion file
	static string FetchedDate(string mapKey)
	{
		DZS_SrcNode root = Source(PathOf(mapKey, SOURCE_FILE));
		if (!root)
			return "";
		DZS_SrcNode at = root.Get("fetchedAt");
		if (!at)
			return root.Get("fetched").m_Str.Substring(0, 16);
		string t = at.m_Str;
		return t.Substring(0, 10) + " " + t.Substring(11, 5) + " UTC";
	}
}
