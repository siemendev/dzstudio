// .dze import: binary format version 8 only, read whole
// before anything changes; objects become one Object-Spawner group, read values kept like the spawner format does.
class DZS_DzeObject : Managed
{
	string m_Type;
	string m_Display;
	float m_Px;
	float m_Py;
	float m_Pz;
	float m_Yaw;
	float m_Pitch;
	float m_Roll;
	float m_Scale;
	bool m_BadPos;
	bool m_BadYpr;
	bool m_BadScale;
	bool m_ScaleZero;
	int m_Flags;
	bool m_Locked;
	bool m_Hidden;
	bool m_EditorOnly;
	bool m_Simulation;
	bool m_Persist;
	bool m_Params;
	bool m_Attach;
}

class DZS_DzeData : Managed
{
	// "" read fine, "format" (older, unknown or damaged), "param" (unknown value type, m_ParamType)
	string m_Error = "format";
	string m_ParamType;
	string m_Map;
	vector m_Cam;
	bool m_CamFinite;
	ref array<ref DZS_DzeObject> m_Objects = new array<ref DZS_DzeObject>();
	int m_Deletions;
	int m_Paths;
	int m_OtherContributors;
	int m_Trailing;
	int m_Size;
}

class DZS_DzeReader : Managed
{
	protected ref array<int> m_W = new array<int>();
	protected int m_N;
	protected int m_Pos;
	protected ref DZS_DzeData m_D;

	static const int MAX_DEPTH = 64;

	protected FileHandle m_Fh;
	protected int m_Stage = 4;
	protected int m_ObjLeft;

	// The whole file as bytes (ReadFile into an int array keeps every byte, also NUL).
	static DZS_DzeData Read(string path)
	{
		DZS_DzeReader r = new DZS_DzeReader();
		r.Begin(path);
		while (!r.Step(0))
		{
		}
		return r.m_D;
	}

	static int FileSize(string path)
	{
		DZS_DzeReader r = new DZS_DzeReader();
		if (!r.Begin(path))
			return -1;
		while (r.LoadSome())
		{
		}
		r.Close();
		return r.m_N;
	}

	DZS_DzeData Data() { return m_D; }

	void ~DZS_DzeReader()
	{
		if (m_Fh)
			CloseFile(m_Fh);
	}

	bool Begin(string path)
	{
		m_D = new DZS_DzeData();
		m_Fh = OpenFile(path, FileMode.READ);
		if (!m_Fh)
		{
			m_Stage = 4;
			return false;
		}
		m_Stage = 0;
		return true;
	}

	void Close()
	{
		if (m_Fh)
			CloseFile(m_Fh);
		m_Fh = null;
		m_Stage = 4;
	}

	// One slice of work; ticks = budget in TickCount units (0 = no limit). True when done.
	bool Step(int ticks)
	{
		int t0 = TickCount(0);
		while (m_Stage < 4)
		{
			if (ticks > 0 && TickCount(0) - t0 > ticks)
				return false;
			if (m_Stage == 0)
			{
				if (!LoadSome())
				{
					CloseFile(m_Fh);
					m_Fh = null;
					m_D.m_Size = m_N;
					m_Stage = 1;
				}
			}
			else if (m_Stage == 1)
			{
				if (ParseHead())
					m_Stage = 2;
				else
					m_Stage = 4;
			}
			else if (m_Stage == 2)
			{
				for (int k = 0; k < 50 && m_ObjLeft > 0; k++)
				{
					DZS_DzeObject obj = new DZS_DzeObject();
					if (!Record(obj, 0))
					{
						m_Stage = 4;
						return true;
					}
					m_D.m_Objects.Insert(obj);
					m_ObjLeft--;
				}
				if (m_ObjLeft == 0)
					m_Stage = 3;
			}
			else
			{
				ParseTail();
				m_Stage = 4;
			}
		}
		return true;
	}

	// Reads up to 64 KB; false at the end of the file.
	protected bool LoadSome()
	{
		int buf[256];
		for (int c = 0; c < 64; c++)
		{
			int n = ReadFile(m_Fh, buf, 1024);
			if (n <= 0)
				return false;
			int words = (n + 3) / 4;
			for (int i = 0; i < words; i++)
				m_W.Insert(buf[i]);
			m_N += n;
			if (n < 1024)
				return false;
		}
		return true;
	}

	protected int Byte(int i)
	{
		int w = m_W[i / 4];
		return (w >> ((i % 4) * 8)) & 255;
	}

	protected int Left() { return m_N - m_Pos; }

	protected bool Int(out int v)
	{
		if (Left() < 4)
			return false;
		int b0 = Byte(m_Pos);
		int b1 = Byte(m_Pos + 1);
		int b2 = Byte(m_Pos + 2);
		int b3 = Byte(m_Pos + 3);
		v = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
		m_Pos += 4;
		return true;
	}

	protected bool Bool(out bool v)
	{
		int i;
		if (!Int(i))
			return false;
		v = i != 0;
		return true;
	}

	// float32 from its bits; NaN and infinity give 0 with finite = false; -0.0 gives 0.
	protected bool Float(out float f, out bool finite)
	{
		int bits;
		if (!Int(bits))
			return false;
		f = FromBits(bits, finite);
		return true;
	}

	static float FromBits(int bits, out bool finite)
	{
		int e = (bits >> 23) & 255;
		int m = bits & 8388607;
		finite = e != 255;
		if (!finite || (e == 0 && m == 0))
			return 0;
		float f;
		if (e == 0)
			f = DZS_F32.Compose(m, -149);
		else
			f = DZS_F32.Compose(m + 8388608, e - 150);
		if (bits < 0)
			f = -f;
		return f;
	}

	protected bool Text(out string s)
	{
		s = "";
		int n;
		if (!Int(n) || n < 0 || n > Left())
			return false;
		int end = m_Pos + n;
		int i = m_Pos;
		while (i < end)
		{
			int b = Byte(i);
			int more = 0;
			if (b < 128)
				more = 0;
			else if (b >= 194 && b <= 223)
				more = 1;
			else if (b >= 224 && b <= 239)
				more = 2;
			else if (b >= 240 && b <= 244)
				more = 3;
			else
				return false;
			if (i + more >= end && more > 0)
				return false;
			for (int k = 1; k <= more; k++)
			{
				int c = Byte(i + k);
				if (c < 128 || c > 191)
					return false;
			}
			for (int j = 0; j <= more; j++)
			{
				int ch = Byte(i + j);
				s += ch.AsciiToString();
			}
			i += more + 1;
		}
		m_Pos = end;
		return true;
	}

	protected bool Vec(out float a, out float b, out float c, out bool finite)
	{
		int count;
		if (!Int(count) || count != 3)
			return false;
		bool fa, fb, fc;
		if (!Float(a, fa) || !Float(b, fb) || !Float(c, fc))
			return false;
		finite = fa && fb && fc;
		return true;
	}

	protected bool Count(out int n, int minBytes)
	{
		if (!Int(n) || n < 0)
			return false;
		return n <= Left() / minBytes;
	}

	protected bool ParseHead()
	{
		if (m_N < 16)
			return false;
		string tag;
		if (!Text(tag) || tag != "EditorBinned")
			return false;
		int version;
		if (!Int(version) || version != 8)
			return false;
		string mapName;
		if (!Text(mapName))
			return false;
		m_D.m_Map = mapName;
		float cx, cy, cz;
		bool cf;
		if (!Vec(cx, cy, cz, cf))
			return false;
		m_D.m_Cam = Vector(cx, cy, cz);
		m_D.m_CamFinite = cf;
		string author;
		if (!Text(author))
			return false;
		int contributors;
		if (!Count(contributors, 4))
			return false;
		for (int i = 0; i < contributors; i++)
		{
			string who;
			if (!Text(who))
				return false;
			if (who != author)
				m_D.m_OtherContributors++;
		}
		int modified, created;
		if (!Int(modified) || !Int(created))
			return false;
		int objects;
		if (!Count(objects, 4))
			return false;
		m_ObjLeft = objects;
		return true;
	}

	protected void ParseTail()
	{
		int deletions;
		if (!Count(deletions, 4))
			return;
		for (int d = 0; d < deletions; d++)
		{
			string dt, dm;
			float dx, dy, dz;
			bool df;
			int dflags;
			if (!Text(dt) || !Vec(dx, dy, dz, df) || !Int(dflags) || !Text(dm))
				return;
		}
		m_D.m_Deletions = deletions;
		int paths;
		if (!Int(paths) || paths < 0)
			return;
		m_D.m_Paths = paths;
		if (paths == 0)
			m_D.m_Trailing = Left();
		m_D.m_Error = "";
	}

	// One object record; depth > 0 for attachments, which are read completely and dropped.
	protected bool Record(DZS_DzeObject obj, int depth)
	{
		if (depth > MAX_DEPTH)
			return false;
		string type, display;
		if (!Text(type) || !Text(display))
			return false;
		float px, py, pz, yy, yp, yr, sc;
		bool pf, yf, sf;
		if (!Vec(px, py, pz, pf) || !Vec(yy, yp, yr, yf) || !Float(sc, sf))
			return false;
		int flags;
		if (!Int(flags))
			return false;
		int params;
		if (!Count(params, 8))
			return false;
		for (int i = 0; i < params; i++)
		{
			string key, kind;
			if (!Text(key) || !Text(kind))
				return false;
			int lt = kind.IndexOf("<");
			int gt = kind.IndexOf(">");
			string inner = "";
			if (lt >= 0 && gt > lt)
				inner = kind.Substring(lt + 1, gt - lt - 1);
			inner.ToLower();
			if (inner == "bool" || inner == "int" || inner == "float")
			{
				int skip;
				if (!Int(skip))
					return false;
			}
			else if (inner == "string")
			{
				string sv;
				if (!Text(sv))
					return false;
			}
			else
			{
				m_D.m_Error = "param";
				m_D.m_ParamType = kind;
				return false;
			}
		}
		bool editorOnly, locked, damage, sim;
		if (!Bool(editorOnly) || !Bool(locked) || !Bool(damage) || !Bool(sim))
			return false;
		string model;
		if (!Text(model))
			return false;
		int attachments;
		if (!Count(attachments, 8))
			return false;
		for (int a = 0; a < attachments; a++)
		{
			int slot;
			if (!Int(slot))
				return false;
			DZS_DzeObject part = new DZS_DzeObject();
			if (!Record(part, depth + 1))
				return false;
		}
		if (flags == 2147483647)
			flags = 30;
		obj.m_Type = type;
		obj.m_Display = display;
		obj.m_BadPos = !pf;
		obj.m_BadYpr = !yf;
		obj.m_BadScale = !sf;
		obj.m_Px = px;
		obj.m_Py = py;
		obj.m_Pz = pz;
		obj.m_Yaw = yy;
		obj.m_Pitch = yp;
		obj.m_Roll = yr;
		obj.m_Scale = sc;
		if (!sf)
			obj.m_Scale = 1;
		else if (sc == 0)
		{
			obj.m_Scale = 1;
			obj.m_ScaleZero = true;
		}
		obj.m_Flags = flags;
		obj.m_Locked = locked || (flags & 64) != 0;
		obj.m_Hidden = (flags & 1024) != 0;
		obj.m_EditorOnly = editorOnly || (flags & 128) != 0;
		obj.m_Simulation = sim || (flags & 256) != 0;
		obj.m_Persist = (flags & 32) != 0;
		obj.m_Params = params > 0;
		obj.m_Attach = attachments > 0;
		return true;
	}
}

// What the import makes of the read data: the map key, the objects as read objects of the spawner format, their
// hidden/locked marks and the hints about what is dropped.
class DZS_DzeImport : Managed
{
	ref DZS_DzeData m_Data;
	string m_Path;
	string m_File;
	string m_Base;
	string m_MapKey;
	ref DZS_ReadResult m_Read = new DZS_ReadResult();
	ref array<bool> m_Hidden = new array<bool>();
	ref array<bool> m_Locked = new array<bool>();
	protected ref DZS_DzeReader m_Reader;
	protected ref DZS_SpawnerFormat m_Conv;
	protected int m_ConvI;
	protected bool m_Done;
	protected int m_N_displays;
	protected int m_N_params;
	protected int m_N_editorOnly;
	protected int m_N_sim;
	protected int m_N_persist;
	protected int m_N_attach;
	protected int m_N_spelled;
	protected int m_N_outside;

	static string MapKeyOf(string world)
	{
		string w = world;
		w.ToLower();
		if (w == "chernarusplus")
			return "chernarus";
		if (w == "enoch")
			return "livonia";
		if (w == "nasdara")
			return "badlands";
		if (w == "sakhal")
			return "sakhal";
		return "";
	}

	// File name without the last extension (basis.v2.dze → basis.v2).
	static string BaseOf(string file)
	{
		string f = file;
		f.Replace("\\", "/");
		int slash = f.LastIndexOf("/");
		string leaf = f.Substring(slash + 1, f.Length() - slash - 1);
		int dot = leaf.LastIndexOf(".");
		if (dot > 0)
			leaf = leaf.Substring(0, dot);
		return leaf;
	}

	// The first allowed folder that differs only in case gets the server's spelling (DZ\plants …).
	static string ServerSpelling(string path, out bool changed)
	{
		changed = false;
		array<string> allowed = {"DZ\\plants_bliss", "DZ\\plants_sakhal", "DZ\\plants", "DZ\\rocks_bliss", "DZ\\rocks_sakhal", "DZ\\rocks"};
		string low = path;
		low.ToLower();
		low.Replace("/", "\\");
		foreach (string a : allowed)
		{
			string al = a;
			al.ToLower();
			int at = low.IndexOf(al);
			if (at < 0)
				continue;
			int endAt = at + al.Length();
			if (endAt < low.Length())
			{
				string next = low.Substring(endAt, 1);
				if (next != "\\")
					continue;
			}
			string sep = path.Substring(at + 2, 1);
			string want = "DZ" + sep + a.Substring(3, a.Length() - 3);
			string have = path.Substring(at, al.Length());
			if (have == want)
				return path;
			string head = path.Substring(0, at);
			string tail = path.Substring(endAt, path.Length() - endAt);
			changed = true;
			return head + want + tail;
		}
		return path;
	}

	static DZS_DzeImport From(string path, string displayName)
	{
		DZS_DzeImport im = Start(path, displayName);
		while (!im.Step(0))
		{
		}
		return im;
	}

	// Reading in slices (Step per frame) so the dialog can show progress.
	static DZS_DzeImport Start(string path, string displayName)
	{
		DZS_DzeImport im = new DZS_DzeImport();
		im.m_Path = path;
		im.m_File = displayName;
		im.m_Base = BaseOf(displayName);
		im.m_Reader = new DZS_DzeReader();
		im.m_Reader.Begin(path);
		im.m_Data = im.m_Reader.Data();
		return im;
	}

	// True when reading and converting are done; ticks = budget in TickCount units (0 = no limit).
	bool Step(int ticks)
	{
		if (m_Done)
			return true;
		int t0 = TickCount(0);
		if (m_Reader)
		{
			if (!m_Reader.Step(ticks))
				return false;
			m_Reader = null;
			if (m_Data.m_Error != "")
			{
				m_Done = true;
				return true;
			}
			m_MapKey = MapKeyOf(m_Data.m_Map);
			m_Conv = new DZS_SpawnerFormat();
			m_ConvI = 0;
		}
		while (m_ConvI < m_Data.m_Objects.Count())
		{
			if (ticks > 0 && TickCount(0) - t0 > ticks)
				return false;
			int stop = Math.Min(m_ConvI + 50, m_Data.m_Objects.Count());
			while (m_ConvI < stop)
			{
				ConvertOne(m_ConvI);
				m_ConvI++;
			}
		}
		ConvertEnd();
		m_Done = true;
		return true;
	}

	string ErrorText()
	{
		if (m_Data.m_Error == "param")
		{
			string de = "Diese .dze enthält einen Zusatzwert, den der Import nicht lesen kann (" + m_Data.m_ParamType + "). Es wurde nichts importiert.";
			string en = "This .dze contains an extra value the import cannot read (" + m_Data.m_ParamType + "). Nothing was imported.";
			return DZS_Texts.Pick(de, en);
		}
		return DZS_Texts.Pick("Diese .dze hat ein älteres oder unbekanntes Format. Öffne sie einmal in dem Programm, mit dem sie erstellt wurde, speichere sie dort neu und importiere sie dann.", "This .dze has an older or unknown format. Open it once in the program it was made with, save it there again and import it then.");
	}

	protected void Hint(int n, string de1, string deN, string en1, string enN, string severity = "hint")
	{
		if (n <= 0)
			return;
		string c = n.ToString();
		if (n == 1)
			m_Read.m_Issues.Add(severity, c + de1, c + en1);
		else
			m_Read.m_Issues.Add(severity, c + deN, c + enN);
	}

	protected void ConvertOne(int i)
	{
		DZS_Issues issues = m_Read.m_Issues;
		float worldSize = DZS_Maps.Size(m_MapKey);
		DZS_DzeObject o = m_Data.m_Objects[i];
		string name = o.m_Type;
		if (DZS_Obj.IsModelPath(name))
		{
			bool changed;
			name = ServerSpelling(name, changed);
			if (changed)
				m_N_spelled++;
		}
		string what = name;
		if (what == "")
			what = "#" + (i + 1);
		DZS_Json warns = DZS_Json.Arr();
		if (o.m_BadPos)
			Warn(issues, warns, "Position", "Position", what);
		if (o.m_BadYpr)
			Warn(issues, warns, "Drehung", "Rotation", what);
		if (o.m_BadScale)
			Warn(issues, warns, "Größe", "Scale", what);
		if (o.m_ScaleZero)
		{
			issues.Add(DZS_Issue.WARNING, "Größe 0 — durch 1 ersetzt", "Size 0 — replaced by 1", what);
			warns.Add(DZS_Json.Str("Größe 0 — durch 1 ersetzt\nSize 0 — replaced by 1"));
		}
		DZS_Json unknown = DZS_Json.Arr();
		DZS_ReadObject ro = m_Conv.FromValues(name, o.m_Px, o.m_Py, o.m_Pz, o.m_Yaw, o.m_Pitch, o.m_Roll, o.m_Scale, "0", "", issues, what, worldSize, unknown, warns);
		if (ro.m_Data.m_Fields.Get("outside") == "1")
		{
			m_N_outside++;
			ro.m_Data.m_Fields.Remove("outside");
		}
		m_Read.m_Objects.Insert(ro);
		m_Hidden.Insert(o.m_Hidden);
		m_Locked.Insert(o.m_Locked);
		string dl = o.m_Display;
		dl.ToLower();
		string tl = o.m_Type;
		tl.ToLower();
		if (o.m_Display != "" && dl != tl)
			m_N_displays++;
		if (o.m_Params)
			m_N_params++;
		if (o.m_EditorOnly)
			m_N_editorOnly++;
		if (o.m_Simulation)
			m_N_sim++;
		if (o.m_Persist)
			m_N_persist++;
		if (o.m_Attach)
			m_N_attach++;
	}

	protected void ConvertEnd()
	{
		DZS_Issues issues = m_Read.m_Issues;
		m_Read.m_Readable = true;
		Hint(m_N_spelled, " Modellpfad in die Schreibweise des Servers gebracht", " Modellpfade in die Schreibweise des Servers gebracht", " model path put into the server’s spelling", " model paths put into the server’s spelling");
		Hint(m_N_displays, " Anzeigename nicht übernommen", " Anzeigenamen nicht übernommen", " display name not taken over", " display names not taken over");
		Hint(m_N_params, " Objekt mit Zusatzwerten (z. B. Licht) — nicht übernommen", " Objekte mit Zusatzwerten (z. B. Licht) — nicht übernommen", " object with extra values (e.g. light) — not taken over", " objects with extra values (e.g. light) — not taken over");
		Hint(m_N_editorOnly, " Objekt war nur für den Editor gedacht — wird jetzt mitgeschrieben", " Objekte waren nur für den Editor gedacht — werden jetzt mitgeschrieben", " object was meant for the editor only — is written now", " objects were meant for the editor only — are written now");
		Hint(m_N_sim, " Objekt mit ‚Simulation‘ — nicht übernommen", " Objekte mit ‚Simulation‘ — nicht übernommen", " object with ‘simulation’ — not taken over", " objects with ‘simulation’ — not taken over");
		Hint(m_N_persist, " Objekt mit Persistenz-Kennzeichen — nicht übernommen", " Objekte mit Persistenz-Kennzeichen — nicht übernommen", " object with persistence mark — not taken over", " objects with persistence mark — not taken over");
		Hint(m_N_attach, " Objekt mit Anbauteilen", " Objekte mit Anbauteilen", " object with attachments", " objects with attachments");
		Hint(m_Data.m_Deletions, " gelöschtes Kartenobjekt", " gelöschte Kartenobjekte", " deleted map object", " deleted map objects");
		if (m_Data.m_Paths > 0)
			issues.Add(DZS_Issue.HINT, "Kamerafahrten werden nicht übernommen", "Camera paths are not taken over");
		Hint(m_Data.m_OtherContributors, " weitere Mitwirkende der Datei — nicht übernommen", " weitere Mitwirkende der Datei — nicht übernommen", " further contributor of the file — not taken over", " further contributors of the file — not taken over");
		Hint(m_Data.m_Trailing, " Byte am Ende der Datei nicht gelesen", " Bytes am Ende der Datei nicht gelesen", " byte at the end of the file not read", " bytes at the end of the file not read", DZS_Issue.WARNING);
		if (m_N_outside > 10)
		{
			string nOut = m_N_outside.ToString();
			issues.Add(DZS_Issue.WARNING, nOut + " Objekte liegen außerhalb der Karte — gehört die Datei zu einer anderen Karte?", nOut + " objects lie outside the map — does the file belong to another map?");
		}
		string l1 = "dze: " + m_File + " map " + m_Data.m_Map + " (" + m_MapKey + "), " + m_Read.m_Objects.Count() + " object(s)";
		string l2 = ", " + m_Data.m_Deletions + " deletion(s), paths " + m_Data.m_Paths + ", trailing " + m_Data.m_Trailing;
		string l3 = ", issues " + issues.Total() + " [" + issues.Title() + "]";
		DZS_Log.Info(l1 + l2 + l3);
	}

	protected void Warn(DZS_Issues issues, DZS_Json warns, string fieldDe, string fieldEn, string what)
	{
		string de = fieldDe + ": keine endliche Zahl — durch den Standardwert ersetzt";
		string en = fieldEn + ": not a finite number — replaced by the default";
		issues.Add(DZS_Issue.WARNING, de, en, what);
		warns.Add(DZS_Json.Str(de + "\n" + en));
	}

	// Camera of a new project: the file's position inside the map, else the map's start point.
	bool CameraInside()
	{
		if (!m_Data.m_CamFinite)
			return false;
		float size = DZS_Maps.Size(m_MapKey);
		vector c = m_Data.m_Cam;
		return c[0] >= 0 && c[2] >= 0 && c[0] <= size && c[2] <= size;
	}
}

// Dialog "Import .dze": every .dze under $saves: and its subfolders (not the project folders), path and size,
// a search field; the sentence above names the folder actually used.
class DZS_DzeFileDialog : DZS_Dialog
{
	protected DZS_ProjectsPlugin m_P;
	protected bool m_IntoOpen;
	protected ref array<string> m_Files = new array<string>();
	protected ref array<ref DZS_ListRow> m_Rows = new array<ref DZS_ListRow>();
	protected ref DZS_SearchField m_Search;
	protected TextWidget m_Where;
	protected TextWidget m_Empty;
	protected string m_Chosen;
	protected string m_Typed = "~";
	protected ref DZS_DzeImport m_Job;
	protected string m_JobLeaf;
	protected float m_JobT;
	protected bool m_Busy;
	protected TextWidget m_BusyText;
	protected ref array<Widget> m_BusyBlocks = new array<Widget>();

	void DZS_DzeFileDialog(DZS_ProjectsPlugin p, bool intoOpen)
	{
		m_P = p;
		m_IntoOpen = intoOpen;
		array<string> all = DZS_Files.ListTree("$saves:");
		foreach (string f : all)
		{
			string low = f;
			low.ToLower();
			if (low.IndexOf("dzstudio/projects/") == 0)
				continue;
			int n = low.Length();
			if (n > 4 && low.Substring(n - 4, 4) == ".dze")
				m_Files.Insert(f);
		}
		m_Files.Sort();
		Setup(LARGE, DZS_Texts.Pick("Projekt", "Project"), DZS_Texts.Pick(".dze importieren", "Import .dze"), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("import", DZS_Texts.Pick("Importieren", "Import"), "main");
		DZS_Log.Info("dze dialog: " + m_Files.Count() + " file(s) under $saves:");
	}

	static string SavesShown()
	{
		string root = DZS_LocalStore.ShownRoot();
		string tail = "\\dzstudio\\Projects";
		int at = root.IndexOf(tail);
		if (at > 0)
			return root.Substring(0, at);
		return root;
	}

	override void BuildContent(Widget inner)
	{
		m_Where = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		string w1 = DZS_Texts.Pick("Der Editor sucht .dze-Dateien in ", "The editor looks for .dze files in ");
		string w2 = SavesShown();
		string w3 = DZS_Texts.Pick(" und den Unterordnern.", " and its subfolders.");
		m_Where.SetText(DZS_W.Literal(w1 + w2 + w3));
		m_Search = new DZS_SearchField("projects.dzsSearch", DZS_Texts.Pick(".dze suchen", "Search .dze"));
		m_Search.SetWidth(300);
		m_Search.Build(inner);
		AddFocusField(m_Search);
		foreach (string f : m_Files)
		{
			string path = "$saves:" + f;
			path.Replace("/", "\\");
			int size = DZS_DzeReader.FileSize(path);
			string meta = f + " · " + SizeText(size);
			int cut = f.LastIndexOf("/") + 1;
			string leaf = f.Substring(cut, f.Length() - cut);
			DZS_ListRow r = new DZS_ListRow(inner, f, leaf, meta, "");
			r.m_OnClick.Insert(OnRowClick);
			r.m_OnDouble.Insert(OnRowDouble);
			m_Rows.Insert(r);
			AddFocusHit(r.m_Hit);
		}
		m_Empty = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("textMuted"));
		m_Empty.SetText(DZS_Texts.Pick("Keine .dze-Datei gefunden", "No .dze file found"));
		if (m_Rows.Count() > 0)
			Choose(m_Rows[0].m_Key);
		AddFootFocus();
	}

	static string SizeText(int bytes)
	{
		if (bytes < 0)
			return "?";
		if (bytes < 1024)
			return bytes.ToString() + " B";
		float kb = bytes / 1024.0;
		return DZS_Num.Fixed(kb, 1) + " KB";
	}

	protected void Choose(string key)
	{
		m_Chosen = key;
		foreach (DZS_ListRow r : m_Rows)
			r.SetSelected(r.m_Key == key);
	}

	void OnRowClick(DZS_ListRow r)
	{
		Choose(r.m_Key);
	}

	void OnRowDouble(DZS_ListRow r)
	{
		Choose(r.m_Key);
		OnFoot("import");
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Where, 0, 0, width, DZS_Scale.Px(20));
		if (m_Busy)
		{
			foreach (DZS_ListRow br : m_Rows)
				br.m_Hit.m_W.Show(false);
			m_Search.Show(false);
			m_Empty.Show(false);
			return DZS_Scale.Px(72);
		}
		m_Search.Place(0, DZS_Scale.Px(28), DZS_Scale.Px(32));
		float y = DZS_Scale.Px(72);
		string typed = m_Search.GetTyped();
		typed.ToLower();
		int shown = 0;
		string firstShown = "";
		bool chosenShown = false;
		foreach (DZS_ListRow r : m_Rows)
		{
			string low = r.m_Key;
			low.ToLower();
			bool show = typed == "" || low.IndexOf(typed) >= 0;
			r.m_Hit.m_W.Show(show);
			if (!show)
				continue;
			if (firstShown == "")
				firstShown = r.m_Key;
			if (r.m_Key == m_Chosen)
				chosenShown = true;
			y += r.Place(0, y, width, DZS_Scale.Px(48)) + DZS_Scale.Px(2);
			shown++;
		}
		if (!chosenShown && firstShown != "")
			Choose(firstShown);
		else if (shown == 0)
			m_Chosen = "";
		m_Empty.Show(shown == 0);
		if (shown == 0)
		{
			DZS_W.Place(m_Empty, 0, y, width, DZS_Scale.Px(24));
			y += DZS_Scale.Px(28);
		}
		return y;
	}

	override void OnFoot(string tag)
	{
		if (tag != "import" || m_Chosen == "")
		{
			Close();
			return;
		}
		string path = "$saves:" + m_Chosen;
		path.Replace("/", "\\");
		if (m_Job)
			return;
		int cut = m_Chosen.LastIndexOf("/") + 1;
		m_JobLeaf = m_Chosen.Substring(cut, m_Chosen.Length() - cut);
		m_Job = DZS_DzeImport.Start(path, m_JobLeaf);
		m_JobT = 0;
		DZS_Log.Info("dze: import of " + m_JobLeaf + " starts");
	}

	override void UpdateContent(float dt)
	{
		if (!m_Job)
			return;
		m_JobT += dt;
		bool done = m_Job.Step(150000);
		if (!done && m_JobT >= 1.0)
			ShowProgress();
		if (m_Busy)
			AnimateBlocks();
		if (!done)
			return;
		DZS_DzeImport im = m_Job;
		m_Job = null;
		DZS_Log.Info("dze: import read in " + DZS_Num.Fixed(m_JobT, 2) + " s, progress shown " + m_Busy);
		Close();
		m_P.ImportRead(im, m_JobLeaf, m_IntoOpen);
	}

	protected void ShowProgress()
	{
		if (m_Busy)
			return;
		m_Busy = true;
		m_BusyText = DZS_W.Text(m_Inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		string bt = DZS_Texts.Pick("Liest ", "Reading ") + m_JobLeaf + " …";
		m_BusyText.SetText(DZS_W.Literal(bt));
		for (int i = 0; i < 4; i++)
		{
			Widget b = DZS_W.Panel(m_Inner, DZS_Color.Get("blockEmpty"));
			m_BusyBlocks.Insert(b);
		}
		foreach (DZS_ListRow r : m_Rows)
			r.m_Hit.m_W.Show(false);
		m_Search.Show(false);
	}

	protected void AnimateBlocks()
	{
		float bs = DZS_Scale.Px(10);
		float top = DZS_Scale.Px(40);
		int lit = Math.Floor(m_JobT / 0.2);
		lit = lit % 4;
		for (int i = 0; i < m_BusyBlocks.Count(); i++)
		{
			DZS_W.Place(m_BusyBlocks[i], i * (bs + DZS_Scale.Px(4)), top, bs, bs);
			if (i == lit)
				m_BusyBlocks[i].SetColor(DZS_Color.Get("accent"));
			else
				m_BusyBlocks[i].SetColor(DZS_Color.Get("blockEmpty"));
		}
		DZS_W.Place(m_BusyText, 4 * (bs + DZS_Scale.Px(4)) + DZS_Scale.Px(8), top - DZS_Scale.Px(5), DZS_Scale.Px(600), DZS_Scale.Px(20));
	}
}

class DZS_DzeNameDialog : DZS_NameDialog
{
	protected DZS_ProjectsPlugin m_P;
	protected ref DZS_DzeImport m_Import;

	void DZS_DzeNameDialog(DZS_ProjectsPlugin p, DZS_DzeImport im)
	{
		m_P = p;
		m_Import = im;
		string info = DZS_Texts.Pick("Karte: ", "Map: ") + DZS_Maps.Display(im.m_MapKey) + " · " + im.m_Read.m_Objects.Count() + DZS_Texts.Pick(" Objekte", " objects");
		SetupName(DZS_Texts.Pick("Projekt", "Project"), DZS_Texts.Pick(".dze als neues Projekt", ".dze as new project"), im.m_Base, info, "create", DZS_Texts.Pick("Anlegen", "Create"));
	}

	override void OnFoot(string tag)
	{
		if (tag == "create")
		{
			string name = Value();
			if (!DZS_Names.ValidProjectName(name))
				return;
			Close();
			m_P.RequestImportNew(name, m_Import);
			return;
		}
		Close();
	}
}
