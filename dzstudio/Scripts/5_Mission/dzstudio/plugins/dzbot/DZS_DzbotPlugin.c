// The dzbot plugin: sign-in by device flow, mods and placements as projects (storage provider), polling for
// new revisions, and dzbot as image source of the catalog. The editor kernel knows nothing of dzbot.
class DZS_DzbotLink : DZS_DzbotListener
{
	DZS_DzbotPlugin m_P;

	override void OnDzbot(DZS_DzbotCall call, DZS_DzbotReply reply)
	{
		m_P.OnReply(call, reply);
	}
}

class DZS_DzbotActions : DZS_ActionHandler
{
	DZS_DzbotPlugin m_P;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (actionId == "dzbot.connect")
		{
			reasonDe = "Schon mit dzbot verbunden";
			reasonEn = "Already connected with dzbot";
			return !m_P.IsConnected();
		}
		if (actionId == "dzbot.disconnect")
		{
			reasonDe = "Nicht mit dzbot verbunden";
			reasonEn = "Not connected with dzbot";
			return m_P.IsConnected();
		}
		if (actionId == "dzbot.rules")
		{
			reasonDe = "Nur in einem dzbot-Projekt-Mod";
			reasonEn = "Only in a dzbot project mod";
			return m_P.OpenProjectMod() != null && m_P.Kernel().GetShown() != null;
		}
		if (actionId == "dzbot.upload")
			return m_P.m_Upload.CanStart(reasonDe, reasonEn);
		if (actionId == "dzbot.manage")
		{
			reasonDe = "Dieses Projekt kannst du in dzbot nicht bearbeiten";
			reasonEn = "You cannot edit this project in dzbot";
			DZS_DzbotProject dp = m_P.OpenProjectMod();
			return dp && dp.m_ManageUrl != "";
		}
		return true;
	}

	override bool IsActionListed(string actionId)
	{
		if (actionId == "dzbot.friends")
			return m_P.IsDzbotSession();
		if (actionId == "dzbot.rules")
			return m_P.OpenProjectMod() != null;
		if (actionId == "dzbot.upload")
			return m_P.m_Upload.LocalOpen();
		if (actionId == "dzbot.manage")
			return m_P.OpenProjectMod() != null;
		return true;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "dzbot.connect")
			m_P.StartConnect();
		else if (actionId == "dzbot.disconnect")
			m_P.AskDisconnect();
		else if (actionId == "dzbot.friends")
			m_P.OpenFriends();
		else if (actionId == "dzbot.rules")
			m_P.OpenRules("");
		else if (actionId == "dzbot.upload")
			m_P.m_Upload.OpenDialog();
		else if (actionId == "dzbot.manage")
		{
			DZS_DzbotProject dp = m_P.OpenProjectMod();
			if (dp && dp.m_ManageUrl != "")
			{
				GetGame().OpenURL(dp.m_ManageUrl);
				DZS_Log.Info("dzbot: browser opened " + dp.m_ManageUrl);
			}
		}
	}
}

// Sign-in plugin "dzbot" for the dzbot collaboration server: the token of the dzbot connection.
class DZS_DzbotAuth : DZS_AuthProvider
{
	DZS_DzbotPlugin m_P;

	override string Token(string baseUrl)
	{
		if (!m_P.IsConnected() || DZS_DzbotPlugin.BaseKey(baseUrl) != m_P.m_CollabUrl)
			return "";
		return m_P.m_Access;
	}

	override string UserName() { return m_P.m_UserName; }
	override void SignIn() { m_P.StartConnect(); }
	override void SignOut() { m_P.AskDisconnect(); }
}

// A request that needs the token: kept so it can be sent again after renewing.
class DZS_DzbotAuthReq : Managed
{
	string m_Slot;
	string m_Tag;
	string m_Path;
	string m_Fields;
	ref array<ref DZS_DzbotSection> m_Parts;
	ref Managed m_Data;
	bool m_Retried;
	string m_Tail;
}

// What the plugin holds of an open dzbot project: file keys, paths and revisions.
class DZS_DzbotFile : Managed
{
	string m_Key;
	string m_Path;
	int m_Rev;
	string m_Role;
}

class DZS_DzbotProject : Managed
{
	string m_Ref;
	string m_Kind;
	string m_Name;
	string m_MapKey;
	string m_Owner;
	string m_Server;
	string m_Session;
	// the web workbench of the mod or placement
	string m_ManageUrl;
	// every map dzbot names for it ("map" and "maps")
	ref array<string> m_Maps = new array<string>();
	// a mod's kind in dzbot: "files", or "project" — a building-block project saved as one file, dzstudio/project.json
	string m_ModKind;
	// a placement's kind in dzbot: "project" — a building-block project like a project mod's, saved as its one file
	// dzstudio/project.json; anything else ("objects") is still on its object list and is converted in dzbot first
	string m_PlacementKind;
	// dzstudio/project.json, the one file dzStudio opens and saves (null until the first save creates it)
	ref DZS_DzbotFile m_ProjectFile;
	// a project mod's settings as dzbot sent them (the project's settings shape, every member kept), and read for the
	// rules' pickers (DZS_DzbotRules)
	ref DZS_Json m_SettingsList;
	ref array<ref DZS_DzbotSetting> m_Settings = new array<ref DZS_DzbotSetting>();
	int m_ManifestRev;

	DZS_DzbotFile FileByKey(string key)
	{
		if (m_ProjectFile && m_ProjectFile.m_Key == key)
			return m_ProjectFile;
		return null;
	}

	int Revision()
	{
		int r = m_ManifestRev;
		if (m_ProjectFile && m_ProjectFile.m_Rev > r)
			r = m_ProjectFile.m_Rev;
		return r;
	}
}

// One save: the same id and body for every repeat.
class DZS_DzbotSave : Managed
{
	string m_Folder;
	string m_SaveId;
	string m_Fields;
	ref array<ref DZS_DzbotSection> m_Parts = new array<ref DZS_DzbotSection>();
	float m_Started;
	int m_Tries;
	// checksums: per part its entry text without the closing brace and the SHA-256 once fed
	ref array<string> m_Entries = new array<string>();
	ref array<string> m_Sums = new array<string>();
	ref DZS_Sha256 m_Hash;
	int m_HashAt;
	ref DZS_Joiner m_Join;
	string m_Tail;
	bool m_Joined;
	// the plan's files: their kernel checksums (DZS_Files.Checksum) formed here in slices for the projects plugin
	ref array<ref DZS_SaveFile> m_Files;
	int m_SumAt;
	ref DZS_FileSum m_Sum;
	bool m_SumsDone;
	int m_HashStart;
	// key -> path, for naming a file in checksum_mismatch
	ref map<string, string> m_PathOf = new map<string, string>();
	bool m_Blocked;
	// key of dzstudio/project.json when this save creates it
	string m_NewProjectKey;
	// a local project uploaded into the mod (DZS_DzbotUpload), not the open project; m_Map is its map
	bool m_Upload;
	string m_Map;
}

// Who may own a new mod: "me" or "org:<id>".
class DZS_DzbotOwner : Managed
{
	string m_Id;
	string m_Name;
	string m_Kind;
}

class DZS_DzbotProvider : DZS_StoreProvider
{
	DZS_DzbotPlugin m_P;

	override string Id() { return "dzbot"; }
	override bool Owns(string folder) { return folder.IndexOf(DZS_DzbotPlugin.PREFIX) == 0; }
	override string Label(string folder) { return m_P.LabelOf(folder); }
	override string PageUrl(string folder) { return m_P.ManageUrlOf(folder); }
	override string PageLabel(string folder) { return DZS_Texts.Pick("In dzbot öffnen", "Open in dzbot"); }
	override void Fetch(string folder) { m_P.Fetch(folder); }
	override void Save(string folder, array<ref DZS_SaveFile> files) { m_P.Save(folder, files); }
	override bool AutoSaveOff(string folder, out string de, out string en)
	{
		de = "Automatisch speichern: bei Platzierungen aus";
		en = "Autosave: off for placements";
		if (m_P.Outdated())
		{
			de = "Automatisch speichern: dzStudio ist veraltet";
			en = "Autosave: dzStudio is out of date";
			return true;
		}
		return m_P.KindOf(folder) == "placement";
	}
	override void Opened(string folder) { m_P.Opened(folder); }
	override DZS_Json BackupState(string folder) { return m_P.BackupState(folder); }
	override void Restored(string folder, DZS_Json state) { m_P.Restored(folder, state); }
	override bool PartChanged(string folder)
	{
		if (m_P.m_PartChanged != folder)
			return false;
		m_P.m_PartChanged = "";
		return true;
	}
	override void Closed(string folder) { m_P.Closed(folder); }
	override string TabLabel()
	{
		if (m_P.IsConnected())
			return DZS_Texts.Pick("dzbot · Server", "dzbot · Servers");
		return "";
	}
	override void RefreshRows() { m_P.RefreshProjects(); }
	override bool RowsLoading() { return m_P.m_ListLoading; }
	override string RowsEmptyText() { return m_P.m_ListError; }
	override void Rows(array<ref DZS_ProjectInfo> into) { m_P.Rows(into); }
	override string RowMeta(DZS_ProjectInfo p) { return m_P.RowMeta(p.m_Folder); }
	override bool RowOff(DZS_ProjectInfo p, out string de, out string en) { return m_P.RowOffOf(p.m_Folder, de, en); }
	override string Address() { return m_P.m_Base; }
}

class DZS_DzbotConnectDialog : DZS_Dialog
{
	DZS_DzbotPlugin m_P;
	protected ref DZS_SignInCode m_Body;

	void DZS_DzbotConnectDialog(DZS_DzbotPlugin p)
	{
		m_P = p;
		Setup(SMALL, "dzbot", DZS_Texts.Pick("Mit dzbot verbinden", "Connect with dzbot"), false);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("browser", DZS_Texts.Pick("Im Browser öffnen", "Open in the browser"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Body = new DZS_SignInCode(inner, DZS_Texts.Pick("Bestätige diesen Code in dzbot:", "Confirm this code in dzbot:"));
		AddFootFocus();
	}

	override void UpdateContent(float dt)
	{
		m_Body.Set(m_P.m_UserCode, m_P.m_VerifyUrl, m_P.m_FlowState, m_P.m_UserCode != "", m_P.m_BrowserOpened);
		m_Body.Tick(dt);
	}

	override float LayoutContent(float width)
	{
		return m_Body.Layout(width);
	}

	override void OnFoot(string tag)
	{
		if (tag == "browser")
		{
			m_P.OpenBrowser();
			return;
		}
		m_P.CancelConnect();
		Close();
	}

	override void Cancel()
	{
		m_P.CancelConnect();
		super.Cancel();
	}
}

[DZS_RegisterPlugin(DZS_DzbotPlugin)]
class DZS_DzbotPlugin : DZS_Plugin
{
	static const string PREFIX = "@dzbot-";
	static const float POLL_EVERY = 20;
	static const float SAVE_ASK_EVERY = 3;
	static const float SAVE_GIVE_UP = 60;
	static const int HASH_MS = 4;
	protected static bool s_BadSumDone;
	protected int m_WatchTick;
	protected int m_WatchLeft = -1;
	protected int m_WatchFrames;
	protected int m_WatchWorst;
	protected int m_WatchOver;
	protected string m_WatchPhase;
	protected string m_WatchWorstPhase;

	string m_Base;
	ref DZS_DzbotClient m_Client;
	protected ref DZS_DzbotLink m_Link;
	ref DZS_DzbotCatalog m_Catalog;
	protected ref DZS_DzbotProvider m_Provider;
	protected ref DZS_DzbotActions m_Actions;
	protected ref DZS_DzbotRules m_Rules;
	protected bool m_Session;
	ref DZS_DzbotFriends m_Friends;
	ref DZS_DzbotFriendsDialog m_FriendsDialog;
	protected ref DZS_DzbotPage m_Page;
	static const string COLLAB_URL = "https://collab.dzbot.de/dzcollab/";
	string m_CollabUrl;
	ref DZS_DzbotAuth m_Auth;
	protected ref map<string, int> m_TestAccounts = new map<string, int>();

	// sign-in
	string m_Access;
	string m_Refresh;
	int m_Expires;
	string m_UserName;
	protected bool m_Renewing;
	protected ref array<ref DZS_DzbotAuthReq> m_Parked = new array<ref DZS_DzbotAuthReq>();
	string m_DeviceCode;
	string m_UserCode;
	string m_VerifyUrl;
	string m_FlowState;
	protected float m_FlowWait;
	protected float m_FlowEnds;
	protected bool m_FlowOn;
	protected bool m_FlowAsking;
	protected ref DZS_DzbotConnectDialog m_Dialog;

	// projects
	ref map<string, ref DZS_DzbotProject> m_Projects = new map<string, ref DZS_DzbotProject>();
	protected ref array<string> m_ListOrder = new array<string>();
	bool m_ListLoading;
	string m_ListError;
	// counts the answers to /editor/projects
	int m_ListRev;
	ref array<ref DZS_DzbotOwner> m_Owners = new array<ref DZS_DzbotOwner>();
	ref DZS_DzbotUpload m_Upload;
	protected string m_OpenFolder;
	protected float m_PollIn;
	protected bool m_Polling;
	protected ref DZS_DzbotSave m_Save;
	protected float m_SaveRetryIn;
	protected float m_Now;
	protected bool m_Asking;
	// "Reload": dzbot's project file reopened through the normal open; the project it is for and the view kept
	protected string m_ReloadRef;
	protected string m_ReloadView;
	// dzbot's project-file revision the poll already asked about (asked once per revision)
	protected int m_AskedRev = -1;

	override string GetName() { return "dzbot"; }

	DZS_Kernel Kernel() { return m_Kernel; }

	// The open project when it is a dzbot project mod, else null.
	DZS_DzbotProject OpenProjectMod()
	{
		if (m_OpenFolder == "")
			return null;
		DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
		if (!dp || !IsProjectMod(dp))
			return null;
		return dp;
	}

	string ManageUrlOf(string folder)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (!dp || !IsProjectMod(dp))
			return "";
		return dp.m_ManageUrl;
	}

	array<ref DZS_DzbotSetting> OpenSettings()
	{
		DZS_DzbotProject dp = OpenProjectMod();
		if (!dp)
			return new array<ref DZS_DzbotSetting>();
		return dp.m_Settings;
	}

	// The rules of the open project mod; a group's block first when one is named.
	void OpenRules(string groupId)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !OpenProjectMod())
			return;
		DZS_RulesDialog d = new DZS_RulesDialog(m_Rules, inst, groupId);
		d.Open();
	}

	// dzbot's settings of the open project mod as it sent them (a copy), null when it sent none.
	DZS_Json OpenSettingsList()
	{
		DZS_DzbotProject dp = OpenProjectMod();
		if (!dp || !dp.m_SettingsList)
			return null;
		return dp.m_SettingsList.Clone();
	}

	// "settings" of a load or poll reply: the list in the project's settings shape (docs/extensions/dzbot.rules.md),
	// kept as it is; a reply without it leaves the settings as they were.
	static void TakeSettings(DZS_SrcNode list, DZS_DzbotProject dp)
	{
		if (!list || !list.IsArr())
			return;
		DZS_Json taken = JsonOf(list);
		KeepSettings(taken, dp);
	}

	static void KeepSettings(DZS_Json list, DZS_DzbotProject dp)
	{
		if (!list || !list.IsArr())
			return;
		dp.m_SettingsList = list;
		dp.m_Settings.Clear();
		for (int i = 0; i < list.Count(); i++)
		{
			DZS_Json entry = list.At(i);
			DZS_DzbotSetting st = DZS_DzbotRules.SettingOf(entry);
			// a section is a heading of dzbot's form, no value a rule can read
			if (st && st.m_Type != DZS_DzbotRules.SECTION)
				dp.m_Settings.Insert(st);
		}
	}

	// A value of a reply as a JSON tree, every member and number text as sent.
	static DZS_Json JsonOf(DZS_SrcNode node)
	{
		// a local: a recursive call must not clobber the caller's parameter
		DZS_SrcNode src = node;
		if (!src)
			return DZS_Json.Null();
		if (src.IsObj())
		{
			DZS_Json obj = DZS_Json.Obj();
			for (int i = 0; i < src.m_Keys.Count(); i++)
			{
				DZS_SrcNode member = src.m_Items[i];
				DZS_Json mj = JsonOf(member);
				obj.Set(src.m_Keys[i], mj);
			}
			return obj;
		}
		if (src.IsArr())
		{
			DZS_Json arr = DZS_Json.Arr();
			for (int k = 0; k < src.m_Items.Count(); k++)
			{
				DZS_SrcNode item = src.m_Items[k];
				DZS_Json ij = JsonOf(item);
				arr.Add(ij);
			}
			return arr;
		}
		if (src.IsStr())
			return DZS_Json.Str(src.m_Str);
		if (src.IsNum())
			return DZS_Json.Num(src.m_Str);
		if (src.IsBool())
			return DZS_Json.Boolean(src.m_Bool);
		return DZS_Json.Null();
	}

	override void OnRegister()
	{
		string shaArg;
		if (GetCLIParam("dzsShaTest", shaArg))
			DZS_Sha256.SelfTest();
		DZS_Settings st = m_Kernel.GetSettings();
		st.Define("dzbot.access", DZS_Json.Str(""));
		st.Define("dzbot.refresh", DZS_Json.Str(""));
		st.Define("dzbot.expires", DZS_Json.Num("0"));
		st.Define("dzbot.user", DZS_Json.Str(""));
		st.Define("dzbot.tokens", DZS_Json.Obj());
		st.Define("dzbot.baseUrl", DZS_Json.Str(""));
		m_Base = st.GetString("dzbot.baseUrl", "");
		string cli;
		if (GetCLIParam("dzsDzbotUrl", cli) && cli != "")
			m_Base = cli;
		if (m_Base == "")
			m_Base = DZS_DzbotClient.DEFAULT_URL;
		m_Base = BaseKey(m_Base);
		LoadTokens();
		m_Link = new DZS_DzbotLink();
		m_Link.m_P = this;
		m_Client = new DZS_DzbotClient(m_Base, m_Link);
		m_Catalog = new DZS_DzbotCatalog();
		m_Catalog.m_P = this;
		m_Provider = new DZS_DzbotProvider();
		m_Provider.m_P = this;
		DZS_StoreProvider.Register(m_Provider);
		m_Actions = new DZS_DzbotActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterAction("dzbot", "dzbot.connect", "Mit dzbot verbinden …", "Connect with dzbot …", null, m_Actions);
		ui.RegisterAction("dzbot", "dzbot.disconnect", "Von dzbot trennen", "Disconnect from dzbot", null, m_Actions);
		ui.SetWhen("dzbot.connect", "scene");
		ui.SetWhen("dzbot.disconnect", "scene");
		ui.AddMenuEntry("project", "dzbot.connect", 120);
		ui.AddMenuEntry("project", "dzbot.disconnect", 121);
		m_Upload = new DZS_DzbotUpload(this);
		ui.RegisterAction("dzbot", "dzbot.upload", "Zu dzbot hochladen …", "Upload to dzbot …", null, m_Actions);
		ui.SetWhen("dzbot.upload", "scene");
		ui.AddMenuEntry("project", "dzbot.upload", 122);
		ui.RegisterAction("dzbot", "dzbot.manage", "In dzbot öffnen", "Open in dzbot", null, m_Actions);
		ui.SetWhen("dzbot.manage", "scene");
		ui.AddMenuEntry("project", "dzbot.manage", 123);
		ui.RegisterAction("dzbot", "dzbot.friends", "Freunde …", "Friends …", null, m_Actions);
		m_Rules = new DZS_DzbotRules();
		m_Rules.m_P = this;
		DZS_ProjectExtensions.Add(m_Rules);
		ui.RegisterAction("dzbot", "dzbot.rules", "dzbot-Regeln …", "dzbot rules …", null, m_Actions);
		ui.SetWhen("dzbot.rules", "scene");
		ui.AddMenuEntry("project", "dzbot.rules", 456);
		DZS_SessionKnobs.Add("dzbot.friends", "Freunde …", "Friends …");
		m_Friends = new DZS_DzbotFriends();
		m_Friends.m_P = this;
		st.RegisterTab("dzbot", "dzbot", "dzbot");
		m_Page = new DZS_DzbotPage();
		m_Page.m_P = this;
		st.RegisterPage("dzbot", m_Page);
		st.Define("dzbot.collabUrl", DZS_Json.Str(""));
		m_CollabUrl = st.GetString("dzbot.collabUrl", "");
		if (GetCLIParam("dzsDzbotCollab", cli) && cli != "")
			m_CollabUrl = cli;
		if (m_CollabUrl == "")
			m_CollabUrl = COLLAB_URL;
		m_CollabUrl = BaseKey(m_CollabUrl);
		m_Auth = new DZS_DzbotAuth();
		m_Auth.m_P = this;
		DZS_SyncServers.RegisterAuth("dzbot", m_Auth);
		DZS_SyncServers.AddFixed(m_CollabUrl, "dzbot");
		string acc;
		if (GetCLIParam("dzsDzbotAccounts", acc))
		{
			array<string> pairs = new array<string>();
			acc.Split(",", pairs);
			foreach (string pr : pairs)
			{
				int colon = pr.IndexOf(":");
				if (colon > 0)
					m_TestAccounts.Set(pr.Substring(0, colon), pr.Substring(colon + 1, pr.Length() - colon - 1).ToInt());
			}
		}
		DZS_Log.Info("dzbot: plugin, address " + m_Base + ", connected " + IsConnected() + " " + m_UserName);
		m_Catalog.Start();
	}

	override void OnSessionStart(string world)
	{
		m_Session = true;
		m_Catalog.OnSession();
	}

	override void OnSessionEnd()
	{
		m_Session = false;
	}

	bool IsConnected() { return m_Refresh != ""; }

	// The running session is on the dzbot server: its address is the dzbot collaboration service.
	bool IsDzbotSession()
	{
		string srv = DZS_SessionPeople.Server();
		if (srv == "")
			return false;
		return BaseKey(srv) == m_CollabUrl;
	}

	// The dzbot account of a participant: userId u‹id›.
	int AccountOf(string userId)
	{
		if (m_TestAccounts.Contains(userId))
			return m_TestAccounts.Get(userId);
		if (userId.Length() < 2 || userId.Substring(0, 1) != "u")
			return 0;
		string num = userId.Substring(1, userId.Length() - 1);
		int id = num.ToInt();
		if (id.ToString() != num)
			return 0;
		return id;
	}

	void OpenFriends()
	{
		if (m_FriendsDialog)
			return;
		m_FriendsDialog = new DZS_DzbotFriendsDialog(this);
		m_FriendsDialog.Open();
	}

	static int Now()
	{
		int y, mo, d, h, mi, s;
		GetYearMonthDay(y, mo, d);
		GetHourMinuteSecond(h, mi, s);
		int days = (y - 2000) * 372 + mo * 31 + d;
		return ((days * 24 + h) * 60 + mi) * 60 + s;
	}

	// Tokens belong to the base address they were issued by and never go to another one (answers to section 11 part 2).
	static string BaseKey(string url)
	{
		string b = url.Trim();
		while (b.Length() > 0)
		{
			int last = b.Length() - 1;
			string tail = b.Substring(last, 1);
			if (tail != "/")
				break;
			b = b.Substring(0, last);
		}
		b.ToLower();
		return b;
	}

	protected void LoadTokens()
	{
		DZS_Settings st = m_Kernel.GetSettings();
		if (st.GetString("dzbot.refresh", "") != "")
		{
			// tokens of earlier builds name no address: dropped, the user connects again
			DZS_Log.Warn("dzbot: tokens without an address dropped");
			st.Reset("dzbot.access");
			st.Reset("dzbot.refresh");
			st.Reset("dzbot.expires");
			st.Reset("dzbot.user");
			st.Flush();
		}
		m_Access = "";
		m_Refresh = "";
		m_Expires = 0;
		m_UserName = "";
		DZS_Json all = st.Get("dzbot.tokens");
		if (!all || !all.IsObj())
			return;
		DZS_Json mine = all.Get(m_Base);
		if (!mine || !mine.IsObj())
			return;
		DZS_Json a = mine.Get("access");
		DZS_Json r = mine.Get("refresh");
		DZS_Json e = mine.Get("expires");
		DZS_Json u = mine.Get("user");
		if (a && a.IsStr())
			m_Access = a.m_Str;
		if (r && r.IsStr())
			m_Refresh = r.m_Str;
		if (e && e.IsNum())
			m_Expires = e.m_Str.ToInt();
		if (u && u.IsStr())
			m_UserName = u.m_Str;
	}

	protected void StoreTokens()
	{
		DZS_Settings st = m_Kernel.GetSettings();
		DZS_Json all = st.Get("dzbot.tokens");
		DZS_Json next;
		if (all && all.IsObj())
			next = all.Clone();
		else
			next = DZS_Json.Obj();
		if (m_Refresh == "")
			next.Remove(m_Base);
		else
		{
			DZS_Json mine = DZS_Json.Obj();
			mine.Set("access", DZS_Json.Str(m_Access));
			mine.Set("refresh", DZS_Json.Str(m_Refresh));
			mine.Set("expires", DZS_Json.Int(m_Expires));
			mine.Set("user", DZS_Json.Str(m_UserName));
			next.Set(m_Base, mine);
		}
		st.Set("dzbot.tokens", next);
		st.Flush();
	}

	override void OnUpdate(float dt)
	{
		m_Now += dt;
		WatchFrame();
		m_Catalog.Update();
		m_Friends.Tick();
		UpdateFlow(dt);
		UpdatePoll(dt);
		UpdateSave(dt);
	}

	DZS_ProjectsPlugin Projects()
	{
		return DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
	}

	protected void Msg(string tone, string de, string en, float secs = 4.0)
	{
		string t = DZS_Texts.Pick(de, en);
		m_Kernel.GetScreen().ShowMessage("dzbot", tone, DZS_W.Literal(t), secs);
	}

	// ---- requests with token ----

	void AuthPost(string slot, string tag, string path, string fields, array<ref DZS_DzbotSection> parts = null, Managed data = null, string tail = "")
	{
		DZS_DzbotAuthReq q = new DZS_DzbotAuthReq();
		q.m_Tail = tail;
		q.m_Slot = slot;
		q.m_Tag = tag;
		q.m_Path = path;
		q.m_Fields = fields;
		q.m_Parts = parts;
		q.m_Data = data;
		SendAuth(q);
	}

	protected void SendAuth(DZS_DzbotAuthReq q)
	{
		if (m_Renewing)
		{
			m_Parked.Insert(q);
			return;
		}
		if (m_Refresh != "" && m_Expires - Now() < 300)
		{
			m_Parked.Insert(q);
			Renew();
			return;
		}
		string json = "{\"token\":" + DZS_DzbotFrame.Q(m_Access);
		if (q.m_Fields != "")
			json += "," + q.m_Fields;
		json += "}";
		int t0 = TickCount(0);
		string body = DZS_DzbotFrame.Build(json, q.m_Parts, q.m_Tail);
		int t1 = TickCount(0);
		m_Client.Post(q.m_Slot, q.m_Tag, q.m_Path, body, q);
		if (q.m_Tail != "")
			DZS_Log.Info("dzbot: request built in " + ((t1 - t0) / 10000) + " ms, posted in " + ((TickCount(0) - t1) / 10000) + " ms");
	}

	protected void Renew()
	{
		if (m_Renewing || Outdated())
			return;
		m_Renewing = true;
		DZS_Log.Info("dzbot: renewing the access token (" + (m_Expires - Now()) + " s left)");
		string json = "{\"refreshToken\":" + DZS_DzbotFrame.Q(m_Refresh) + "}";
		string body = DZS_DzbotFrame.Build(json);
		m_Client.Post("main", "auth.refresh", "/editor/auth/refresh", body);
	}

	protected void OnRenewed(DZS_DzbotReply r)
	{
		m_Renewing = false;
		if (r.Ok() && r.Str("state") == "approved" && r.Str("accessToken") != "")
		{
			m_Access = r.Str("accessToken");
			m_Refresh = r.Str("refreshToken");
			m_Expires = Now() + r.Int("expiresIn");
			StoreTokens();
			DZS_Log.Info("dzbot: token renewed, " + r.Int("expiresIn") + " s");
			array<ref DZS_DzbotAuthReq> parked = new array<ref DZS_DzbotAuthReq>();
			foreach (DZS_DzbotAuthReq q : m_Parked)
				parked.Insert(q);
			m_Parked.Clear();
			foreach (DZS_DzbotAuthReq q2 : parked)
				SendAuth(q2);
			return;
		}
		if (!r.m_Framed)
		{
			DZS_Log.Warn("dzbot: renewing failed, not reachable: " + r.m_Fail);
			FailParked("dzbot nicht erreichbar", "dzbot not reachable");
			return;
		}
		DZS_Log.Warn("dzbot: renewing refused: " + r.Error());
		Forget();
		FailParked("Neu mit dzbot verbinden", "Connect with dzbot again");
		Msg("danger", "Neu mit dzbot verbinden", "Connect with dzbot again", 8.0);
	}

	protected void FailParked(string de, string en)
	{
		array<ref DZS_DzbotAuthReq> parked = new array<ref DZS_DzbotAuthReq>();
		foreach (DZS_DzbotAuthReq q : m_Parked)
			parked.Insert(q);
		m_Parked.Clear();
		foreach (DZS_DzbotAuthReq q2 : parked)
		{
			DZS_DzbotReply fake = new DZS_DzbotReply();
			fake.m_Fail = DZS_Texts.Pick(de, en);
			Dispatch(q2.m_Tag, fake, q2);
		}
	}

	protected void Forget()
	{
		if (m_Friends)
		{
			m_Friends.m_State = "";
			m_Friends.m_List.Clear();
			m_Friends.m_Rev++;
		}
		m_Access = "";
		m_Refresh = "";
		m_Expires = 0;
		m_UserName = "";
		StoreTokens();
	}

	// ---- answers ----

	string VersionKey()
	{
		return "dzbot:" + m_Base;
	}

	bool Outdated()
	{
		return DZS_Version.IsBlocked(VersionKey());
	}

	// A user action that needs dzbot while dzbot refuses this version: the dialog again, nothing sent.
	protected bool StopOutdated()
	{
		return DZS_Version.ShowIfBlocked(VersionKey());
	}

	protected void OnOutdated(DZS_DzbotReply r)
	{
		bool first = !Outdated();
		m_Client.Block();
		m_Parked.Clear();
		m_Renewing = false;
		m_Polling = false;
		if (m_Dialog)
			m_Dialog.Close();
		CancelConnect();
		DZS_Version.Outdated(VersionKey(), "dzbot", r.Str("minVersion"), r.Str("updateUrl"), false);
		if (first && m_OpenFolder != "")
			PlateFor(m_OpenFolder);
	}

	void OnReply(DZS_DzbotCall call, DZS_DzbotReply r)
	{
		string tag = call.m_Tag;
		if (r.m_Framed)
		{
			string uv = r.Str("updateVersion");
			if (uv != "")
				DZS_Version.Recommended(uv, r.Str("updateUrl"));
			if (r.Error() == "client_outdated")
				OnOutdated(r);
		}
		if (tag == "catalog.revision")
		{
			m_Catalog.OnRevision(r);
			return;
		}
		if (tag == "catalog.data")
		{
			m_Catalog.OnData(r);
			return;
		}
		if (tag == "catalog.image")
		{
			m_Catalog.OnImage(call, r);
			return;
		}
		if (!r.m_Framed && r.m_Fail == "version")
			VersionMismatch(r.m_Version);
		if (tag == "auth.device")
		{
			OnDevice(r);
			return;
		}
		if (tag == "auth.token")
		{
			OnToken(r);
			return;
		}
		if (tag == "auth.refresh")
		{
			OnRenewed(r);
			return;
		}
		if (tag == "auth.revoke")
			return;
		DZS_DzbotAuthReq q = DZS_DzbotAuthReq.Cast(call.m_Data);
		string err = r.Error();
		if (q && r.m_Framed && (err == "token_expired" || err == "token_invalid") && !q.m_Retried && m_Refresh != "")
		{
			q.m_Retried = true;
			DZS_Log.Info("dzbot: " + err + " — renew once and repeat " + q.m_Tag);
			m_Parked.Insert(q);
			Renew();
			return;
		}
		if (r.m_Framed && err == "token_invalid")
		{
			Forget();
			Msg("danger", "Neu mit dzbot verbinden", "Connect with dzbot again", 8.0);
		}
		Dispatch(tag, r, q);
	}

	// Another major version of the editor endpoints: no connection, the user has to update.
	protected void VersionMismatch(string ver)
	{
		DZS_Log.Warn("dzbot: answers with version '" + ver + "', this dzStudio speaks " + DZS_DzbotFrame.Expected());
		CancelConnect();
		string de = "dzbot antwortet mit Version " + ver + " — dzStudio aktualisieren";
		string en = "dzbot answers with version " + ver + " — update dzStudio";
		string t = DZS_Texts.Pick(de, en);
		m_Kernel.GetScreen().ShowMessage("dzbot.version", "danger", DZS_W.Literal(t), 12.0);
	}

	protected void Dispatch(string tag, DZS_DzbotReply r, DZS_DzbotAuthReq q)
	{
		if (tag == "friends")
			m_Friends.OnList(r);
		else if (tag == "friends.set")
			m_Friends.OnSet(r, DZS_DzbotFriendSet.Cast(q.m_Data));
		else if (tag == "projects")
			OnProjects(r);
		else if (tag == "load")
			OnLoad(r, q);
		else if (tag == "poll")
			OnPoll(r);
		else if (tag == "save")
			OnSaved(r);
		else if (tag == "reload")
			OnReload(r, q);
		else if (tag == "keep")
			OnKeep(r);
		else if (tag == "upload.create")
			m_Upload.OnCreated(r);
		else if (tag == "upload.load")
			m_Upload.OnLoaded(r);
	}

	// ---- device flow ----

	void StartConnect()
	{
		if (m_FlowOn || StopOutdated())
			return;
		m_FlowOn = true;
		m_UserCode = "";
		m_VerifyUrl = "";
		m_BrowserOpened = false;
		m_FlowState = DZS_Texts.Pick("Holt einen Code …", "Getting a code …");
		m_Dialog = new DZS_DzbotConnectDialog(this);
		m_Dialog.Open();
		string body = DZS_DzbotFrame.Build("{}");
		m_Client.Post("main", "auth.device", "/editor/auth/device", body);
	}

	void CancelConnect()
	{
		if (!m_FlowOn)
			return;
		m_FlowOn = false;
		m_Dialog = null;
		DZS_Log.Info("dzbot: connecting cancelled");
	}

	bool m_BrowserOpened;

	void OpenBrowser()
	{
		if (m_VerifyUrl == "")
			return;
		GetGame().OpenURL(m_VerifyUrl);
		m_BrowserOpened = true;
		DZS_Log.Info("dzbot: browser opened " + m_VerifyUrl);
	}

	protected void OnDevice(DZS_DzbotReply r)
	{
		if (!m_FlowOn)
			return;
		if (!r.Ok())
		{
			m_FlowState = DZS_Texts.Pick("dzbot nicht erreichbar", "dzbot not reachable") + " (" + r.Message() + ")";
			DZS_Log.Warn("dzbot: device code failed: " + r.Message());
			return;
		}
		m_DeviceCode = r.Str("deviceCode");
		m_UserCode = r.Str("userCode");
		m_VerifyUrl = r.Str("verificationUrl");
		float interval = r.Int("interval");
		if (interval < 5)
			interval = 5;
		m_FlowWait = interval;
		m_FlowEnds = m_Now + r.Int("expiresIn");
		m_FlowState = DZS_Texts.Pick("Wartet auf Bestätigung …", "Waiting for confirmation …");
		DZS_Log.Info("dzbot: device code " + m_UserCode + ", " + m_VerifyUrl);
		string nb;
		if (!GetCLIParam("dzsDzbotNoBrowser", nb))
			OpenBrowser();
	}

	protected void UpdateFlow(float dt)
	{
		if (!m_FlowOn || m_DeviceCode == "" || m_FlowAsking)
			return;
		m_FlowWait -= dt;
		if (m_FlowWait > 0)
			return;
		m_FlowWait = 5;
		if (m_Now > m_FlowEnds)
		{
			EndFlow("abgelaufen", "Der Code ist abgelaufen", "The code has expired");
			return;
		}
		m_FlowAsking = true;
		string json = "{\"deviceCode\":" + DZS_DzbotFrame.Q(m_DeviceCode) + "}";
		string body = DZS_DzbotFrame.Build(json);
		m_Client.Post("main", "auth.token", "/editor/auth/token", body);
	}

	protected void OnToken(DZS_DzbotReply r)
	{
		m_FlowAsking = false;
		if (!m_FlowOn)
			return;
		if (!r.m_Framed)
		{
			m_FlowState = DZS_Texts.Pick("dzbot nicht erreichbar — versucht es weiter", "dzbot not reachable — trying again");
			return;
		}
		string state = r.Str("state");
		DZS_Log.Info("dzbot: device state " + state);
		if (state == "waiting")
			return;
		if (state == "approved")
		{
			m_Access = r.Str("accessToken");
			m_Refresh = r.Str("refreshToken");
			m_Expires = Now() + r.Int("expiresIn");
			DZS_SrcNode user = r.m_Json.Get("user");
			if (user && user.IsObj())
				m_UserName = DZS_DzbotCatalog.Text(user, "name");
			StoreTokens();
			DZS_Log.Info("dzbot: connected as " + m_UserName + ", tokens " + m_Access.Length() + "/" + m_Refresh.Length() + " characters, " + r.Int("expiresIn") + " s");
			m_FlowOn = false;
			if (m_Dialog)
				m_Dialog.Close();
			m_Dialog = null;
			Msg("success", "Mit dzbot verbunden als " + m_UserName, "Connected with dzbot as " + m_UserName);
			return;
		}
		if (state == "denied" || state == "abgelehnt" || state == "refused")
			EndFlow(state, "In dzbot abgelehnt", "Refused in dzbot");
		else if (state == "expired")
			EndFlow(state, "Der Code ist abgelaufen", "The code has expired");
		else
			EndFlow(state, "Verbinden fehlgeschlagen (" + state + ")", "Connecting failed (" + state + ")");
	}

	protected void EndFlow(string state, string de, string en)
	{
		DZS_Log.Info("dzbot: device flow ends: " + state);
		m_FlowOn = false;
		if (m_Dialog)
			m_Dialog.Close();
		m_Dialog = null;
		Msg("danger", de, en, 6.0);
	}

	void AskDisconnect()
	{
		DZS_ConfirmDialog c = new DZS_ConfirmDialog("dzbot", DZS_Texts.Pick("Von dzbot trennen", "Disconnect from dzbot"), DZS_Texts.Pick("Der Editor vergisst die Anmeldung; offene dzbot-Projekte lassen sich danach nur lokal speichern.", "The editor forgets the sign-in; open dzbot projects can then only be saved locally."));
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("disconnect", DZS_Texts.Pick("Trennen", "Disconnect"), "danger");
		c.m_OnResult.Insert(OnDisconnectAnswer);
		c.Open();
	}

	void OnDisconnectAnswer(DZS_Dialog d, string result)
	{
		if (result != "disconnect")
			return;
		if (m_Refresh != "")
		{
			string json = "{\"token\":" + DZS_DzbotFrame.Q(m_Refresh) + "}";
			string body = DZS_DzbotFrame.Build(json);
			m_Client.Post("main", "auth.revoke", "/editor/auth/revoke", body);
		}
		string who = m_UserName;
		Forget();
		m_Projects.Clear();
		m_ListOrder.Clear();
		m_Owners.Clear();
		DZS_Log.Info("dzbot: disconnected (" + who + ")");
		Msg("info", "Von dzbot getrennt", "Disconnected from dzbot");
		if (m_OpenFolder != "")
			PlateFor(m_OpenFolder);
	}

	// ---- project list (browser tab) ----

	static string FolderOf(string pref)
	{
		string f = pref;
		f.Replace(":", "-");
		return PREFIX + f;
	}

	static string RefOf(string folder)
	{
		string r = folder.Substring(PREFIX.Length(), folder.Length() - PREFIX.Length());
		int dash = r.IndexOf("-");
		if (dash < 0)
			return r;
		string kind = r.Substring(0, dash);
		string id = r.Substring(dash + 1, r.Length() - dash - 1);
		return kind + ":" + id;
	}

	string KindOf(string folder)
	{
		string pref = RefOf(folder);
		int colon = pref.IndexOf(":");
		if (colon < 0)
			return "";
		return pref.Substring(0, colon);
	}

	string LabelOf(string folder)
	{
		if (KindOf(folder) == "placement")
			return DZS_Texts.Pick("dzbot-Platzierung", "dzbot placement");
		return DZS_Texts.Pick("dzbot-Mod", "dzbot mod");
	}

	void RefreshProjects()
	{
		if (!IsConnected())
		{
			m_ListError = DZS_Texts.Pick("Nicht mit dzbot verbunden", "Not connected with dzbot");
			return;
		}
		m_ListLoading = true;
		m_ListError = "";
		AuthPost("main", "projects", "/editor/projects", "");
	}

	protected string MapOf(DZS_SrcNode p)
	{
		string m = DZS_DzbotCatalog.Text(p, "map");
		if (m != "")
			return m;
		DZS_SrcNode maps = p.Get("maps");
		DZS_ProjectsPlugin pp = Projects();
		string loaded = "";
		if (pp && !pp.IsHome())
			loaded = pp.LoadedMapKey();
		string first = "";
		if (maps && maps.IsArr())
		{
			foreach (DZS_SrcNode mn : maps.m_Items)
			{
				if (!mn.IsStr())
					continue;
				if (first == "")
					first = mn.m_Str;
				if (mn.m_Str == loaded)
					return loaded;
			}
		}
		return first;
	}

	DZS_DzbotProject TakeProject(DZS_SrcNode p)
	{
		string pref = DZS_DzbotCatalog.Text(p, "ref");
		if (pref == "")
			return null;
		DZS_DzbotProject dp = m_Projects.Get(pref);
		if (!dp)
		{
			dp = new DZS_DzbotProject();
			dp.m_Ref = pref;
			m_Projects.Set(pref, dp);
		}
		dp.m_Kind = DZS_DzbotCatalog.Text(p, "kind");
		dp.m_ModKind = DZS_DzbotCatalog.Text(p, "modKind");
		dp.m_PlacementKind = DZS_DzbotCatalog.Text(p, "placementKind");
		dp.m_Name = DZS_DzbotCatalog.Text(p, "name");
		dp.m_MapKey = MapOf(p);
		dp.m_Owner = DZS_DzbotCatalog.Text(p, "owner");
		dp.m_Server = DZS_DzbotCatalog.Text(p, "server");
		dp.m_ManageUrl = DZS_DzbotCatalog.Text(p, "manageUrl");
		dp.m_Maps.Clear();
		string fixedMap = DZS_DzbotCatalog.Text(p, "map");
		if (fixedMap != "")
			dp.m_Maps.Insert(fixedMap);
		DZS_SrcNode maps = p.Get("maps");
		if (maps && maps.IsArr())
		{
			foreach (DZS_SrcNode mn : maps.m_Items)
			{
				if (mn.IsStr() && dp.m_Maps.Find(mn.m_Str) < 0)
					dp.m_Maps.Insert(mn.m_Str);
			}
		}
		return dp;
	}

	protected void OnProjects(DZS_DzbotReply r)
	{
		m_ListLoading = false;
		m_ListRev++;
		if (!r.m_Framed)
		{
			m_ListError = DZS_Texts.Pick("dzbot nicht erreichbar", "dzbot not reachable");
			return;
		}
		if (!r.Ok())
		{
			m_ListError = r.Message();
			return;
		}
		m_ListOrder.Clear();
		DZS_SrcNode list = r.m_Json.Get("projects");
		if (list && list.IsArr())
		{
			foreach (DZS_SrcNode p : list.m_Items)
			{
				DZS_DzbotProject dp = TakeProject(p);
				if (dp)
					m_ListOrder.Insert(dp.m_Ref);
			}
		}
		m_Owners.Clear();
		DZS_SrcNode owners = r.m_Json.Get("owners");
		if (owners && owners.IsArr())
		{
			foreach (DZS_SrcNode onode : owners.m_Items)
			{
				DZS_DzbotOwner ow = new DZS_DzbotOwner();
				ow.m_Id = DZS_DzbotCatalog.Text(onode, "id");
				ow.m_Name = DZS_DzbotCatalog.Text(onode, "name");
				ow.m_Kind = DZS_DzbotCatalog.Text(onode, "kind");
				if (ow.m_Id != "")
					m_Owners.Insert(ow);
			}
		}
		DZS_Log.Info("dzbot: project list " + m_ListOrder.Count() + " mod(s) and placement(s)");
	}

	// The refs of the last project list, in its order.
	array<string> ListOrder()
	{
		return m_ListOrder;
	}

	void Rows(array<ref DZS_ProjectInfo> into)
	{
		foreach (string pref : m_ListOrder)
		{
			DZS_DzbotProject dp = m_Projects.Get(pref);
			DZS_ProjectInfo info = new DZS_ProjectInfo();
			info.m_Folder = FolderOf(pref);
			info.m_Name = dp.m_Name;
			info.m_MapKey = dp.m_MapKey;
			into.Insert(info);
		}
	}

	// dzStudio edits project mods and placements built as projects; a mod of files stays in the workbench, a placement
	// still on its object list is converted in dzbot first.
	bool RowOffOf(string folder, out string de, out string en)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (!dp)
			return false;
		if (IsFilesMod(dp))
		{
			de = FILES_MOD_DE;
			en = FILES_MOD_EN;
			return true;
		}
		if (dp.m_Kind != "placement" || HoldsProject(dp))
			return false;
		de = OBJECTS_PLACEMENT_DE;
		en = OBJECTS_PLACEMENT_EN;
		return true;
	}

	static const string PROJECT_PATH = "dzstudio/project.json";
	static const string FILES_MOD_DE = "Datei-Mods bearbeitest du in der dzbot-Werkbank; in dzStudio nur Projekt-Mods";
	static const string FILES_MOD_EN = "Edit file mods in the dzbot workbench; dzStudio edits project mods";
	static const string OBJECTS_PLACEMENT_DE = "Muss zuerst in dzbot umgewandelt werden";
	static const string OBJECTS_PLACEMENT_EN = "Must be converted in dzbot first";

	static bool IsFilesMod(DZS_DzbotProject dp)
	{
		return dp.m_Kind == "mod" && dp.m_ModKind != "project";
	}

	static bool IsProjectMod(DZS_DzbotProject dp)
	{
		return dp.m_Kind == "mod" && dp.m_ModKind == "project";
	}

	// A project mod or a placement built as a project: one file, dzstudio/project.json, opened and saved as the
	// project file. Only a project mod has settings and the dzbot rules.
	static bool HoldsProject(DZS_DzbotProject dp)
	{
		if (IsProjectMod(dp))
			return true;
		return dp.m_Kind == "placement" && dp.m_PlacementKind == "project";
	}

	// The text of dzstudio/project.json in a load reply, null when the mod has none.
	protected array<string> ProjectChunksOf(DZS_DzbotReply r)
	{
		DZS_SrcNode files = r.m_Json.Get("files");
		if (!files || !files.IsArr())
			return null;
		foreach (DZS_SrcNode f : files.m_Items)
		{
			if (DZS_DzbotCatalog.Text(f, "role") != "project")
				continue;
			DZS_DzbotSection sec = r.Find("file", DZS_DzbotCatalog.Text(f, "key"));
			if (sec)
				return sec.m_Chunks;
		}
		return null;
	}

	string RowMeta(string folder)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (!dp)
			return "";
		string mapName = DZS_Texts.Pick("unbekannt", "unknown");
		if (dp.m_MapKey != "")
			mapName = DZS_Maps.Display(dp.m_MapKey);
		string kind = "Mod";
		if (dp.m_Kind == "placement")
			kind = DZS_Texts.Pick("Platzierung", "Placement");
		string meta = kind + " · " + mapName;
		if (dp.m_Server != "")
			meta += " · " + dp.m_Server;
		if (dp.m_Owner != "")
			meta += " · " + dp.m_Owner;
		return meta;
	}

	// ---- open: the current state into the folder ----

	void Fetch(string folder)
	{
		string pref = RefOf(folder);
		if (StopOutdated())
		{
			Projects().Fetched(folder, false, "dzStudio ist veraltet", "dzStudio is out of date");
			return;
		}
		if (!IsConnected())
		{
			Projects().Fetched(folder, false, "Nicht mit dzbot verbunden — Projekt › Mit dzbot verbinden …", "Not connected with dzbot — Project › Connect with dzbot …");
			return;
		}
		DZS_Log.Info("dzbot: loading " + pref);
		m_Kernel.GetScreen().ShowMessage("dzbot", "info", DZS_W.Literal(DZS_Texts.Pick("Lädt aus dzbot …", "Loading from dzbot …")), 10.0);
		string fields = "\"project\":" + DZS_DzbotFrame.Q(pref);
		AuthPost("main", "load", "/editor/project/load", fields, null, null);
	}

	protected void OnLoad(DZS_DzbotReply r, DZS_DzbotAuthReq q)
	{
		m_Kernel.GetScreen().HideMessage("dzbot");
		DZS_ProjectsPlugin pp = Projects();
		if (!r.m_Framed)
		{
			DZS_Log.Warn("dzbot: load failed: " + r.m_Fail);
			string failFolder = "";
			if (pp)
				pp.Fetched(failFolder, false, "dzbot nicht erreichbar", "dzbot not reachable");
			return;
		}
		if (!r.Ok())
		{
			// dzbot's reason as it says it ("not found", a placement it cannot open yet …)
			string m = r.Message();
			if (m == "")
				m = r.Error();
			if (pp)
				pp.Fetched("", false, "dzbot: " + m, "dzbot: " + m);
			return;
		}
		DZS_SrcNode pj = r.m_Json.Get("project");
		if (!pj)
			return;
		DZS_DzbotProject dp = TakeProject(pj);
		DZS_SrcNode sess = r.m_Json.Get("session");
		if (sess && !sess.IsNull())
		{
			pp.Fetched("", false, "Für dieses Projekt läuft eine gemeinsame Sitzung — Gemeinsam bearbeiten gibt es in diesem Editor noch nicht", "A shared session runs for this project — shared editing is not in this editor yet");
			return;
		}
		if (IsFilesMod(dp))
		{
			pp.Fetched("", false, FILES_MOD_DE, FILES_MOD_EN);
			return;
		}
		if (!HoldsProject(dp))
		{
			pp.Fetched("", false, OBJECTS_PLACEMENT_DE, OBJECTS_PLACEMENT_EN);
			return;
		}
		array<string> projectChunks = ProjectChunksOf(r);
		if (IsProjectMod(dp))
			TakeSettings(r.m_Json.Get("settings"), dp);
		string projectPart = "";
		DZS_DzbotSection msec = r.Find("manifest", "manifest");
		if (msec)
			projectPart = ProjectPartOf(msec.m_Chunks);
		// several maps and none in the project file: ask once, the choice goes into the project file with the next save
		array<string> maps = MapsOf(pj);
		string fixedMap = DZS_DzbotCatalog.Text(pj, "map");
		bool chose = false;
		if (fixedMap == "" && maps.Count() > 1)
		{
			string kept = ChosenMap(projectPart);
			if (DZS_ProjectFile.IsV2(projectChunks))
				kept = DZS_WorkFolder.MapOf(DZS_WorkFolder.RootOf(projectChunks));
			map<string, string> carry = DZS_Handover.Carry();
			if (m_PendMap == "" && carry.Get("dzbot.pendRef") == dp.m_Ref)
			{
				m_PendMap = carry.Get("dzbot.pendMap");
				m_PendRef = dp.m_Ref;
			}
			if (m_PendMap != "" && m_PendRef == dp.m_Ref)
			{
				dp.m_MapKey = m_PendMap;
				chose = true;
				// the choice must survive the world change that opening on another map starts
				string loadedKey = "";
				DZS_ProjectsPlugin lp = Projects();
				if (lp && !lp.IsHome())
					loadedKey = lp.LoadedMapKey();
				if (loadedKey == m_PendMap)
				{
					carry.Remove("dzbot.pendRef");
					carry.Remove("dzbot.pendMap");
				}
			}
			else if (kept != "" && maps.Find(kept) >= 0)
				dp.m_MapKey = kept;
			else
			{
				AskMap(dp, maps, r, q);
				return;
			}
		}
		m_PendMap = "";
		m_PendRef = "";
		m_PendLoad = null;
		if (dp.m_MapKey == "")
		{
			pp.Fetched("", false, "Projekt ohne Karte", "Project without a map");
			return;
		}
		if (chose)
		{
			m_PartChanged = FolderOf(dp.m_Ref);
			string mn = DZS_LootPlugin.MapName(dp.m_MapKey);
			Msg("info", "Karte " + mn + " gewählt — steht nach dem Speichern in der Projektdatei", "Map " + mn + " chosen — kept in the project file after saving", 6.0);
		}
		string folder = FolderOf(dp.m_Ref);
		DZS_Files.MakeDirs(DZS_LocalStore.Dir(folder));
		DZS_DzbotFile loadedFile = ProjectFileOf(r);
		dp.m_ProjectFile = loadedFile;
		DZS_SrcNode man = r.m_Json.Get("manifest");
		dp.m_ManifestRev = 0;
		if (man && man.IsObj())
		{
			DZS_SrcNode mrev = man.Get("revision");
			if (mrev)
				dp.m_ManifestRev = mrev.m_Str.ToInt();
		}
		// the project file of the mod or placement opens like a local one (through its working folder)
		string mapSet = "";
		if (chose)
			mapSet = dp.m_MapKey;
		// a reload keeps the camera where the user is (on the same map), not where the last saver left it
		bool reloading = m_ReloadRef != "" && m_ReloadRef == dp.m_Ref && pp.m_Open && pp.m_Open.m_Folder == folder;
		string view = "";
		if (reloading && pp.m_Open.m_MapKey == dp.m_MapKey)
			view = m_ReloadView;
		array<string> own = DZS_ProjectFile.RewriteV2(projectChunks, dp.m_Name, mapSet, view, "", true);
		if (!own && projectChunks)
		{
			// an empty project in its place would replace dzbot's project file at the next save
			DZS_Log.Warn("dzbot: project file of " + dp.m_Ref + " is no readable version 2 — not opened");
			m_ReloadRef = "";
			m_ReloadView = "";
			pp.Fetched("", false, "Die Projektdatei in dzbot ist für dieses dzStudio nicht lesbar (Format oder Version) — nicht geöffnet", "dzbot's project file is not readable for this dzStudio (format or version) — not opened");
			return;
		}
		if (!own)
			own = DZS_LocalStore.NewFile(dp.m_Name, dp.m_MapKey, "", "objects.json", DZS_Texts.Get("core.group.objects"));
		array<string> keep = new array<string>();
		RemoveStale(folder, keep);
		DZS_Files.Write(DZS_LocalStore.Path(folder, DZS_ProjectFile.NAME), own);
		string loadedHead = "dzbot: loaded project " + dp.m_Kind + " " + dp.m_Ref + " '" + dp.m_Name + "'";
		DZS_Log.Info(loadedHead + " map " + dp.m_MapKey + ", project file " + (projectChunks != null) + ", reload " + reloading);
		if (reloading)
		{
			// the local changes are given up for dzbot's state: no backup question at the reopen
			m_ReloadRef = "";
			m_ReloadView = "";
			pp.DeleteBackup();
		}
		pp.Fetched(folder, true, "", "");
	}

	// dzbot's project file in a load or poll reply (key, path, revision), null when the mod or placement has none.
	DZS_DzbotFile ProjectFileOf(DZS_DzbotReply r)
	{
		DZS_SrcNode files = r.m_Json.Get("files");
		if (!files || !files.IsArr())
			return null;
		foreach (DZS_SrcNode f : files.m_Items)
		{
			if (DZS_DzbotCatalog.Text(f, "role") != "project")
				continue;
			DZS_DzbotFile df = new DZS_DzbotFile();
			df.m_Key = DZS_DzbotCatalog.Text(f, "key");
			df.m_Path = DZS_DzbotCatalog.Text(f, "path");
			df.m_Role = "project";
			DZS_SrcNode rev = f.Get("revision");
			if (rev)
				df.m_Rev = rev.m_Str.ToInt();
			return df;
		}
		return null;
	}

	// dzbot holds a project file this editor has not opened: another one, or a newer revision of the same.
	static bool IsNewer(DZS_DzbotProject dp, DZS_DzbotFile cur)
	{
		if (!cur)
			return false;
		DZS_DzbotFile held = dp.m_ProjectFile;
		if (!held || held.m_Key != cur.m_Key)
			return true;
		return cur.m_Rev > held.m_Rev;
	}

	DZS_DzbotProject ProjectOf(string folder)
	{
		return m_Projects.Get(RefOf(folder));
	}

	// mirror folder whose project file got the map choice (opens with 1 unsaved change so the choice can be saved)
	string m_PartChanged;
	protected ref DZS_DzbotReply m_PendLoad;
	protected ref DZS_DzbotAuthReq m_PendQ;
	protected string m_PendMap;
	protected string m_PendRef;
	protected ref array<string> m_PendMaps;

	static array<string> MapsOf(DZS_SrcNode p)
	{
		array<string> r = new array<string>();
		DZS_SrcNode maps = p.Get("maps");
		if (!maps || !maps.IsArr())
			return r;
		foreach (DZS_SrcNode mn : maps.m_Items)
		{
			if (mn.IsStr() && DZS_Maps.Available(mn.m_Str))
				r.Insert(mn.m_Str);
		}
		return r;
	}

	// plugins.dzbot.data.map of the project part, "" when not chosen yet
	static string ChosenMap(string part)
	{
		if (part == "")
			return "";
		array<string> chunks = DZS_Files.Chunk(part);
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
			return "";
		DZS_SrcNode pl = doc.m_Root.Get("plugins");
		if (!pl || !pl.IsObj())
			return "";
		DZS_SrcNode mine = pl.Get("dzbot");
		if (!mine || !mine.IsObj())
			return "";
		DZS_SrcNode data = mine.Get("data");
		if (!data || !data.IsObj())
			return "";
		return DZS_DzbotCatalog.Text(data, "map");
	}

	protected void AskMap(DZS_DzbotProject dp, array<string> maps, DZS_DzbotReply r, DZS_DzbotAuthReq q)
	{
		m_PendLoad = r;
		m_PendQ = q;
		m_PendRef = dp.m_Ref;
		m_PendMaps = maps;
		string loaded = "";
		DZS_ProjectsPlugin pp = Projects();
		if (pp && !pp.IsHome())
			loaded = pp.LoadedMapKey();
		if (maps.Find(loaded) < 0)
			loaded = maps[0];
		string de = "„" + dp.m_Name + "“ gilt für mehrere Karten. Mit welcher Karte soll das Projekt öffnen? Die Wahl steht nach dem Speichern in der Projektdatei (für alle Bearbeiter).";
		string en = "\"" + dp.m_Name + "\" applies to several maps. Which map should the project open with? The choice is kept in the project file once saved (for every editor).";
		string kick = "dzbot";
		string ttl = DZS_Texts.Pick("Karte wählen", "Choose a map");
		string txt = DZS_Texts.Pick(de, en);
		DZS_LootTextDialog d = new DZS_LootTextDialog(kick, ttl, txt);
		d.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		foreach (string m : maps)
		{
			string kind = "side";
			if (m == loaded)
				kind = "main";
			string label = DZS_LootPlugin.MapName(m);
			d.AddFootButton("map:" + m, label, kind);
		}
		d.m_OnResult.Insert(OnMapAnswer);
		d.Open();
		DZS_Log.Info("dzbot: map asked for " + dp.m_Ref + " (" + maps.Count() + " maps, preselected " + loaded + ")");
	}

	void OnMapAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("dzbot: map answer " + result);
		DZS_DzbotReply r = m_PendLoad;
		DZS_DzbotAuthReq q = m_PendQ;
		m_PendLoad = null;
		m_PendQ = null;
		if (result.IndexOf("map:") != 0 || !r)
		{
			m_PendMap = "";
			m_PendRef = "";
			DZS_Handover.Carry().Remove("dzbot.pendRef");
			DZS_Handover.Carry().Remove("dzbot.pendMap");
			DZS_ProjectsPlugin pp = Projects();
			if (pp)
				pp.Fetched("", false, "Öffnen abgebrochen", "Opening cancelled");
			return;
		}
		m_PendMap = result.Substring(4, result.Length() - 4);
		DZS_Handover.Carry().Set("dzbot.pendRef", m_PendRef);
		DZS_Handover.Carry().Set("dzbot.pendMap", m_PendMap);
		OnLoad(r, q);
	}

	// The manifest is the project part; a manifest that is a whole project file gives its "project" member.
	protected string ProjectPartOf(array<string> chunks)
	{
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
			return "";
		DZS_SrcNode part = doc.m_Root.Get("project");
		if (part && part.IsObj())
			return doc.Raw(part);
		DZS_SrcNode groups = doc.m_Root.Get("groups");
		if (groups)
			return doc.Raw(doc.m_Root);
		return "";
	}

	protected void RemoveStale(string folder, array<string> keep)
	{
		array<string> names;
		array<bool> dirs;
		DZS_Files.List(DZS_LocalStore.Dir(folder), names, dirs);
		for (int i = 0; i < names.Count(); i++)
		{
			if (dirs[i])
				continue;
			string n = names[i];
			string low = n;
			low.ToLower();
			if (low.IndexOf(".json") < 0 || n == DZS_ProjectFile.NAME || keep.Find(n) >= 0)
				continue;
			DeleteFile(DZS_LocalStore.Path(folder, n));
		}
	}

	// Base revisions of the open project for the backup head, so a restored project polls and saves against them.
	DZS_Json BackupState(string folder)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (!dp)
			return null;
		DZS_Json st = DZS_Json.Obj();
		st.Set("ref", DZS_Json.Str(dp.m_Ref));
		st.Set("kind", DZS_Json.Str(dp.m_Kind));
		st.Set("modKind", DZS_Json.Str(dp.m_ModKind));
		st.Set("placementKind", DZS_Json.Str(dp.m_PlacementKind));
		st.Set("name", DZS_Json.Str(dp.m_Name));
		st.Set("map", DZS_Json.Str(dp.m_MapKey));
		st.Set("manifest", DZS_Json.Int(dp.m_ManifestRev));
		st.Set("manageUrl", DZS_Json.Str(dp.m_ManageUrl));
		DZS_DzbotFile pf = dp.m_ProjectFile;
		if (pf)
		{
			DZS_Json pj = DZS_Json.Obj();
			pj.Set("key", DZS_Json.Str(pf.m_Key));
			pj.Set("path", DZS_Json.Str(pf.m_Path));
			pj.Set("rev", DZS_Json.Int(pf.m_Rev));
			st.Set("project", pj);
		}
		if (IsProjectMod(dp) && dp.m_SettingsList)
		{
			DZS_Json stList = dp.m_SettingsList.Clone();
			st.Set("settings", stList);
		}
		return st;
	}

	void Restored(string folder, DZS_Json st)
	{
		string pref = RefOf(folder);
		if (m_Projects.Get(pref) || !st || st.GetStr("ref") != pref)
			return;
		DZS_DzbotProject dp = new DZS_DzbotProject();
		dp.m_Ref = pref;
		dp.m_Kind = st.GetStr("kind");
		dp.m_ModKind = st.GetStr("modKind");
		dp.m_PlacementKind = st.GetStr("placementKind");
		dp.m_Name = st.GetStr("name");
		dp.m_MapKey = st.GetStr("map");
		dp.m_ManifestRev = st.GetInt("manifest");
		dp.m_ManageUrl = st.GetStr("manageUrl");
		DZS_Json pj = st.Get("project");
		if (pj)
		{
			DZS_DzbotFile pf = new DZS_DzbotFile();
			pf.m_Key = pj.GetStr("key");
			pf.m_Path = pj.GetStr("path");
			pf.m_Rev = pj.GetInt("rev");
			pf.m_Role = "project";
			dp.m_ProjectFile = pf;
		}
		DZS_Json sts = st.Get("settings");
		if (sts && sts.IsArr())
		{
			DZS_Json stsCopy = sts.Clone();
			KeepSettings(stsCopy, dp);
		}
		m_Projects.Set(pref, dp);
		// a backup of a file mod or of a placement on its object list (from before) stays readable; saving it to dzbot
		// is refused (Save), Save as … keeps it locally
		DZS_Log.Info("dzbot: restored " + pref + " at revision " + dp.Revision() + ", project " + HoldsProject(dp));
	}

	void Opened(string folder)
	{
		m_OpenFolder = folder;
		m_PollIn = POLL_EVERY;
		m_AskedRev = -1;
		PlateFor(folder);
	}

	void Closed(string folder)
	{
		if (m_OpenFolder == folder)
			m_OpenFolder = "";
		m_Client.Drop("poll");
		m_Polling = false;
	}

	protected void PlateFor(string folder)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		string rev = "";
		if (dp)
			rev = dp.Revision().ToString();
		if (Outdated())
		{
			m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("dzbot · dzStudio veraltet", "dzbot · dzStudio out of date"), "danger");
			return;
		}
		if (!IsConnected())
		{
			m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("dzbot · getrennt", "dzbot · disconnected"), "danger");
			return;
		}
		if (rev == "")
		{
			m_Kernel.GetScreen().SetProviderPlate("dzbot", "success");
			return;
		}
		m_Kernel.GetScreen().SetProviderPlate("dzbot · Rev. " + rev, "success");
	}

	// ---- polling: not while saving, never two at once ----

	protected void UpdatePoll(float dt)
	{
		if (m_OpenFolder == "" || !IsConnected() || m_Save || m_Polling || m_Asking || Outdated())
			return;
		m_PollIn -= dt;
		if (m_PollIn > 0)
			return;
		m_PollIn = POLL_EVERY;
		DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
		if (!dp)
			return;
		// the project file held, so dzbot answers "changed" (with the content) only when it has a newer one
		string held = "";
		DZS_DzbotFile pf = dp.m_ProjectFile;
		if (pf)
			held = "{\"key\":" + DZS_DzbotFrame.Q(pf.m_Key) + ",\"revision\":" + pf.m_Rev + "}";
		string fields = "\"project\":" + DZS_DzbotFrame.Q(dp.m_Ref) + ",\"files\":[" + held + "],\"manifest\":" + dp.m_ManifestRev;
		m_Polling = true;
		AuthPost("poll", "poll", "/editor/project/revisions", fields);
	}

	protected void OnPoll(DZS_DzbotReply r)
	{
		m_Polling = false;
		if (m_Save || m_OpenFolder == "")
			return;
		if (!r.m_Framed)
		{
			m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("dzbot · nicht erreichbar", "dzbot · not reachable"), "danger");
			DZS_Log.Warn("dzbot: poll: not reachable (" + r.m_Fail + ")");
			return;
		}
		if (!r.Ok())
		{
			DZS_Log.Warn("dzbot: poll: " + r.Error() + " " + r.Message());
			if (r.Error() == "not_found" || r.Error() == "forbidden")
				Msg("danger", "dzbot: Projekt nicht mehr da oder kein Zugriff — lokal speichern mit Speichern unter …", "dzbot: project gone or no access — save locally with Save as …", 8.0);
			return;
		}
		DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
		if (!dp)
			return;
		PlateFor(m_OpenFolder);
		if (IsProjectMod(dp))
			TakeSettings(r.m_Json.Get("settings"), dp);
		DZS_DzbotFile cur = ProjectFileOf(r);
		if (IsNewer(dp, cur))
			OnNewerInDzbot(dp, cur);
		DZS_Log.Info("dzbot: poll done");
	}

	// dzbot has a newer project file: without local changes it is reopened at once; with them the conflict question
	// (once per revision). Not in the middle of an open, a dialog, a gesture or a shared session — the next poll again.
	protected void OnNewerInDzbot(DZS_DzbotProject dp, DZS_DzbotFile cur)
	{
		DZS_ProjectsPlugin pp = Projects();
		DZS_Instance inst = m_Kernel.GetShown();
		if (!pp || !inst || !pp.m_Open || pp.IsOpening() || m_Asking || m_ReloadRef != "")
			return;
		if (m_Kernel.GetScreen().HasDialog() || m_Kernel.IsGestureRunning())
			return;
		DZS_SaveRules rules = DZS_SaveRules.Get();
		if (rules && rules.StateSeq(inst) >= 0)
			return;
		DZS_Log.Info("dzbot: poll: project file " + cur.m_Key + " revision " + cur.m_Rev + " in dzbot, unsaved here " + inst.IsUnsaved());
		if (!inst.IsUnsaved())
		{
			StartReload(dp);
			return;
		}
		if (m_AskedRev == cur.m_Rev)
			return;
		m_AskedRev = cur.m_Rev;
		array<string> paths = new array<string>();
		paths.Insert(cur.m_Path);
		AskConflict(paths);
	}

	protected void AskConflict(array<string> paths)
	{
		if (m_Asking)
			return;
		m_Asking = true;
		string list = "";
		foreach (string p : paths)
		{
			if (list != "")
				list += ", ";
			list += p;
		}
		string de = "In dzbot geändert, während hier ungespeicherte Änderungen offen sind: " + list;
		de += ". Neu laden öffnet den Stand aus dzbot und verwirft die Änderungen hier; mit Meine behalten ersetzt das nächste Speichern den Stand in dzbot, auch dort geänderte Teile, die dzStudio nur anzeigt (außerhalb von dzStudio angelegt).";
		string en = "Changed in dzbot while there are unsaved changes here: " + list;
		en += ". Reload opens dzbot's state and discards the changes here; with Keep mine the next save replaces dzbot's state, including changes made there to parts dzStudio only shows (made outside dzStudio).";
		DZS_ConfirmDialog c = new DZS_ConfirmDialog("dzbot", DZS_Texts.Pick("In dzbot geändert", "Changed in dzbot"), DZS_Texts.Pick(de, en));
		c.AddFootButton("copy", DZS_Texts.Pick("Als Konfliktkopie speichern", "Save as conflict copy"), "side");
		c.AddFootButton("keep", DZS_Texts.Pick("Meine behalten", "Keep mine"), "side");
		c.AddFootButton("reload", DZS_Texts.Pick("Neu laden", "Reload"), "danger");
		c.m_OnResult.Insert(OnConflictAnswer);
		c.Open();
		DZS_Log.Info("dzbot: conflict question: " + list);
	}

	void OnConflictAnswer(DZS_Dialog d, string result)
	{
		m_Asking = false;
		DZS_Log.Info("dzbot: conflict answer " + result);
		DZS_ProjectsPlugin pp = Projects();
		DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
		if (result == "reload" && dp)
			StartReload(dp);
		else if (result == "keep" && dp)
		{
			// dzbot's current revision becomes the base: the next save replaces dzbot's state with this one
			string fields = "\"project\":" + DZS_DzbotFrame.Q(dp.m_Ref);
			AuthPost("main", "keep", "/editor/project/load", fields);
		}
		else if (result == "copy" && pp && pp.m_Open)
		{
			string name = pp.m_Open.m_Name + DZS_Texts.Pick(" (Konfliktkopie)", " (conflict copy)");
			pp.StartSave(false, "", name);
		}
	}

	// "Reload": dzbot's project file is fetched and the project reopened from it through the normal open (working
	// folder, sliced, with progress); the local changes are given up, the camera stays where the user is.
	protected void StartReload(DZS_DzbotProject dp)
	{
		DZS_ProjectsPlugin pp = Projects();
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_SaveRules rules = DZS_SaveRules.Get();
		if (inst && rules && rules.StateSeq(inst) >= 0)
		{
			Msg("danger", "Neu laden geht nicht während einer gemeinsamen Sitzung", "Reloading is not possible during a shared session", 6.0);
			return;
		}
		m_ReloadRef = dp.m_Ref;
		m_ReloadView = "";
		if (pp && pp.m_Open && pp.m_Open.m_MapKey == dp.m_MapKey)
			m_ReloadView = pp.CameraView();
		DZS_Log.Info("dzbot: reloading " + dp.m_Ref + " from dzbot");
		m_Kernel.GetScreen().ShowMessage("dzbot", "info", DZS_W.Literal(DZS_Texts.Pick("Lädt aus dzbot …", "Loading from dzbot …")), 10.0);
		string fields = "\"project\":" + DZS_DzbotFrame.Q(dp.m_Ref);
		AuthPost("main", "reload", "/editor/project/load", fields);
	}

	protected void OnReload(DZS_DzbotReply r, DZS_DzbotAuthReq q)
	{
		string pref = m_ReloadRef;
		if (pref == "" || m_OpenFolder != FolderOf(pref))
		{
			// the project was closed meanwhile: nothing to reopen
			m_ReloadRef = "";
			m_ReloadView = "";
			m_Kernel.GetScreen().HideMessage("dzbot");
			return;
		}
		if (!r.m_Framed || !r.Ok())
		{
			m_ReloadRef = "";
			m_ReloadView = "";
			m_Kernel.GetScreen().HideMessage("dzbot");
			string why = r.Message();
			if (why == "")
				why = r.Error();
			if (why == "")
				why = r.m_Fail;
			Msg("danger", "Neu laden fehlgeschlagen: " + why, "Reloading failed: " + why, 6.0);
			return;
		}
		// the same steps as opening; a refusal there (file mod, shared session …) leaves the project open as it is
		OnLoad(r, q);
		if (m_ReloadRef == "")
			Msg("info", "Aus dzbot neu geladen", "Reloaded from dzbot");
		m_ReloadRef = "";
		m_ReloadView = "";
	}

	// "Keep mine": the revision of dzbot's project file becomes the base of the next save, the local state stays.
	protected void OnKeep(DZS_DzbotReply r)
	{
		if (!r.m_Framed || !r.Ok() || m_OpenFolder == "")
		{
			string why = r.Message();
			if (why == "")
				why = r.m_Fail;
			Msg("danger", "dzbot nicht erreichbar: " + why, "dzbot not reachable: " + why, 6.0);
			return;
		}
		DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
		if (!dp)
			return;
		DZS_DzbotFile cur = ProjectFileOf(r);
		if (cur)
		{
			dp.m_ProjectFile = cur;
			m_AskedRev = cur.m_Rev;
		}
		PlateFor(m_OpenFolder);
		DZS_Log.Info("dzbot: keeping the local state on dzbot's revision " + dp.Revision());
		Msg("info", "Deine Fassung bleibt — das nächste Speichern ersetzt den Stand in dzbot", "Your version stays — the next save replaces dzbot's state", 6.0);
	}

	// ---- save ----

	void Save(string folder, array<ref DZS_SaveFile> files)
	{
		DZS_ProjectsPlugin pp = Projects();
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (StopOutdated())
		{
			pp.ProviderSaved("dzStudio ist veraltet — lokal speichern mit Speichern unter …", "dzStudio is out of date — save locally with Save as …");
			return;
		}
		if (!dp || !IsConnected())
		{
			pp.ProviderSaved("Nicht mit dzbot verbunden — lokal speichern mit Speichern unter …", "Not connected with dzbot — save locally with Save as …");
			return;
		}
		// only a project mod or a placement built as a project is saved here; a file mod or a placement still on its
		// object list (a backup from before) is not
		if (IsFilesMod(dp))
		{
			pp.ProviderSaved(FILES_MOD_DE + " — lokal speichern mit Speichern unter …", FILES_MOD_EN + " — save locally with Save as …");
			return;
		}
		if (!HoldsProject(dp))
		{
			pp.ProviderSaved(OBJECTS_PLACEMENT_DE + " — lokal speichern mit Speichern unter …", OBJECTS_PLACEMENT_EN + " — save locally with Save as …");
			return;
		}
		DZS_DzbotSave sv = new DZS_DzbotSave();
		sv.m_Folder = folder;
		sv.m_Files = files;
		BeginSave(dp, sv, files);
	}

	// A local project's file into a dzbot project mod (DZS_DzbotUpload), not into the open project: it replaces
	// dp.m_ProjectFile, or creates dzstudio/project.json when that is null. false while another save runs.
	bool SaveUpload(DZS_DzbotProject dp, array<string> chunks, string mapKey)
	{
		if (m_Save)
			return false;
		DZS_DzbotSave sv = new DZS_DzbotSave();
		sv.m_Folder = FolderOf(dp.m_Ref);
		sv.m_Upload = true;
		sv.m_Map = mapKey;
		sv.m_Files = new array<ref DZS_SaveFile>();
		array<ref DZS_SaveFile> files = new array<ref DZS_SaveFile>();
		DZS_SaveFile pf = new DZS_SaveFile();
		pf.m_Rel = DZS_ProjectFile.NAME;
		pf.m_Chunks = chunks;
		files.Insert(pf);
		BeginSave(dp, sv, files);
		return true;
	}

	protected void BeginSave(DZS_DzbotProject dp, DZS_DzbotSave sv, array<ref DZS_SaveFile> files)
	{
		sv.m_SaveId = DZS_Random.Code(24);
		// the project file (format 2) is the one part; it holds no manifest part of its own (a "project" member was
		// format 1), so no manifest is sent and the file is not parsed for one
		ProjectPart(dp, sv, files);
		sv.m_Started = m_Now;
		sv.m_HashStart = TickCount(0);
		m_Save = sv;
		m_Client.m_SaveOpen = true;
		m_Client.Drop("poll");
		m_Polling = false;
		if (!sv.m_Upload)
			m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("dzbot · speichert …", "dzbot · saving …"), "warning");
		DZS_Log.Info("dzbot: save " + sv.m_SaveId + " of " + dp.m_Ref + ", " + sv.m_Parts.Count() + " section(s), upload " + sv.m_Upload + ", checksums first");
		HashSave();
	}

	// The end of a save: to the projects plugin, or to the upload it belongs to.
	protected void Report(DZS_DzbotSave sv, string de, string en)
	{
		if (sv.m_Upload)
			m_Upload.Saved(de, en);
		else
			Projects().ProviderSaved(de, en);
	}

	protected void PlateAfter(DZS_DzbotSave sv)
	{
		if (!sv.m_Upload)
			PlateFor(sv.m_Folder);
	}

	// SHA-256 of every part in slices of a few milliseconds per frame (DayZ 1.30: about 1 s per MB), then the request.
	protected void HashSave()
	{
		DZS_DzbotSave sv = m_Save;
		if (!sv || sv.m_Fields != "")
			return;
		if (!sv.m_Blocked && TickCount(0) - sv.m_HashStart > 10000000)
		{
			sv.m_Blocked = true;
			m_Kernel.GetScreen().ShowMessage("dzbot.saving", "info", DZS_W.Literal(DZS_Texts.Pick("Speichern … (Prüfsummen für dzbot)", "Saving … (checksums for dzbot)")), 60.0);
		}
		while (sv.m_HashAt < sv.m_Parts.Count())
		{
			if (!sv.m_Hash)
				sv.m_Hash = new DZS_Sha256();
			DZS_DzbotSection sec = sv.m_Parts[sv.m_HashAt];
			if (!sv.m_Hash.Feed(sec.m_Chunks, HASH_MS))
				return;
			string sum = sv.m_Hash.Finish();
			sv.m_Sums.Insert(sum);
			DZS_Log.Info("dzbot: sha256 " + sec.m_Kind + " " + sec.m_Key + " " + sum);
			sv.m_Hash = null;
			sv.m_HashAt++;
		}
		if (!sv.m_SumsDone)
		{
			while (sv.m_SumAt < sv.m_Files.Count())
			{
				DZS_SaveFile sf = sv.m_Files[sv.m_SumAt];
				if (!sf.m_Delete && sf.m_Chunks)
				{
					if (!sv.m_Sum)
						sv.m_Sum = new DZS_FileSum();
					if (!sv.m_Sum.Feed(sf.m_Chunks, HASH_MS))
						return;
					sf.m_Sum = sv.m_Sum.Result();
					sv.m_Sum = null;
				}
				sv.m_SumAt++;
			}
			sv.m_SumsDone = true;
			return;
		}
		string bad;
		if (GetCLIParam("dzsDzbotBadSum", bad) && !s_BadSumDone && sv.m_Sums.Count() > 0)
		{
			// developer check of checksum_mismatch: the first file's checksum as if one byte had changed on the way
			s_BadSumDone = true;
			for (int b = 0; b < sv.m_Parts.Count(); b++)
			{
				if (sv.m_Parts[b].m_Kind != "file")
					continue;
				string was = sv.m_Sums[b];
				string first = was.Substring(0, 1);
				string flip = "0";
				if (first == "0")
					flip = "1";
				sv.m_Sums[b] = flip + was.Substring(1, was.Length() - 1);
				DZS_Log.Warn("dzbot: -dzsDzbotBadSum: checksum of part " + b + " changed to " + sv.m_Sums[b]);
				break;
			}
		}
		string list = "";
		for (int i = 0; i < sv.m_Parts.Count(); i++)
		{
			string qs = DZS_DzbotFrame.Q(sv.m_Sums[i]);
			string withSum = sv.m_Entries[i] + ",\"checksum\":" + qs + "}";
			if (list != "")
				list += ",";
			list += withSum;
		}
		DZS_DzbotProject dp = m_Projects.Get(RefOf(sv.m_Folder));
		string qp = DZS_DzbotFrame.Q(dp.m_Ref);
		string qi = DZS_DzbotFrame.Q(sv.m_SaveId);
		// the project file is no merged target: "merged" stays empty
		sv.m_Fields = "\"project\":" + qp + ",\"saveId\":" + qi + ",\"files\":[" + list + "],\"merged\":[]";
		string saveMap = dp.m_MapKey;
		if (sv.m_Map != "")
			saveMap = sv.m_Map;
		if (saveMap != "")
			sv.m_Fields += ",\"map\":" + DZS_DzbotFrame.Q(saveMap);
		int ms = (TickCount(0) - sv.m_HashStart) / 10000;
		DZS_Log.Info("dzbot: checksums of " + sv.m_Parts.Count() + " part(s) in " + ms + " ms");
		array<string> pieces = DZS_DzbotFrame.TailPieces(sv.m_Parts);
		sv.m_Join = new DZS_Joiner(pieces);
		sv.m_HashStart = TickCount(0);
	}

	// the sections of the request joined into one string over several frames, then the request
	protected void JoinSave()
	{
		DZS_DzbotSave sv = m_Save;
		if (!sv.m_Joined)
		{
			sv.m_Joined = sv.m_Join.Step(HASH_MS);
			return;
		}
		sv.m_Tail = sv.m_Join.Result();
		sv.m_Join = null;
		int ms = (TickCount(0) - sv.m_HashStart) / 10000;
		DZS_Log.Info("dzbot: request of " + sv.m_Tail.Length() + " bytes joined in " + ms + " ms");
		sv.m_Started = m_Now;
		int t0 = TickCount(0);
		SendSave();
		DZS_Log.Info("dzbot: save handed to the request in " + ((TickCount(0) - t0) / 10000) + " ms");
	}

	// A project mod or project placement holds the project as building blocks: the saved project file (blocks and the
	// extensions — the editor part and the dzbot rules among them) replaces dzstudio/project.json, for a placement its
	// one file (key "placement").
	protected void ProjectPart(DZS_DzbotProject dp, DZS_DzbotSave sv, array<ref DZS_SaveFile> files)
	{
		DZS_DzbotFile pf = dp.m_ProjectFile;
		array<string> chunks = null;
		foreach (DZS_SaveFile f : files)
		{
			if (!f.m_Delete && f.m_Rel == DZS_ProjectFile.NAME)
				chunks = f.m_Chunks;
		}
		if (!chunks)
		{
			DZS_Log.Warn("dzbot: project " + dp.m_Ref + ": no project file in the save, nothing of the project saved");
			return;
		}
		if (!pf)
		{
			// the mod has no project file yet: this save creates it
			string key = NewKey();
			DZS_DzbotSection cs = DZS_DzbotSection.FromChunks("file", key, "0", chunks);
			sv.m_Parts.Insert(cs);
			sv.m_Entries.Insert("{\"key\":" + DZS_DzbotFrame.Q(key) + ",\"path\":" + DZS_DzbotFrame.Q(PROJECT_PATH) + ",\"type\":\"replace\",\"create\":true");
			sv.m_PathOf.Set(key, PROJECT_PATH);
			sv.m_NewProjectKey = key;
			DZS_Log.Info("dzbot: save creates " + PROJECT_PATH + ", " + DZS_Files.Length(chunks) + " bytes");
			return;
		}
		string rv = pf.m_Rev.ToString();
		DZS_DzbotSection ps = DZS_DzbotSection.FromChunks("file", pf.m_Key, rv, chunks);
		sv.m_Parts.Insert(ps);
		sv.m_Entries.Insert("{\"key\":" + DZS_DzbotFrame.Q(pf.m_Key) + ",\"revision\":" + pf.m_Rev);
		sv.m_PathOf.Set(pf.m_Key, pf.m_Path);
		DZS_Log.Info("dzbot: save sends the project file, " + DZS_Files.Length(chunks) + " bytes");
	}

	static string NewKey()
	{
		DZS_Random.EnsureSeeded();
		string hex = "";
		string digits = "0123456789abcdef";
		for (int i = 0; i < 32; i++)
		{
			int v = Math.RandomInt(0, 16);
			hex += digits.Get(v);
		}
		string a = hex.Substring(0, 8);
		string b = hex.Substring(8, 4);
		string c = hex.Substring(12, 4);
		string d = hex.Substring(16, 4);
		string e = hex.Substring(20, 12);
		return a + "-" + b + "-4" + c.Substring(1, 3) + "-a" + d.Substring(1, 3) + "-" + e;
	}

	protected void SendSave()
	{
		m_Save.m_Tries++;
		AuthPost("main", "save", "/editor/project/save", m_Save.m_Fields, null, null, m_Save.m_Tail);
	}

	protected void UpdateSave(float dt)
	{
		if (m_Save && m_Save.m_Fields == "")
		{
			HashSave();
			return;
		}
		if (m_Save && m_Save.m_Join)
		{
			JoinSave();
			return;
		}
		if (!m_Save || m_SaveRetryIn <= 0)
			return;
		m_SaveRetryIn -= dt;
		if (m_SaveRetryIn > 0)
			return;
		if (m_Now - m_Save.m_Started > SAVE_GIVE_UP)
		{
			DZS_Log.Warn("dzbot: save " + m_Save.m_SaveId + " unclear after " + m_Save.m_Tries + " tries");
			DZS_DzbotSave unclear = m_Save;
			EndSave();
			Report(unclear, "Speichern unklar — der Stand in dzbot wird geprüft", "Saving unclear — checking dzbot's state");
			// a poll at once: a newer project file in dzbot asks (local changes) or reopens; none keeps the local state
			m_PollIn = 0;
			return;
		}
		DZS_Log.Info("dzbot: asking again for save " + m_Save.m_SaveId);
		SendSave();
	}

	// longest frames from the start of a dzbot save until 30 frames after its end (log only)
	protected void WatchFrame()
	{
		int now = TickCount(0);
		int ms = (now - m_WatchTick) / 10000;
		m_WatchTick = now;
		if (m_Save && m_WatchLeft < 0)
		{
			m_WatchLeft = 30;
			m_WatchFrames = 0;
			m_WatchWorst = 0;
			m_WatchOver = 0;
			m_WatchPhase = "start";
			return;
		}
		if (m_WatchLeft < 0)
			return;
		m_WatchFrames++;
		if (ms > 50)
		{
			m_WatchOver++;
			DZS_Log.Info("dzbot: save frame " + m_WatchFrames + " took " + ms + " ms (after " + m_WatchPhase + ")");
		}
		if (ms > m_WatchWorst)
		{
			m_WatchWorst = ms;
			m_WatchWorstPhase = m_WatchPhase;
		}
		if (m_Save)
		{
			m_WatchPhase = "send";
			if (m_Save.m_Fields == "")
				m_WatchPhase = "checksums";
			else if (m_Save.m_Join)
				m_WatchPhase = "join";
			return;
		}
		m_WatchPhase = "end";
		m_WatchLeft--;
		if (m_WatchLeft > 0)
			return;
		m_WatchLeft = -1;
		DZS_Log.Info("dzbot: save frames " + m_WatchFrames + ", longest " + m_WatchWorst + " ms (after " + m_WatchWorstPhase + "), over 50 ms: " + m_WatchOver);
	}

	protected void EndSave()
	{
		m_Kernel.GetScreen().HideMessage("dzbot.saving");
		m_Save = null;
		m_SaveRetryIn = 0;
		m_Client.m_SaveOpen = false;
		m_PollIn = POLL_EVERY;
		m_Client.Pump();
	}

	protected void OnSaved(DZS_DzbotReply r)
	{
		DZS_DzbotSave sv = m_Save;
		if (!sv)
			return;
		string err = r.Error();
		if (r.m_TimedOut || (r.m_Framed && (err == "running" || r.Str("state") == "running")))
		{
			DZS_Log.Info("dzbot: save " + sv.m_SaveId + " " + err + " — asking again in 3 s");
			m_SaveRetryIn = SAVE_ASK_EVERY;
			return;
		}
		if (!r.m_Framed)
		{
			EndSave();
			PlateAfter(sv);
			if (!sv.m_Upload)
				m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("dzbot · nicht erreichbar", "dzbot · not reachable"), "danger");
			Report(sv, "dzbot nicht erreichbar", "dzbot not reachable");
			return;
		}
		DZS_DzbotProject dp = m_Projects.Get(RefOf(sv.m_Folder));
		if (!r.Ok())
		{
			EndSave();
			PlateAfter(sv);
			if (err == "client_outdated")
			{
				Report(sv, "dzStudio ist veraltet — nichts gespeichert", "dzStudio is out of date — nothing saved");
				return;
			}
			if (err == "conflict")
			{
				array<string> paths = new array<string>();
				DZS_SrcNode cs = r.m_Json.Get("conflicts");
				if (cs && cs.IsArr())
				{
					foreach (DZS_SrcNode cn : cs.m_Items)
					{
						string ck = DZS_DzbotCatalog.Text(cn, "key");
						DZS_DzbotFile cf = null;
						if (dp)
							cf = dp.FileByKey(ck);
						if (cf)
							paths.Insert(cf.m_Path);
						else
							paths.Insert(ck);
					}
				}
				Report(sv, "In dzbot geändert — nichts gespeichert", "Changed in dzbot — nothing saved");
				if (!sv.m_Upload)
					AskConflict(paths);
				return;
			}
			if (err == "checksum_mismatch")
			{
				string bad = "";
				DZS_SrcNode mm = r.m_Json.Get("mismatches");
				if (mm && mm.IsArr() && mm.m_Items.Count() > 0)
				{
					DZS_SrcNode first = mm.m_Items[0];
					bad = DZS_DzbotCatalog.Text(first, "name");
					string path;
					if (sv.m_PathOf.Find(bad, path))
						bad = path;
					string mk = DZS_DzbotCatalog.Text(first, "kind");
					string me = DZS_DzbotCatalog.Text(first, "expected");
					string ma = DZS_DzbotCatalog.Text(first, "actual");
					DZS_Log.Warn("dzbot: checksum mismatch " + mk + " " + bad + " expected " + me + " actual " + ma);
					if (mk == "merged" && ma == "unsupported")
					{
						Report(sv, "dzbot kann den Stand von " + bad + " nicht zusammensetzen — nichts gespeichert", "dzbot cannot compose " + bad + " — nothing saved");
						return;
					}
				}
				Report(sv, bad + " kam in dzbot nicht richtig an", bad + " did not arrive correctly in dzbot");
				return;
			}
			string m = r.Message();
			Report(sv, "dzbot: " + m, "dzbot: " + m);
			return;
		}
		if (!dp)
		{
			DZS_Log.Warn("dzbot: saved " + sv.m_SaveId + ", but its project is no longer listed");
			EndSave();
			PlateAfter(sv);
			Report(sv, "", "");
			return;
		}
		if (sv.m_NewProjectKey != "")
		{
			DZS_DzbotFile npf = new DZS_DzbotFile();
			npf.m_Key = sv.m_NewProjectKey;
			npf.m_Path = PROJECT_PATH;
			npf.m_Role = "project";
			dp.m_ProjectFile = npf;
		}
		DZS_SrcNode revs = r.m_Json.Get("revisions");
		if (revs && revs.IsArr())
		{
			foreach (DZS_SrcNode rn : revs.m_Items)
			{
				string key = DZS_DzbotCatalog.Text(rn, "key");
				DZS_SrcNode rv = rn.Get("revision");
				if (!rv)
					continue;
				int rev = rv.m_Str.ToInt();
				if (key == "manifest")
					dp.m_ManifestRev = rev;
				else
				{
					DZS_DzbotFile f = dp.FileByKey(key);
					if (f)
						f.m_Rev = rev;
				}
			}
		}
		string written = "";
		DZS_SrcNode wl = r.m_Json.Get("written");
		if (wl && wl.IsArr())
			written = wl.Count().ToString();
		DZS_Log.Info("dzbot: saved " + sv.m_SaveId + " after " + sv.m_Tries + " try/tries, written " + written + ", replayed " + r.Str("replayed") + ", revision " + dp.Revision());
		EndSave();
		PlateAfter(sv);
		Report(sv, "", "");
	}
}
