// "Apply to mission folder …": the open project as building blocks, applied to a base (the map's vanilla mission,
// fetched on demand, or a folder the user put into dzstudio\Missions), the changed files written to
// dzstudio\Output\<project>. The game reads and writes only below its own folders, so a server's mission folder has
// to be copied there first and the output copied back. The chosen base is remembered per project.
class DZS_PatchBase : Managed
{
	string m_Key;
	string m_Name;
	string m_Dir;
	string m_Meta;
	bool m_Vanilla;
}

class DZS_PatchApply : Managed
{
	static const string MISSIONS = "$saves:dzstudio\\Missions";
	static const string OUTPUT = "$saves:dzstudio\\Output";
	static const string SETTING = "patcher.bases";

	DZS_Kernel m_Kernel;
	ref DZS_LootVanilla m_Fetch;
	ref DZS_PatchBase m_Pending;

	void DZS_PatchApply(DZS_Kernel k)
	{
		m_Kernel = k;
	}

	DZS_ProjectsPlugin Projects()
	{
		return DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
	}

	// The map of the open project: the loaded world, else the map the project file names.
	string MapKey()
	{
		DZS_ProjectsPlugin pp = Projects();
		if (!pp || !pp.m_Open)
			return "";
		if (pp.m_Open.m_MapKey != "")
			return pp.m_Open.m_MapKey;
		DZS_ProjectInfo info = DZS_LocalStore.Info(pp.m_Open.m_Folder);
		if (info)
			return info.m_MapKey;
		return "";
	}

	bool CanApply(out string reasonDe, out string reasonEn)
	{
		DZS_ProjectsPlugin pp = Projects();
		if (!pp || !pp.m_Open || !m_Kernel.GetShown())
		{
			reasonDe = "Kein Projekt offen";
			reasonEn = "No project open";
			return false;
		}
		if (m_Fetch)
		{
			reasonDe = "Vanilla-Mission wird geladen";
			reasonEn = "Loading the vanilla mission";
			return false;
		}
		return true;
	}

	// Shown form of a folder below $saves:dzstudio ("Documents\DayZ\dzstudio\Output\Forest Camp").
	static string Shown(string sub)
	{
		string root = DZS_LocalStore.ShownRoot();
		int cut = root.LastIndexOf("\\");
		if (cut > 0)
			root = root.Substring(0, cut);
		return root + "\\" + sub;
	}

	static string OutputName(string projectName)
	{
		string bad = "\\/:*?\"<>|";
		string r = "";
		for (int i = 0; i < projectName.Length(); i++)
		{
			string c = projectName.Get(i);
			if (bad.Contains(c))
				r += "-";
			else
				r += c;
		}
		r.TrimInPlace();
		if (r == "")
			r = "project";
		return r;
	}

	// The vanilla mission of the map first, then every folder in dzstudio\Missions.
	array<ref DZS_PatchBase> Bases(string mapKey)
	{
		array<ref DZS_PatchBase> r = new array<ref DZS_PatchBase>();
		DZS_PatchBase v = new DZS_PatchBase();
		v.m_Key = "vanilla";
		v.m_Vanilla = true;
		v.m_Name = "Vanilla " + DZS_LootPlugin.MapName(mapKey);
		v.m_Dir = DZS_LootVanilla.MissionDir(mapKey);
		if (DZS_LootVanilla.HaveMission(mapKey))
			v.m_Meta = DZS_Texts.Pick("Bohemia · geladen", "Bohemia · fetched");
		else
			v.m_Meta = DZS_Texts.Pick("Bohemia · wird beim Anwenden geladen (Internet)", "Bohemia · fetched when applying (internet)");
		r.Insert(v);
		DZS_Files.MakeDirs(MISSIONS);
		array<string> names;
		array<bool> folders;
		DZS_Files.List(MISSIONS, names, folders);
		for (int i = 0; i < names.Count(); i++)
		{
			if (!folders[i])
				continue;
			DZS_PatchBase b = new DZS_PatchBase();
			b.m_Key = "mission:" + names[i];
			b.m_Name = names[i];
			b.m_Dir = MISSIONS + "\\" + names[i];
			b.m_Meta = "Missions\\" + names[i];
			if (!FileExist(b.m_Dir + "\\db\\types.xml") && !FileExist(b.m_Dir + "\\cfgeventspawns.xml"))
				b.m_Meta += DZS_Texts.Pick(" · sieht nicht wie ein Missionsordner aus", " · does not look like a mission folder");
			r.Insert(b);
		}
		return r;
	}

	string Remembered(string folder)
	{
		DZS_Json all = m_Kernel.GetSettings().Get(SETTING);
		if (!all || !all.IsObj())
			return "vanilla";
		string k = all.GetStr(folder);
		if (k == "")
			return "vanilla";
		return k;
	}

	void Remember(string folder, string key)
	{
		DZS_Settings st = m_Kernel.GetSettings();
		DZS_Json all = st.Get(SETTING);
		DZS_Json copy = DZS_Json.Obj();
		if (all && all.IsObj())
			copy = all.Clone();
		DZS_Json kj = DZS_Json.Str(key);
		copy.Set(folder, kj);
		st.Set(SETTING, copy);
	}

	void Ask()
	{
		DZS_ProjectsPlugin pp = Projects();
		string mapKey = MapKey();
		array<ref DZS_PatchBase> bases = Bases(mapKey);
		string want = Remembered(pp.m_Open.m_Folder);
		int pick = 0;
		for (int i = 0; i < bases.Count(); i++)
		{
			if (bases[i].m_Key == want)
				pick = i;
		}
		string outShown = Shown("Output\\" + OutputName(pp.m_Open.m_Name));
		DZS_ApplyDialog d = new DZS_ApplyDialog(this, bases, pick, outShown);
		d.Open();
	}

	void Go(DZS_PatchBase b)
	{
		DZS_ProjectsPlugin pp = Projects();
		Remember(pp.m_Open.m_Folder, b.m_Key);
		if (b.m_Vanilla && !DZS_LootVanilla.HaveMission(MapKey()))
		{
			m_Pending = b;
			m_Fetch = new DZS_LootVanilla();
			m_Fetch.m_OnDone.Insert(OnFetched);
			if (!m_Fetch.StartMission(MapKey()))
			{
				m_Fetch = null;
				Failed(DZS_Texts.Pick("kein Netz", "no network"));
				return;
			}
			m_Kernel.GetScreen().ShowBanner("patcher.fetch", 20, "info", DZS_Texts.Pick("Vanilla-Mission wird geladen …", "Loading the vanilla mission …"));
			return;
		}
		Run(b);
	}

	void OnFetched(DZS_LootVanilla v)
	{
		m_Kernel.GetScreen().HideBanner("patcher.fetch");
		bool ok = v.m_Ok;
		string why = v.m_Why;
		m_Fetch = null;
		if (!ok)
		{
			Failed(why);
			return;
		}
		DZS_PatchBase b = m_Pending;
		m_Pending = null;
		Run(b);
	}

	protected void Failed(string why)
	{
		m_Kernel.GetScreen().ShowMessage("patcher.apply", "warning", DZS_Texts.Pick("Anwenden nicht möglich: ", "Could not apply: ") + why, 10.0);
		DZS_Log.Info("patch: apply failed: " + why);
	}

	void Run(DZS_PatchBase b)
	{
		DZS_ProjectsPlugin pp = Projects();
		DZS_Instance inst = m_Kernel.GetShown();
		if (!pp || !pp.m_Open || !inst)
			return;
		string name = pp.m_Open.m_Name;
		string mapKey = MapKey();
		string outName = OutputName(name);
		string outDir = OUTPUT + "\\" + outName;
		int t0 = TickCount(0);
		DZS_Json project = DZS_ProjectV2.Build(m_Kernel, inst, name, mapKey);
		DZS_Files.DeleteTree(outDir);
		string err;
		DZS_Patcher p = DZS_ProjectV2.Apply(project, b.m_Dir, outDir, err);
		if (!p)
		{
			Failed(err);
			return;
		}
		DeleteFile(outDir + "\\report.json");
		array<string> lines = DZS_ApplyResult.Lines(p.m_Report, b.m_Name);
		string text = "";
		foreach (string l : lines)
			text += l + "\n";
		array<string> chunks = DZS_Files.Chunk(text);
		DZS_Files.Write(outDir + "\\dzstudio-report.txt", chunks);
		int ms = (TickCount(0) - t0) / 10000;
		DZS_Log.Info("patch: applied '" + name + "' to " + b.m_Dir + " in " + ms + " ms -> " + outDir + ", " + p.m_OutPaths.Count() + " file(s)");
		DZS_ApplyResultDialog d = new DZS_ApplyResultDialog(p.m_Report, b.m_Name, Shown("Output\\" + outName), p.m_OutPaths.Count());
		d.Open();
	}
}

// The report in words: counts, what needs a look, the files.
class DZS_ApplyResult
{
	static bool Fine(string status)
	{
		return status == "applied" || status == "already" || status == "gone" || status == "deduplicated";
	}

	static int CountOf(DZS_PatchReport r, string which)
	{
		int n = 0;
		foreach (DZS_PatchEntry e : r.m_Entries)
		{
			string s = e.m_Status;
			if (which == "ok" && Fine(s))
				n++;
			else if (which == "overrode" && s == "overrode")
				n++;
			else if (which == "missing" && s == "orphaned")
				n++;
			else if (which == "refused" && (s == "invalid" || s == "ambiguous" || s == "collision"))
				n++;
		}
		return n;
	}

	static string StatusText(string s)
	{
		if (s == "overrode")
			return DZS_Texts.Pick("überschrieben (auf dem Server stand etwas anderes)", "replaced (the server had another value)");
		if (s == "orphaned")
			return DZS_Texts.Pick("gibt es in dieser Basis nicht", "not in this base");
		if (s == "ambiguous")
			return DZS_Texts.Pick("mehrdeutig", "ambiguous");
		if (s == "collision")
			return DZS_Texts.Pick("gibt es schon", "already exists");
		if (s == "invalid")
			return DZS_Texts.Pick("abgelehnt", "refused");
		return s;
	}

	static string FileText(string s)
	{
		if (s == "written")
			return DZS_Texts.Pick("geändert", "changed");
		if (s == "created")
			return DZS_Texts.Pick("neu", "new");
		if (s == "unchanged")
			return DZS_Texts.Pick("unverändert", "unchanged");
		if (s == "missing")
			return DZS_Texts.Pick("fehlt in der Basis", "missing in the base");
		if (s == "broken")
			return DZS_Texts.Pick("in der Basis defekt", "broken in the base");
		if (s == "invalid")
			return DZS_Texts.Pick("nicht geschrieben", "not written");
		return s;
	}

	// "lootType Land_Shed_M1 · lootmax" for an entry.
	static string What(DZS_PatchEntry e)
	{
		string w = e.m_Kind;
		if (e.m_What == "registration")
			w = e.m_Field;
		if (e.m_Key && e.m_Key.IsObj())
		{
			foreach (string k : e.m_Key.m_Keys)
			{
				DZS_Json v = e.m_Key.Get(k);
				if (v && (v.IsStr() || v.IsNum()))
					w += " " + v.m_Str;
			}
		}
		else if (e.m_Id != "")
			w += " " + e.m_Id;
		if (e.m_Value != "")
			w += " " + e.m_Value;
		if (e.m_Field != "" && e.m_What != "registration")
			w += " · " + e.m_Field;
		return w;
	}

	static array<string> Notable(DZS_PatchReport r, int max)
	{
		array<string> found = new array<string>();
		foreach (DZS_PatchEntry e : r.m_Entries)
		{
			if (Fine(e.m_Status))
				continue;
			if (found.Count() >= max)
				break;
			string d = "";
			if (e.m_Detail != "")
				d = " [" + e.m_Detail + "]";
			found.Insert(What(e) + " — " + StatusText(e.m_Status) + d);
		}
		return found;
	}

	static array<string> Lines(DZS_PatchReport r, string baseName)
	{
		array<string> l = new array<string>();
		l.Insert(DZS_Texts.Pick("Angewendet auf ", "Applied to ") + baseName);
		string counts = CountOf(r, "ok").ToString() + DZS_Texts.Pick(" übernommen, ", " applied, ");
		counts += CountOf(r, "overrode").ToString() + DZS_Texts.Pick(" überschrieben, ", " replaced, ");
		counts += CountOf(r, "missing").ToString() + DZS_Texts.Pick(" fehlen, ", " missing, ");
		counts += CountOf(r, "refused").ToString() + DZS_Texts.Pick(" abgelehnt", " refused");
		l.Insert(counts);
		l.Insert("");
		array<string> all = Notable(r, 1000);
		foreach (string n : all)
			l.Insert(n);
		if (all.Count() > 0)
			l.Insert("");
		array<DZS_PatchFileNote> files = r.SortedFiles();
		foreach (DZS_PatchFileNote f : files)
			l.Insert(f.m_Path + " — " + FileText(f.m_Status));
		if (r.m_Notes.Find("enableCfgGameplayFile") >= 0)
		{
			l.Insert("");
			l.Insert(DZS_Texts.Pick("Hinweis: Der Server braucht enableCfgGameplayFile = 1 in serverDZ.cfg.", "Note: the server needs enableCfgGameplayFile = 1 in serverDZ.cfg."));
		}
		return l;
	}
}

// Choosing the base: one row per base, the chosen one marked; the output folder below.
class DZS_ApplyDialog : DZS_Dialog
{
	protected DZS_PatchApply m_A;
	protected ref array<ref DZS_PatchBase> m_Bases;
	protected int m_Pick;
	protected string m_Out;
	protected ref array<TextWidget> m_Names = new array<TextWidget>();
	protected ref array<TextWidget> m_Metas = new array<TextWidget>();
	protected ref array<ref DZS_Button> m_Picks = new array<ref DZS_Button>();
	protected TextWidget m_BaseLbl;
	protected TextWidget m_OutLbl;
	protected TextWidget m_OutPath;
	protected TextWidget m_Hint;

	void DZS_ApplyDialog(DZS_PatchApply a, array<ref DZS_PatchBase> bases, int pick, string outShown)
	{
		m_A = a;
		m_Bases = bases;
		m_Pick = pick;
		m_Out = outShown;
		Setup(MEDIUM, DZS_Texts.Pick("Projekt", "Project"), DZS_Texts.Pick("Auf Missionsordner anwenden", "Apply to mission folder"), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button go = AddFootButton("apply", DZS_Texts.Pick("Anwenden", "Apply"), "main");
		go.m_Hit.m_Primary = true;
		m_OnResult.Insert(OnAnswer);
	}

	override void BuildContent(Widget inner)
	{
		m_BaseLbl = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		m_BaseLbl.SetText(DZS_Texts.Pick("BASIS", "BASE"));
		for (int i = 0; i < m_Bases.Count(); i++)
		{
			TextWidget n = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
			n.SetText(m_Bases[i].m_Name);
			TextWidget m = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
			m.SetText(m_Bases[i].m_Meta);
			DZS_Button b = new DZS_Button(inner, "side", DZS_Texts.Pick("Wählen", "Choose"));
			b.m_Tag = i.ToString();
			b.m_OnClick.Insert(OnPick);
			m_Names.Insert(n);
			m_Metas.Insert(m);
			m_Picks.Insert(b);
			AddFocusHit(b.m_Hit);
		}
		m_Hint = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Hint.SetText(DZS_Texts.Pick("Eigene Server-Dateien: Missionsordner nach ", "Your server's files: copy the mission folder to ") + DZS_PatchApply.Shown("Missions") + DZS_Texts.Pick(" kopieren.", "."));
		m_OutLbl = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		m_OutLbl.SetText(DZS_Texts.Pick("AUSGABE (NUR GEÄNDERTE DATEIEN)", "OUTPUT (CHANGED FILES ONLY)"));
		m_OutPath = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_OutPath.SetText(m_Out);
		Mark();
		AddFootFocus();
	}

	protected void Mark()
	{
		for (int i = 0; i < m_Picks.Count(); i++)
		{
			bool on = i == m_Pick;
			if (on)
			{
				m_Picks[i].SetLabel(DZS_Texts.Pick("Gewählt", "Chosen"));
				m_Names[i].SetColor(DZS_Color.Get("accentText"));
			}
			else
			{
				m_Picks[i].SetLabel(DZS_Texts.Pick("Wählen", "Choose"));
				m_Names[i].SetColor(DZS_Color.Get("text"));
			}
			m_Picks[i].SetEnabled(!on);
		}
	}

	void OnPick(DZS_Button b)
	{
		m_Pick = b.m_Tag.ToInt();
		Mark();
		Layout();
	}

	override float BoxWidth()
	{
		float w = super.BoxWidth();
		for (int i = 0; i < m_Metas.Count(); i++)
		{
			float need = Math.Max(DZS_W.TextWidth(m_Metas[i]), DZS_W.TextWidth(m_Names[i])) + m_Picks[i].GetWidth() + DZS_Scale.Px(64);
			w = Math.Max(w, need);
		}
		w = Math.Max(w, DZS_W.TextWidth(m_OutPath) + DZS_Scale.Px(48));
		w = Math.Max(w, DZS_W.TextWidth(m_Hint) + DZS_Scale.Px(48));
		return Math.Min(w, DZS_Scale.Px(960));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		DZS_W.Place(m_BaseLbl, 0, y, width, DZS_Scale.Px(16));
		y += DZS_Scale.Px(22);
		for (int i = 0; i < m_Names.Count(); i++)
		{
			float bw = m_Picks[i].GetWidth();
			DZS_W.Place(m_Names[i], 0, y, width - bw - DZS_Scale.Px(8), DZS_Scale.Px(20));
			DZS_W.Place(m_Metas[i], 0, y + DZS_Scale.Px(22), width - bw - DZS_Scale.Px(8), DZS_Scale.Px(18));
			m_Picks[i].Place(width - bw, y + DZS_Scale.Px(8));
			y += DZS_Scale.Px(50);
		}
		DZS_W.Place(m_Hint, 0, y, width, DZS_Scale.Px(18));
		y += DZS_Scale.Px(34);
		DZS_W.Place(m_OutLbl, 0, y, width, DZS_Scale.Px(16));
		y += DZS_Scale.Px(22);
		DZS_W.Place(m_OutPath, 0, y, width, DZS_Scale.Px(20));
		y += DZS_Scale.Px(24);
		return y;
	}

	void OnAnswer(DZS_Dialog d, string result)
	{
		if (result != "apply")
			return;
		// after this dialog has closed, so the result opens on top of the editor
		DZS_PatchBase chosen = m_Bases[m_Pick];
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(m_A.Go, 1, false, chosen);
	}
}

// After applying: the counts, what needs a look (up to six lines), the files, the output folder.
class DZS_ApplyResultDialog : DZS_Dialog
{
	protected ref array<string> m_Lines = new array<string>();
	protected ref array<string> m_Tones = new array<string>();
	protected ref array<TextWidget> m_Widgets = new array<TextWidget>();
	protected string m_Out;

	void DZS_ApplyResultDialog(DZS_PatchReport r, string baseName, string outShown, int fileCount)
	{
		m_Out = outShown;
		Add(CountLine(r), "text");
		array<string> notable = DZS_ApplyResult.Notable(r, 6);
		int more = DZS_ApplyResult.CountOf(r, "overrode") + DZS_ApplyResult.CountOf(r, "missing") + DZS_ApplyResult.CountOf(r, "refused") - notable.Count();
		if (notable.Count() > 0)
			Add("", "text");
		foreach (string n : notable)
			Add(n, "warning");
		if (more > 0)
			Add(DZS_Texts.Pick("… und ", "… and ") + more + DZS_Texts.Pick(" weitere (siehe dzstudio-report.txt)", " more (see dzstudio-report.txt)"), "textQuiet");
		Add("", "text");
		array<DZS_PatchFileNote> files = r.SortedFiles();
		foreach (DZS_PatchFileNote f : files)
			Add(f.m_Path + " — " + DZS_ApplyResult.FileText(f.m_Status), "textQuiet");
		Add("", "text");
		if (fileCount > 0)
			Add(DZS_Texts.Pick("In den Missionsordner des Servers kopieren (vorhandene Dateien ersetzen):", "Copy into the server's mission folder (replace existing files):"), "text");
		else
			Add(DZS_Texts.Pick("Nichts zu ändern – die Basis entspricht schon dem Projekt.", "Nothing to change – the base already matches the project."), "text");
		Add(outShown, "accentText");
		if (r.m_Notes.Find("enableCfgGameplayFile") >= 0)
			Add(DZS_Texts.Pick("Der Server braucht enableCfgGameplayFile = 1 in serverDZ.cfg.", "The server needs enableCfgGameplayFile = 1 in serverDZ.cfg."), "textQuiet");
		Setup(MEDIUM, DZS_Texts.Pick("Projekt", "Project"), DZS_Texts.Pick("Angewendet auf ", "Applied to ") + baseName, false);
		AddFootButton("copy", DZS_Texts.Pick("Pfad kopieren", "Copy path"), "side");
		DZS_Button ok = AddFootButton("done", DZS_Texts.Pick("Fertig", "Done"), "main");
		ok.m_Hit.m_Primary = true;
		m_OnResult.Insert(OnAnswer);
	}

	protected void Add(string text, string tone)
	{
		m_Lines.Insert(text);
		m_Tones.Insert(tone);
	}

	protected static string CountLine(DZS_PatchReport r)
	{
		string s = DZS_ApplyResult.CountOf(r, "ok").ToString() + DZS_Texts.Pick(" übernommen", " applied");
		s += " · " + DZS_ApplyResult.CountOf(r, "overrode") + DZS_Texts.Pick(" überschrieben", " replaced");
		s += " · " + DZS_ApplyResult.CountOf(r, "missing") + DZS_Texts.Pick(" fehlen", " missing");
		s += " · " + DZS_ApplyResult.CountOf(r, "refused") + DZS_Texts.Pick(" abgelehnt", " refused");
		return s;
	}

	override void BuildContent(Widget inner)
	{
		for (int i = 0; i < m_Lines.Count(); i++)
		{
			int size = 13;
			if (i == 0)
				size = 15;
			TextWidget t = DZS_W.Text(inner, DZS_Font.READ, size, DZS_Color.Get(m_Tones[i]));
			t.SetText(m_Lines[i]);
			m_Widgets.Insert(t);
		}
		AddFootFocus();
	}

	override float BoxWidth()
	{
		float w = super.BoxWidth();
		foreach (TextWidget t : m_Widgets)
			w = Math.Max(w, DZS_W.TextWidth(t) + DZS_Scale.Px(48));
		return Math.Min(w, DZS_Scale.Px(1100));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		for (int i = 0; i < m_Widgets.Count(); i++)
		{
			float h = DZS_Scale.Px(19);
			if (m_Lines[i] == "")
				h = DZS_Scale.Px(8);
			if (i == 0)
				h = DZS_Scale.Px(24);
			DZS_W.Place(m_Widgets[i], 0, y, width, h);
			y += h;
		}
		return y;
	}

	void OnAnswer(DZS_Dialog d, string result)
	{
		if (result == "copy")
			GetGame().CopyToClipboard(m_Out);
	}
}
