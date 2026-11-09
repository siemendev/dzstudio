// Projects: main menu, browser, new/open/save/save as, autosave, backups, the
// question before leaving a project, back to the main menu and quit. Storage: the local folder.
class DZS_ProjectsActions : DZS_ActionHandler
{
	DZS_ProjectsPlugin m_P;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		return m_P.ActionEnabled(actionId, reasonDe, reasonEn);
	}

	override void OnAction(string actionId)
	{
		m_P.OnAction(actionId);
	}
}

class DZS_ProjectsMessages : DZS_MessageListener
{
	DZS_ProjectsPlugin m_P;

	override void OnMessageButton(string kind)
	{
		m_P.OnMessageButton(kind);
	}
}

// One opening in progress.
class DZS_OpenJob : Managed
{
	static const int GROUPS = 0;
	static const int LOAD = 1;
	static const int SHOW = 2;

	string m_Folder;
	// where the group files are read: the folder, or the working folder of a version 2 project
	string m_Src;
	ref DZS_OpenProject m_Op;
	ref DZS_PFData m_Pf;
	DZS_StoreProvider m_Lp;
	bool m_ManifestMissing;
	int m_Skip;
	ref array<ref DZS_PFGroup> m_Order = new array<ref DZS_PFGroup>();
	ref array<string> m_OrderFile = new array<string>();
	ref array<string> m_Colors = new array<string>();
	ref DZS_Instance m_Inst;
	ref DZS_OpBuilder m_B;
	int m_Errors;
	int m_ObjectCount;
	int m_Phase;
	int m_Gi;
	int m_Step;
	string m_File;
	string m_Gid;
	bool m_GHidden;
	bool m_GLocked;
	ref array<string> m_Chunks;
	ref DZS_FileSum m_Sum;
	DZS_Format m_Fmt;
	ref DZS_FormatReader m_Reader;
	ref DZS_ReadResult m_RR;
	ref array<DZS_PFObject> m_Matched;
	int m_Oi;
	int m_ShowTotal;
	ref DZS_ProgressDialog m_Progress;
	int m_T0;
	float m_Started;
	int m_Frames;
	float m_Slowest;
	int m_TRead;
	int m_TSum;
	int m_TParse;
	int m_TBuild;
	DZS_Operation m_LoadOp;
	int m_LoadAt;
	bool m_Loaded;
	bool m_Restore;
	ref DZS_Json m_Backup;
	ref DZS_Json m_BackupHead;
	ref DZS_SrcJsonSteps m_Parse;
	ref DZS_SrcDoc m_Doc;
	DZS_SrcNode m_Elems;
	int m_Ei;
	ref DZS_Operation m_Built;
	string m_Missing;

	float LoadShare()
	{
		if (!m_LoadOp || m_LoadOp.CountChanges() == 0)
			return 0;
		return m_LoadAt / (m_LoadOp.CountChanges() * 1.0);
	}

	// Share of the groups done, 0 … 1.
	float Share()
	{
		int n = m_Order.Count();
		if (n == 0)
			return 1;
		float part = 0;
		if (m_Step == 1)
			part = 0.05;
		else if (m_Step == 2 && m_Reader)
			part = 0.05 + 0.8 * m_Reader.Progress();
		else if (m_Step == 4)
			part = 0.85;
		else if (m_Step == 3 && m_RR)
		{
			part = 0.85;
			if (m_RR.m_Objects.Count() > 0)
				part += 0.15 * m_Oi / m_RR.m_Objects.Count();
		}
		return (m_Gi + part) / n;
	}
}

class DZS_PlanBuild : Managed
{
	DZS_Instance m_Inst;
	bool m_NoLoot;
	ref DZS_SavePlan m_Plan;
	ref array<string> m_Groups;
	ref array<string> m_Entries;
	int m_Next;
	ref DZS_FormatWriter m_Writer;
	DZS_StateFreeze m_Freeze;
	string m_Gid;
	string m_File;
	string m_Name;
	string m_Color;
	string m_GPlugins;
	// group element id -> its spawner block id
	ref map<string, string> m_BlockIds;
	int m_Frames;
	float m_Slowest;
	float m_Started;
}

class DZS_SavePlan : Managed
{
	ref array<ref DZS_SaveFile> m_Files;
	ref DZS_Issues m_Issues;
	// the editor's project part and the building blocks, composed into the project file (again with the new name for
	// Save As)
	string m_ProjectPart;
	ref DZS_Json m_Blocks;
	string m_Base;
	string m_Name;
	bool m_SaveAs;
	// "Save copy …": written like Save As, but the open project stays where it is
	bool m_Copy;
	// the written copy has loot off; the open project is not touched
	bool m_NoLoot;
	// Save As into a provider's new project
	bool m_ToProvider;
	string m_NoteDe;
	string m_NoteEn;
	// what to do after success: "", "leave"
	string m_Then;
	bool m_Auto;
	ref DZS_SaveMark m_Mark;

	void DZS_SavePlan()
	{
		m_Files = new array<ref DZS_SaveFile>();
		m_Issues = new DZS_Issues();
	}
}

[DZS_RegisterPlugin(DZS_ProjectsPlugin)]
// The group "Objects" for new objects when the project has none that takes them:
// bound to objects.json, or objects (2).json … if that binding already exists; next colour of the row.
class DZS_LocalGroupMaker : DZS_GroupMaker
{
	protected DZS_ProjectsPlugin m_P;

	void DZS_LocalGroupMaker(DZS_ProjectsPlugin p)
	{
		m_P = p;
	}

	override string NewObjectGroup(DZS_Instance inst, DZS_OpBuilder b)
	{
		array<string> colors = new array<string>();
		array<string> files = new array<string>();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			if (inst.GetKind(g) != DZS_KIND_GROUP)
				continue;
			string c, f;
			inst.GetField(g, DZS_Obj.GROUP_COLOR, c);
			inst.GetField(g, DZS_Obj.GROUP_FILE, f);
			colors.Insert(c);
			f.ToLower();
			files.Insert(f);
		}
		string file = "objects.json";
		int n = 1;
		while (true)
		{
			string low = file;
			low.ToLower();
			if (files.Find(low) < 0)
				break;
			n++;
			file = "objects (" + n + ").json";
		}
		map<string, string> gf = new map<string, string>();
		gf.Set(DZS_Obj.GROUP_NAME, DZS_Texts.Get("core.group.objects"));
		gf.Set(DZS_Obj.GROUP_COLOR, DZS_Palette.Next(colors));
		gf.Set(DZS_Obj.GROUP_FILE, file);
		gf.Set(DZS_Obj.GROUP_FORMAT, "spawner");
		array<ref DZS_Companion> comps = new array<ref DZS_Companion>();
		comps.Insert(new DZS_Companion(DZS_LocalStore.GROUP_PLUGINS, 1, "[]"));
		string id = b.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, gf, false, false, comps);
		DZS_Log.Info("projects: new group Objects " + id + " file " + file);
		return id;
	}
}

class DZS_ProjectsPlugin : DZS_Plugin
{
	static const string BACKUP_DIR = "$profile:dzstudio\\backups";
	static const float QUIET = 5.0;
	static const float GAP = 30.0;

	protected ref DZS_ProjectsActions m_Actions;
	protected ref DZS_ProjectsMessages m_Msg;
	ref DZS_OpenProject m_Open;
	protected string m_Fetched;
	protected ref DZS_SavePlan m_ProvPlan;
	protected string m_ProvFolder;
	protected ref DZS_SavePlan m_LatePlan;
	protected string m_LateFolder;
	protected bool m_AutoOffShown;
	protected ref DZS_HomeMenu m_Home;
	protected ref DZS_ProjectBar m_Bar;
	protected string m_World;
	protected float m_Now;
	protected float m_LastStepAt = -1;
	protected float m_FirstUnsavedAt;
	protected bool m_Pending;
	protected float m_LastAutoStart = -1000;
	protected float m_LastBackupAt = -1000;
	protected bool m_AutoPaused;
	protected string m_AutoPauseReason;
	protected bool m_Busy;
	protected ref DZS_SavePlan m_Plan;
	protected ref DZS_Issues m_LastIssues;
	protected string m_LastIssuesKind;
	// what waits for the question "Unsaved changes": open, new, home, quit, restore
	protected string m_After;
	protected string m_AfterArg;
	protected string m_AfterArg2;
	protected bool m_StartPending;
	// a .dze read for a new project, waiting for the project to load
	protected ref DZS_DzeImport m_ImportNew;
	protected ref DZS_DzeImport m_ImportAsk;

	override string GetName() { return "projects"; }

	override void OnRegister()
	{
		m_Actions = new DZS_ProjectsActions();
		m_Actions.m_P = this;
		m_Msg = new DZS_ProjectsMessages();
		m_Msg.m_P = this;
		m_Kernel.GetTypes().RegisterOperationType("projects", "projects:edit", 1);
		m_Kernel.GetTypes().RegisterCompanionType("projects", DZS_LocalStore.GROUP_PLUGINS, 1);
		m_Kernel.GetTypes().RegisterCompanionType("projects", DZS_LocalStore.GROUP_ENTRY, 1);
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("projects", "Projekt", "Project");
		array<string> kn = {"Ctrl+N"};
		array<string> ko = {"Ctrl+O"};
		array<string> ks = {"Ctrl+Shift+S"};
		ui.RegisterAction("projects", "projects.new", "Neues Projekt …", "New project …", kn, m_Actions);
		ui.RegisterAction("projects", "projects.browse", "Projekte verwalten …", "Manage projects …", ko, m_Actions);
		ui.RegisterAction("projects", "projects.saveAs", "Speichern unter …", "Save as …", ks, m_Actions);
		ui.RegisterAction("projects", "projects.import", "Importieren …", "Import …", null, m_Actions);
		ui.SetGroup("projects.import", "projects");
		ui.SetWhen("projects.import", "scene");
		ui.AddMenuEntry("project", "projects.import", 400, true);
		ui.RegisterAction("projects", "projects.home", "Zurück ins Hauptmenü", "Back to the main menu", null, m_Actions);
		ui.RegisterAction("projects", "projects.quit", "Beenden", "Quit", null, m_Actions);
		ui.SetGroup("projects.new", "projects");
		ui.SetGroup("projects.browse", "projects");
		ui.SetGroup("projects.saveAs", "projects");
		ui.SetWhen("projects.new", "scene,mainmenu");
		ui.SetWhen("projects.browse", "scene,mainmenu");
		ui.SetWhen("projects.saveAs", "scene");
		ui.SetListed("projects.home", false);
		ui.SetListed("projects.quit", false);
		ui.AddMenuEntry("project", "projects.new", 100);
		ui.AddMenuEntry("project", "projects.browse", 110);
		ui.AddMenuEntry("project", "projects.saveAs", 210);
		ui.AddMenuEntry("project", "projects.home", 600);
		ui.AddMenuEntry("project", "projects.quit", 610);
		DZS_Settings st = m_Kernel.GetSettings();
		st.RegisterTab("projects", "Projekt", "Project");
		st.DefineToggle("projects.autosave", "projects", "Automatisch speichern", "Save automatically", "Speichert 5 s nach der letzten Änderung", "Saves 5 s after the last change", true);
		st.Define("projects.recent", DZS_Json.Arr());
		st.Define("projects.lastMap", DZS_Json.Str(""));
		m_Kernel.SetSaveHandler(m_Actions);
		m_Bar = new DZS_ProjectBar(this);
		ui.RegisterPanel("projects", "projects.groups", "Projekt", "Project", "right", "layers", m_Bar);
		ui.SetPanelMain("projects.groups");
		DZS_LocalStore.EnsureRoot();
		DZS_Files.MakeDirs(BACKUP_DIR);
	}

	override void OnSessionStart(string world)
	{
		m_World = world;
		m_Kernel.SetGroupMaker(new DZS_LocalGroupMaker(this));
		m_Home = new DZS_HomeMenu(this);
		m_StartPending = true;
	}

	// Runs in the first frame, after every plugin (the camera too) has started.
	protected void AfterStart()
	{
		m_StartPending = false;
		string world = m_World;
		string kind = DZS_Handover.s_Kind;
		if (world == "")
		{
			if (kind != "")
			{
				StartWorld(DZS_Handover.s_MapKey);
				return;
			}
			ShowHome(true);
			OfferBackups();
			return;
		}
		if (kind == "open")
		{
			string ofolder = DZS_Handover.s_Folder;
			DZS_Handover.Clear();
			DZS_StoreProvider hp = ProviderOf(ofolder);
			if (hp)
			{
				m_Fetched = "";
				hp.Fetch(ofolder);
			}
			else
				LoadProject(ofolder, true);
		}
		else if (kind == "new")
		{
			string folder = DZS_Handover.s_Folder;
			DZS_Handover.Clear();
			FinishNew(folder);
		}
		else if (kind == "importnew")
		{
			string ifolder = DZS_Handover.s_Folder;
			string ipath = DZS_Handover.s_Backup;
			DZS_Handover.Clear();
			int cut = ipath.LastIndexOf("\\") + 1;
			string leaf = ipath.Substring(cut, ipath.Length() - cut);
			m_ImportNew = DZS_DzeImport.From(ipath, leaf);
			FinishNew(ifolder);
		}
		else if (kind == "restore")
		{
			string file = DZS_Handover.s_Backup;
			DZS_Handover.Clear();
			RestoreBackup(file);
		}
		else if (kind == "home")
		{
			DZS_Handover.Clear();
			ShowHome(true);
		}
		else if (kind == "shared")
		{
			string sname = DZS_Handover.s_Name;
			string splate = DZS_Handover.s_Plate;
			string sfolder = DZS_Handover.s_Folder;
			array<string> schunks = new array<string>();
			if (DZS_Handover.s_Chunks)
				schunks.Copy(DZS_Handover.s_Chunks);
			DZS_Handover.Clear();
			OpenSharedNow(sname, schunks, splate, sfolder);
		}
	}

	// A project from the text form of a shared session: it keeps every id, and the
	// plugin of the sync slot decides about saving. Without a folder it has no place of its own; with the folder of a
	// provider (a project held by the server) saving goes to that provider. Another map is loaded first.
	void OpenShared(string name, string mapKey, array<string> chunks, string plate, string folder = "")
	{
		if (DZS_Maps.WorldOf(mapKey) != m_World)
		{
			DZS_Handover.SetShared(name, mapKey, chunks, plate, folder);
			SwitchWorld(mapKey);
			return;
		}
		CloseProject(true);
		OpenSharedNow(name, chunks, plate, folder);
	}

	bool IsShared() { return m_Open && m_Open.m_Provider == "shared"; }

	protected void OpenSharedNow(string name, array<string> chunks, string plate, string folder = "")
	{
		ShowHome(false);
		string error;
		DZS_Instance inst = m_Kernel.OpenFromState(chunks, error);
		if (!inst)
		{
			DZS_Log.Error("open shared: " + error);
			Message("projects.open", "danger", "Öffnen fehlgeschlagen: " + error, "Opening failed: " + error);
			ShowHome(true);
			return;
		}
		DZS_OpenProject op = new DZS_OpenProject();
		op.m_Provider = "shared";
		op.m_Folder = "";
		DZS_StoreProvider sfp = ProviderOf(folder);
		if (folder != "" && sfp)
		{
			op.m_Provider = sfp.Id();
			op.m_Folder = folder;
		}
		op.m_Name = name;
		op.m_MapKey = LoadedMapKey();
		m_Open = op;
		m_Kernel.ShowInstance(inst);
		inst.SetSaveMode("opened");
		m_Kernel.GetScreen().SetProviderPlate(plate, "");
		if (sfp && folder != "")
			sfp.Opened(folder);
		DZS_CameraPlugin cam = Cam();
		if (cam)
			cam.GoToStart();
		m_LastStepAt = -1;
		m_Pending = false;
		DZS_Log.Info("project open from a shared state: '" + name + "', " + inst.CountElements() + " element(s)");
		m_Kernel.GetEvents().FirePluginEvent("projects.sharedOpened", null);
	}

	override void OnSessionEnd()
	{
		if (m_Home)
			m_Home.Destroy();
	}

	DZS_Kernel Kernel() { return m_Kernel; }
	string LoadedMapKey() { return DZS_Maps.KeyOfWorld(m_World); }
	DZS_Instance Shown() { return m_Kernel.GetShown(); }
	bool IsHome() { return m_Kernel.GetScreen().IsHome(); }

	DZS_CameraPlugin Cam()
	{
		return DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
	}

	string ProjectName()
	{
		if (m_Open)
			return m_Open.m_Name;
		return "";
	}

	// ---- main menu ----

	void ShowHome(bool on)
	{
		m_Kernel.GetScreen().SetHome(on);
		if (on)
		{
			m_Home.Open(LastMap());
			m_Kernel.GetScreen().RaiseMessages();
		}
		else
			m_Home.Close();
	}

	string LastMap()
	{
		string m = m_Kernel.GetSettings().GetString("projects.lastMap");
		if (DZS_Maps.WorldOf(m) == "")
			return "chernarus";
		return m;
	}

	// ---- recently opened (user setting: provider, place, time) ----

	array<ref DZS_ProjectInfo> Recent(string mapKey, int max, bool withProviders = true)
	{
		array<ref DZS_ProjectInfo> all = DZS_LocalStore.List(withProviders);
		DZS_Json rec = m_Kernel.GetSettings().Get("projects.recent");
		array<ref DZS_ProjectInfo> r = new array<ref DZS_ProjectInfo>();
		DZS_Json keep = DZS_Json.Arr();
		bool changed = false;
		if (rec && rec.IsArr())
		{
			for (int i = 0; i < rec.Count(); i++)
			{
				string loc = rec.At(i).GetStr("loc");
				DZS_ProjectInfo found = null;
				foreach (DZS_ProjectInfo p : all)
				{
					if (p.m_Folder == loc)
						found = p;
				}
				if (!found)
				{
					changed = true;
					continue;
				}
				keep.Add(rec.At(i));
				if ((mapKey == "" || found.m_MapKey == mapKey) && (max <= 0 || r.Count() < max))
					r.Insert(found);
			}
		}
		if (changed)
			m_Kernel.GetSettings().Set("projects.recent", keep);
		return r;
	}

	string RecentStamp(string folder, string field)
	{
		DZS_Json rec = m_Kernel.GetSettings().Get("projects.recent");
		if (!rec || !rec.IsArr())
			return "";
		for (int i = 0; i < rec.Count(); i++)
		{
			if (rec.At(i).GetStr("loc") == folder)
				return rec.At(i).GetStr(field);
		}
		return "";
	}

	protected void Touch(string folder, string field)
	{
		if (folder == "")
			return;
		DZS_Json rec = m_Kernel.GetSettings().Get("projects.recent").Clone();
		DZS_Json mine = null;
		DZS_Json others = DZS_Json.Arr();
		if (rec && rec.IsArr())
		{
			for (int i = 0; i < rec.Count(); i++)
			{
				if (rec.At(i).GetStr("loc") == folder)
					mine = rec.At(i);
				else
					others.Add(rec.At(i));
			}
		}
		bool known = mine != null;
		if (!mine)
		{
			mine = DZS_Json.Obj();
			string provId = "local";
			DZS_StoreProvider tp = ProviderOf(folder);
			if (tp)
				provId = tp.Id();
			mine.Set("provider", DZS_Json.Str(provId));
			mine.Set("loc", DZS_Json.Str(folder));
		}
		string stamp = Stamp();
		mine.Set(field, DZS_Json.Str(stamp));
		DZS_Json next = DZS_Json.Arr();
		if (field == "opened" || !known)
		{
			next.Add(mine);
			for (int k = 0; k < others.Count(); k++)
				next.Add(others.At(k));
		}
		else
		{
			for (int j = 0; j < rec.Count(); j++)
			{
				if (rec.At(j).GetStr("loc") == folder)
					next.Add(mine);
				else
					next.Add(rec.At(j));
			}
		}
		m_Kernel.GetSettings().Set("projects.recent", next);
	}

	protected void Forget(string folder)
	{
		DZS_Json rec = m_Kernel.GetSettings().Get("projects.recent");
		DZS_Json next = DZS_Json.Arr();
		if (rec && rec.IsArr())
		{
			for (int i = 0; i < rec.Count(); i++)
			{
				if (rec.At(i).GetStr("loc") != folder)
					next.Add(rec.At(i));
			}
		}
		m_Kernel.GetSettings().Set("projects.recent", next);
	}

	static string Stamp()
	{
		int y, mo, d, h, mi, s;
		GetYearMonthDay(y, mo, d);
		GetHourMinuteSecond(h, mi, s);
		string sy = y.ToString();
		string smo = mo.ToStringLen(2);
		string sd = d.ToStringLen(2);
		string sh = h.ToStringLen(2);
		string smi = mi.ToStringLen(2);
		string ss = s.ToStringLen(2);
		return sy + "-" + smo + "-" + sd + " " + sh + ":" + smi + ":" + ss;
	}

	// "2026-11-02 10:42:13" → "02.11.2026 10:42"
	static string ShowStamp(string st)
	{
		if (st.Length() < 16)
			return st;
		string a = st.Substring(8, 2);
		string b = st.Substring(5, 2);
		string c = st.Substring(0, 4);
		string d = st.Substring(11, 5);
		return a + "." + b + "." + c + " " + d;
	}

	// ---- actions ----

	bool ActionEnabled(string id, out string de, out string en)
	{
		if (m_Busy)
			return false;
		DZS_SaveRules rules = DZS_SaveRules.Get();
		if (id == "core.save")
		{
			DZS_Instance inst = Shown();
			if (inst && rules && rules.SaveBlocked(inst, de, en))
				return false;
			if (!inst || !m_Open || !inst.IsUnsaved())
			{
				de = "Keine ungespeicherten Änderungen";
				en = "No unsaved changes";
				return false;
			}
			return true;
		}
		if (id == "projects.saveAs")
		{
			DZS_Instance sa = Shown();
			if (sa && rules && rules.SaveAsBlocked(sa, de, en))
				return false;
			return m_Open != null && !IsHome();
		}
		if (id == "projects.import")
		{
			if (!m_Open || IsHome() || !Shown())
				return false;
			if (GroupsFixedReason() != "")
			{
				de = "Eine Platzierung hat nur eine Gruppe — .dze als neues Projekt importieren";
				en = "A placement has only one group — import the .dze as a new project";
				return false;
			}
			return true;
		}
		if (id == "projects.new" || id == "projects.browse")
		{
			if (IsHome())
				return DZS_Maps.Available(m_Home.Chosen());
			return true;
		}
		return true;
	}

	void OnAction(string id)
	{
		DZS_Log.Info("projects action " + id);
		if (m_Busy)
			return;
		if (id == "core.save")
			StartSave(false, "");
		else if (id == "projects.new")
			OpenNewDialog(CurrentMapForDialogs());
		else if (id == "projects.browse")
			OpenBrowser(CurrentMapForDialogs());
		else if (id == "projects.saveAs")
			OpenSaveAsDialog();
		else if (id == "projects.import")
			OpenDzeDialog(true);
		else if (id == "projects.home")
			Leave("home", "", "");
		else if (id == "projects.quit")
			Leave("quit", "", "");
	}

	string CurrentMapForDialogs()
	{
		if (IsHome())
			return m_Home.Chosen();
		return LoadedMapKey();
	}

	// ---- dialogs ----

	void OpenNewDialog(string mapKey)
	{
		if (!DZS_Maps.Available(mapKey))
			return;
		DZS_NewProjectDialog d = new DZS_NewProjectDialog(this, mapKey);
		d.Open();
	}

	void OpenDzeDialog(bool intoOpen)
	{
		DZS_DzeFileDialog d = new DZS_DzeFileDialog(this, intoOpen);
		d.Open();
	}

	// After choosing a file: read it whole; nothing changes when it cannot be read.
	void ImportFile(string path, string leaf, bool intoOpen)
	{
		DZS_DzeImport im = DZS_DzeImport.From(path, leaf);
		ImportRead(im, leaf, intoOpen);
	}

	void ImportRead(DZS_DzeImport im, string leaf, bool intoOpen)
	{
		if (im.m_Data.m_Error != "")
		{
			DZS_Log.Info("dze: " + leaf + " not imported: " + im.m_Data.m_Error + " " + im.m_Data.m_ParamType);
			Message("projects.dze", "danger", im.ErrorText(), im.ErrorText(), 10.0);
			return;
		}
		if (!intoOpen)
		{
			if (im.m_MapKey == "")
			{
				string de = "Karte " + im.m_Data.m_Map + " wird nicht unterstützt";
				string en = "Map " + im.m_Data.m_Map + " is not supported";
				DZS_Log.Info("dze: " + de);
				Message("projects.dze", "danger", de, en, 10.0);
				return;
			}
			if (!DZS_Maps.Available(im.m_MapKey))
			{
				string shownMap = DZS_Maps.Display(im.m_MapKey);
				Message("projects.dze", "danger", shownMap + " ist nicht verfügbar", shownMap + " is not available", 10.0);
				return;
			}
			DZS_DzeNameDialog nd = new DZS_DzeNameDialog(this, im);
			nd.Open();
			return;
		}
		if (!m_Open || !Shown())
			return;
		if (im.m_MapKey != m_Open.m_MapKey)
		{
			string fileMap = im.m_Data.m_Map;
			if (im.m_MapKey != "")
				fileMap = DZS_Maps.Display(im.m_MapKey);
			string projMap = DZS_Maps.Display(m_Open.m_MapKey);
			DZS_Issues ask = new DZS_Issues();
			ask.Add(DZS_Issue.WARNING, "Die Datei gehört zu " + fileMap + ", das Projekt zu " + projMap, "The file belongs to " + fileMap + ", the project to " + projMap);
			m_ImportAsk = im;
			DZS_CheckListDialog cd = new DZS_CheckListDialog(ask, "import", "Import", "Import");
			cd.m_OnResult.Insert(OnImportAsk);
			cd.Open();
			DZS_Log.Info("dze: map differs: " + fileMap + " / " + projMap);
			return;
		}
		ImportInto(im);
	}

	void OnImportAsk(DZS_Dialog d, string result)
	{
		DZS_Log.Info("dze: map question " + result);
		DZS_DzeImport im = m_ImportAsk;
		m_ImportAsk = null;
		if (result == "save" && im)
			ImportInto(im);
	}

	// Into the open project: a new group named after the file, one step "Import ‹file›";
	// the camera stays.
	protected void ImportInto(DZS_DzeImport im)
	{
		DZS_Instance inst = Shown();
		if (!inst || !m_Open)
			return;
		array<string> names = new array<string>();
		array<string> files = new array<string>();
		array<string> colors = new array<string>();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			if (inst.GetKind(g) != DZS_KIND_GROUP)
				continue;
			string gn, gfile, gc;
			inst.GetField(g, DZS_Obj.GROUP_NAME, gn);
			inst.GetField(g, DZS_Obj.GROUP_FILE, gfile);
			inst.GetField(g, DZS_Obj.GROUP_COLOR, gc);
			gn.ToLower();
			gfile.ToLower();
			names.Insert(gn);
			files.Insert(gfile);
			colors.Insert(gc);
		}
		string name = im.m_Base;
		string file = im.m_Base + ".json";
		int n = 1;
		while (true)
		{
			string ln = name;
			ln.ToLower();
			string lf = file;
			lf.ToLower();
			if (names.Find(ln) < 0 && files.Find(lf) < 0)
				break;
			n++;
			name = im.m_Base + " (" + n + ")";
			file = name + ".json";
		}
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		map<string, string> gf = new map<string, string>();
		gf.Set(DZS_Obj.GROUP_NAME, name);
		gf.Set(DZS_Obj.GROUP_COLOR, DZS_Palette.Next(colors));
		gf.Set(DZS_Obj.GROUP_FILE, file);
		gf.Set(DZS_Obj.GROUP_FORMAT, "spawner");
		array<ref DZS_Companion> gcomp = new array<ref DZS_Companion>();
		gcomp.Insert(new DZS_Companion(DZS_LocalStore.GROUP_PLUGINS, 1, "[]"));
		string gid = b.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, gf, false, false, gcomp);
		AddImported(b, gid, im);
		string de = im.m_File + " importieren";
		string en = "Import " + im.m_File;
		DZS_SubmitResult r = b.Commit(de, en);
		DZS_Log.Info("dze: imported into the open project: group '" + name + "' (" + file + "), " + im.m_Read.m_Objects.Count() + " object(s), ok " + r.Ok());
		DZS_Issues issues = im.m_Read.m_Issues;
		if (issues.Total() > 0)
		{
			m_LastIssues = issues;
			m_LastIssuesKind = "open";
			string t1 = DZS_Texts.Pick("Importiert · ", "Imported · ");
			string t2 = issues.Title();
			string t3 = DZS_Texts.Pick("Anzeigen", "Show");
			m_Kernel.GetScreen().ShowMessageWithButton("projects.opened", "info", t1 + t2, 10.0, t3, m_Msg);
		}
		LogIssues(issues);
	}

	protected void AddImported(DZS_OpBuilder b, string gid, DZS_DzeImport im)
	{
		for (int i = 0; i < im.m_Read.m_Objects.Count(); i++)
		{
			DZS_ReadObject ro = im.m_Read.m_Objects[i];
			array<ref DZS_Companion> oc = new array<ref DZS_Companion>();
			for (int ci = 0; ci < ro.m_Data.m_Companions.Count(); ci++)
				oc.Insert(ro.m_Data.m_Companions.GetElement(ci));
			bool hidden = im.m_Hidden[i];
			bool locked = im.m_Locked[i];
			b.Create(DZS_KIND_OBJECT, gid, DZS_END, ro.m_Data.m_Fields, hidden, locked, oc);
		}
	}

	protected void LogIssues(DZS_Issues issues)
	{
		foreach (DZS_Issue li : issues.m_Items)
			DZS_Log.Info("dze issue " + li.m_Severity + ": " + li.m_What + " | " + li.m_De);
	}

	// As a new project: a local project with one group named after the file; the load carries the objects, then it
	// is saved at once.
	void RequestImportNew(string name, DZS_DzeImport im)
	{
		m_ImportNew = im;
		Leave("importnew", name, im.m_MapKey);
	}

	protected void GoImportNew(string name, string mapKey)
	{
		DZS_DzeImport im = m_ImportNew;
		if (!im || !DZS_Maps.Available(mapKey))
			return;
		if (DZS_Maps.WorldOf(mapKey) == m_World && Cam() && Cam().IsStarted())
			CloseProject(true);
		string folder = DZS_LocalStore.NewProject(name, mapKey, "", im.m_Base + ".json", im.m_Base);
		Touch(folder, "opened");
		if (DZS_Maps.WorldOf(mapKey) != m_World)
		{
			DZS_Handover.Set("importnew", folder, mapKey, im.m_Path);
			SwitchWorld(mapKey);
			return;
		}
		FinishNew(folder);
	}

	void OpenBrowser(string mapKey)
	{
		DZS_BrowserDialog d = new DZS_BrowserDialog(this, mapKey);
		d.Open();
	}

	protected string m_SaveAsNoteDe;
	protected string m_SaveAsNoteEn;
	protected bool m_SaveAsNoLoot;

	static string CopyName(string name)
	{
		return name + DZS_Texts.Pick(" Kopie", " copy");
	}

	// noteDe/En: a warning shown instead of the usual confirmation once this save-as has written. noLoot: the copy is
	// written without loot and opens with loot off.
	void OpenSaveAsDialog(string noteDe = "", string noteEn = "", bool noLoot = false)
	{
		if (!m_Open)
			return;
		m_SaveAsNoteDe = noteDe;
		m_SaveAsNoteEn = noteEn;
		m_SaveAsNoLoot = noLoot;
		DZS_SaveAsDialog d = new DZS_SaveAsDialog(this, CopyName(m_Open.m_Name));
		d.Open();
	}

	// Save a copy: a new local project from the own state; the session's project stays open.
	protected bool m_CopyNoLoot;
	protected string m_CopyNoteDe;
	protected string m_CopyNoteEn;

	// noLoot: "Save as own copy …" with loot texts missing — only the written copy has
	// loot off.
	void OpenCopyDialog(bool noLoot = false, string noteDe = "", string noteEn = "")
	{
		if (!m_Open)
			return;
		m_CopyNoLoot = noLoot;
		m_CopyNoteDe = noteDe;
		m_CopyNoteEn = noteEn;
		DZS_SaveAsDialog d = new DZS_SaveAsDialog(this, CopyName(m_Open.m_Name), true);
		d.Open();
	}

	void SaveCopy(string name)
	{
		if (!m_Open)
			return;
		StartSave(false, "", name, true);
	}

	void Message(string kind, string tone, string de, string en, float seconds = 3.0)
	{
		m_Kernel.GetScreen().ShowMessage(kind, tone, DZS_Texts.Pick(de, en), seconds);
	}

	// ---- leaving the open project ----

	void Leave(string what, string arg, string arg2)
	{
		m_After = what;
		m_AfterArg = arg;
		m_AfterArg2 = arg2;
		DZS_Instance inst = Shown();
		DZS_SaveRules rules = DZS_SaveRules.Get();
		if (rules && inst && m_Open && rules.InterceptLeave(what, arg, arg2))
		{
			m_After = "";
			return;
		}
		if (!inst || !m_Open || !inst.IsUnsaved() || (rules && rules.NoLocalWork(inst)))
		{
			Continue();
			return;
		}
		string name = m_Open.m_Name;
		string text;
		string mode = inst.GetSaveMode();
		int n = inst.GetUnsavedCount();
		if (mode == "restored")
		{
			int more = inst.GetHistoryUnsaved();
			text = DZS_Texts.Pick(name + " wurde aus einer Sicherung wiederhergestellt und ist noch nicht gespeichert.", name + " was restored from a backup and is not saved yet.");
			if (more > 0)
				text += DZS_Texts.Pick(" · " + more + " weitere Änderungen", " · " + more + " more changes");
		}
		else if (mode == "manifest" && n == 1)
			text = DZS_Texts.Pick("Die Projektdatei von " + name + " ist noch nicht geschrieben.", "The project file of " + name + " is not written yet.");
		else
		{
			string de = name + " hat " + n + DZS_Issues.Plural(n, " ungespeicherte Änderung.", " ungespeicherte Änderungen.");
			string en = name + " has " + n + DZS_Issues.Plural(n, " unsaved change.", " unsaved changes.");
			text = DZS_Texts.Pick(de, en);
		}
		string tx101 = DZS_Texts.Pick("Projekt", "Project");
		string tx102 = DZS_Texts.Pick("Ungespeicherte Änderungen", "Unsaved changes");
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(tx101, tx102, text);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("discard", DZS_Texts.Pick("Nicht speichern", "Don’t save"), "danger");
		c.AddFootButton("save", DZS_Texts.Pick("Speichern", "Save"), "main");
		c.m_OnResult.Insert(OnLeaveAnswer);
		c.Open();
	}

	void OnLeaveAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("unsaved question: " + result);
		if (result == "save")
			StartSave(false, "leave");
		else if (result == "discard")
		{
			DeleteBackup();
			Continue();
		}
		else
			m_After = "";
	}

	protected void Continue()
	{
		string what = m_After;
		string arg = m_AfterArg;
		string arg2 = m_AfterArg2;
		m_After = "";
		if (what == "home")
		{
			string key = LoadedMapKey();
			if (m_Open)
				key = m_Open.m_MapKey;
			CloseProject(true);
			if (key != "")
				m_Kernel.GetSettings().Set("projects.lastMap", DZS_Json.Str(key));
			ShowHome(true);
		}
		else if (what == "quit")
		{
			CloseProject(true);
			DZS_Log.Info("quit");
			m_Kernel.GetSettings().Flush();
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).Call(GetGame().RequestExit, IDC_MAIN_QUIT);
		}
		else if (what == "open")
			GoOpen(arg);
		else if (what == "new")
			GoNew(arg, arg2);
		else if (what == "importnew")
			GoImportNew(arg, arg2);
		else if (what == "restore")
			GoRestore(arg);
	}

	// Closes the open project; quietly = it was saved or the user said "Don't save".
	void CloseProject(bool quietly)
	{
		DZS_Instance inst = Shown();
		if (inst && m_Open && !inst.IsUnsaved())
			DeleteBackup();
		if (inst)
			m_Kernel.CloseInstance(inst);
		if (m_Open)
		{
			DZS_StoreProvider cp = ProviderOf(m_Open.m_Folder);
			if (cp)
				cp.Closed(m_Open.m_Folder);
		}
		m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Get("core.provider.local"), "");
		DZS_ProjectExtensions.Reset();
		m_AutoOffShown = false;
		m_Kernel.GetScreen().SetStatusIcon("projects.autoOff", false, "", "", "");
		m_Open = null;
		m_Pending = false;
		m_AutoPaused = false;
		m_Kernel.GetScreen().SetStatusIcon("projects.autoPaused", false, "", "", "");
		m_Kernel.GetScreen().HideMessage("checklist");
		m_Kernel.GetScreen().HideMessage("projects.saved");
	}

	// ---- open ----

	void AddProvider(DZS_StoreProvider p)
	{
		DZS_StoreProvider.Register(p);
		if (m_Home && IsHome())
			m_Home.RebuildPlate();
	}

	DZS_StoreProvider ProviderOf(string folder)
	{
		array<ref DZS_StoreProvider> provs = DZS_StoreProvider.All();
		foreach (DZS_StoreProvider p : provs)
		{
			if (p.Owns(folder))
				return p;
		}
		return null;
	}

	string ProviderLabel(string folder)
	{
		DZS_StoreProvider p = ProviderOf(folder);
		if (p)
			return p.Label(folder);
		return DZS_Texts.Pick("Lokal", "Local");
	}

	string GroupOfFile(DZS_Instance inst, string rel)
	{
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			if (inst.GetKind(g) != DZS_KIND_GROUP)
				continue;
			string f;
			inst.GetField(g, DZS_Obj.GROUP_FILE, f);
			if (f == rel)
				return g;
		}
		return "";
	}

	protected void CreateObjects(DZS_OpBuilder b, string gid, DZS_ReadResult rr)
	{
		foreach (DZS_ReadObject ro : rr.m_Objects)
		{
			array<ref DZS_Companion> oc = new array<ref DZS_Companion>();
			for (int ci = 0; ci < ro.m_Data.m_Companions.Count(); ci++)
				oc.Insert(ro.m_Data.m_Companions.GetElement(ci));
			b.Create(DZS_KIND_OBJECT, gid, DZS_END, ro.m_Data.m_Fields, false, false, oc);
		}
	}

	// A provider's file changed and its group has no own changes: replace the objects as a system operation.
	// The folder already holds the new text.
	bool ReloadGroup(string rel, array<string> chunks)
	{
		DZS_Instance inst = Shown();
		if (!inst || !m_Open)
			return false;
		string gid = GroupOfFile(inst, rel);
		if (gid == "")
			return false;
		DZS_Format fmt = m_Kernel.FormatForFile(rel);
		DZS_ReadResult rr = fmt.Read(chunks);
		if (!rr.m_Readable)
			return false;
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		array<string> kids = inst.GetChildren(gid);
		foreach (string k : kids)
			b.Delete(k);
		CreateObjects(b, gid, rr);
		DZS_Operation op = b.GetOperation();
		DZS_SubmitResult r = inst.SubmitSystem(op, false, true);
		m_Open.Remember(rel, DZS_Files.Checksum(chunks));
		DZS_Log.Info("provider: group " + rel + " reloaded, " + rr.m_Objects.Count() + " object(s), ok " + r.Ok());
		return r.Ok();
	}

	// A new file of the provider becomes a new group (system operation).
	bool AddGroup(string rel, array<string> chunks)
	{
		DZS_Instance inst = Shown();
		if (!inst || !m_Open || GroupOfFile(inst, rel) != "")
			return false;
		DZS_Format fmt = m_Kernel.FormatForFile(rel);
		if (!fmt)
			return false;
		DZS_ReadResult rr = fmt.Read(chunks);
		array<string> colors = new array<string>();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			string c;
			if (inst.GetField(g, DZS_Obj.GROUP_COLOR, c))
				colors.Insert(c);
		}
		map<string, string> gf = new map<string, string>();
		gf.Set(DZS_Obj.GROUP_NAME, Leaf(rel));
		gf.Set(DZS_Obj.GROUP_COLOR, DZS_Palette.Next(colors));
		gf.Set(DZS_Obj.GROUP_FILE, rel);
		gf.Set(DZS_Obj.GROUP_FORMAT, fmt.GetId());
		array<ref DZS_Companion> gcomp = new array<ref DZS_Companion>();
		gcomp.Insert(new DZS_Companion(DZS_LocalStore.GROUP_PLUGINS, 1, "[]"));
		foreach (DZS_Companion rc : rr.m_GroupCompanions)
			gcomp.Insert(rc);
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		string gid = b.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, gf, false, false, gcomp);
		if (rr.m_Readable)
			CreateObjects(b, gid, rr);
		DZS_Operation op = b.GetOperation();
		DZS_SubmitResult r = inst.SubmitSystem(op, false, true);
		m_Open.Remember(rel, DZS_Files.Checksum(chunks));
		DZS_Log.Info("provider: new group " + rel + ", " + rr.m_Objects.Count() + " object(s), ok " + r.Ok());
		return r.Ok();
	}

	bool RemoveGroup(string rel)
	{
		DZS_Instance inst = Shown();
		if (!inst || !m_Open)
			return false;
		string gid = GroupOfFile(inst, rel);
		if (gid == "")
			return false;
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		array<string> kids = inst.GetChildren(gid);
		foreach (string k : kids)
			b.Delete(k);
		b.Delete(gid);
		DZS_Operation op = b.GetOperation();
		DZS_SubmitResult r = inst.SubmitSystem(op, false, true);
		DZS_Log.Info("provider: group " + rel + " removed, ok " + r.Ok());
		return r.Ok();
	}

	// The provider has put the current state into the folder (or failed); opening goes on.
	void Fetched(string folder, bool ok, string de, string en)
	{
		if (!ok)
		{
			Message("projects.open", "danger", de, en, 6.0);
			if (IsHome() || !Shown())
				ShowHome(true);
			return;
		}
		m_Fetched = folder;
		GoOpen(folder);
	}

	void RequestOpen(string folder)
	{
		Leave("open", folder, "");
	}

	protected void GoOpen(string folder)
	{
		DZS_StoreProvider fp = ProviderOf(folder);
		if (fp && m_Fetched != folder)
		{
			fp.Fetch(folder);
			return;
		}
		m_Fetched = "";
		if (!DZS_LocalStore.Exists(folder))
		{
			Message("projects.open", "danger", "Projekt nicht gefunden", "Project not found");
			return;
		}
		string file = FindBackup("local", folder);
		if (file != "")
		{
			AskBackup(folder, file);
			return;
		}
		OpenChecked(folder);
	}

	protected void AskBackup(string folder, string file)
	{
		DZS_Json b = ReadBackup(file);
		string name = folder;
		string when = "";
		if (b)
		{
			name = b.GetStr("name");
			when = ShowStamp(b.GetStr("time"));
		}
		string tx103 = DZS_Texts.Pick("Projekt", "Project");
		string tx104 = DZS_Texts.Pick("Sicherung gefunden", "Backup found");
		string tx105 = DZS_Texts.Pick("Für " + name + " gibt es nicht gespeicherte Arbeit vom " + when, "There is unsaved work for " + name + " from " + when);
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(tx103, tx104, tx105);
		c.m_Tag = folder;
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("discard", DZS_Texts.Pick("Verwerfen", "Discard"), "danger");
		c.AddFootButton("restore", DZS_Texts.Pick("Wiederherstellen", "Restore"), "main");
		c.m_OnResult.Insert(OnBackupAnswer);
		c.Open();
	}

	void OnBackupAnswer(DZS_Dialog d, string result)
	{
		string folder = d.m_Tag;
		string file = FindBackup("local", folder);
		if (result == "restore")
			GoRestore(file);
		else if (result == "discard")
		{
			DiscardBackup(file);
			DZS_Files.Delete(file + ".state");
			DZS_Files.Delete(file + ".state2");
			OpenChecked(folder);
		}
	}

	// Map available and loaded? Otherwise load its world first.
	protected void OpenChecked(string folder)
	{
		DZS_PFData pf = DZS_ProjectFile.Read(DZS_Files.Read(DZS_LocalStore.File(folder)));
		if (pf.m_Status == "newer")
		{
			Message("projects.open", "danger", "Diese Projektdatei stammt aus einer neueren Version des Editors. Aktualisiere den Editor, um das Projekt zu öffnen.", "This project file comes from a newer version of the editor. Update the editor to open the project.", 5.0);
			return;
		}
		string mapKey = pf.m_MapKey;
		if (pf.m_Status == "ok" && pf.m_MapRaw != "" && mapKey == "")
		{
			Message("projects.open", "danger", pf.m_MapRaw + " ist nicht verfügbar", pf.m_MapRaw + " is not available");
			return;
		}
		if (mapKey == "")
			mapKey = CurrentMapForDialogs();
		if (!DZS_Maps.Available(mapKey))
		{
			string mapName = DZS_Maps.Display(mapKey);
			Message("projects.open", "danger", mapName + " ist nicht verfügbar", mapName + " is not available");
			return;
		}
		if (DZS_Maps.WorldOf(mapKey) != m_World)
		{
			DZS_Handover.Set("open", folder, mapKey);
			SwitchWorld(mapKey);
			return;
		}
		CloseProject(true);
		LoadProject(folder, true);
	}

	protected void SwitchWorld(string mapKey)
	{
		DZS_Log.Info("loading world " + DZS_Maps.WorldOf(mapKey) + " for " + DZS_Handover.s_Kind);
		if (m_World == "")
		{
			StartWorld(mapKey);
			return;
		}
		CloseProject(true);
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).Call(GetGame().GetMission().AbortMission);
	}

	protected void StartWorld(string mapKey)
	{
		m_Home.Close();
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(DZS_MapStart.StartMap, 100, false, DZS_Maps.WorldOf(mapKey));
	}

	// Reads every group file and the project file and brings it all in as one load. Sliced:
	// about 4 ms per frame with "Opening … n %" over everything until the scene shows all objects (no frame over 50 ms).
	// A project file of version 2 is opened through its working folder (DZS_WorkFolder): the group and loot files are
	// read there, the folder stays where the project is saved.
	void LoadProject(string folder, bool sliced = false, DZS_Json backupHead = null)
	{
		DZS_Log.Info("open project " + folder);
		array<string> pchunks = DZS_Files.Read(DZS_LocalStore.File(folder));
		bool v2 = DZS_ProjectFile.IsV2(pchunks);
		if (v2 && WaitForBase(DZS_WorkFolder.RootOf(pchunks), folder, sliced, backupHead))
			return;
		DZS_OpenJob j = new DZS_OpenJob();
		j.m_BackupHead = backupHead;
		j.m_T0 = TickCount(0);
		j.m_Started = m_Now;
		j.m_Folder = folder;
		string skipText;
		if (GetCLIParam("dzsOpenSkip", skipText))
			j.m_Skip = skipText.ToInt();
		ShowHome(false);
		DZS_OpenProject op = new DZS_OpenProject();
		j.m_Op = op;
		op.m_Folder = folder;
		DZS_StoreProvider lp = ProviderOf(folder);
		j.m_Lp = lp;
		if (lp)
			op.m_Provider = lp.Id();
		DZS_Issues issues = op.m_OpenIssues;
		if (pchunks)
			op.Remember(DZS_ProjectFile.NAME, DZS_Files.Checksum(pchunks));
		else
			op.Remember(DZS_ProjectFile.NAME, "");
		string src = folder;
		// an older project's files, taken along when its folder was converted, are not read any more once it is saved
		if (v2 && !lp && folder.IndexOf("$profile:") != 0 && DZS_Files.IsFolder(DZS_LocalStore.Dir(folder)))
			DZS_Files.DeleteTree(DZS_LocalStore.Dir(folder));
		if (v2)
		{
			DZS_WorkResult wr = DZS_WorkFolder.Build(folder, pchunks, LoadedMapKey(), issues);
			if (wr.m_Error != "")
			{
				DZS_Log.Error("open: project file of " + folder + ": " + wr.m_Error);
				Message("projects.open", "danger", "Projektdatei nicht lesbar: " + wr.m_Error, "Project file not readable: " + wr.m_Error, 6.0);
				ShowHome(true);
				return;
			}
			DZS_ProjectExtensions.Load(DZS_WorkFolder.RootOf(pchunks), issues);
			pchunks = wr.m_Manifest;
			src = wr.m_Dir;
			op.m_Work = src;
			op.m_Base = wr.m_Base;
			op.m_Origin = wr.m_Origin;
		}
		j.m_Src = src;
		DZS_PFData pf = DZS_ProjectFile.Read(pchunks);
		j.m_Pf = pf;
		bool manifestMissing = pf.m_Status != "ok" || !pf.m_HasProject;
		if (manifestMissing)
			issues.Add(DZS_Issue.WARNING, "Projektdatei fehlte oder war nicht lesbar — wird beim nächsten Speichern neu geschrieben", "Project file was missing or unreadable — it is written again at the next save");
		else
			issues.Append(pf.m_Issues);
		if (lp)
			lp.OpenIssues(folder, issues);
		op.m_Name = folder;
		if (pf.m_Status == "ok" && pf.m_Name != "")
			op.m_Name = pf.m_Name;
		op.m_MapKey = LoadedMapKey();
		array<string> files = DZS_LocalStore.GroupFiles(src);
		array<bool> taken = new array<bool>();
		foreach (string f0 : files)
			taken.Insert(false);
		array<ref DZS_PFGroup> order = j.m_Order;
		array<string> orderFile = j.m_OrderFile;
		array<string> usedRel = new array<string>();
		if (!manifestMissing)
		{
			foreach (DZS_PFGroup g : pf.m_Groups)
			{
				string rel = g.m_File;
				rel.Replace("\\", "/");
				if (DZS_LocalStore.InvalidRel(rel))
				{
					issues.Add(DZS_Issue.WARNING, "Gruppen-Eintrag mit ungültiger Datei „" + g.m_File + "“ fällt weg", "Group entry with invalid file \"" + g.m_File + "\" is dropped");
					continue;
				}
				string key = DZS_OpenProject.Key(rel);
				if (usedRel.Find(key) >= 0)
				{
					issues.Add(DZS_Issue.WARNING, "Zwei Einträge nennen „" + g.m_File + "“ — der zweite fällt weg", "Two entries name \"" + g.m_File + "\" — the second is dropped");
					continue;
				}
				int at = -1;
				for (int i = 0; i < files.Count(); i++)
				{
					if (DZS_OpenProject.Key(files[i]) == key)
						at = i;
				}
				if (at < 0)
				{
					string ename = g.m_Name;
					if (ename == "")
						ename = g.m_File;
					issues.Add(DZS_Issue.HINT, "Gruppe " + ename + ": Datei " + g.m_File + " nicht gefunden — aus dem Projekt entfernt", "Group " + ename + ": file " + g.m_File + " not found — removed from the project");
					continue;
				}
				usedRel.Insert(key);
				taken[at] = true;
				order.Insert(g);
				orderFile.Insert(files[at]);
			}
		}
		array<string> rest = new array<string>();
		for (int r = 0; r < files.Count(); r++)
		{
			if (!taken[r])
				rest.Insert(files[r]);
		}
		SortPaths(rest);
		if (backupHead && !manifestMissing)
			rest.Clear();
		foreach (string rf : rest)
		{
			order.Insert(null);
			orderFile.Insert(rf);
		}
		// the load: project, groups, objects
		DZS_Instance inst = m_Kernel.NewLoadInstance();
		j.m_Inst = inst;
		DZS_OpBuilder b = inst.BeginOperation("core:load", 1);
		j.m_B = b;
		map<string, string> pfields = new map<string, string>();
		pfields.Set("lootEnabled", DZS_Str.Bool(pf.m_LootEnabled));
		pfields.Set("lootHidden", DZS_Str.Bool(pf.m_LootHidden));
		pfields.Set("lootLocked", DZS_Str.Bool(pf.m_LootLocked));
		pfields.Set("writtenTypes", WrittenJson(pf.m_WrittenTypes, pf.m_WrittenOrig));
		string pid = pf.m_ProjectId;
		if (pf.m_LootEnabled && pid == "" && !ProviderOf(folder) && !v2)
		{
			pid = NewProjectId();
			issues.Add(DZS_Issue.WARNING, "Kennung des Projekts (loot.projectId) fehlte oder war ungültig — neue Kennung " + pid, "Project id (loot.projectId) was missing or invalid — new id " + pid);
			manifestMissing = true;
		}
		else if (pf.m_ProjectIdBad && !pf.m_LootEnabled)
			issues.Add(DZS_Issue.WARNING, "project.loot.projectId ist ungültig", "project.loot.projectId is invalid");
		pfields.Set("lootProjectId", pid);
		array<ref DZS_Companion> pcomp = new array<ref DZS_Companion>();
		pcomp.Insert(new DZS_Companion(DZS_LocalStore.GROUP_PLUGINS, 1, pf.m_Plugins));
		b.Create(DZS_KIND_PROJECT, "", "", pfields, false, false, pcomp, DZS_PROJECT_ID);
		DZS_StoreProvider chp = ProviderOf(folder);
		if (chp && chp.PartChanged(folder))
			manifestMissing = true;
		if (!pf.m_LootEnabled && pf.m_WrittenTypes.Count() > 0)
		{
			issues.Add(DZS_Issue.WARNING, "Loot ist aus, aber " + pf.m_WrittenTypes.Count() + " geschriebene Typen sind noch nicht zurückgesetzt — das nächste Speichern setzt sie zurück", "Loot is off, but " + pf.m_WrittenTypes.Count() + " written types are not reset yet — the next save resets them");
			manifestMissing = true;
		}
		j.m_ManifestMissing = manifestMissing;
		if (!sliced)
		{
			while (!StepOpenGroups(j, 1000000))
			{
			}
			FinishOpen(j);
			return;
		}
		m_Opening = j;
		j.m_Progress = new DZS_ProgressDialog(DZS_Texts.Pick("Projekt", "Project"), op.m_Name, DZS_Texts.Pick("Öffnet", "Opening"));
		j.m_Progress.Open();
	}

	protected ref DZS_OpenJob m_Opening;
	// opening waits for the vanilla mission, the base of a project with loot
	protected ref DZS_LootVanilla m_BaseFetch;
	protected string m_WaitFolder;
	protected bool m_WaitSliced;
	protected ref DZS_Json m_WaitHead;

	// True when the project needs its base's loot files and they are not here yet: the vanilla mission is fetched
	// first and opening goes on afterwards (without the base the project's loot would be lost at the next save).
	protected bool WaitForBase(DZS_Json root, string folder, bool sliced, DZS_Json backupHead)
	{
		if (!root || !DZS_WorkFolder.NeedsBase(root))
			return false;
		string mk = DZS_WorkFolder.MapOf(root);
		if (DZS_Maps.WorldOf(mk) == "")
			mk = LoadedMapKey();
		if (DZS_WorkFolder.UsableBase(DZS_WorkFolder.BaseOf(root), mk, null) != "")
			return false;
		if (m_BaseFetch)
			return true;
		m_WaitFolder = folder;
		m_WaitSliced = sliced;
		m_WaitHead = backupHead;
		m_BaseFetch = new DZS_LootVanilla();
		m_BaseFetch.m_OnDone.Insert(OnBaseFetched);
		if (!m_BaseFetch.StartMission(mk))
		{
			m_BaseFetch = null;
			Message("projects.open", "danger", "Die Vanilla-Mission (Basis des Projekts) lässt sich nicht laden: kein Netz", "The vanilla mission (the project's base) cannot be fetched: no network", 8.0);
			ShowHome(true);
			return true;
		}
		ShowHome(false);
		string shown = DZS_Maps.Display(mk);
		m_Kernel.GetScreen().ShowBanner("projects.base", 20, "info", DZS_Texts.Pick("Vanilla-Mission " + shown + " wird geladen (Basis des Projekts) …", "Loading the vanilla mission " + shown + " (the project's base) …"));
		DZS_Log.Info("open: " + folder + " waits for the vanilla mission of " + mk);
		return true;
	}

	void OnBaseFetched(DZS_LootVanilla v)
	{
		m_Kernel.GetScreen().HideBanner("projects.base");
		bool ok = v.m_Ok;
		string why = v.m_Why;
		m_BaseFetch = null;
		DZS_Json head = m_WaitHead;
		m_WaitHead = null;
		if (!ok)
		{
			Message("projects.open", "danger", "Basis des Projekts nicht geladen (" + why + ") — Öffnen abgebrochen", "The project's base was not fetched (" + why + ") — opening cancelled", 8.0);
			ShowHome(true);
			return;
		}
		LoadProject(m_WaitFolder, m_WaitSliced, head);
	}

	bool IsOpening() { return m_Opening != null; }

	protected void StepOpening()
	{
		DZS_OpenJob j = m_Opening;
		if (!j)
			return;
		int t0 = TickCount(0);
		if (j.m_Restore && j.m_Phase == DZS_OpenJob.GROUPS)
		{
			string rerr;
			if (!StepRestoreBuild(j, OPEN_SLICE_MS, rerr))
			{
				RestoreFailed(j, rerr);
				return;
			}
		}
		else if (j.m_Phase == DZS_OpenJob.GROUPS)
		{
			if (StepOpenGroups(j, OPEN_SLICE_MS))
			{
				j.m_Phase = DZS_OpenJob.LOAD;
				j.m_Progress.SetShare(0.9);
			}
			else
				j.m_Progress.SetShare(0.9 * j.Share());
		}
		else if (j.m_Phase == DZS_OpenJob.LOAD)
		{
			string lerr;
			if (!StepLoad(j, OPEN_SLICE_MS, lerr))
			{
				if (j.m_Restore)
				{
					RestoreFailed(j, lerr);
					return;
				}
				DZS_Log.Error("open: " + lerr);
				Message("projects.open", "danger", "Öffnen fehlgeschlagen: " + lerr, "Opening failed: " + lerr);
				ShowHome(true);
				m_Opening = null;
				j.m_Progress.Close();
				return;
			}
			if (!j.m_Loaded)
			{
				j.m_Progress.SetShare(0.9 + 0.05 * j.LoadShare());
				return;
			}
			if (j.m_Restore)
				FinishRestore(j);
			else if (!FinishOpen(j))
			{
				m_Opening = null;
				j.m_Progress.Close();
				return;
			}
			j.m_Phase = DZS_OpenJob.SHOW;
			DZS_SceneApi scene = DZS_SceneApi.Get();
			if (scene)
				j.m_ShowTotal = scene.PendingObjects();
		}
		else
		{
			DZS_SceneApi sc = DZS_SceneApi.Get();
			int left = 0;
			if (sc)
				left = sc.PendingObjects();
			if (left > 0 && j.m_ShowTotal > 0)
			{
				float shownShare = 1.0 - left / (j.m_ShowTotal * 1.0);
				j.m_Progress.SetShare(0.95 + 0.05 * shownShare);
			}
			else
			{
				m_Opening = null;
				j.m_Progress.Close();
				DZS_Log.Info("open sliced: " + j.m_Frames + " frames over " + DZS_Num.Fixed(m_Now - j.m_Started, 2) + " s, slowest frame " + DZS_Num.Fixed(j.m_Slowest, 1) + " ms");
				if (!j.m_Restore && !j.m_BackupHead)
					ShowOpenIssues(j);
				return;
			}
		}
		float ms = (TickCount(0) - t0) / 10000.0;
		j.m_Frames++;
		if (ms > j.m_Slowest)
			j.m_Slowest = ms;
	}

	// Groups and objects into the load builder; true when all are in.
	protected bool StepOpenGroups(DZS_OpenJob j, float ms)
	{
		int t0 = TickCount(0);
		float limit = ms * 10000;
		DZS_OpenProject op = j.m_Op;
		DZS_Issues issues = op.m_OpenIssues;
		DZS_OpBuilder b = j.m_B;
		while (j.m_Gi < j.m_Order.Count())
		{
			float left = ms - (TickCount(0) - t0) / 10000.0;
			if (left <= 0)
				return false;
			int tk = TickCount(0);
			if (j.m_Step == 0)
			{
				j.m_File = j.m_OrderFile[j.m_Gi];
				j.m_Chunks = DZS_Files.Read(DZS_LocalStore.Path(j.m_Src, j.m_File));
				if (!j.m_Chunks)
					j.m_Chunks = new array<string>();
				j.m_Sum = new DZS_FileSum();
				j.m_Step = 1;
				j.m_TRead += TickCount(0) - tk;
				continue;
			}
			if (j.m_Step == 1)
			{
				int sumMs = left;
				if (sumMs < 1)
					sumMs = 1;
				bool summed = j.m_Sum.Feed(j.m_Chunks, sumMs);
				j.m_TSum += TickCount(0) - tk;
				if (!summed)
					continue;
				if (j.m_Src == j.m_Folder)
					op.Remember(j.m_File, j.m_Sum.Result());
				j.m_Fmt = m_Kernel.FormatForFile(j.m_File);
				j.m_Reader = j.m_Fmt.BeginRead(j.m_Chunks);
				j.m_Step = 2;
				continue;
			}
			if (j.m_Step == 2)
			{
				bool read = j.m_Reader.Step(left);
				j.m_TParse += TickCount(0) - tk;
				if (!read)
					continue;
				j.m_Step = 4;
				continue;
			}
			if (j.m_Step == 4)
			{
				StartOpenGroup(j);
				j.m_TBuild += TickCount(0) - tk;
				float startMs = (TickCount(0) - tk) / 10000.0;
				if (startMs > 20)
					DZS_Log.Info("open: group " + j.m_File + " started in " + DZS_Num.Fixed(startMs, 1) + " ms");
				continue;
			}
			DZS_ReadResult rr = j.m_RR;
			while (j.m_Oi < rr.m_Objects.Count())
			{
				int oi = j.m_Oi;
				DZS_ReadObject ro = rr.m_Objects[oi];
				bool oh = false;
				bool ol = false;
				if (oi < j.m_Matched.Count() && j.m_Matched[oi])
				{
					oh = j.m_Matched[oi].m_Hidden;
					ol = j.m_Matched[oi].m_Locked;
				}
				array<ref DZS_Companion> oc = new array<ref DZS_Companion>();
				for (int ci = 0; ci < ro.m_Data.m_Companions.Count(); ci++)
					oc.Insert(ro.m_Data.m_Companions.GetElement(ci));
				if (j.m_GHidden)
					oh = true;
				if (j.m_GLocked)
					ol = true;
				b.Create(DZS_KIND_OBJECT, j.m_Gid, DZS_END, ro.m_Data.m_Fields, oh, ol, oc);
				j.m_ObjectCount++;
				j.m_Oi++;
				if ((j.m_Oi & 15) == 0 && TickCount(0) - t0 >= limit)
					break;
			}
			j.m_TBuild += TickCount(0) - tk;
			if (j.m_Oi < rr.m_Objects.Count())
				continue;
			NextOpenGroup(j);
		}
		return true;
	}

	protected void NextOpenGroup(DZS_OpenJob j)
	{
		j.m_Gi++;
		j.m_Step = 0;
		j.m_Chunks = null;
		j.m_Sum = null;
		j.m_Reader = null;
		j.m_RR = null;
		j.m_Matched = null;
		j.m_Oi = 0;
	}

	// The group element of a read file; its objects follow in slices.
	protected void StartOpenGroup(DZS_OpenJob j)
	{
		DZS_OpenProject op = j.m_Op;
		DZS_Issues issues = op.m_OpenIssues;
		DZS_OpBuilder b = j.m_B;
		int gi = j.m_Gi;
		DZS_PFGroup entry = j.m_Order[gi];
		string file = j.m_File;
		string gname = Leaf(file);
		string color = "";
		bool ghidden = false;
		bool glocked = false;
		if (entry)
		{
			if (entry.m_Name != "")
				gname = entry.m_Name;
			else
				issues.Add(DZS_Issue.WARNING, "Gruppe ohne gültigen Namen — " + gname, "Group without a valid name — " + gname);
			color = entry.m_Color;
			if (color == "")
				issues.Add(DZS_Issue.WARNING, "Gruppe " + gname + ": Farbe ungültig — nächste Farbe der Reihe", "Group " + gname + ": invalid colour — next colour of the row");
			ghidden = entry.m_Hidden;
			glocked = entry.m_Locked;
		}
		if (color == "")
			color = DZS_Palette.Next(j.m_Colors);
		j.m_Colors.Insert(color);
		DZS_Format fmt = j.m_Fmt;
		DZS_ReadResult rr = j.m_Reader.Result();
		j.m_RR = rr;
		map<string, string> gf = new map<string, string>();
		gf.Set(DZS_Obj.GROUP_NAME, gname);
		gf.Set(DZS_Obj.GROUP_COLOR, color);
		gf.Set(DZS_Obj.GROUP_FILE, file);
		gf.Set(DZS_Obj.GROUP_FORMAT, fmt.GetId());
		array<ref DZS_Companion> gcomp = new array<ref DZS_Companion>();
		string gplugins = "[]";
		if (entry)
			gplugins = entry.m_Plugins;
		gcomp.Insert(new DZS_Companion(DZS_LocalStore.GROUP_PLUGINS, 1, gplugins));
		if (!rr.m_Readable)
		{
			gf.Set("unreadable", "1");
			if (entry)
				gcomp.Insert(new DZS_Companion(DZS_LocalStore.GROUP_ENTRY, 1, entry.m_RawEntry));
			j.m_Errors++;
		}
		foreach (DZS_Companion rc : rr.m_GroupCompanions)
			gcomp.Insert(rc);
		// a group's mark lives on its objects; older projects marked the group itself
		string gid = b.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, gf, false, false, gcomp);
		j.m_Gid = gid;
		j.m_GHidden = ghidden;
		j.m_GLocked = glocked;
		issues.Append(rr.m_Issues, gname, file);
		if (m_ImportNew && (file == m_ImportNew.m_Base + ".json" || Leaf(file) == DZS_ProjectV2.GroupId(m_ImportNew.m_Base)))
		{
			AddImported(b, gid, m_ImportNew);
			j.m_ObjectCount += m_ImportNew.m_Read.m_Objects.Count();
			issues.Append(m_ImportNew.m_Read.m_Issues, gname, file);
			DZS_Log.Info("dze: new project '" + op.m_Name + "' from " + m_ImportNew.m_File + ", " + m_ImportNew.m_Read.m_Objects.Count() + " object(s)");
			LogIssues(m_ImportNew.m_Read.m_Issues);
		}
		j.m_Matched = new array<DZS_PFObject>();
		j.m_Oi = 0;
		j.m_Step = 3;
		if (!rr.m_Readable)
		{
			rr.m_Objects.Clear();
			return;
		}
		if (entry && (j.m_Skip & 1) == 0)
		{
			int lostL = 0;
			int lostH = 0;
			j.m_Matched = DZS_LocalStore.Match(entry, rr, lostL, lostH);
			if (lostL + lostH > 0)
			{
				string lostDe = DZS_LocalStore.LostText(gname, lostL, lostH, false);
				string lostEn = DZS_LocalStore.LostText(gname, lostL, lostH, true);
				issues.Add(DZS_Issue.HINT, lostDe, lostEn);
			}
		}
	}

	// The load as one system operation, then the project is shown (the scene places its objects over the next frames).
	protected bool FinishOpen(DZS_OpenJob j)
	{
		DZS_OpenProject op = j.m_Op;
		DZS_Issues issues = op.m_OpenIssues;
		DZS_PFData pf = j.m_Pf;
		DZS_Instance inst = j.m_Inst;
		string folder = j.m_Folder;
		int skip = j.m_Skip;
		string error;
		int tk = TickCount(0);
		if (!j.m_Loaded && !inst.LoadBuilt(j.m_B, error))
		{
			DZS_Log.Error("open: " + error);
			Message("projects.open", "danger", "Öffnen fehlgeschlagen: " + error, "Opening failed: " + error);
			ShowHome(true);
			return false;
		}
		int tLoad = TickCount(0) - tk;
		if ((skip & 16) == 0)
			m_Open = op;
		if (j.m_Lp)
			j.m_Lp.Opened(folder);
		m_Kernel.AddLoaded(inst);
		tk = TickCount(0);
		m_Kernel.ShowInstance(inst);
		int tShow = TickCount(0) - tk;
		DZS_Log.Info("open timing: read " + DZS_Num.Fixed(j.m_TRead / 10000.0, 0) + " ms, checksum " + DZS_Num.Fixed(j.m_TSum / 10000.0, 0) + " ms, parse " + DZS_Num.Fixed(j.m_TParse / 10000.0, 0) + " ms, build " + DZS_Num.Fixed(j.m_TBuild / 10000.0, 0) + " ms, load " + DZS_Num.Fixed(tLoad / 10000.0, 0) + " ms, show " + DZS_Num.Fixed(tShow / 10000.0, 0) + " ms, total " + DZS_Num.Fixed((TickCount(0) - j.m_T0) / 10000.0, 0) + " ms");
		if ((skip & 2) == 0)
		{
			if (j.m_ManifestMissing)
				inst.SetSaveMode("manifest");
			else
				inst.SetSaveMode("opened");
		}
		DZS_CameraPlugin cam = Cam();
		if (cam && (skip & 4) == 0)
		{
			if (pf.m_HasCamera && InsideMap(pf.m_CamPos))
				cam.SetPose(pf.m_CamPos, pf.m_Heading, pf.m_Pitch, false);
			else
				cam.GoToStart();
		}
		if (j.m_BackupHead)
		{
			DZS_Json bh = j.m_BackupHead;
			op.m_Folder = bh.GetStr("location");
			op.m_Provider = bh.GetStr("provider");
			op.m_Name = bh.GetStr("name");
			op.m_Sums.Clear();
			DZS_Json sums = bh.Get("sums");
			if (sums && sums.IsObj())
			{
				for (int k = 0; k < sums.m_Keys.Count(); k++)
					op.m_Sums.Set(sums.m_Keys[k], sums.GetStr(sums.m_Keys[k]));
			}
			inst.SetSaveMode("restored");
			RestoredToProvider(op, bh);
			Touch(op.m_Folder, "opened");
			DZS_Log.Info("restored '" + op.m_Name + "' from the files in " + folder + ", " + inst.CountElements() + " element(s)");
		}
		else if ((skip & 8) == 0)
		{
			Touch(folder, "opened");
			m_Kernel.GetSettings().Set("projects.lastMap", DZS_Json.Str(op.m_MapKey));
		}
		m_LastStepAt = -1;
		m_Pending = false;
		DZS_Log.Info("project open: '" + op.m_Name + "' (" + folder + "), " + j.m_Order.Count() + " group(s), " + j.m_ObjectCount + " object(s), " + j.m_Errors + " unreadable, issues " + issues.Total() + " [" + issues.Title() + "]");
		if (cam)
			DZS_Log.Info("project open: camera speed stage " + cam.GetStage() + ", stored " + m_Kernel.GetSettings().GetNumber("camera.speedStage", -1));
		if (!j.m_Progress && !j.m_BackupHead)
			ShowOpenIssues(j);
		return true;
	}

	// The load in parts, each about ms long; true while all is well (j.m_Loaded once the last part is in).
	protected bool StepLoad(DZS_OpenJob j, float ms, out string error)
	{
		if (!j.m_LoadOp)
		{
			if (j.m_Built)
				j.m_LoadOp = j.m_Built;
			else
				j.m_LoadOp = j.m_B.GetOperation();
		}
		int t0 = TickCount(0);
		float limit = ms * 10000;
		int n = j.m_LoadOp.CountChanges();
		while (j.m_LoadAt < n)
		{
			int to = Math.Min(j.m_LoadAt + 250, n);
			if (!j.m_Inst.LoadPart(j.m_LoadOp, j.m_LoadAt, to, error))
				return false;
			j.m_LoadAt = to;
			if (TickCount(0) - t0 >= limit)
				break;
		}
		if (j.m_LoadAt >= n)
			j.m_Loaded = true;
		return true;
	}

	protected void ShowOpenIssues(DZS_OpenJob j)
	{
		if ((j.m_Skip & 32) != 0)
			return;
		DZS_Issues issues = j.m_Op.m_OpenIssues;
		foreach (DZS_Issue li : issues.m_Items)
			DZS_Log.Info("open issue " + li.m_Severity + ": " + li.m_What + " | " + li.m_Group + " | " + li.m_File + " | " + li.m_De);
		if (issues.Count(DZS_Issue.ERROR) + issues.Count(DZS_Issue.WARNING) + issues.Count(DZS_Issue.HINT) > 0)
		{
			m_LastIssues = issues;
			m_LastIssuesKind = "open";
			string tx10a = DZS_Texts.Pick("Geöffnet · ", "Opened · ");
			string tx10b = issues.Title();
			string tx10 = tx10a + tx10b;
			string tx11 = DZS_Texts.Pick("Anzeigen", "Show");
			m_Kernel.GetScreen().ShowMessageWithButton("projects.opened", "info", tx10, 10.0, tx11, m_Msg);
		}
	}

	bool InsideMap(vector p)
	{
		float size = GetGame().GetWorld().GetWorldSize();
		return p[0] >= 0 && p[2] >= 0 && p[0] <= size && p[2] <= size && p[1] == p[1];
	}

	static string Leaf(string rel)
	{
		int slash = rel.LastIndexOf("/");
		string leaf = rel.Substring(slash + 1, rel.Length() - slash - 1);
		int dot = leaf.LastIndexOf(".");
		if (dot > 0)
			leaf = leaf.Substring(0, dot);
		return leaf;
	}

	// writtenTypes in the project state: [{"name": …, "original": text or null}] (originals "" = null).
	static string WrittenJson(array<string> names, array<string> origs)
	{
		DZS_Json wt = DZS_Json.Arr();
		for (int i = 0; i < names.Count(); i++)
		{
			DZS_Json one = DZS_Json.Obj();
			string nm = names[i];
			one.Set("name", DZS_Json.Str(nm));
			string orig = origs[i];
			if (orig == "")
				one.Set("original", DZS_Json.Null());
			else
				one.Set("original", DZS_Json.Str(orig));
			wt.Add(one);
		}
		array<string> chunks = wt.ToChunks();
		return DZS_SpawnerFormat.Join(chunks);
	}

	static void ReadWritten(string json, array<string> names, array<string> origs)
	{
		string err;
		DZS_Json wj = DZS_Json.ParseText(json, err);
		if (!wj || !wj.IsArr())
			return;
		for (int t = 0; t < wj.Count(); t++)
		{
			DZS_Json one = wj.At(t);
			if (!one || !one.IsObj())
				continue;
			DZS_Json n = one.Get("name");
			DZS_Json o = one.Get("original");
			if (!n || !n.IsStr())
				continue;
			names.Insert(n.m_Str);
			string orig = "";
			if (o && o.IsStr())
				orig = o.m_Str;
			origs.Insert(orig);
		}
	}

	static string NewProjectId()
	{
		DZS_Random.EnsureSeeded();
		string hex = "0123456789abcdef";
		string r = "";
		for (int i = 0; i < 16; i++)
			r += hex.Get(Math.RandomInt(0, 16));
		return r;
	}

	static void SortPaths(array<string> a)
	{
		for (int i = 1; i < a.Count(); i++)
		{
			string v = a[i];
			int j = i - 1;
			while (j >= 0 && DZS_LocalStore.ComparePaths(a[j], v) > 0)
			{
				a[j + 1] = a[j];
				j--;
			}
			a[j + 1] = v;
		}
	}

	// ---- new project ----

	void RequestNew(string name, string mapKey)
	{
		Leave("new", name, mapKey);
	}

	protected void GoNew(string name, string mapKey)
	{
		if (!DZS_Maps.Available(mapKey))
			return;
		string view = "";
		if (DZS_Maps.WorldOf(mapKey) == m_World && Cam() && Cam().IsStarted())
		{
			CloseProject(true);
			Cam().GoToStart();
			view = CameraView();
		}
		string groupName = DZS_Texts.Get("core.group.objects");
		string folder = DZS_LocalStore.NewProject(name, mapKey, view, "objects.json", groupName);
		Touch(folder, "opened");
		if (DZS_Maps.WorldOf(mapKey) != m_World)
		{
			DZS_Handover.Set("new", folder, mapKey);
			SwitchWorld(mapKey);
			return;
		}
		FinishNew(folder);
	}

	// After the world is there: the start camera goes into the new project file, then it opens as "new".
	protected void FinishNew(string folder)
	{
		DZS_CameraPlugin cam = Cam();
		string path = DZS_LocalStore.File(folder);
		array<string> cur = DZS_Files.Read(path);
		DZS_PFData pf = DZS_ProjectFile.Read(cur);
		if (cam && pf.m_Status == "ok" && pf.m_RawView == "")
		{
			cam.GoToStart();
			if (m_ImportNew && m_ImportNew.CameraInside())
				cam.SetPose(m_ImportNew.m_Data.m_Cam, cam.GetYaw(), cam.GetPitch(), false);
			array<string> withView = DZS_ProjectFile.RewriteV2(cur, "", "", CameraView());
			if (!withView)
				withView = DZS_ProjectFile.Compose(pf.m_Name, pf.m_MapKey, pf.m_RawProject, CameraView());
			DZS_Files.Write(path, withView);
		}
		LoadProject(folder);
		DZS_Instance inst = Shown();
		if (inst)
			inst.SetSaveMode("new");
		if (m_ImportNew && inst)
		{
			m_ImportNew = null;
			StartSave(false, "");
		}
		m_ImportNew = null;
	}


	string CameraView()
	{
		DZS_CameraPlugin cam = Cam();
		if (!cam || !cam.IsStarted())
			return "";
		return DZS_ProjectFile.ViewPart(cam.GetPosition(), cam.GetYaw(), cam.GetPitch());
	}

	// ---- saving ----

	// then: "" or "leave" (continue m_After afterwards); saveAsName for Save as.
	// One save at a time: a save asked for while the provider's last one is undecided is queued
	protected bool m_SaveQueued;
	protected bool m_QueuedAuto;
	protected string m_QueuedThen;

	void StartSave(bool isAuto, string then, string saveAsName = "", bool copy = false)
	{
		DZS_Instance inst = Shown();
		if (!inst || !m_Open || m_Busy || m_Building || m_Outside)
			return;
		AbortBackupFiles("save");
		DZS_SaveRules rules = DZS_SaveRules.Get();
		string blockDe, blockEn;
		if (rules && saveAsName == "" && rules.SaveBlocked(inst, blockDe, blockEn))
			return;
		if (saveAsName == "")
		{
			DZS_StoreProvider running = ProviderOf(m_Open.m_Folder);
			if (running && running.SaveRunning(m_Open.m_Folder))
			{
				bool first = !m_SaveQueued;
				if (first)
					DZS_Log.Info("save waits until the running one is decided");
				m_QueuedAuto = isAuto && (first || m_QueuedAuto);
				m_SaveQueued = true;
				m_QueuedThen = then;
				inst.SetSaving(true);
				return;
			}
		}
		bool noLoot = copy && m_CopyNoLoot;
		if (saveAsName != "" && !copy && m_SaveAsNoLoot)
		{
			noLoot = true;
			m_SaveAsNoLoot = false;
		}
		DZS_SaveMark mark = inst.SaveMark();
		m_Building = BeginPlan(inst, noLoot);
		m_Building.m_Started = m_Now;
		DZS_SavePlan plan = m_Building.m_Plan;
		plan.m_Mark = mark;
		plan.m_NoLoot = noLoot;
		plan.m_Then = then;
		plan.m_Auto = isAuto;
		if (saveAsName != "")
		{
			plan.m_SaveAs = true;
			plan.m_Name = saveAsName;
			plan.m_Copy = copy;
		}
		if (copy)
		{
			plan.m_NoLoot = noLoot;
			plan.m_NoteDe = m_CopyNoteDe;
			plan.m_NoteEn = m_CopyNoteEn;
			m_CopyNoLoot = false;
			m_CopyNoteDe = "";
			m_CopyNoteEn = "";
		}
		inst.SetSaving(true);
		StepBuilding();
	}

	// Saving: the plan is built in slices of about 4 ms; changes meanwhile count for the next save.
	protected ref DZS_PlanBuild m_Building;

	protected void StepBuilding()
	{
		DZS_PlanBuild b = m_Building;
		if (!b)
			return;
		DZS_Instance inst = Shown();
		if (inst != b.m_Inst || !m_Open)
		{
			m_Building = null;
			if (b.m_Inst)
			{
				b.m_Inst.SetSaving(false);
				EndPlanFreeze(b);
			}
			DZS_Log.Info("save plan stopped: project changed");
			return;
		}
		int t0 = TickCount(0);
		bool done = StepPlan(b, SLICE_MS);
		float ms = (TickCount(0) - t0) / 10000.0;
		b.m_Frames++;
		if (ms > b.m_Slowest)
			b.m_Slowest = ms;
		if (!done)
			return;
		m_Building = null;
		inst.SetSaving(false);
		ContinueSave(b.m_Plan, b);
	}

	protected void ContinueSave(DZS_SavePlan plan, DZS_PlanBuild b)
	{
		bool isAuto = plan.m_Auto;
		m_Plan = plan;
		DZS_Issues issues = plan.m_Issues;
		int bad = issues.Count(DZS_Issue.ERROR) + issues.Count(DZS_Issue.WARNING);
		DZS_Log.Info("save plan: " + plan.m_Files.Count() + " file(s) in " + b.m_Frames + " frames over " + DZS_Num.Fixed(m_Now - b.m_Started, 2) + " s, slowest frame " + DZS_Num.Fixed(b.m_Slowest, 1) + " ms, issues [" + issues.Title() + "]" + " auto " + isAuto);
		if (bad > 0)
		{
			if (isAuto)
			{
				DZS_Issues serious = new DZS_Issues();
				foreach (DZS_Issue si : issues.m_Items)
				{
					if (si.m_Severity != DZS_Issue.HINT)
						serious.m_Items.Insert(si);
				}
				PauseAuto(serious.Title());
				m_Plan = null;
				return;
			}
			m_LastIssues = issues;
			m_LastIssuesKind = "save";
			DZS_CheckListDialog d = new DZS_CheckListDialog(issues, "save", "Speichern", "Save");
			d.m_OnResult.Insert(OnCheckList);
			d.Open();
			return;
		}
		CheckOutside();
	}

	void OnCheckList(DZS_Dialog d, string result)
	{
		DZS_Log.Info("check list: " + result);
		if (result == "save")
		{
			CheckOutside();
			return;
		}
		if (result == "show")
			ShowObject(DZS_CheckListDialog.Cast(d).m_ShowId);
		DZS_SavePlan was = m_Plan;
		Abort();
		if (result == "fixed" && was)
			StartSave(was.m_Auto, was.m_Then, SaveAsNameOf(was), was.m_Copy);
	}

	// The editor never changes cfggameplay.json; a newly written spawner file
	// is offered as the ready entry for objectSpawnersArr.
	protected void ShowSpawnerEntry(string entry)
	{
		m_SpawnerEntry = entry;
		string t = DZS_Texts.Pick("Für cfggameplay.json › objectSpawnersArr: ", "For cfggameplay.json › objectSpawnersArr: ") + DZS_W.Literal(entry);
		m_Kernel.GetScreen().ShowMessageWithButton("projects.spawnerEntry", "info", t, 10.0, DZS_Texts.Pick("Kopieren", "Copy"), m_Msg);
		DZS_Log.Info("spawner entry shown: " + entry);
	}

	protected bool IsGroupFile(string rel)
	{
		DZS_Instance inst = Shown();
		if (!inst)
			return false;
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string gid : groups)
		{
			string file;
			inst.GetField(gid, DZS_Obj.GROUP_FILE, file);
			if (file == rel)
				return true;
		}
		return false;
	}

	protected string SaveAsNameOf(DZS_SavePlan p)
	{
		if (p.m_SaveAs)
			return p.m_Name;
		return "";
	}

	protected void Abort()
	{
		DZS_SavePlan plan = m_Plan;
		m_Plan = null;
		if (plan && plan.m_Then == "leave")
			m_After = "";
	}

	void ShowObject(string id)
	{
		DZS_Instance inst = Shown();
		DZS_CameraPlugin cam = Cam();
		if (!inst || !cam)
			return;
		DZS_Screen scr = m_Kernel.GetScreen();
		if (cam.GetMap() && cam.GetMap().IsOpen())
			cam.GetMap().Close("show");
		if (scr.HasKeyboardOwner("fullsearch"))
			m_Kernel.GetUi().Trigger("catalog.full");
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel && inst.Has(id) && inst.GetKind(id) == DZS_KIND_OBJECT)
		{
			array<string> one = new array<string>();
			one.Insert(id);
			sel.SetSelection(one, id);
			m_Kernel.GetEvents().FirePluginEvent("camera.focusRequest", null);
		}
		else
		{
			string pos;
			if (inst.GetField(id, DZS_Obj.POS, pos))
			{
				vector p = DZS_Obj.QuickVec(pos);
				cam.FocusOn(p - "1 1 1", p + "1 1 1", true);
			}
		}
		string tx8 = DZS_Texts.Pick("Prüfliste", "Check list");
		string tx9 = DZS_Texts.Pick("Wieder öffnen", "Open again");
		m_Kernel.GetScreen().ShowMessageWithButton("checklist", "info", tx8, 10.0, tx9, m_Msg);
	}

	// Files changed outside the editor since they were read or written? Ask before overwriting.
	protected void CheckOutside()
	{
		DZS_SavePlan plan = m_Plan;
		if (!plan)
			return;
		if (plan.m_SaveAs || ProviderOf(m_Open.m_Folder))
		{
			Write();
			return;
		}
		m_Outside = new DZS_OutsideCheck(m_Open.m_Folder, plan.m_Files, m_Open);
		DZS_Instance inst = Shown();
		if (inst)
			inst.SetSaving(true);
		StepOutside();
	}

	protected ref DZS_OutsideCheck m_Outside;

	protected void StepOutside()
	{
		if (!m_Outside)
			return;
		DZS_Instance inst = Shown();
		if (!m_Plan || !m_Open || !inst)
		{
			m_Outside.Abort();
			m_Outside = null;
			return;
		}
		if (!m_Outside.Step(SLICE_MS))
			return;
		array<string> changed = m_Outside.m_Changed;
		DZS_Log.Info("changed outside checked in " + m_Outside.m_Frames + " frames");
		m_Outside = null;
		inst.SetSaving(false);
		DZS_SavePlan plan = m_Plan;
		if (changed.Count() == 0)
		{
			Write();
			return;
		}
		if (plan.m_Auto)
		{
			PauseAuto(DZS_Texts.Pick("Dateien außerhalb geändert", "Files changed outside"));
			m_Plan = null;
			return;
		}
		string list = "";
		foreach (string c : changed)
		{
			if (list != "")
				list += ", ";
			list += c;
		}
		DZS_Log.Info("changed outside: " + list);
		string tx5 = DZS_Texts.Pick("Speichern", "Save");
		string tx6 = DZS_Texts.Pick("Außerhalb geändert", "Changed outside");
		string tx7 = DZS_Texts.Pick("Dateien wurden außerhalb des Editors geändert: " + list, "Files were changed outside the editor: " + list);
		DZS_ConfirmDialog q = new DZS_ConfirmDialog(tx5, tx6, tx7);
		q.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		q.AddFootButton("overwrite", DZS_Texts.Pick("Überschreiben", "Overwrite"), "danger");
		q.m_OnResult.Insert(OnOutsideAnswer);
		q.Open();
	}

	void OnOutsideAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("changed outside answer: " + result);
		if (result == "overwrite")
			Write();
		else
			Abort();
	}

	protected void Write()
	{
		DZS_SavePlan plan = m_Plan;
		m_Plan = null;
		if (!plan)
			return;
		DZS_Instance inst = Shown();
		string folder = m_Open.m_Folder;
		if (plan.m_SaveAs)
		{
			for (int di = plan.m_Files.Count() - 1; di >= 0; di--)
			{
				if (plan.m_Files[di].m_Delete)
					plan.m_Files.Remove(di);
			}
			folder = DZS_LocalStore.FolderFor(plan.m_Name);
			// the project file carries the new name
			DZS_SaveFile pfile = plan.m_Files[plan.m_Files.Count() - 1];
			pfile.m_Chunks = DZS_ProjectFile.ComposeV2(plan.m_Blocks, plan.m_Name, m_Open.m_MapKey, plan.m_Base, plan.m_ProjectPart, CameraView(), DZS_ProjectExtensions.Save());
		}
		m_Busy = true;
		if (plan.m_Copy)
		{
			StartWrite(plan, folder, "copy");
			return;
		}
		inst.SetSaving(true);
		DZS_SaveRules savingRules = DZS_SaveRules.Get();
		if (savingRules)
			savingRules.Saving(inst);
		DZS_StoreProvider sp = null;
		if (!plan.m_SaveAs)
			sp = ProviderOf(folder);
		if (sp)
		{
			m_ProvPlan = plan;
			m_ProvFolder = folder;
			DZS_Log.Info("save: " + plan.m_Files.Count() + " file(s) handed to provider " + sp.Id());
			sp.Save(folder, plan.m_Files);
			return;
		}
		StartWrite(plan, folder, "save");
	}

	protected ref DZS_LocalWrite m_Writing;
	protected ref DZS_SavePlan m_WritingPlan;
	protected string m_WritingFolder;
	protected float m_WritingStart;
	// "save", "copy" (Save copy), "mirror" / "late" (the folder after a provider's save)
	protected string m_WritingKind;
	protected bool m_WritingNoFollow;

	protected void StartWrite(DZS_SavePlan plan, string folder, string kind, bool noFollow = false)
	{
		m_Writing = new DZS_LocalWrite(folder, plan.m_Files);
		m_WritingPlan = plan;
		m_WritingFolder = folder;
		m_WritingStart = m_Now;
		m_WritingKind = kind;
		m_WritingNoFollow = noFollow;
		StepWriting();
	}

	protected void StepWriting()
	{
		if (!m_Writing)
			return;
		if (!m_Writing.Step(SLICE_MS))
			return;
		DZS_LocalWrite w = m_Writing;
		DZS_SavePlan plan = m_WritingPlan;
		string folder = m_WritingFolder;
		m_Writing = null;
		m_WritingPlan = null;
		DZS_Log.Info(m_WritingKind + " files written in " + w.m_Frames + " frames over " + DZS_Num.Fixed(m_Now - m_WritingStart, 2) + " s, slowest frame " + DZS_Num.Fixed(w.m_Slowest, 1) + " ms");
		if (m_WritingKind == "copy")
		{
			m_Busy = false;
			if (w.m_Fail != "")
			{
				Message("projects.save", "danger", "Kopie nicht gespeichert: " + w.m_Fail, "Copy not saved: " + w.m_Fail, 5.0);
				return;
			}
			Touch(folder, "saved");
			if (plan.m_NoteDe != "")
				Message("projects.save", "warning", plan.m_NoteDe, plan.m_NoteEn, 4.0);
			else
				Message("projects.save", "success", "Kopie " + plan.m_Name + " gespeichert", "Copy " + plan.m_Name + " saved", 3.0);
			DZS_Log.Info("copy saved: " + plan.m_Files.Count() + " file(s) to " + folder + ", loot off " + plan.m_NoLoot);
			return;
		}
		if (m_WritingKind == "mirror" || m_WritingKind == "late")
		{
			AfterWrite(plan, folder, "", m_WritingNoFollow);
			if (m_WritingKind == "late")
				AfterLate();
			return;
		}
		AfterWrite(plan, folder, w.m_Fail);
	}

	// Answer of a provider's save: "" = saved there; the folder is then brought to the same state.
	// noFollow: the store kept a later save instead (superseded) — the save ends as saved, without its consequences.
	void ProviderSaved(string failDe, string failEn, bool noFollow = false)
	{
		DZS_SavePlan plan = m_ProvPlan;
		string folder = m_ProvFolder;
		m_ProvPlan = null;
		if (!plan)
			return;
		string fail = "";
		m_LatePlan = null;
		if (failDe != "")
		{
			fail = DZS_Texts.Pick(failDe, failEn);
			if (!plan.m_ToProvider)
			{
				m_LatePlan = plan;
				m_LateFolder = folder;
			}
		}
		if (failDe == "" && !noFollow)
		{
			StartWrite(plan, folder, "mirror", noFollow);
			return;
		}
		AfterWrite(plan, folder, fail, noFollow);
	}

	// A provider save already shown as failed got its answer after all (project/save is
	// repeated in the background up to 10 min): it ends as saved, with its consequences unless noFollow.
	bool ProviderSavedLate(bool noFollow)
	{
		DZS_SavePlan plan = m_LatePlan;
		m_LatePlan = null;
		if (!plan || m_Busy || m_ProvPlan)
			return false;
		plan.m_Then = "";
		plan.m_Auto = false;
		DZS_Instance inst = Shown();
		if (inst)
			inst.SetSaveFailed(false);
		m_Busy = true;
		if (!noFollow)
		{
			StartWrite(plan, m_LateFolder, "late", noFollow);
			return true;
		}
		AfterWrite(plan, m_LateFolder, "", noFollow);
		AfterLate();
		return true;
	}

	protected void AfterLate()
	{
		DZS_Instance inst = Shown();
		if (m_SaveQueued && inst)
			inst.SetSaving(true);
		Message("projects.saveLate", "success", "Speichern doch gelungen", "Saving succeeded after all", 3.0);
	}

	// The provider gave a save shown as failed up for good (10 min without an answer).
	void ProviderLateGivenUp()
	{
		m_LatePlan = null;
	}

	// The provider's save was given up by the user (e.g. "Cancel" in its own question): nothing written, no failure.
	void ProviderCancelled()
	{
		DZS_SavePlan plan = m_ProvPlan;
		m_ProvPlan = null;
		m_Busy = false;
		DZS_Instance inst = Shown();
		if (inst)
			inst.SetSaving(false);
		if (plan && plan.m_Then == "leave")
			m_After = "";
	}

	protected void AfterWrite(DZS_SavePlan plan, string folder, string fail, bool noFollow = false)
	{
		DZS_Instance inst = Shown();
		if (!inst)
		{
			m_Busy = false;
			return;
		}
		inst.SetSaving(false);
		m_Busy = false;
		DZS_SaveRules savedRules = DZS_SaveRules.Get();
		if (fail != "" && plan.m_ToProvider)
		{
			DZS_Log.Warn("save as to provider failed: " + fail);
			Message("projects.save", "danger", "Speichern fehlgeschlagen: " + fail, "Saving failed: " + fail, 5.0);
			return;
		}
		if (fail != "")
		{
			if (savedRules)
				savedRules.Saved(inst, false);
			inst.SetSaveFailed(true);
			DZS_Log.Warn("save failed: " + fail);
			if (fail.IndexOf("incomplete:") == 0)
			{
				string rest = fail.Substring(11, fail.Length() - 11);
				Message("projects.save", "danger", "Speichern unvollständig:" + rest, "Saving incomplete:" + rest, 8.0);
			}
			else
				Message("projects.save", "danger", "Speichern fehlgeschlagen: " + fail, "Saving failed: " + fail, 5.0);
			if (plan.m_Auto)
				WriteBackup();
			if (plan.m_Then == "leave")
				m_After = "";
			return;
		}
		if (plan.m_SaveAs)
		{
			DeleteBackup();
			Touch(folder, "opened");
			DZS_StoreProvider was = ProviderOf(m_Open.m_Folder);
			if (m_Open.m_Provider == "shared")
				m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Get("core.provider.local"), "");
			if (was)
			{
				was.Closed(m_Open.m_Folder);
				m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Get("core.provider.local"), "");
				m_Kernel.GetScreen().SetStatusIcon("projects.autoOff", false, "", "", "");
			}
			m_Open.m_Provider = "local";
			// an older project keeps reading its loot and group files where they are
			if (m_Open.m_Work == "")
				m_Open.m_Work = m_Open.m_Folder;
			if (plan.m_ToProvider)
			{
				DZS_StoreProvider now = ProviderOf(folder);
				if (now)
					m_Open.m_Provider = now.Id();
			}
			m_Open.m_Folder = folder;
			m_Open.m_Name = plan.m_Name;
			m_Open.m_Sums.Clear();
			DZS_SaveContributor dropped = DZS_SaveContributor.Cast(m_Kernel.GetService("save.loot"));
			if (plan.m_NoLoot && dropped && inst)
				dropped.Dropped(inst);
			if (m_SaveAsNoteDe != "")
			{
				Message("projects.save", "warning", m_SaveAsNoteDe, m_SaveAsNoteEn, 4.0);
				m_SaveAsNoteDe = "";
			}
		}
		string fresh = "";
		foreach (DZS_SaveFile f : plan.m_Files)
		{
			if (f.m_Delete)
			{
				m_Open.m_Sums.Remove(DZS_OpenProject.Key(f.m_Rel));
				continue;
			}
			bool isNew = m_Open.Remembered(f.m_Rel) == "";
			if (isNew && f.m_Rel != DZS_ProjectFile.NAME && IsGroupFile(f.m_Rel) && !ProviderOf(folder))
			{
				string rel = f.m_Rel;
				rel.Replace("\\", "/");
				if (fresh != "")
					fresh += ", ";
				fresh += "\"" + rel + "\"";
			}
			string fsum = f.m_Sum;
			if (fsum == "")
				fsum = DZS_Files.Checksum(f.m_Chunks);
			m_Open.Remember(f.m_Rel, fsum);
		}
		if (fresh != "")
			ShowSpawnerEntry(fresh);
		if (plan.m_Mark)
			inst.MarkSavedAt(plan.m_Mark);
		else
			inst.MarkSavedNow();
		if (savedRules)
			savedRules.Saved(inst, true);
		DeleteBackup();
		m_AutoPaused = false;
		m_Kernel.GetScreen().SetStatusIcon("projects.autoPaused", false, "", "", "");
		m_Kernel.GetScreen().HideMessage("checklist");
		m_Pending = inst.IsUnsaved();
		if (m_Pending)
		{
			m_LastStepAt = m_Now;
			m_FirstUnsavedAt = m_Now;
		}
		Touch(m_Open.m_Folder, "saved");
		DZS_Log.Info("saved " + plan.m_Files.Count() + " file(s) to " + folder + (" auto " + plan.m_Auto) + " at " + inst.GetSavedClock());
		DZS_SaveContributor savedHook = DZS_SaveContributor.Cast(m_Kernel.GetService("save.loot"));
		if (noFollow)
			DZS_Log.Info("save ended without its consequences (a later save holds everything)");
		else if (savedHook && !plan.m_NoLoot)
			savedHook.Saved(inst, folder);
		int hints = plan.m_Issues.Count(DZS_Issue.HINT);
		int serious = plan.m_Issues.Count(DZS_Issue.WARNING) + plan.m_Issues.Count(DZS_Issue.ERROR);
		if (hints > 0 && serious == 0 && !plan.m_Auto)
		{
			m_LastIssues = plan.m_Issues;
			m_LastIssuesKind = "save";
			string tx3a = DZS_Texts.Pick("Gespeichert · ", "Saved · ");
			string tx3b = plan.m_Issues.Title();
			string tx3 = tx3a + tx3b;
			string tx4 = DZS_Texts.Pick("Anzeigen", "Show");
			m_Kernel.GetScreen().ShowMessageWithButton("projects.saved", "success", tx3, 10.0, tx4, m_Msg);
		}
		if (plan.m_Then == "leave")
			Continue();
	}

	// Every group through its format (for its check list and the hidden and locked objects), then the one project file.
	protected DZS_SavePlan BuildPlan(DZS_Instance inst, bool noLoot = false)
	{
		DZS_PlanBuild b = BeginPlan(inst, noLoot);
		while (!StepPlan(b, 1000000))
		{
		}
		return b.m_Plan;
	}

	protected DZS_PlanBuild BeginPlan(DZS_Instance inst, bool noLoot)
	{
		DZS_PlanBuild b = new DZS_PlanBuild();
		b.m_Inst = inst;
		b.m_NoLoot = noLoot;
		b.m_Plan = new DZS_SavePlan();
		b.m_Groups = new array<string>();
		b.m_Groups.Copy(inst.GetChildren(DZS_PROJECT_ID));
		b.m_Entries = new array<string>();
		b.m_BlockIds = DZS_ProjectV2.GroupIds(inst);
		b.m_Freeze = inst.BeginFreeze();
		return b;
	}

	// The files show the state at the start of the save: changes meanwhile are read from the frozen copies.
	protected bool StepPlan(DZS_PlanBuild b, float ms)
	{
		DZS_Instance inst = b.m_Inst;
		inst.SetView(b.m_Freeze);
		bool done = StepPlanIn(b, ms);
		inst.SetView(null);
		if (done)
			EndPlanFreeze(b);
		return done;
	}

	protected void EndPlanFreeze(DZS_PlanBuild b)
	{
		if (!b.m_Freeze)
			return;
		DZS_Log.Info("save plan: " + b.m_Freeze.Count() + " element(s) changed meanwhile, written as at the start");
		b.m_Inst.EndFreeze(b.m_Freeze);
		b.m_Freeze = null;
	}

	protected bool StepPlanIn(DZS_PlanBuild b, float ms)
	{
		DZS_Instance inst = b.m_Inst;
		DZS_SavePlan plan = b.m_Plan;
		int t0 = TickCount(0);
		while (true)
		{
			float left = ms - (TickCount(0) - t0) / 10000.0;
			if (left <= 0)
				return false;
			if (b.m_Writer)
			{
				if (!b.m_Writer.Step(left))
					return false;
				EndGroup(b);
				continue;
			}
			if (b.m_Next >= b.m_Groups.Count())
				break;
			string gid = b.m_Groups[b.m_Next];
			b.m_Next++;
			if (!inst.Has(gid) || inst.GetKind(gid) != DZS_KIND_GROUP)
				continue;
			string file, name, color, fmtId, unreadable;
			inst.GetField(gid, DZS_Obj.GROUP_FILE, file);
			inst.GetField(gid, DZS_Obj.GROUP_NAME, name);
			inst.GetField(gid, DZS_Obj.GROUP_COLOR, color);
			inst.GetField(gid, DZS_Obj.GROUP_FORMAT, fmtId);
			inst.GetField(gid, "unreadable", unreadable);
			int v;
			string gplugins = "[]";
			inst.GetCompanion(gid, DZS_LocalStore.GROUP_PLUGINS, v, gplugins);
			if (unreadable == "1")
			{
				DZS_Issue skip = plan.m_Issues.Add(DZS_Issue.WARNING, "Datei nicht lesbar — die Gruppe bleibt leer im Projekt", "File not readable — the group stays empty in the project");
				skip.m_Group = name;
				skip.m_File = file;
				continue;
			}
			b.m_Gid = gid;
			b.m_File = file;
			b.m_Name = name;
			b.m_Color = color;
			b.m_GPlugins = gplugins;
			b.m_Writer = m_Kernel.GetFormat(fmtId).BeginWrite(inst, gid);
		}
		FinishPlan(b);
		return true;
	}

	protected void EndGroup(DZS_PlanBuild b)
	{
		DZS_Instance inst = b.m_Inst;
		DZS_SavePlan plan = b.m_Plan;
		DZS_WriteResult wr = b.m_Writer.Result();
		b.m_Writer = null;
		string gid = b.m_Gid;
		plan.m_Issues.Append(wr.m_Issues, b.m_Name, b.m_File);
		array<ref DZS_PFObject> objs = new array<ref DZS_PFObject>();
		array<string> ids = wr.m_Ids;
		if (!ids)
			ids = inst.GetChildren(gid);
		for (int i = 0; i < ids.Count(); i++)
		{
			bool h = inst.IsHidden(ids[i]);
			bool l = inst.IsLocked(ids[i]);
			if (!h && !l)
				continue;
			DZS_PFObject po = new DZS_PFObject();
			po.m_Index = i;
			po.m_Name = wr.m_Names[i];
			po.m_X = wr.m_FileX[i];
			po.m_Y = wr.m_FileY[i];
			po.m_Z = wr.m_FileZ[i];
			po.m_Hidden = h;
			po.m_Locked = l;
			objs.Insert(po);
		}
		string block;
		if (!b.m_BlockIds.Find(gid, block))
			return;
		string entry = DZS_ProjectFile.GroupEntry(block, b.m_Name, b.m_Color, inst.IsHidden(gid), inst.IsLocked(gid), ids.Count(), objs, b.m_GPlugins, "block");
		b.m_Entries.Insert(entry);
	}

	// The project file: the building blocks (DZS_ProjectV2) and the editor part. Loot is written into no file; its
	// contributor still checks the entries and follows its working state.
	protected void FinishPlan(DZS_PlanBuild b)
	{
		DZS_Instance inst = b.m_Inst;
		DZS_SavePlan plan = b.m_Plan;
		bool noLoot = b.m_NoLoot;
		array<string> entries = b.m_Entries;
		string le, lh, ll;
		inst.GetField(DZS_PROJECT_ID, "lootEnabled", le);
		inst.GetField(DZS_PROJECT_ID, "lootHidden", lh);
		inst.GetField(DZS_PROJECT_ID, "lootLocked", ll);
		array<string> types = new array<string>();
		array<string> origs = new array<string>();
		DZS_SaveContributor contributor = DZS_SaveContributor.Cast(m_Kernel.GetService("save.loot"));
		if (noLoot)
		{
			contributor = null;
			le = "false";
		}
		if (contributor)
		{
			array<ref DZS_SaveFileOut> extra = new array<ref DZS_SaveFileOut>();
			contributor.Contribute(inst, m_Open.ReadFolder(), extra, plan.m_Issues, types, origs);
		}
		int pv;
		string pplugins = "[]";
		inst.GetCompanion(DZS_PROJECT_ID, DZS_LocalStore.GROUP_PLUGINS, pv, pplugins);
		pplugins = DZS_ProjectSections.Merge(inst, pplugins);
		array<string> none = new array<string>();
		string proj = DZS_ProjectFile.ProjectPart(entries, le == "true", lh == "true", ll == "true", "", none, none, true, pplugins);
		plan.m_ProjectPart = proj;
		plan.m_Blocks = DZS_ProjectV2.Build(m_Kernel, inst, m_Open.m_Name, m_Open.m_MapKey, noLoot);
		plan.m_Base = m_Open.m_Base;
		DZS_SaveFile pf = new DZS_SaveFile();
		pf.m_Rel = DZS_ProjectFile.NAME;
		pf.m_Chunks = DZS_ProjectFile.ComposeV2(plan.m_Blocks, m_Open.m_Name, m_Open.m_MapKey, plan.m_Base, proj, CameraView(), DZS_ProjectExtensions.Save());
		plan.m_Files.Insert(pf);
	}

	// The base chosen for the open project goes into its project file at once (a provider's project at its next save).
	void BaseChosen(string key)
	{
		if (!m_Open || m_Open.m_Base == key)
			return;
		m_Open.m_Base = key;
		if (ProviderOf(m_Open.m_Folder) || m_Open.m_Provider == "shared")
			return;
		string path = DZS_LocalStore.File(m_Open.m_Folder);
		array<string> cur = DZS_Files.Read(path);
		if (!cur || DZS_Files.Checksum(cur) != m_Open.Remembered(DZS_ProjectFile.NAME))
			return;
		array<string> next = DZS_ProjectFile.RewriteV2(cur, "", "", "", key);
		if (!next || !DZS_Files.Write(path, next))
			return;
		m_Open.Remember(DZS_ProjectFile.NAME, DZS_Files.Checksum(next));
		DZS_Log.Info("projects: base " + key + " written into the project file");
	}

	// ---- delete a group ----

	protected string m_DeleteGroup;

	string GroupsFixedReason()
	{
		if (!m_Open)
			return "";
		DZS_StoreProvider sp = ProviderOf(m_Open.m_Folder);
		string de, en;
		if (sp && sp.GroupsFixed(m_Open.m_Folder, de, en))
			return DZS_Texts.Pick(de, en);
		return "";
	}

	void AskDeleteGroup(string gid)
	{
		DZS_Instance inst = Shown();
		if (!inst || !inst.Has(gid) || inst.GetKind(gid) != DZS_KIND_GROUP || GroupsFixedReason() != "")
			return;
		m_DeleteGroup = gid;
		string name, file, unreadable;
		inst.GetField(gid, DZS_Obj.GROUP_NAME, name);
		inst.GetField(gid, DZS_Obj.GROUP_FILE, file);
		inst.GetField(gid, "unreadable", unreadable);
		int n = inst.GetChildren(gid).Count();
		string de, en;
		if (unreadable == "1")
		{
			de = "Die Gruppe wird aus dem Projekt entfernt. Die Datei ließ sich nicht lesen.";
			en = "The group is removed from the project. The file could not be read.";
		}
		else if (n == 0)
		{
			de = "Die leere Gruppe wird aus dem Projekt entfernt.";
			en = "The empty group is removed from the project.";
		}
		else
		{
			de = "Die Gruppe und ihre " + n + " Objekte werden aus dem Projekt entfernt.";
			en = "The group and its " + n + " objects are removed from the project.";
			if (n == 1)
			{
				de = "Die Gruppe und ihr 1 Objekt werden aus dem Projekt entfernt.";
				en = "The group and its 1 object are removed from the project.";
			}
		}
		de += " Rückgängig mit Strg+Z.";
		en += " Undo with Ctrl+Z.";
		string kick = DZS_Texts.Pick("Projekt", "Project");
		string title = DZS_Texts.Pick("Gruppe " + name + " löschen?", "Delete group " + name + "?");
		string body = DZS_Texts.Pick(de, en);
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(kick, title, body);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("delete", DZS_Texts.Pick("Gruppe löschen", "Delete group"), "danger");
		c.m_OnResult.Insert(OnDeleteGroupAnswer);
		c.Open();
		DZS_Log.Info("projects: delete group question " + name + " (" + n + " objects, file " + file + ")");
	}

	void OnDeleteGroupAnswer(DZS_Dialog d, string result)
	{
		DZS_Instance inst = Shown();
		string gid = m_DeleteGroup;
		m_DeleteGroup = "";
		DZS_Log.Info("projects: delete group answer " + result);
		if (result != "delete" || !inst || !inst.Has(gid))
			return;
		string name;
		inst.GetField(gid, DZS_Obj.GROUP_NAME, name);
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		b.Delete(gid);
		DZS_SubmitResult r = b.Commit("Gruppe " + name + " löschen", "Delete group " + name);
		DZS_Log.Info("projects: group " + name + " deleted, ok " + r.Ok());
	}

	void SaveAs(string name)
	{
		if (!m_Open)
			return;
		StartSave(false, "", name);
	}

	// Save As with a server as target: the plan goes to the provider that owns the new folder; on success the open
	// project moves there as after Save As, on failure nothing changes (the old project keeps its unsaved changes).
	bool SaveAsToProvider(string name, string folder)
	{
		DZS_Instance inst = Shown();
		DZS_StoreProvider sp = ProviderOf(folder);
		if (!inst || !m_Open || m_Busy || !sp)
			return false;
		DZS_SavePlan plan = BuildPlan(inst);
		plan.m_SaveAs = true;
		plan.m_ToProvider = true;
		plan.m_Name = name;
		for (int di = plan.m_Files.Count() - 1; di >= 0; di--)
		{
			if (plan.m_Files[di].m_Delete)
				plan.m_Files.Remove(di);
		}
		DZS_SaveFile pfile = plan.m_Files[plan.m_Files.Count() - 1];
		pfile.m_Chunks = DZS_ProjectFile.ComposeV2(plan.m_Blocks, name, m_Open.m_MapKey, plan.m_Base, plan.m_ProjectPart, CameraView(), DZS_ProjectExtensions.Save());
		DZS_Files.MakeDirs(DZS_LocalStore.Dir(folder));
		m_Busy = true;
		inst.SetSaving(true);
		m_ProvPlan = plan;
		m_ProvFolder = folder;
		DZS_Log.Info("save as: " + plan.m_Files.Count() + " file(s) to the new project " + name + " of provider " + sp.Id());
		sp.Save(folder, plan.m_Files);
		return true;
	}

	// ---- rename and delete (browser) ----

	bool RenameProject(string folder, string newName)
	{
		bool isOpen = m_Open && m_Open.m_Folder == folder;
		if (isOpen && (m_Busy || m_Plan || m_Building || m_Outside || m_Writing))
		{
			Message("projects.rename", "warning", "Erst nach dem Speichern umbenennen", "Rename after saving");
			return false;
		}
		if (!isOpen && FindBackup("local", folder) != "")
		{
			Message("projects.rename", "warning", "Für das Projekt gibt es eine Sicherung — erst öffnen", "The project has a backup — open it first", 5.0);
			return false;
		}
		string sum;
		string newId;
		if (!DZS_LocalStore.Rename(folder, newName, newId, sum))
		{
			Message("projects.rename", "danger", "Umbenennen fehlgeschlagen", "Renaming failed");
			return false;
		}
		if (newId != folder)
		{
			if (isOpen)
				DeleteBackup();
			Relocate(folder, newId);
			string work = DZS_WorkFolder.DirOf(folder);
			if (!isOpen && DZS_Files.IsFolder(work))
				DZS_Files.DeleteTree(work);
		}
		if (isOpen)
		{
			m_Open.m_Folder = newId;
			m_Open.m_Name = newName;
			m_Open.Remember(DZS_ProjectFile.NAME, sum);
			if (Shown() && Shown().IsUnsaved())
				WriteBackup();
		}
		DZS_Log.Info("renamed " + folder + " to '" + newName + "' (" + newId + DZS_LocalStore.EXT + ")");
		return true;
	}

	// The recently opened entry and the remembered base of a project follow it to its new id.
	protected void Relocate(string from, string to)
	{
		DZS_Json rec = m_Kernel.GetSettings().Get("projects.recent");
		if (rec && rec.IsArr())
		{
			DZS_Json next = rec.Clone();
			for (int i = 0; i < next.Count(); i++)
			{
				if (next.At(i).GetStr("loc") == from)
				{
					DZS_Json loc = DZS_Json.Str(to);
					next.At(i).Set("loc", loc);
				}
			}
			m_Kernel.GetSettings().Set("projects.recent", next);
		}
		DZS_Json bases = m_Kernel.GetSettings().Get(DZS_PatchApply.SETTING);
		if (bases && bases.IsObj() && bases.Has(from))
		{
			DZS_Json moved = bases.Clone();
			DZS_Json key = bases.Get(from).Clone();
			moved.Set(to, key);
			moved.Remove(from);
			m_Kernel.GetSettings().Set(DZS_PatchApply.SETTING, moved);
		}
	}

	bool DeleteProject(string folder)
	{
		if (m_Open && m_Open.m_Folder == folder)
			return false;
		bool ok = DZS_LocalStore.Delete(folder);
		string bdir = BackupDir("local", folder);
		if (DZS_Files.Exists(bdir))
			DZS_Files.DeleteTree(bdir);
		DZS_Files.Delete(BackupFile("local", folder));
		DZS_Files.Delete(BackupFile("local", folder) + ".state");
		DZS_Files.Delete(BackupFile("local", folder) + ".state2");
		Forget(folder);
		DZS_Log.Info("deleted project " + folder + DZS_LocalStore.EXT + ": " + ok);
		return ok;
	}

	// ---- autosave and backup ----

	int m_StateVer;

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		m_StateVer++;
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		m_StateVer++;
	}

	override void OnHistoryChanged(DZS_Instance inst)
	{
		if (inst != Shown() || !m_Open)
			return;
		if (inst.IsSaving())
			return;
		m_LastStepAt = m_Now;
		if (!m_Pending && inst.IsUnsaved())
			m_FirstUnsavedAt = m_Now;
		m_Pending = inst.IsUnsaved();
		if (!inst.IsUnsaved())
			DeleteBackup();
	}

	protected void UpdateQueuedSave()
	{
		if (!m_SaveQueued)
			return;
		DZS_Instance inst = Shown();
		if (!inst || !m_Open)
		{
			m_SaveQueued = false;
			return;
		}
		DZS_StoreProvider sp = ProviderOf(m_Open.m_Folder);
		if (m_Busy || (sp && sp.SaveRunning(m_Open.m_Folder)))
			return;
		m_SaveQueued = false;
		inst.SetSaving(false);
		DZS_Log.Info("save: the running one is decided, starting the waiting save");
		StartSave(m_QueuedAuto, m_QueuedThen);
	}

	override void OnUpdate(float dt)
	{
		if (m_StartPending)
			AfterStart();
		m_Now += dt;
		UpdateQueuedSave();
		if (m_Home && IsHome())
			m_Home.Update(dt);
		DZS_Instance inst = Shown();
		StepOpening();
		StepBuilding();
		StepOutside();
		StepWriting();
		StepSlices();
		StepBackupFiles();
		if (inst && m_Open && !m_Slice && InSession(inst) && inst.IsUnsaved() && m_Now - m_SliceStart >= SLICE_GAP && !m_Kernel.IsGestureRunning() && !m_Kernel.GetScreen().HasDialog())
		{
			if (m_SliceStart > -1000 || m_Now - m_LastStepAt >= QUIET)
				BeginSlices(inst);
		}
		if (!inst || !m_Open || !m_Pending || m_Busy || m_Plan || m_Building || m_Outside)
			return;
		DZS_SaveRules autoRules = DZS_SaveRules.Get();
		float latest = 0;
		if (autoRules)
			latest = autoRules.AutoSaveLatest(inst);
		bool overdue = latest > 0 && m_Now - m_FirstUnsavedAt >= latest;
		if ((m_Now - m_LastStepAt < QUIET && !overdue) || m_Kernel.IsGestureRunning() || m_Kernel.GetScreen().HasDialog())
			return;
		if (!inst.IsUnsaved())
		{
			m_Pending = false;
			return;
		}
		bool autoOn = m_Kernel.GetSettings().GetBool("projects.autosave", true) && !m_AutoPaused;
		if (autoRules && autoRules.NoAutoSave(inst))
			autoOn = false;
		DZS_StoreProvider ap = ProviderOf(m_Open.m_Folder);
		string offDe, offEn;
		if (autoOn && ap && ap.AutoSaveOff(m_Open.m_Folder, offDe, offEn))
		{
			autoOn = false;
			if (!m_AutoOffShown)
			{
				m_AutoOffShown = true;
				m_Kernel.GetScreen().SetStatusIcon("projects.autoOff", true, "circle-pause", DZS_Texts.Pick(offDe, offEn), "");
				DZS_Log.Info("autosave off: " + offDe);
			}
		}
		if (autoOn)
		{
			if (m_Now - m_LastAutoStart < GAP)
				return;
			m_LastAutoStart = m_Now;
			m_Pending = false;
			DZS_Log.Info("autosave starts");
			StartSave(true, "");
			if (inst.IsUnsaved() && !m_AutoPaused)
				WriteBackup();
			return;
		}
		if (m_Now - m_LastBackupAt < GAP)
			return;
		m_Pending = false;
		WriteBackup();
	}

	protected void PauseAuto(string reason)
	{
		m_AutoPaused = true;
		m_AutoPauseReason = reason;
		DZS_Log.Info("autosave paused: " + reason);
		m_Kernel.GetScreen().SetStatusIcon("projects.autoPaused", true, "circle-pause", DZS_Texts.Pick("Automatisch speichern angehalten: ", "Autosave paused: ") + reason, "warning");
		string tx1 = DZS_Texts.Pick("Automatisch speichern angehalten: ", "Autosave paused: ") + reason;
		string tx2 = DZS_Texts.Pick("Speichern …", "Save …");
		m_Kernel.GetScreen().ShowMessageWithButton("projects.autoPaused", "warning", tx1, 10.0, tx2, m_Msg);
		WriteBackup();
	}

	protected string m_SpawnerEntry;

	void OnMessageButton(string kind)
	{
		DZS_Log.Info("message button " + kind);
		if (kind == "projects.spawnerEntry")
		{
			GetGame().CopyToClipboard(m_SpawnerEntry);
			m_Kernel.GetScreen().HideMessage(kind);
			Message("projects.spawnerCopied", "success", "Eintrag kopiert", "Entry copied", 2.0);
			DZS_Log.Info("spawner entry copied: " + m_SpawnerEntry);
		}
		else if (kind == "projects.autoPaused")
		{
			m_Kernel.GetScreen().HideMessage(kind);
			StartSave(false, "");
		}
		else if (kind == "projects.opened" || kind == "projects.saved" || kind == "checklist")
		{
			m_Kernel.GetScreen().HideMessage(kind);
			if (m_LastIssues)
			{
				string mode = "read";
				if (kind == "checklist" && m_LastIssuesKind == "save")
					mode = "read";
				string kde = "Speichern";
				string ken = "Save";
				if (m_LastIssuesKind == "open")
				{
					kde = "Öffnen";
					ken = "Open";
				}
				DZS_CheckListDialog d = new DZS_CheckListDialog(m_LastIssues, mode, kde, ken);
				d.m_OnResult.Insert(OnReadList);
				d.Open();
			}
		}
	}

	void OnReadList(DZS_Dialog d, string result)
	{
		if (result == "show")
			ShowObject(DZS_CheckListDialog.Cast(d).m_ShowId);
	}

	static string BackupFile(string provider, string loc)
	{
		string safe = "";
		string bad = "\\/:*?\"<>|. ";
		for (int i = 0; i < loc.Length(); i++)
		{
			string c = loc.Get(i);
			if (bad.Contains(c))
				safe += "_";
			else
				safe += c;
		}
		return BACKUP_DIR + "\\" + provider + "-" + safe + ".json";
	}

	// The whole project state in its text form plus provider, place, name, map, camera and time, in slices.
	void WriteBackup()
	{
		DZS_Instance inst = Shown();
		if (!inst || !m_Open || !inst.IsUnsaved())
			return;
		DZS_SaveRules rules = DZS_SaveRules.Get();
		if (rules && rules.NoLocalWork(inst))
			return;
		if (InSession(inst))
			return;
		m_LastBackupAt = m_Now;
		if (!m_BJob && !m_BWrite)
			BeginBackupFiles(inst);
	}

	// ---- backup as a folder: the files Save As would write, plus the head ----

	static const string BACKUP_HEAD = "backup.dzstudio";
	protected ref DZS_PlanBuild m_BJob;
	protected ref DZS_LocalWrite m_BWrite;
	protected ref array<ref DZS_SaveFile> m_BFiles;
	protected ref array<string> m_BKeep = new array<string>();
	protected DZS_Instance m_BInst;
	protected string m_BDir;
	protected float m_BStart;
	protected int m_BFrames;
	protected float m_BSlowest;

	static string BackupDir(string provider, string loc)
	{
		string flat = BackupFile(provider, loc);
		return flat.Substring(0, flat.Length() - 5);
	}

	// The head of the backup of a project: in its folder, else the older single file ("" = none).
	static string FindBackup(string provider, string loc)
	{
		string head = BackupDir(provider, loc) + "\\" + BACKUP_HEAD;
		if (DZS_Files.Exists(head))
			return head;
		string flat = BackupFile(provider, loc);
		if (DZS_Files.Exists(flat))
			return flat;
		return "";
	}

	protected void BeginBackupFiles(DZS_Instance inst)
	{
		if (m_Busy || m_Plan || m_Building || m_Outside || m_Writing)
			return;
		m_BInst = inst;
		m_BDir = BackupDir(m_Open.m_Provider, m_Open.m_Folder);
		m_BStart = m_Now;
		m_BFrames = 0;
		m_BSlowest = 0;
		m_BJob = BeginPlan(inst, false);
		DZS_Log.Info("backup files start: " + m_BDir);
	}

	protected void AbortBackupFiles(string why)
	{
		if (!m_BJob && !m_BWrite)
			return;
		if (m_BJob)
			EndPlanFreeze(m_BJob);
		m_BJob = null;
		m_BWrite = null;
		m_BFiles = null;
		DZS_Log.Info("backup files stopped: " + why);
	}

	protected void StepBackupFiles()
	{
		if (!m_BJob && !m_BWrite)
			return;
		if (m_BInst != Shown() || !m_Open)
		{
			AbortBackupFiles("project changed");
			return;
		}
		int t0 = TickCount(0);
		m_BFrames++;
		if (m_BJob)
		{
			if (StepPlan(m_BJob, SLICE_MS))
			{
				DZS_SavePlan plan = m_BJob.m_Plan;
				m_BJob = null;
				m_BFiles = new array<ref DZS_SaveFile>();
				foreach (DZS_SaveFile f : plan.m_Files)
				{
					if (!f.m_Delete)
						m_BFiles.Insert(f);
				}
				m_BKeep.Clear();
				DZS_Json head = BackupHead(m_BInst);
				head.Set("files", DZS_Json.Boolean(true));
				DZS_SaveFile hf = new DZS_SaveFile();
				hf.m_Rel = BACKUP_HEAD;
				hf.m_Chunks = head.ToChunks(true);
				m_BFiles.Insert(hf);
				DZS_Files.MakeDirs(m_BDir);
				m_BWrite = new DZS_LocalWrite(m_BDir, m_BFiles);
			}
		}
		else if (m_BWrite.Step(SLICE_MS))
		{
			string fail = m_BWrite.m_Fail;
			m_BWrite = null;
			if (fail == "")
				DropOtherBackupFiles();
			m_LastBackupAt = m_Now;
			DZS_Log.Info("backup files written: " + m_BDir + " ok " + (fail == "") + ", " + m_BFiles.Count() + " file(s) in " + m_BFrames + " frames over " + DZS_Num.Fixed(m_Now - m_BStart, 2) + " s, slowest frame " + DZS_Num.Fixed(m_BSlowest, 1) + " ms " + fail);
			m_BFiles = null;
			return;
		}
		float ms = (TickCount(0) - t0) / 10000.0;
		if (ms > m_BSlowest)
			m_BSlowest = ms;
	}

	// The folder holds exactly the files just written; an older single-file backup of the project goes.
	protected void DropOtherBackupFiles()
	{
		array<string> keep = new array<string>();
		foreach (string kr : m_BKeep)
		{
			string kk = kr;
			kk.Replace("/", "\\");
			kk.ToLower();
			keep.Insert(kk);
		}
		foreach (DZS_SaveFile f : m_BFiles)
		{
			string k = f.m_Rel;
			k.Replace("/", "\\");
			k.ToLower();
			keep.Insert(k);
		}
		array<string> all = DZS_Files.ListTree(m_BDir);
		foreach (string rel : all)
		{
			string low = rel;
			low.Replace("/", "\\");
			low.ToLower();
			if (keep.Find(low) < 0)
			{
				DZS_Files.Delete(DZS_LocalStore.Path(m_BDir, rel));
				DZS_Log.Info("backup files: dropped " + rel);
			}
		}
		string flat = BackupFile(m_Open.m_Provider, m_Open.m_Folder);
		DZS_Files.Delete(flat + ".state");
		DZS_Files.Delete(flat + ".state2");
		DZS_Files.Delete(flat);
	}

	// After a crash while a backup folder was replaced: a complete copy of the head means all copies are complete
	// and count before their files; otherwise the files stay (the backup before) and the copies go.
	static void RecoverBackupDir(string dir)
	{
		string head = dir + "\\" + BACKUP_HEAD;
		bool useCopies = DZS_Files.Exists(head + ".new") && ReadBackup(head + ".new") != null;
		array<string> all = DZS_Files.ListTree(dir);
		foreach (string rel : all)
		{
			int n = rel.Length();
			if (n > 4 && rel.Substring(n - 4, 4) == ".old")
			{
				DZS_Files.Delete(dir + "\\" + rel);
				continue;
			}
			if (n <= 4 || rel.Substring(n - 4, 4) != ".new" || rel == BACKUP_HEAD + ".new")
				continue;
			string copy = dir + "\\" + rel;
			if (useCopies)
				DZS_Files.Copy(copy, dir + "\\" + rel.Substring(0, n - 4));
			DZS_Files.Delete(copy);
		}
		if (DZS_Files.Exists(head + ".new"))
		{
			if (useCopies)
				DZS_Files.Copy(head + ".new", head);
			DZS_Files.Delete(head + ".new");
			DZS_Log.Info("backup " + dir + " recovered, copies used " + useCopies);
		}
	}

	protected DZS_Json BackupHead(DZS_Instance inst)
	{
		DZS_Json root = DZS_Json.Obj();
		root.Set("format", DZS_Json.Str("dzstudio-backup"));
		root.Set("version", DZS_Json.Int(1));
		root.Set("provider", DZS_Json.Str(m_Open.m_Provider));
		root.Set("location", DZS_Json.Str(m_Open.m_Folder));
		root.Set("name", DZS_Json.Str(m_Open.m_Name));
		root.Set("map", DZS_Json.Str(m_Open.m_MapKey));
		root.Set("time", DZS_Json.Str(Stamp()));
		root.Set("unsaved", DZS_Json.Int(inst.GetUnsavedCount()));
		root.Set("mode", DZS_Json.Str(inst.GetSaveMode()));
		DZS_Json sums = DZS_Json.Obj();
		for (int i = 0; i < m_Open.m_Sums.Count(); i++)
			sums.Set(m_Open.m_Sums.GetKey(i), DZS_Json.Str(m_Open.m_Sums.GetElement(i)));
		root.Set("sums", sums);
		DZS_StoreProvider sp = ProviderOf(m_Open.m_Folder);
		if (sp)
		{
			DZS_Json pst = sp.BackupState(m_Open.m_Folder);
			if (pst)
				root.Set("providerState", pst);
		}
		DZS_CameraPlugin cam = Cam();
		if (cam && cam.IsStarted())
		{
			vector p = cam.GetPosition();
			DZS_Json c = DZS_Json.Obj();
			DZS_Json cp = DZS_Json.Arr();
			cp.Add(DZS_Json.Num(DZS_F32.ToText(p[0])));
			cp.Add(DZS_Json.Num(DZS_F32.ToText(p[1])));
			cp.Add(DZS_Json.Num(DZS_F32.ToText(p[2])));
			c.Set("pos", cp);
			c.Set("heading", DZS_Json.Num(DZS_F32.ToText(cam.GetYaw())));
			c.Set("pitch", DZS_Json.Num(DZS_F32.ToText(cam.GetPitch())));
			root.Set("camera", c);
		}
		return root;
	}

	void DeleteBackup()
	{
		if (!m_Open)
			return;
		AbortSlices("deleted");
		AbortBackupFiles("deleted");
		string dir = BackupDir(m_Open.m_Provider, m_Open.m_Folder);
		if (DZS_Files.Exists(dir))
		{
			DZS_Files.DeleteTree(dir);
			DZS_Log.Info("backup deleted: " + dir);
		}
		string file = BackupFile(m_Open.m_Provider, m_Open.m_Folder);
		DZS_Files.Delete(file + ".state");
		DZS_Files.Delete(file + ".state2");
		if (DZS_Files.Exists(file))
		{
			DZS_Files.Delete(file);
			DZS_Log.Info("backup deleted: " + file);
		}
	}

	// ---- backup in a session: at most every 2 minutes, built in slices of about 4 ms ----

	static const float SLICE_GAP = 120;
	static const float SLICE_MS = 4;
	static const float OPEN_SLICE_MS = 20;
	protected DZS_StateSlicer m_Slice;
	protected DZS_Instance m_SliceInst;
	protected ref DZS_Json m_SliceHead;
	protected string m_SliceFile;
	protected FileHandle m_SliceFh;
	protected int m_SliceChunk;
	protected int m_SliceFrames;
	protected float m_SliceMaxMs;
	protected float m_SliceStart = -1000;
	protected string m_SliceState;

	protected bool InSession(DZS_Instance inst)
	{
		DZS_SaveRules rules = DZS_SaveRules.Get();
		return rules && rules.AutoSaveLatest(inst) > 0 && !rules.NoLocalWork(inst);
	}

	protected void BeginSlices(DZS_Instance inst)
	{
		m_SliceStart = m_Now;
		m_SliceInst = inst;
		m_SliceFrames = 0;
		m_SliceMaxMs = 0;
		m_SliceChunk = -1;
		m_SliceFile = BackupFile(m_Open.m_Provider, m_Open.m_Folder);
		int seq = -1;
		DZS_SaveRules rules = DZS_SaveRules.Get();
		if (rules)
			seq = rules.StateSeq(inst);
		m_SliceHead = BackupHead(inst);
		m_SliceHead.Set("seq", DZS_Json.Int(seq));
		m_SliceState = "state";
		if (DZS_Files.Exists(m_SliceFile + ".state"))
			m_SliceState = "state2";
		m_SliceHead.Set("stateFile", DZS_Json.Str(m_SliceState));
		int t0 = TickCount(0);
		m_Slice = inst.BeginSlices();
		DZS_Json kindList = DZS_Json.Arr();
		foreach (string kn : m_Slice.m_Kinds)
			kindList.Add(DZS_Json.Str(kn));
		m_SliceHead.Set("kinds", kindList);
		float ms = (TickCount(0) - t0) / 10000.0;
		DZS_Log.Info("backup slices start at seq " + seq + ", " + m_Slice.Count() + " elements, order in " + ms + " ms");
	}

	protected void AbortSlices(string why)
	{
		if (!m_Slice)
			return;
		if (m_SliceInst)
			m_SliceInst.EndSlices(m_Slice);
		if (m_SliceFh)
			CloseFile(m_SliceFh);
		m_SliceFh = null;
		m_Slice = null;
		m_SliceInst = null;
		DZS_Log.Info("backup slices stopped: " + why);
	}

	protected void StepSlices()
	{
		if (!m_Slice)
			return;
		if (m_SliceInst != Shown() || !m_Open)
		{
			AbortSlices("project changed");
			return;
		}
		int t0 = TickCount(0);
		m_SliceFrames++;
		if (!m_Slice.IsDone())
			m_Slice.Step(SLICE_MS);
		else if (m_SliceChunk < 0)
		{
			DZS_Files.MakeDirs(BACKUP_DIR);
			m_SliceFh = OpenFile(m_SliceFile + "." + m_SliceState, FileMode.WRITE);
			m_SliceChunk = 0;
			if (!m_SliceFh)
			{
				AbortSlices("cannot write " + m_SliceFile + "." + m_SliceState);
				return;
			}
		}
		else if (m_SliceChunk < m_Slice.m_Chunks.Count())
		{
			while (m_SliceChunk < m_Slice.m_Chunks.Count())
			{
				FPrint(m_SliceFh, m_Slice.m_Chunks[m_SliceChunk]);
				m_SliceChunk++;
				if ((m_SliceChunk & 31) == 0 && (TickCount(0) - t0) / 10000.0 >= SLICE_MS)
					break;
			}
		}
		else
		{
			CloseFile(m_SliceFh);
			m_SliceFh = null;
			int bytes = DZS_Files.Length(m_Slice.m_Chunks);
			array<string> head = m_SliceHead.ToChunks(true);
			bool ok = DZS_Files.Write(m_SliceFile + ".new", head);
			if (ok)
			{
				ok = DZS_Files.Copy(m_SliceFile + ".new", m_SliceFile);
				DZS_Files.Delete(m_SliceFile + ".new");
			}
			if (ok)
			{
				string other = ".state2";
				if (m_SliceState == "state2")
					other = ".state";
				DZS_Files.Delete(m_SliceFile + other);
			}
			m_SliceInst.EndSlices(m_Slice);
			m_LastBackupAt = m_Now;
			DZS_Log.Info("backup slices written: " + m_SliceFile + " ok " + ok + ", " + bytes + " bytes in " + m_SliceFrames + " frames over " + (m_Now - m_SliceStart) + " s, slowest frame " + m_SliceMaxMs + " ms");
			m_Slice = null;
			m_SliceInst = null;
			return;
		}
		float ms = (TickCount(0) - t0) / 10000.0;
		if (ms > m_SliceMaxMs)
			m_SliceMaxMs = ms;
	}

	static DZS_Json ReadBackup(string file)
	{
		array<string> chunks = DZS_Files.Read(file);
		if (!chunks)
			return null;
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc.m_Root || !doc.m_Root.IsObj())
			return null;
		DZS_Json j = doc.m_Root.ToJson();
		if (j.GetStr("format") != "dzstudio-backup")
			return null;
		return j;
	}

	// All backups, newest first; a readable copy beside a broken backup wins (and the copy goes).
	array<string> Backups()
	{
		array<string> r = new array<string>();
		array<string> names;
		array<bool> folders;
		DZS_Files.List(BACKUP_DIR, names, folders);
		foreach (string n : names)
		{
			if (n.Length() > 9 && n.Substring(n.Length() - 9, 9) == ".json.new")
			{
				string main = BACKUP_DIR + "\\" + n.Substring(0, n.Length() - 4);
				string copy = BACKUP_DIR + "\\" + n;
				if (ReadBackup(copy) && !ReadBackup(main))
					DZS_Files.Copy(copy, main);
				DZS_Files.Delete(copy);
			}
		}
		DZS_Files.List(BACKUP_DIR, names, folders);
		array<string> times = new array<string>();
		for (int mi = 0; mi < names.Count(); mi++)
		{
			string m = names[mi];
			string headFile = "";
			if (folders[mi])
			{
				RecoverBackupDir(BACKUP_DIR + "\\" + m);
				headFile = BACKUP_DIR + "\\" + m + "\\" + BACKUP_HEAD;
			}
			else if (m.Length() >= 5 && m.Substring(m.Length() - 5, 5) == ".json")
			{
				if (DZS_Files.Exists(BACKUP_DIR + "\\" + m.Substring(0, m.Length() - 5) + "\\" + BACKUP_HEAD))
					continue;
				headFile = BACKUP_DIR + "\\" + m;
			}
			if (headFile == "")
				continue;
			DZS_Json b = ReadBackup(headFile);
			if (!b)
				continue;
			string t = b.GetStr("time");
			int at = 0;
			while (at < times.Count() && times[at] > t)
				at++;
			times.InsertAt(t, at);
			r.InsertAt(headFile, at);
		}
		return r;
	}

	protected void OfferBackups()
	{
		array<string> list = Backups();
		if (list.Count() == 0)
			return;
		DZS_RestoreDialog d = new DZS_RestoreDialog(this, list);
		d.Open();
	}

	void RequestRestore(string file)
	{
		Leave("restore", file, "");
	}

	// Why a backup cannot be opened: its project file comes from a newer editor or
	// the format plugin of one of its groups is missing; "" = it opens. Only backups as a folder carry their files.
	static string BackupBlocked(string head, DZS_Json b, bool german)
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!b.GetBool("files"))
		{
			DZS_Json kinds = b.Get("kinds");
			if (!k || !kinds || !kinds.IsArr())
				return "";
			for (int ki = 0; ki < kinds.Count(); ki++)
			{
				string kind = kinds.At(ki).m_Str;
				if (!k.GetTypes().KnowsKind(kind))
				{
					string plugin = DZS_Str.PluginOfType(kind);
					if (german)
						return "Plugin " + plugin + " fehlt (" + kind + ")";
					return "Plugin " + plugin + " is missing (" + kind + ")";
				}
			}
			return "";
		}
		int hl = BACKUP_HEAD.Length();
		string dir = head.Substring(0, head.Length() - hl - 1);
		DZS_PFData pf = DZS_ProjectFile.Read(DZS_Files.Read(dir + "\\" + DZS_ProjectFile.NAME));
		if (pf.m_Status == "newer")
		{
			if (german)
				return "Projektdatei aus einer neueren Version des Editors";
			return "Project file from a newer version of the editor";
		}
		if (pf.m_V2)
			return "";
		foreach (DZS_PFGroup g : pf.m_Groups)
		{
			if (k && !k.FormatForFile(g.m_File))
			{
				if (german)
					return "Format-Plugin für " + g.m_File + " fehlt";
				return "Format plugin for " + g.m_File + " is missing";
			}
		}
		return "";
	}

	// The map of a backup; a head without one takes the map of the project file in its folder ("" = unknown).
	static string BackupMap(DZS_Json b)
	{
		string mapKey = b.GetStr("map");
		if (mapKey != "")
			return mapKey;
		string folder = b.GetStr("location");
		if (folder == "")
			return "";
		DZS_PFData pf = DZS_ProjectFile.Read(DZS_Files.Read(DZS_LocalStore.Path(folder, DZS_ProjectFile.NAME)));
		if (pf.m_Status == "ok")
			return pf.m_MapKey;
		return "";
	}

	protected void GoRestore(string file)
	{
		DZS_Json b = ReadBackup(file);
		if (!b)
			return;
		string mapKey = BackupMap(b);
		if (mapKey == "")
		{
			Message("projects.open", "danger", "Karte der Sicherung unbekannt", "Map of the backup unknown");
			return;
		}
		string blockedDe = BackupBlocked(file, b, true);
		if (blockedDe != "")
		{
			Message("projects.open", "danger", "Sicherung lässt sich nicht öffnen: " + blockedDe, "Backup cannot be opened: " + BackupBlocked(file, b, false), 5.0);
			return;
		}
		if (!DZS_Maps.Available(mapKey))
		{
			string mapName = DZS_Maps.Display(mapKey);
			Message("projects.open", "danger", mapName + " ist nicht verfügbar", mapName + " is not available");
			return;
		}
		if (DZS_Maps.WorldOf(mapKey) != m_World)
		{
			DZS_Handover.Set("restore", b.GetStr("location"), mapKey, file);
			SwitchWorld(mapKey);
			return;
		}
		CloseProject(true);
		RestoreBackup(file);
	}

	// Opens the project from the backup: same ids, "Restored — not saved". In slices like opening.
	protected void RestoreBackup(string file)
	{
		DZS_Json b = ReadBackup(file);
		if (!b)
			return;
		if (b.GetBool("files"))
		{
			int hl = BACKUP_HEAD.Length();
			LoadProject(file.Substring(0, file.Length() - hl - 1), true, b);
			if (m_Opening)
				m_Opening.m_Progress.SetVerb(DZS_Texts.Pick("Stellt wieder her", "Restoring"));
			return;
		}
		ShowHome(false);
		array<string> chunks = new array<string>();
		DZS_Json st = b.Get("state");
		if (st)
		{
			for (int i = 0; i < st.Count(); i++)
				chunks.Insert(st.At(i).m_Str);
		}
		else
		{
			string stateFile = b.GetStr("stateFile");
			if (stateFile != "state2")
				stateFile = "state";
			array<string> sliced = DZS_Files.Read(file + "." + stateFile);
			if (sliced)
				chunks = sliced;
		}
		DZS_OpenProject op = new DZS_OpenProject();
		op.m_Folder = b.GetStr("location");
		op.m_Name = b.GetStr("name");
		op.m_MapKey = BackupMap(b);
		DZS_Json sums = b.Get("sums");
		if (sums && sums.IsObj())
		{
			for (int k = 0; k < sums.m_Keys.Count(); k++)
			{
				string rel = sums.m_Keys[k];
				string sum = sums.GetStr(rel);
				op.m_Sums.Set(rel, sum);
			}
		}
		DZS_Log.Info("restore: " + op.m_Sums.Count() + " file checksum(s) from the backup");
		DZS_OpenJob j = new DZS_OpenJob();
		j.m_T0 = TickCount(0);
		j.m_Started = m_Now;
		j.m_Restore = true;
		j.m_Backup = b;
		j.m_File = file;
		j.m_Op = op;
		j.m_Folder = op.m_Folder;
		j.m_Inst = m_Kernel.NewLoadInstance();
		j.m_Parse = new DZS_SrcJsonSteps(chunks, 3);
		j.m_Built = new DZS_Operation("core:load", 1);
		m_Opening = j;
		j.m_Progress = new DZS_ProgressDialog(DZS_Texts.Pick("Sicherung", "Backup"), op.m_Name, DZS_Texts.Pick("Stellt wieder her", "Restoring"));
		j.m_Progress.Open();
	}

	// The text form of the backup read and turned into the changes of the load; false with an error.
	protected bool StepRestoreBuild(DZS_OpenJob j, float ms, out string error)
	{
		int t0 = TickCount(0);
		float limit = ms * 10000;
		if (!j.m_Doc)
		{
			if (!j.m_Parse.Step(ms))
			{
				j.m_Progress.SetShare(0.6 * j.m_Parse.Progress());
				return true;
			}
			j.m_Doc = j.m_Parse.Result();
			j.m_TParse = TickCount(0) - j.m_T0;
			DZS_SrcNode root = j.m_Doc.m_Root;
			if (!root)
			{
				error = j.m_Doc.m_Error;
				return false;
			}
			if (!root.IsObj() || !root.Get("textform") || !root.Get("state"))
			{
				error = "not a project state";
				return false;
			}
			int tf = root.Get("textform").m_Str.ToInt();
			if (tf > DZS_TEXTFORM_VERSION)
			{
				error = "text form version " + tf + " is newer than " + DZS_TEXTFORM_VERSION;
				return false;
			}
			DZS_SrcNode st = root.Get("state");
			DZS_SrcNode elems = null;
			if (st.IsObj())
				elems = st.Get("elements");
			if (!elems || !elems.IsArr())
			{
				error = "state without elements";
				return false;
			}
			j.m_Elems = elems;
			DZS_SrcNode kinds = st.Get("kinds");
			if (kinds && kinds.IsObj())
			{
				for (int i = 0; i < kinds.m_Keys.Count(); i++)
				{
					string kn = kinds.m_Keys[i];
					if (!j.m_Inst.KnowsKind(kn))
						j.m_Missing += " " + DZS_Str.PluginOfType(kn) + " (" + kn + " v" + kinds.m_Items[i].m_Str + ")";
				}
			}
		}
		while (j.m_Ei < j.m_Elems.Count())
		{
			DZS_Json ej = j.m_Elems.m_Items[j.m_Ei].ToJson();
			DZS_ElementData e = DZS_ElementData.FromJson(ej, error);
			if (!e)
				return false;
			if (!j.m_Inst.KnowsKind(e.m_Kind) && !j.m_Missing.Contains(e.m_Kind))
				j.m_Missing += " " + DZS_Str.PluginOfType(e.m_Kind) + " (" + e.m_Kind + ")";
			DZS_Change c = DZS_Change.Create(e);
			j.m_Built.AddChange(c);
			j.m_Ei++;
			if ((j.m_Ei & 15) == 0 && TickCount(0) - t0 >= limit)
				break;
		}
		j.m_Progress.SetShare(0.6 + 0.3 * j.m_Ei / Math.Max(1, j.m_Elems.Count()));
		if (j.m_Ei < j.m_Elems.Count())
			return true;
		if (j.m_Missing != "")
		{
			error = "fehlende Plugins:" + j.m_Missing;
			return false;
		}
		j.m_Phase = DZS_OpenJob.LOAD;
		return true;
	}

	// A restored project of a storage provider stays one: plate, polling and saving go through the provider.
	protected void RestoredToProvider(DZS_OpenProject op, DZS_Json head)
	{
		DZS_StoreProvider rp = ProviderOf(op.m_Folder);
		if (!rp)
			return;
		op.m_Provider = rp.Id();
		rp.Restored(op.m_Folder, head.Get("providerState"));
		rp.Opened(op.m_Folder);
		DZS_Log.Info("restore: project of provider " + rp.Id() + " at " + op.m_Folder);
	}

	protected void RestoreFailed(DZS_OpenJob j, string error)
	{
		m_Opening = null;
		j.m_Progress.Close();
		m_Open = null;
		Message("projects.open", "danger", "Sicherung lässt sich nicht öffnen: " + error, "Backup cannot be opened: " + error, 5.0);
		ShowHome(true);
	}

	protected void FinishRestore(DZS_OpenJob j)
	{
		DZS_Instance inst = j.m_Inst;
		DZS_OpenProject op = j.m_Op;
		DZS_Json b = j.m_Backup;
		m_Open = op;
		m_Kernel.AddLoaded(inst);
		m_Kernel.ShowInstance(inst);
		inst.SetSaveMode("restored");
		DZS_Json c = b.Get("camera");
		DZS_CameraPlugin cam = Cam();
		if (c && cam)
		{
			DZS_Json p = c.Get("pos");
			DZS_Json p0 = p.At(0);
			DZS_Json p1 = p.At(1);
			DZS_Json p2 = p.At(2);
			DZS_Json ch = c.Get("heading");
			DZS_Json cp = c.Get("pitch");
			float fx = p0.m_Str.ToFloat();
			float fy = p1.m_Str.ToFloat();
			float fz = p2.m_Str.ToFloat();
			float fh = ch.m_Str.ToFloat();
			float fp = cp.m_Str.ToFloat();
			vector cpos = Vector(fx, fy, fz);
			cam.SetPose(cpos, fh, fp, false);
		}
		m_Pending = false;
		RestoredToProvider(op, b);
		Touch(op.m_Folder, "opened");
		DZS_Log.Info("restore timing: parse " + DZS_Num.Fixed(j.m_TParse / 10000.0, 0) + " ms of wall time, total " + DZS_Num.Fixed((TickCount(0) - j.m_T0) / 10000.0, 0) + " ms");
		DZS_Log.Info("restored '" + op.m_Name + "' from " + j.m_File + ", " + inst.CountElements() + " element(s)");
	}

	bool BackupFilesChanged(DZS_Json b)
	{
		DZS_Json sums = b.Get("sums");
		if (!sums || !sums.IsObj())
			return false;
		string folder = b.GetStr("location");
		for (int k = 0; k < sums.m_Keys.Count(); k++)
		{
			string rel = sums.m_Keys[k];
			array<string> cur = DZS_Files.Read(DZS_LocalStore.Path(folder, rel));
			string now = "";
			if (cur)
				now = DZS_Files.Checksum(cur);
			if (now != sums.GetStr(rel))
				return true;
		}
		return false;
	}

	void DiscardBackup(string file)
	{
		int hl = BACKUP_HEAD.Length();
		if (file.Length() > hl && file.Substring(file.Length() - hl, hl) == BACKUP_HEAD)
		{
			DZS_Files.DeleteTree(file.Substring(0, file.Length() - hl - 1));
			DZS_Log.Info("backup discarded: " + file);
			return;
		}
		DZS_Files.Delete(file);
		DZS_Files.Delete(file + ".state");
		DZS_Files.Delete(file + ".state2");
		DZS_Log.Info("backup discarded: " + file);
	}
}
