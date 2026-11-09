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
		return true;
	}

	override bool IsActionListed(string actionId)
	{
		if (actionId == "dzbot.friends")
			return m_P.IsDzbotSession();
		if (actionId == "dzbot.rules")
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
	// a mod's kind in dzbot: "files", or "project" — a building-block project saved as one file, dzstudio/project.json
	string m_ModKind;
	ref DZS_DzbotFile m_ProjectFile;
	// a project mod's settings, for the rules (DZS_DzbotRules)
	ref array<ref DZS_DzbotSetting> m_Settings = new array<ref DZS_DzbotSetting>();
	ref array<ref DZS_DzbotFile> m_Files = new array<ref DZS_DzbotFile>();
	int m_ManifestRev;
	ref DZS_DzbotLootState m_Loot;

	DZS_DzbotFile FileByPath(string path)
	{
		foreach (DZS_DzbotFile f : m_Files)
		{
			if (f.m_Path == path)
				return f;
		}
		return null;
	}

	DZS_DzbotFile FileByKey(string key)
	{
		if (m_ProjectFile && m_ProjectFile.m_Key == key)
			return m_ProjectFile;
		foreach (DZS_DzbotFile f : m_Files)
		{
			if (f.m_Key == key)
				return f;
		}
		return null;
	}

	int Revision()
	{
		int r = m_ManifestRev;
		foreach (DZS_DzbotFile f : m_Files)
		{
			if (f.m_Rev > r)
				r = f.m_Rev;
		}
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
	ref array<string> m_NewPaths = new array<string>();
	ref array<string> m_NewKeys = new array<string>();
	float m_Started;
	int m_Tries;
	// checksums: per part its entry text without the closing brace, its path for merged ("" =
	// none) and the SHA-256 once fed
	ref array<string> m_Entries = new array<string>();
	ref array<string> m_Targets = new array<string>();
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
	string m_ManifestEntry;
	int m_HashStart;
	// key -> path, for naming a file in checksum_mismatch
	ref map<string, string> m_PathOf = new map<string, string>();
	// loot: the written working states, their comparison forms, the change list and written types
	ref map<string, ref array<string>> m_LootIn = new map<string, ref array<string>>();
	ref array<ref DZS_XmlCanon> m_Canon = new array<ref DZS_XmlCanon>();
	ref array<ref DZS_LootChange> m_Changes = new array<ref DZS_LootChange>();
	ref array<string> m_Written = new array<string>();
	ref array<string> m_Origs = new array<string>();
	bool m_LootBuilt;
	ref array<string> m_Merged = new array<string>();
	ref array<string> m_Deleted = new array<string>();
	ref array<string> m_Order = new array<string>();
	ref array<ref DZS_DzbotLootFile> m_NewLoot = new array<ref DZS_DzbotLootFile>();
	bool m_Blocked;
	// key of dzstudio/project.json when this save creates it
	string m_NewProjectKey;
}

class DZS_DzbotProvider : DZS_StoreProvider
{
	DZS_DzbotPlugin m_P;

	override string Id() { return "dzbot"; }
	override bool Owns(string folder) { return folder.IndexOf(DZS_DzbotPlugin.PREFIX) == 0; }
	override string Label(string folder) { return m_P.LabelOf(folder); }
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
			return "dzbot";
		return "";
	}
	override void RefreshRows() { m_P.RefreshProjects(); }
	override bool RowsLoading() { return m_P.m_ListLoading; }
	override string RowsEmptyText() { return m_P.m_ListError; }
	override void Rows(array<ref DZS_ProjectInfo> into) { m_P.Rows(into); }
	override string RowMeta(DZS_ProjectInfo p) { return m_P.RowMeta(p.m_Folder); }
	override bool RowOff(DZS_ProjectInfo p, out string de, out string en) { return m_P.RowOffOf(p.m_Folder, de, en); }
	override string Address() { return m_P.m_Base; }
	override DZS_LootOrigin LootOrigin(string folder) { return m_P.LootOriginOf(folder); }
	override void OpenIssues(string folder, DZS_Issues into) { m_P.OpenIssuesOf(folder, into); }
	override bool GroupsFixed(string folder, out string de, out string en)
	{
		DZS_DzbotProject dp = m_P.ProjectOf(folder);
		if (!dp || dp.m_Kind != "placement")
			return false;
		de = "Eine Platzierung hat genau eine Gruppe";
		en = "A placement has exactly one group";
		return true;
	}
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
	protected string m_OpenFolder;
	protected float m_PollIn;
	protected bool m_Polling;
	protected ref DZS_DzbotSave m_Save;
	protected float m_SaveRetryIn;
	protected float m_Now;
	protected bool m_Asking;

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

	// "settings" of a load or poll reply: [{id, title, type, multiple, options: [{value, label}], props: [{name}]}].
	static void TakeSettings(DZS_SrcNode list, array<ref DZS_DzbotSetting> into)
	{
		if (!list || !list.IsArr())
			return;
		into.Clear();
		foreach (DZS_SrcNode n : list.m_Items)
		{
			DZS_DzbotSetting st = new DZS_DzbotSetting();
			st.m_Id = DZS_DzbotCatalog.Text(n, "id");
			if (st.m_Id == "")
				continue;
			st.m_Title = DZS_DzbotCatalog.Text(n, "title");
			st.m_Type = DZS_DzbotCatalog.Text(n, "type");
			DZS_SrcNode mul = n.Get("multiple");
			st.m_Multiple = mul && mul.IsBool() && mul.m_Bool;
			DZS_SrcNode opts = n.Get("options");
			if (opts && opts.IsArr())
			{
				foreach (DZS_SrcNode o : opts.m_Items)
					st.m_Options.Insert(DZS_DzbotCatalog.Text(o, "value"));
			}
			DZS_SrcNode props = n.Get("props");
			if (props && props.IsArr())
			{
				foreach (DZS_SrcNode pr : props.m_Items)
					st.m_Props.Insert(DZS_DzbotCatalog.Text(pr, "name"));
			}
			into.Insert(st);
		}
	}

	static DZS_Json SettingsJson(array<ref DZS_DzbotSetting> list)
	{
		DZS_Json arr = DZS_Json.Arr();
		foreach (DZS_DzbotSetting st : list)
		{
			DZS_Json o = DZS_Json.Obj();
			o.Set("id", DZS_Json.Str(st.m_Id));
			o.Set("title", DZS_Json.Str(st.m_Title));
			o.Set("type", DZS_Json.Str(st.m_Type));
			o.Set("multiple", DZS_Json.Boolean(st.m_Multiple));
			DZS_Json opts = DZS_Json.Arr();
			foreach (string ov : st.m_Options)
			{
				DZS_Json oo = DZS_Json.Obj();
				oo.Set("value", DZS_Json.Str(ov));
				opts.Add(oo);
			}
			o.Set("options", opts);
			DZS_Json props = DZS_Json.Arr();
			foreach (string pv : st.m_Props)
			{
				DZS_Json po = DZS_Json.Obj();
				po.Set("name", DZS_Json.Str(pv));
				props.Add(po);
			}
			o.Set("props", props);
			arr.Add(o);
		}
		return arr;
	}

	override void OnRegister()
	{
		string shaArg;
		if (GetCLIParam("dzsShaTest", shaArg))
		{
			DZS_Sha256.SelfTest();
			DZS_XmlCanon.SelfTest();
		}
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
			OnReload(r);
		else if (tag == "base")
			OnBase(r);
		else if (tag == "lootstate")
			OnLootState(r);
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

	protected DZS_DzbotProject TakeProject(DZS_SrcNode p)
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
		dp.m_Name = DZS_DzbotCatalog.Text(p, "name");
		dp.m_MapKey = MapOf(p);
		dp.m_Owner = DZS_DzbotCatalog.Text(p, "owner");
		dp.m_Server = DZS_DzbotCatalog.Text(p, "server");
		return dp;
	}

	protected void OnProjects(DZS_DzbotReply r)
	{
		m_ListLoading = false;
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
		DZS_Log.Info("dzbot: project list " + m_ListOrder.Count() + " mod(s) and placement(s)");
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
			info.m_Groups = dp.m_Files.Count();
			into.Insert(info);
		}
	}

	// dzStudio edits project mods and placements; a mod of files stays in the workbench.
	bool RowOffOf(string folder, out string de, out string en)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (!dp || !IsFilesMod(dp))
			return false;
		de = FILES_MOD_DE;
		en = FILES_MOD_EN;
		return true;
	}

	static const string PROJECT_PATH = "dzstudio/project.json";
	static const string FILES_MOD_DE = "Datei-Mods bearbeitest du in der dzbot-Werkbank; in dzStudio nur Projekt-Mods";
	static const string FILES_MOD_EN = "Edit file mods in the dzbot workbench; dzStudio edits project mods";

	static bool IsFilesMod(DZS_DzbotProject dp)
	{
		return dp.m_Kind == "mod" && dp.m_ModKind != "project";
	}

	static bool IsProjectMod(DZS_DzbotProject dp)
	{
		return dp.m_Kind == "mod" && dp.m_ModKind == "project";
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
			string m = r.Message();
			if (pp)
				pp.Fetched("", false, "Projekt nicht gefunden: " + m, "Project not found: " + m);
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
		bool asProject = IsProjectMod(dp);
		array<string> projectChunks = null;
		if (asProject)
		{
			projectChunks = ProjectChunksOf(r);
			TakeSettings(r.m_Json.Get("settings"), dp.m_Settings);
		}
		string projectPart = "";
		DZS_DzbotSection msec = r.Find("manifest", "manifest");
		if (msec)
			projectPart = ProjectPartOf(msec.m_Chunks);
		// several maps and none remembered: ask once, the choice goes into the manifest (answers to section 11)
		array<string> maps = MapsOf(pj);
		string fixedMap = DZS_DzbotCatalog.Text(pj, "map");
		bool chose = false;
		if (fixedMap == "" && maps.Count() > 1)
		{
			string kept = ChosenMap(projectPart);
			if (asProject && DZS_ProjectFile.IsV2(projectChunks))
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
			if (!asProject)
				projectPart = WithMap(projectPart, dp.m_MapKey);
			m_PartChanged = FolderOf(dp.m_Ref);
			string mn = DZS_LootPlugin.MapName(dp.m_MapKey);
			Msg("info", "Karte " + mn + " gewählt — steht nach dem Speichern im Manifest", "Map " + mn + " chosen — kept in the manifest after saving", 6.0);
		}
		string folder = FolderOf(dp.m_Ref);
		DZS_Files.MakeDirs(DZS_LocalStore.Dir(folder));
		array<string> keep = new array<string>();
		dp.m_Files.Clear();
		dp.m_ProjectFile = null;
		dp.m_Loot = new DZS_DzbotLootState();
		m_LootSecs.Clear();
		DZS_SrcNode files = r.m_Json.Get("files");
		int groups = 0;
		if (files && files.IsArr())
		{
			foreach (DZS_SrcNode f : files.m_Items)
			{
				DZS_DzbotFile df = new DZS_DzbotFile();
				df.m_Key = DZS_DzbotCatalog.Text(f, "key");
				df.m_Path = DZS_DzbotCatalog.Text(f, "path");
				df.m_Role = DZS_DzbotCatalog.Text(f, "role");
				DZS_SrcNode rev = f.Get("revision");
				if (rev)
					df.m_Rev = rev.m_Str.ToInt();
				if (asProject)
				{
					if (df.m_Role == "project")
						dp.m_ProjectFile = df;
					continue;
				}
				if (df.m_Role == "loot_proto" || df.m_Role == "loot_pos")
				{
					string ft = DZS_DzbotCatalog.Text(f, "type");
					DZS_SrcNode mg = f.Get("managed");
					bool managed = mg && mg.IsBool() && mg.m_Bool;
					if (managed && (ft == "delete" || ft == "merge_add") && !dp.m_Loot.Own(df.m_Path, ft))
					{
						DZS_DzbotLootFile lf = new DZS_DzbotLootFile();
						lf.m_Key = df.m_Key;
						lf.m_Path = df.m_Path;
						lf.m_Type = ft;
						lf.m_Rev = df.m_Rev;
						dp.m_Loot.m_Files.Insert(lf);
						DZS_DzbotSection ls = r.Find("file", df.m_Key);
						if (ls)
							m_LootSecs.Set(df.m_Key, ls);
					}
					continue;
				}
				if (df.m_Role == "project")
				{
					dp.m_ProjectFile = df;
					continue;
				}
				if (df.m_Role != "spawner")
					continue;
				DZS_DzbotSection sec = r.Find("file", df.m_Key);
				if (!sec)
					continue;
				dp.m_Files.Insert(df);
				WriteText(folder, df.m_Path, sec.m_Chunks);
				keep.Insert(df.m_Path);
				groups++;
			}
		}
		DZS_SrcNode man = r.m_Json.Get("manifest");
		dp.m_ManifestRev = 0;
		if (man && man.IsObj())
		{
			DZS_SrcNode mrev = man.Get("revision");
			if (mrev)
				dp.m_ManifestRev = mrev.m_Str.ToInt();
		}
		RemoveStale(folder, keep);
		if (asProject)
		{
			// the project file of the mod opens like a local one (through its working folder)
			string mapSet = "";
			if (chose)
				mapSet = dp.m_MapKey;
			array<string> own = DZS_ProjectFile.RewriteV2(projectChunks, dp.m_Name, mapSet, "");
			if (!own)
				own = DZS_LocalStore.NewFile(dp.m_Name, dp.m_MapKey, "", "objects.json", DZS_Texts.Get("core.group.objects"));
			DZS_Files.Write(DZS_LocalStore.Path(folder, DZS_ProjectFile.NAME), own);
			dp.m_Loot = null;
			DZS_Log.Info("dzbot: loaded project mod " + dp.m_Ref + " '" + dp.m_Name + "' map " + dp.m_MapKey + ", project file " + (projectChunks != null));
			pp.Fetched(folder, true, "", "");
			return;
		}
		array<string> pfile = DZS_ProjectFile.Compose(dp.m_Name, dp.m_MapKey, projectPart, "");
		DZS_Files.Write(DZS_LocalStore.Path(folder, DZS_ProjectFile.NAME), pfile);
		DZS_Log.Info("dzbot: loaded " + dp.m_Ref + " '" + dp.m_Name + "' map " + dp.m_MapKey + ", " + groups + " group file(s), " + dp.m_Loot.m_Files.Count() + " own loot delta(s), manifest rev " + dp.m_ManifestRev + " (" + projectPart.Length() + " bytes)");
		if (dp.m_Kind != "mod")
		{
			dp.m_Loot.m_Origin.m_NoDe = "Loot gibt es nur in dzbot-Mods, nicht in Platzierungen";
			dp.m_Loot.m_Origin.m_NoEn = "Loot exists only in dzbot mods, not in placements";
			pp.Fetched(folder, true, "", "");
			return;
		}
		StartBase(dp);
	}

	// ---- loot: base and vanilla state ----

	protected ref map<string, ref DZS_DzbotSection> m_LootSecs = new map<string, ref DZS_DzbotSection>();
	protected ref array<string> m_BasePaths = new array<string>();
	protected ref map<string, ref DZS_DzbotReply> m_BaseReplies = new map<string, ref DZS_DzbotReply>();
	protected string m_BaseRef;

	// After a restore (Restored): only the loot state of the mod, from its own delta files and the base; the files of
	// the open project and the revisions of its groups stay as the backup has them.
	protected string m_RebuildRef;
	protected bool m_Rebuild;

	protected void OnLootState(DZS_DzbotReply r)
	{
		DZS_DzbotProject dp = m_Projects.Get(m_RebuildRef);
		if (!dp || !r.Ok())
		{
			DZS_Log.Warn("dzbot: loot state after restore not built: " + r.Error() + " " + r.m_Fail);
			return;
		}
		dp.m_Loot = new DZS_DzbotLootState();
		m_LootSecs.Clear();
		DZS_SrcNode files = r.m_Json.Get("files");
		if (files && files.IsArr())
		{
			foreach (DZS_SrcNode f : files.m_Items)
			{
				string role = DZS_DzbotCatalog.Text(f, "role");
				if (role != "loot_proto" && role != "loot_pos")
					continue;
				string ft = DZS_DzbotCatalog.Text(f, "type");
				string path = DZS_DzbotCatalog.Text(f, "path");
				DZS_SrcNode mg = f.Get("managed");
				bool managed = mg && mg.IsBool() && mg.m_Bool;
				if (!managed || (ft != "delete" && ft != "merge_add") || dp.m_Loot.Own(path, ft))
					continue;
				DZS_DzbotLootFile lf = new DZS_DzbotLootFile();
				lf.m_Key = DZS_DzbotCatalog.Text(f, "key");
				lf.m_Path = path;
				lf.m_Type = ft;
				DZS_SrcNode rev = f.Get("revision");
				if (rev)
					lf.m_Rev = rev.m_Str.ToInt();
				dp.m_Loot.m_Files.Insert(lf);
				DZS_DzbotSection ls = r.Find("file", lf.m_Key);
				if (ls)
					m_LootSecs.Set(lf.m_Key, ls);
			}
		}
		m_Rebuild = true;
		StartBase(dp);
	}

	protected void StartBase(DZS_DzbotProject dp)
	{
		m_BaseRef = dp.m_Ref;
		m_BasePaths.Clear();
		m_BaseReplies.Clear();
		m_BasePaths.Insert(DZS_LootFiles.PROTO);
		m_BasePaths.Insert(DZS_LootFiles.POS);
		m_Kernel.GetScreen().ShowMessage("dzbot", "info", DZS_W.Literal(DZS_Texts.Pick("Lädt Loot-Basis aus dzbot …", "Loading the loot base from dzbot …")), 10.0);
		NextBase();
	}

	protected void NextBase()
	{
		DZS_DzbotProject dp = m_Projects.Get(m_BaseRef);
		if (!dp)
			return;
		int n = m_BaseReplies.Count();
		if (n >= m_BasePaths.Count())
		{
			ComposeLoot(dp);
			return;
		}
		string path = m_BasePaths[n];
		string qr = DZS_DzbotFrame.Q(dp.m_Ref);
		string qp = DZS_DzbotFrame.Q(path);
		string qm = DZS_DzbotFrame.Q(dp.m_MapKey);
		string fields = "\"project\":" + qr + ",\"path\":" + qp + ",\"map\":" + qm;
		string bk = dp.m_Loot.BaseKey(path);
		if (bk != "")
			fields += ",\"key\":" + DZS_DzbotFrame.Q(bk);
		AuthPost("main", "base", "/editor/project/base", fields);
	}

	protected void OnBase(DZS_DzbotReply r)
	{
		DZS_DzbotProject dp = m_Projects.Get(m_BaseRef);
		if (!dp)
			return;
		int n = m_BaseReplies.Count();
		string path = m_BasePaths[n];
		m_BaseReplies.Set(path, r);
		string st = "";
		if (r.Ok())
		{
			DZS_SrcNode b = r.m_Json.Get("base");
			if (b)
				st = DZS_DzbotCatalog.Text(b, "state");
		}
		DZS_Log.Info("dzbot: base " + path + ": " + st + " " + r.Error() + " " + r.m_Fail);
		NextBase();
	}

	protected DZS_DzbotSection BaseSec(string path, string kind)
	{
		DZS_DzbotReply r = m_BaseReplies.Get(path);
		if (!r || !r.Ok())
			return null;
		return r.Find(kind, path);
	}

	protected DZS_DzbotSection BaseSecOf(string path, string kind, string key)
	{
		DZS_DzbotReply r = m_BaseReplies.Get(path);
		if (!r || !r.Ok())
			return null;
		return r.Find(kind, key);
	}

	// The files of the mod after the own delta on a path, in their order.
	protected void TakeAfter(DZS_DzbotLootState st, string path)
	{
		array<ref DZS_DzbotAfter> list = new array<ref DZS_DzbotAfter>();
		st.m_After.Set(path, list);
		DZS_DzbotReply r = m_BaseReplies.Get(path);
		if (!r || !r.Ok())
			return;
		DZS_SrcNode after = r.m_Json.Get("after");
		if (!after || !after.IsArr())
			return;
		foreach (DZS_SrcNode e : after.m_Items)
		{
			string ak = DZS_DzbotCatalog.Text(e, "key");
			if (st.ByKey(ak))
				continue;
			DZS_DzbotAfter a = new DZS_DzbotAfter();
			a.m_Key = ak;
			a.m_Type = DZS_DzbotCatalog.Text(e, "type");
			DZS_SrcNode rv = e.Get("revision");
			if (rv)
				a.m_Rev = rv.m_Str.ToInt();
			DZS_SrcNode tp = e.Get("template");
			a.m_Template = tp && tp.IsBool() && tp.m_Bool;
			DZS_DzbotSection sec = r.Find("after", a.m_Key);
			if (sec)
				a.m_Chunks = sec.m_Chunks;
			else
				a.m_Chunks = new array<string>();
			list.Insert(a);
		}
		if (list.Count() > 0)
			DZS_Log.Info("dzbot: " + path + ": " + list.Count() + " file(s) of the mod after the own delta");
	}

	protected array<string> OwnChunks(DZS_DzbotLootState st, string path, string type)
	{
		DZS_DzbotLootFile f = st.Own(path, type);
		if (!f)
			return null;
		DZS_DzbotSection sec = m_LootSecs.Get(f.m_Key);
		if (!sec)
			return null;
		return sec.m_Chunks;
	}

	protected void ComposeLoot(DZS_DzbotProject dp)
	{
		m_Kernel.GetScreen().HideMessage("dzbot");
		string folder = FolderOf(dp.m_Ref);
		DZS_DzbotLootState st = dp.m_Loot;
		DZS_DzbotSection bp = BaseSec(DZS_LootFiles.PROTO, "base");
		DZS_DzbotSection bq = BaseSec(DZS_LootFiles.POS, "base");
		string why = "";
		if (!bp || !bq)
		{
			DZS_DzbotReply rp = m_BaseReplies.Get(DZS_LootFiles.PROTO);
			if (rp && rp.Ok())
			{
				DZS_SrcNode bn = rp.m_Json.Get("base");
				if (bn)
					why = DZS_DzbotCatalog.Text(bn, "state");
			}
			else if (rp)
				why = rp.Error() + rp.m_Fail;
		}
		bool rebuild = m_Rebuild;
		m_Rebuild = false;
		array<string> stale = {DZS_LootFiles.PROTO, DZS_LootFiles.POS, DZS_LootFiles.LIMITS, "vanilla/" + DZS_LootFiles.PROTO, "vanilla/" + DZS_LootFiles.LIMITS};
		if (!rebuild)
		{
			foreach (string sp : stale)
				DeleteFile(DZS_LocalStore.Path(folder, sp));
		}
		if (why == "")
		{
			int t0 = TickCount(0);
			array<string> pd = OwnChunks(st, DZS_LootFiles.PROTO, "delete");
			array<string> pa = OwnChunks(st, DZS_LootFiles.PROTO, "merge_add");
			array<string> qd = OwnChunks(st, DZS_LootFiles.POS, "delete");
			array<string> qa = OwnChunks(st, DZS_LootFiles.POS, "merge_add");
			array<string> protoOut = DZS_DzbotLoot.ComposeProto(bp.m_Chunks, pd, pa, st);
			array<string> pos = DZS_DzbotLoot.ComposePos(bq.m_Chunks, qd, qa, st);
			if (!protoOut || !pos)
				why = "unreadable";
			else if (rebuild)
			{
				TakeAfter(st, DZS_LootFiles.PROTO);
				TakeAfter(st, DZS_LootFiles.POS);
				DZS_Log.Info("dzbot: loot state after restore: " + st.m_Files.Count() + " own delta file(s), " + st.m_Origin.m_OwnTypes.Count() + " own entr(y/ies), " + st.m_RowKeys.Count() + " own row(s)");
			}
			else
			{
				WriteText(folder, DZS_LootFiles.PROTO, protoOut);
				WriteText(folder, DZS_LootFiles.POS, pos);
				DZS_DzbotSection bl = BaseSecOf(DZS_LootFiles.PROTO, "limits", DZS_LootFiles.LIMITS);
				if (bl)
				{
					WriteText(folder, DZS_LootFiles.LIMITS, bl.m_Chunks);
					WriteText(folder, "vanilla/" + DZS_LootFiles.LIMITS, bl.m_Chunks);
				}
				DZS_DzbotSection vp = BaseSec(DZS_LootFiles.PROTO, "vanilla");
				if (vp)
					WriteText(folder, "vanilla/" + DZS_LootFiles.PROTO, vp.m_Chunks);
				TakeAfter(st, DZS_LootFiles.PROTO);
				TakeAfter(st, DZS_LootFiles.POS);
				int ms = (TickCount(0) - t0) / 10000;
				DZS_Log.Info("dzbot: loot working state composed in " + ms + " ms: " + st.m_Origin.m_OwnTypes.Count() + " own entr(y/ies), " + st.m_RowKeys.Count() + " own row(s), " + st.m_DelKeys.Count() + " base row(s) removed, vanilla " + (vp != null));
			}
		}
		if (why != "")
		{
			st.m_Origin.m_NoDe = "Loot lässt sich in dieser Mod nicht bearbeiten: dzbot liefert keine Basis (" + why + ")";
			st.m_Origin.m_NoEn = "Loot cannot be edited in this mod: dzbot gives no base (" + why + ")";
			DZS_Log.Warn("dzbot: no loot base for " + dp.m_Ref + ": " + why);
		}
		m_BaseReplies.Clear();
		m_LootSecs.Clear();
		if (rebuild)
			return;
		Projects().Fetched(folder, true, "", "");
	}

	// A hint per loot path with files of the mod after the own delta.
	void OpenIssuesOf(string folder, DZS_Issues into)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (!dp || !dp.m_Loot)
			return;
		array<string> paths = {DZS_LootFiles.PROTO, DZS_LootFiles.POS};
		foreach (string path : paths)
		{
			array<ref DZS_DzbotAfter> list = dp.m_Loot.m_After.Get(path);
			if (!list || list.Count() == 0)
				continue;
			int n = list.Count();
			string de = path + ": " + n + DZS_Issues.Plural(n, " weitere Datei der Mod gilt", " weitere Dateien der Mod gelten") + " beim Übertragen nach dem, was dzStudio schreibt";
			string en = path + ": " + n + DZS_Issues.Plural(n, " more file of the mod applies", " more files of the mod apply") + " after what dzStudio writes when transferring";
			into.Add(DZS_Issue.HINT, de, en);
		}
	}

	DZS_DzbotProject ProjectOf(string folder)
	{
		return m_Projects.Get(RefOf(folder));
	}

	DZS_LootOrigin LootOriginOf(string folder)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
		if (!dp || !dp.m_Loot)
			return null;
		return dp.m_Loot.m_Origin;
	}

	// mirror folder whose project part got the map choice (opens with 1 unsaved change so the choice can be saved)
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

	// The project part with plugins.dzbot set to the chosen map (a minimal part when the mod has no manifest yet).
	static string WithMap(string part, string mapKey)
	{
		string q = DZS_DzbotFrame.Q(mapKey);
		string entry = "\"dzbot\": {\"version\": 1, \"data\": {\"map\": " + q + "}}";
		if (part == "")
			return "{\"groups\": [], \"loot\": {\"enabled\": false, \"hidden\": false, \"locked\": false, \"projectId\": null, \"writtenTypes\": []}, \"plugins\": {" + entry + "}}";
		array<string> chunks = DZS_Files.Chunk(part);
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
			return part;
		DZS_SrcNode pl = doc.m_Root.Get("plugins");
		if (!pl || !pl.IsObj())
		{
			int close = doc.m_Root.m_End - 1;
			string head = doc.Slice(0, close);
			string tail = doc.Slice(close, doc.m_Root.m_End);
			return head + ", \"plugins\": {" + entry + "}" + tail;
		}
		DZS_SrcNode mine = pl.Get("dzbot");
		int total = part.Length();
		if (mine)
		{
			string before = doc.Slice(0, mine.m_KeyStart);
			string after = doc.Slice(mine.m_End, total);
			return before + entry + after;
		}
		string pre = doc.Slice(0, pl.m_Start + 1);
		string post = doc.Slice(pl.m_Start + 1, total);
		if (pl.m_Items.Count() > 0)
			return pre + entry + "," + post;
		return pre + entry + post;
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
		string de = "„" + dp.m_Name + "“ gilt für mehrere Karten. Mit welcher Karte soll das Projekt öffnen? Die Wahl steht danach im Manifest der Mod (für alle Bearbeiter).";
		string en = "\"" + dp.m_Name + "\" applies to several maps. Which map should the project open with? The choice is then kept in the mod’s manifest (for every editor).";
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

	protected void WriteText(string folder, string rel, array<string> chunks)
	{
		string path = DZS_LocalStore.Path(folder, rel);
		int slash = rel.LastIndexOf("/");
		if (slash > 0)
			DZS_Files.MakeDirs(DZS_LocalStore.Path(folder, rel.Substring(0, slash)));
		DZS_Files.Write(path, chunks);
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
		st.Set("name", DZS_Json.Str(dp.m_Name));
		st.Set("map", DZS_Json.Str(dp.m_MapKey));
		st.Set("manifest", DZS_Json.Int(dp.m_ManifestRev));
		DZS_Json files = DZS_Json.Arr();
		foreach (DZS_DzbotFile f : dp.m_Files)
		{
			DZS_Json fj = DZS_Json.Obj();
			fj.Set("key", DZS_Json.Str(f.m_Key));
			fj.Set("path", DZS_Json.Str(f.m_Path));
			fj.Set("rev", DZS_Json.Int(f.m_Rev));
			fj.Set("role", DZS_Json.Str(f.m_Role));
			files.Add(fj);
		}
		st.Set("files", files);
		DZS_DzbotFile pf = dp.m_ProjectFile;
		if (pf)
		{
			DZS_Json pj = DZS_Json.Obj();
			pj.Set("key", DZS_Json.Str(pf.m_Key));
			pj.Set("path", DZS_Json.Str(pf.m_Path));
			pj.Set("rev", DZS_Json.Int(pf.m_Rev));
			st.Set("project", pj);
		}
		if (IsProjectMod(dp))
			st.Set("settings", SettingsJson(dp.m_Settings));
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
		dp.m_Name = st.GetStr("name");
		dp.m_MapKey = st.GetStr("map");
		dp.m_ManifestRev = st.GetInt("manifest");
		DZS_Json files = st.Get("files");
		if (files)
		{
			for (int i = 0; i < files.Count(); i++)
			{
				DZS_Json fj = files.At(i);
				DZS_DzbotFile f = new DZS_DzbotFile();
				f.m_Key = fj.GetStr("key");
				f.m_Path = fj.GetStr("path");
				f.m_Rev = fj.GetInt("rev");
				f.m_Role = fj.GetStr("role");
				dp.m_Files.Insert(f);
			}
		}
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
		if (sts)
		{
			array<string> stText = sts.ToChunks(false);
			DZS_SrcDoc stDoc = DZS_SrcJson.Parse(stText);
			if (stDoc && stDoc.m_Root)
				TakeSettings(stDoc.m_Root, dp.m_Settings);
		}
		dp.m_Loot = new DZS_DzbotLootState();
		if (dp.m_Kind != "mod")
		{
			dp.m_Loot.m_Origin.m_NoDe = "Loot gibt es nur in dzbot-Mods, nicht in Platzierungen";
			dp.m_Loot.m_Origin.m_NoEn = "Loot exists only in dzbot mods, not in placements";
		}
		m_Projects.Set(pref, dp);
		DZS_Log.Info("dzbot: restored " + pref + " at revision " + dp.Revision() + ", " + dp.m_Files.Count() + " file(s)");
		if (IsProjectMod(dp))
		{
			dp.m_Loot = null;
			return;
		}
		if (dp.m_Kind == "mod" && !IsConnected())
		{
			dp.m_Loot.m_Origin.m_NoDe = "Loot dieses wiederhergestellten Mods erst nach dem Verbinden mit dzbot und erneutem Öffnen";
			dp.m_Loot.m_Origin.m_NoEn = "Loot of this restored mod only after connecting with dzbot and opening it again";
		}
		else if (dp.m_Kind == "mod")
		{
			// the loot working files came with the backup; the state a save needs (own deltas, rows) is built again
			m_RebuildRef = pref;
			AuthPost("main", "lootstate", "/editor/project/load", "\"project\":" + DZS_DzbotFrame.Q(pref), null, null);
		}
	}

	void Opened(string folder)
	{
		m_OpenFolder = folder;
		m_PollIn = POLL_EVERY;
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
		string held = "";
		foreach (DZS_DzbotFile f : dp.m_Files)
		{
			if (held != "")
				held += ",";
			held += "{\"key\":" + DZS_DzbotFrame.Q(f.m_Key) + ",\"revision\":" + f.m_Rev + "}";
		}
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
			TakeSettings(r.m_Json.Get("settings"), dp.m_Settings);
		DZS_ProjectsPlugin pp = Projects();
		DZS_Instance inst = m_Kernel.GetShown();
		bool dirty = inst && inst.IsUnsaved();
		array<string> changed = new array<string>();
		map<string, bool> present = new map<string, bool>();
		DZS_SrcNode files = r.m_Json.Get("files");
		if (files && files.IsArr())
		{
			foreach (DZS_SrcNode f : files.m_Items)
			{
				string key = DZS_DzbotCatalog.Text(f, "key");
				string path = DZS_DzbotCatalog.Text(f, "path");
				string role = DZS_DzbotCatalog.Text(f, "role");
				string state = DZS_DzbotCatalog.Text(f, "state");
				present.Set(key, true);
				if (role != "spawner" || state == "unchanged")
					continue;
				DZS_DzbotSection sec = r.Find("file", key);
				if (!sec)
					continue;
				int rev = 0;
				DZS_SrcNode rn = f.Get("revision");
				if (rn)
					rev = rn.m_Str.ToInt();
				DZS_DzbotFile df = dp.FileByKey(key);
				if (!df)
				{
					df = new DZS_DzbotFile();
					df.m_Key = key;
					df.m_Path = path;
					df.m_Role = role;
					df.m_Rev = rev;
					dp.m_Files.Insert(df);
					WriteText(m_OpenFolder, path, sec.m_Chunks);
					pp.AddGroup(path, sec.m_Chunks);
					Msg("info", "Neue Datei aus dzbot: " + path, "New file from dzbot: " + path, 6.0);
					continue;
				}
				DZS_Log.Info("dzbot: poll: " + path + " changed in dzbot, rev " + df.m_Rev + " -> " + rev + ", own changes " + dirty);
				if (dirty)
				{
					changed.Insert(path);
					continue;
				}
				df.m_Rev = rev;
				WriteText(m_OpenFolder, path, sec.m_Chunks);
				pp.ReloadGroup(path, sec.m_Chunks);
			}
		}
		for (int i = dp.m_Files.Count() - 1; i >= 0; i--)
		{
			DZS_DzbotFile gone = dp.m_Files[i];
			if (!present.Contains(gone.m_Key))
			{
				DZS_Log.Info("dzbot: poll: " + gone.m_Path + " is gone in dzbot");
				AskGone(gone.m_Path);
			}
		}
		if (changed.Count() > 0)
			AskConflict(changed);
		DZS_Log.Info("dzbot: poll done, " + changed.Count() + " conflict(s)");
	}

	protected void AskGone(string path)
	{
		if (m_Asking)
			return;
		m_Asking = true;
		DZS_ConfirmDialog c = new DZS_ConfirmDialog("dzbot", DZS_Texts.Pick("Datei in dzbot gelöscht", "File deleted in dzbot"), DZS_Texts.Pick(path + " gibt es in dzbot nicht mehr.", path + " no longer exists in dzbot."));
		c.m_Tag = path;
		c.AddFootButton("keep", DZS_Texts.Pick("Behalten und beim Speichern neu anlegen", "Keep and create it again when saving"), "side");
		c.AddFootButton("remove", DZS_Texts.Pick("Gruppe entfernen", "Remove the group"), "danger");
		c.m_OnResult.Insert(OnGoneAnswer);
		c.Open();
	}

	void OnGoneAnswer(DZS_Dialog d, string result)
	{
		m_Asking = false;
		DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
		if (!dp)
			return;
		DZS_DzbotFile f = dp.FileByPath(d.m_Tag);
		if (f)
			dp.m_Files.RemoveItem(f);
		if (result == "remove")
			Projects().RemoveGroup(d.m_Tag);
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
		string en = "Changed in dzbot while there are unsaved changes here: " + list;
		DZS_ConfirmDialog c = new DZS_ConfirmDialog("dzbot", DZS_Texts.Pick("In dzbot geändert", "Changed in dzbot"), DZS_Texts.Pick(de, en));
		c.AddFootButton("copy", DZS_Texts.Pick("Als Konfliktkopie speichern", "Save as conflict copy"), "side");
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
		if (result == "reload")
		{
			DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
			if (!dp)
				return;
			string fields = "\"project\":" + DZS_DzbotFrame.Q(dp.m_Ref);
			AuthPost("main", "reload", "/editor/project/load", fields);
		}
		else if (result == "copy" && pp && pp.m_Open)
		{
			string name = pp.m_Open.m_Name + DZS_Texts.Pick(" (Konfliktkopie)", " (conflict copy)");
			pp.StartSave(false, "", name);
		}
	}

	protected void OnReload(DZS_DzbotReply r)
	{
		if (!r.Ok() || m_OpenFolder == "")
		{
			Msg("danger", "Neu laden fehlgeschlagen: " + r.Message(), "Reloading failed: " + r.Message());
			return;
		}
		DZS_DzbotProject dp = m_Projects.Get(RefOf(m_OpenFolder));
		DZS_ProjectsPlugin pp = Projects();
		DZS_SrcNode files = r.m_Json.Get("files");
		int n = 0;
		if (files && files.IsArr())
		{
			foreach (DZS_SrcNode f : files.m_Items)
			{
				string key = DZS_DzbotCatalog.Text(f, "key");
				DZS_DzbotFile df = dp.FileByKey(key);
				DZS_DzbotSection sec = r.Find("file", key);
				if (!df || !sec)
					continue;
				DZS_SrcNode rn = f.Get("revision");
				if (rn)
					df.m_Rev = rn.m_Str.ToInt();
				if (df.m_Role == "project")
					continue;
				WriteText(m_OpenFolder, df.m_Path, sec.m_Chunks);
				pp.ReloadGroup(df.m_Path, sec.m_Chunks);
				n++;
			}
		}
		DZS_Instance inst = m_Kernel.GetShown();
		if (inst)
			inst.MarkSavedNow();
		PlateFor(m_OpenFolder);
		DZS_Log.Info("dzbot: reloaded " + n + " file(s) from dzbot");
		Msg("info", "Aus dzbot neu geladen", "Reloaded from dzbot");
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
		DZS_DzbotSave sv = new DZS_DzbotSave();
		sv.m_Folder = folder;
		sv.m_Files = files;
		sv.m_SaveId = DZS_Random.Code(24);
		bool asProject = dp.m_ModKind == "project";
		array<ref DZS_SaveFile> use = files;
		if (!asProject)
			use = PlacementFiles(dp, files);
		foreach (DZS_SaveFile f : use)
		{
			if (f.m_Delete || (asProject && f.m_Rel != DZS_ProjectFile.NAME))
				continue;
			if (f.m_Rel == DZS_ProjectFile.NAME)
			{
				string part = ProjectPartOf(f.m_Chunks);
				if (part == "")
					continue;
				string mr = dp.m_ManifestRev.ToString();
				DZS_DzbotSection ms = DZS_DzbotSection.Make("manifest", "manifest", mr, part);
				sv.m_Parts.Insert(ms);
				sv.m_Entries.Insert("{\"revision\":" + dp.m_ManifestRev);
				sv.m_Targets.Insert("");
				sv.m_PathOf.Set("manifest", "Manifest");
				continue;
			}
			if (f.m_Rel == DZS_LootFiles.PROTO || f.m_Rel == DZS_LootFiles.POS)
			{
				if (dp.m_Kind == "mod" && dp.m_Loot && dp.m_Loot.m_Origin.m_NoDe == "")
					sv.m_LootIn.Set(f.m_Rel, f.m_Chunks);
				continue;
			}
			DZS_DzbotFile df = dp.FileByPath(f.m_Rel);
			string entry;
			if (df)
			{
				entry = "{\"key\":" + DZS_DzbotFrame.Q(df.m_Key) + ",\"revision\":" + df.m_Rev;
				string rv = df.m_Rev.ToString();
				DZS_DzbotSection fs = DZS_DzbotSection.FromChunks("file", df.m_Key, rv, f.m_Chunks);
				sv.m_Parts.Insert(fs);
				sv.m_PathOf.Set(df.m_Key, f.m_Rel);
			}
			else
			{
				string low = f.m_Rel;
				low.ToLower();
				if (low.IndexOf(".json") < 0 || dp.m_Kind == "placement")
					continue;
				string key = NewKey();
				entry = "{\"key\":" + DZS_DzbotFrame.Q(key) + ",\"path\":" + DZS_DzbotFrame.Q(f.m_Rel) + ",\"type\":\"replace\",\"create\":true";
				DZS_DzbotSection ns = DZS_DzbotSection.FromChunks("file", key, "0", f.m_Chunks);
				sv.m_Parts.Insert(ns);
				sv.m_NewPaths.Insert(f.m_Rel);
				sv.m_NewKeys.Insert(key);
				sv.m_PathOf.Set(key, f.m_Rel);
			}
			sv.m_Entries.Insert(entry);
			sv.m_Targets.Insert(f.m_Rel);
		}
		if (asProject)
			ProjectPart(dp, sv, files);
		else
			RemovedGroups(dp, files, sv);
		if (sv.m_LootIn.Count() > 0)
		{
			DZS_LootPlugin lp = DZS_LootPlugin.Cast(m_Kernel.GetPlugin("loot"));
			if (lp)
			{
				foreach (DZS_LootChange lc : lp.m_Changes)
					sv.m_Changes.Insert(lc);
				lp.SaveWritten(sv.m_Written, sv.m_Origs);
			}
			for (int li = 0; li < sv.m_LootIn.Count(); li++)
			{
				string lpth = sv.m_LootIn.GetKey(li);
				array<string> lch = sv.m_LootIn.GetElement(li);
				DZS_XmlCanon job = new DZS_XmlCanon(lpth, lch);
				array<ref DZS_DzbotAfter> afterList = dp.m_Loot.m_After.Get(lpth);
				if (afterList)
				{
					foreach (DZS_DzbotAfter af : afterList)
						job.AddAfter(af);
				}
				DZS_Log.Info("dzbot: " + lpth + " parsed for the comparison form in " + job.m_Ms + " ms");
				sv.m_Canon.Insert(job);
			}
		}
		sv.m_Started = m_Now;
		sv.m_HashStart = TickCount(0);
		m_Save = sv;
		m_Client.m_SaveOpen = true;
		m_Client.Drop("poll");
		m_Polling = false;
		m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("dzbot · speichert …", "dzbot · saving …"), "warning");
		DZS_Log.Info("dzbot: save " + sv.m_SaveId + " of " + dp.m_Ref + ", " + sv.m_Parts.Count() + " section(s), checksums first");
		HashSave();
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
		if (!sv.m_LootBuilt)
		{
			foreach (DZS_XmlCanon cj : sv.m_Canon)
			{
				if (!cj.Step(HASH_MS))
					return;
			}
			if (!BuildLoot(sv))
				return;
			sv.m_LootBuilt = true;
			return;
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
			string badKind = "file";
			if (bad == "manifest")
				badKind = "manifest";
			for (int b = 0; b < sv.m_Parts.Count(); b++)
			{
				if (sv.m_Parts[b].m_Kind != badKind)
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
		string merged = "";
		bool manifest = false;
		for (int i = 0; i < sv.m_Parts.Count(); i++)
		{
			string qs = DZS_DzbotFrame.Q(sv.m_Sums[i]);
			string withSum = sv.m_Entries[i] + ",\"checksum\":" + qs + "}";
			DZS_DzbotSection p = sv.m_Parts[i];
			if (p.m_Kind == "manifest")
			{
				sv.m_ManifestEntry = withSum;
				manifest = true;
				continue;
			}
			if (list != "")
				list += ",";
			list += withSum;
			string target = sv.m_Targets[i];
			if (target == "")
				continue;
			if (merged != "")
				merged += ",";
			string qt = DZS_DzbotFrame.Q(target);
			merged += "{\"target\":" + qt + ",\"checksum\":" + qs + "}";
		}
		foreach (string me : sv.m_Merged)
		{
			if (merged != "")
				merged += ",";
			merged += me;
		}
		DZS_DzbotProject dp = m_Projects.Get(RefOf(sv.m_Folder));
		string qp = DZS_DzbotFrame.Q(dp.m_Ref);
		string qi = DZS_DzbotFrame.Q(sv.m_SaveId);
		sv.m_Fields = "\"project\":" + qp + ",\"saveId\":" + qi + ",\"files\":[" + list + "],\"merged\":[" + merged + "]";
		if (sv.m_Deleted.Count() > 0)
		{
			string dl = "";
			foreach (string de : sv.m_Deleted)
			{
				if (dl != "")
					dl += ",";
				dl += de;
			}
			sv.m_Fields += ",\"deleted\":[" + dl + "]";
		}
		if (sv.m_Order.Count() > 0)
		{
			string ol = "";
			foreach (string ok : sv.m_Order)
			{
				if (ol != "")
					ol += ",";
				ol += DZS_DzbotFrame.Q(ok);
			}
			sv.m_Fields += ",\"order\":[" + ol + "]";
		}
		if (dp.m_MapKey != "")
			sv.m_Fields += ",\"map\":" + DZS_DzbotFrame.Q(dp.m_MapKey);
		if (manifest)
			sv.m_Fields += ",\"manifest\":" + sv.m_ManifestEntry;
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

	// A project mod holds the project as building blocks: the saved project file (blocks, the editor part and the
	// extensions, the rules "dzbot.rules" among them — DZS_ProjectExtensions) replaces dzstudio/project.json.
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
			DZS_Log.Warn("dzbot: project mod " + dp.m_Ref + ": no project file in the save, nothing of the project saved");
			return;
		}
		if (!pf)
		{
			// the mod has no project file yet: this save creates it
			string key = NewKey();
			DZS_DzbotSection cs = DZS_DzbotSection.FromChunks("file", key, "0", chunks);
			sv.m_Parts.Insert(cs);
			sv.m_Entries.Insert("{\"key\":" + DZS_DzbotFrame.Q(key) + ",\"path\":" + DZS_DzbotFrame.Q(PROJECT_PATH) + ",\"type\":\"replace\",\"create\":true");
			sv.m_Targets.Insert("");
			sv.m_PathOf.Set(key, PROJECT_PATH);
			sv.m_NewProjectKey = key;
			DZS_Log.Info("dzbot: save creates " + PROJECT_PATH + ", " + DZS_Files.Length(chunks) + " bytes");
			return;
		}
		string rv = pf.m_Rev.ToString();
		DZS_DzbotSection ps = DZS_DzbotSection.FromChunks("file", pf.m_Key, rv, chunks);
		sv.m_Parts.Insert(ps);
		sv.m_Entries.Insert("{\"key\":" + DZS_DzbotFrame.Q(pf.m_Key) + ",\"revision\":" + pf.m_Rev);
		sv.m_Targets.Insert("");
		sv.m_PathOf.Set(pf.m_Key, pf.m_Path);
		DZS_Log.Info("dzbot: save sends the project file, " + DZS_Files.Length(chunks) + " bytes");
	}

	// A placement keeps its one spawner file and its manifest: both made from the saved project file.
	protected array<ref DZS_SaveFile> PlacementFiles(DZS_DzbotProject dp, array<ref DZS_SaveFile> files)
	{
		array<ref DZS_SaveFile> r = new array<ref DZS_SaveFile>();
		foreach (DZS_SaveFile f : files)
		{
			if (f.m_Delete || f.m_Rel != DZS_ProjectFile.NAME || !DZS_ProjectFile.IsV2(f.m_Chunks))
			{
				r.Insert(f);
				continue;
			}
			DZS_Json root = DZS_WorkFolder.RootOf(f.m_Chunks);
			string err;
			DZS_PatchProject pj = DZS_PatchProject.FromJson(root, err);
			if (!pj)
			{
				DZS_Log.Warn("dzbot: project file not readable for the placement: " + err);
				continue;
			}
			string target = "";
			if (dp.m_Files.Count() > 0)
				target = dp.m_Files[0].m_Path;
			map<string, string> paths = new map<string, string>();
			foreach (DZS_PatchBlock b : pj.m_Blocks)
			{
				if (b.m_Kind != "spawnerGroup" || target == "" || paths.Count() > 0)
					continue;
				array<string> text = DZS_WorkFolder.GroupText(b);
				paths.Set(b.m_Id, target);
				DZS_SaveFile sf = new DZS_SaveFile();
				sf.m_Rel = target;
				sf.m_Chunks = text;
				r.Insert(sf);
			}
			DZS_SaveFile mf = new DZS_SaveFile();
			mf.m_Rel = DZS_ProjectFile.NAME;
			mf.m_Chunks = DZS_WorkFolder.Manifest(root, dp.m_MapKey, paths);
			r.Insert(mf);
		}
		return r;
	}

	// Groups taken out of the project: their files go in the same save.
	protected void RemovedGroups(DZS_DzbotProject dp, array<ref DZS_SaveFile> files, DZS_DzbotSave sv)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || dp.m_Kind != "mod")
			return;
		map<string, bool> live = new map<string, bool>();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string gid : groups)
		{
			if (inst.GetKind(gid) != DZS_KIND_GROUP)
				continue;
			string file;
			inst.GetField(gid, DZS_Obj.GROUP_FILE, file);
			live.Set(file, true);
		}
		foreach (DZS_DzbotFile f : dp.m_Files)
		{
			if (live.Contains(f.m_Path))
				continue;
			string q = DZS_DzbotFrame.Q(f.m_Key);
			sv.m_Deleted.Insert("{\"key\":" + q + ",\"revision\":" + f.m_Rev + "}");
			sv.m_PathOf.Set(f.m_Key, f.m_Path);
			DZS_Log.Info("dzbot: save deletes " + f.m_Path + " (group taken out)");
		}
	}

	// Deltas of the loot paths in this save: at most one delete and one merge_add per path,
	// emptied own deltas deleted, the expected comparison form per path in merged.
	protected bool BuildLoot(DZS_DzbotSave sv)
	{
		DZS_DzbotProject dp = m_Projects.Get(RefOf(sv.m_Folder));
		if (!dp || !dp.m_Loot)
			return true;
		DZS_DzbotLootState st = dp.m_Loot;
		foreach (DZS_XmlCanon cj : sv.m_Canon)
		{
			if (cj.m_Error != "")
			{
				DZS_Log.Warn("dzbot: " + cj.m_Name + " not readable for the comparison form: " + cj.m_Error);
				continue;
			}
			string qt = DZS_DzbotFrame.Q(cj.m_Name);
			string qs = DZS_DzbotFrame.Q(cj.m_Sum);
			string noMerged;
			if (GetCLIParam("dzsDzbotNoMerged", noMerged))
				DZS_Log.Warn("dzbot: -dzsDzbotNoMerged: no expectation for " + cj.m_Name);
			else if (cj.m_NoExpect != "")
				DZS_Log.Info("dzbot: no expectation for " + cj.m_Name + " (" + cj.m_NoExpect + ")");
			else
				sv.m_Merged.Insert("{\"target\":" + qt + ",\"checksum\":" + qs + "}");
			DZS_Log.Info("dzbot: comparison form of " + cj.m_Name + " " + cj.m_Sum);
			array<string> del;
			array<string> add;
			if (cj.m_Name == DZS_LootFiles.PROTO)
			{
				DZS_DzbotLoot.ProtoDelta(cj.m_Doc, sv.m_Written, sv.m_Origs, del, add);
				st.m_NextTypes = new array<string>();
				st.m_NextOrigs = new array<string>();
				for (int w = 0; w < sv.m_Written.Count(); w++)
				{
					string wn = sv.m_Written[w];
					string wo = sv.m_Origs[w];
					st.m_NextTypes.Insert(wn);
					st.m_NextOrigs.Insert(wo);
				}
			}
			else
				DZS_DzbotLoot.PosDelta(st, sv.m_Changes, del, add);
			cj.m_Doc = null;
			LootPart(sv, st, cj.m_Name, "delete", del);
			LootPart(sv, st, cj.m_Name, "merge_add", add);
			string dk = KeyOfPart(sv, st, cj.m_Name, "delete", del);
			string ak = KeyOfPart(sv, st, cj.m_Name, "merge_add", add);
			if (dk != "" && ak != "")
			{
				sv.m_Order.Insert(dk);
				sv.m_Order.Insert(ak);
			}
		}
		return true;
	}

	protected string KeyOfPart(DZS_DzbotSave sv, DZS_DzbotLootState st, string path, string type, array<string> chunks)
	{
		if (!chunks)
			return "";
		DZS_DzbotLootFile f = st.Own(path, type);
		if (f)
			return f.m_Key;
		foreach (DZS_DzbotLootFile n : sv.m_NewLoot)
		{
			if (n.m_Path == path && n.m_Type == type)
				return n.m_Key;
		}
		return "";
	}

	protected void LootPart(DZS_DzbotSave sv, DZS_DzbotLootState st, string path, string type, array<string> chunks)
	{
		DZS_DzbotLootFile own = st.Own(path, type);
		if (!chunks)
		{
			if (own)
			{
				string qd = DZS_DzbotFrame.Q(own.m_Key);
				sv.m_Deleted.Insert("{\"key\":" + qd + ",\"revision\":" + own.m_Rev + "}");
				sv.m_PathOf.Set(own.m_Key, path);
				DZS_Log.Info("dzbot: save deletes the emptied " + type + " delta of " + path);
			}
			return;
		}
		string entry;
		string key;
		if (own)
		{
			key = own.m_Key;
			entry = "{\"key\":" + DZS_DzbotFrame.Q(key) + ",\"revision\":" + own.m_Rev;
			string rv = own.m_Rev.ToString();
			DZS_DzbotSection fs = DZS_DzbotSection.FromChunks("file", key, rv, chunks);
			sv.m_Parts.Insert(fs);
		}
		else
		{
			key = NewKey();
			string qk = DZS_DzbotFrame.Q(key);
			string qp = DZS_DzbotFrame.Q(path);
			string qt = DZS_DzbotFrame.Q(type);
			entry = "{\"key\":" + qk + ",\"path\":" + qp + ",\"type\":" + qt + ",\"managed\":true";
			DZS_DzbotSection ns = DZS_DzbotSection.FromChunks("file", key, "-", chunks);
			sv.m_Parts.Insert(ns);
			DZS_DzbotLootFile nf = new DZS_DzbotLootFile();
			nf.m_Key = key;
			nf.m_Path = path;
			nf.m_Type = type;
			sv.m_NewLoot.Insert(nf);
		}
		sv.m_Entries.Insert(entry);
		sv.m_Targets.Insert("");
		sv.m_PathOf.Set(key, path + " (" + type + ")");
		int bytes = DZS_Files.Length(chunks);
		DZS_Log.Info("dzbot: save " + type + " delta of " + path + ", " + bytes + " bytes");
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
			string folder = m_Save.m_Folder;
			EndSave();
			Projects().ProviderSaved("Speichern unklar — der Stand wird aus dzbot neu geladen", "Saving unclear — the state is loaded from dzbot again");
			DZS_DzbotProject dp = m_Projects.Get(RefOf(folder));
			if (dp)
			{
				string fields = "\"project\":" + DZS_DzbotFrame.Q(dp.m_Ref);
				AuthPost("main", "reload", "/editor/project/load", fields);
			}
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
		DZS_ProjectsPlugin pp = Projects();
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
			PlateFor(sv.m_Folder);
			m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("dzbot · nicht erreichbar", "dzbot · not reachable"), "danger");
			pp.ProviderSaved("dzbot nicht erreichbar", "dzbot not reachable");
			return;
		}
		DZS_DzbotProject dp = m_Projects.Get(RefOf(sv.m_Folder));
		if (!r.Ok())
		{
			EndSave();
			PlateFor(sv.m_Folder);
			if (err == "client_outdated")
			{
				pp.ProviderSaved("dzStudio ist veraltet — nichts gespeichert", "dzStudio is out of date — nothing saved");
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
						DZS_DzbotFile cf = dp.FileByKey(ck);
						if (cf)
							paths.Insert(cf.m_Path);
						else
							paths.Insert(ck);
					}
				}
				pp.ProviderSaved("In dzbot geändert — nichts gespeichert", "Changed in dzbot — nothing saved");
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
						pp.ProviderSaved("dzbot kann den Stand von " + bad + " nicht zusammensetzen — nichts gespeichert", "dzbot cannot compose " + bad + " — nothing saved");
						return;
					}
				}
				pp.ProviderSaved(bad + " kam in dzbot nicht richtig an", bad + " did not arrive correctly in dzbot");
				return;
			}
			string m = r.Message();
			pp.ProviderSaved("dzbot: " + m, "dzbot: " + m);
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
		for (int i = 0; i < sv.m_NewKeys.Count(); i++)
		{
			DZS_DzbotFile nf = new DZS_DzbotFile();
			nf.m_Key = sv.m_NewKeys[i];
			nf.m_Path = sv.m_NewPaths[i];
			nf.m_Role = "spawner";
			dp.m_Files.Insert(nf);
		}
		DZS_DzbotLootState lst = dp.m_Loot;
		if (lst)
		{
			foreach (DZS_DzbotLootFile nl : sv.m_NewLoot)
				lst.m_Files.Insert(nl);
			if (lst.m_NextRowKeys)
			{
				lst.m_RowKeys = lst.m_NextRowKeys;
				lst.m_RowText = lst.m_NextRowText;
				lst.m_DelKeys = lst.m_NextDelKeys;
				lst.m_Origin.m_OwnRows.Clear();
				foreach (string rk : lst.m_RowKeys)
					lst.m_Origin.m_OwnRows.Set(rk, true);
			}
			if (lst.m_NextTypes)
			{
				lst.m_Origin.m_OwnTypes = lst.m_NextTypes;
				lst.m_Origin.m_Origs = lst.m_NextOrigs;
			}
			lst.m_NextRowKeys = null;
			lst.m_NextTypes = null;
		}
		DZS_SrcNode dels = r.m_Json.Get("deleted");
		string deletedText = "";
		if (dels && dels.IsArr())
		{
			foreach (DZS_SrcNode dn : dels.m_Items)
			{
				string dk = dn.m_Str;
				deletedText += " " + dk;
				for (int di = dp.m_Files.Count() - 1; di >= 0; di--)
				{
					DZS_DzbotFile dfi = dp.m_Files[di];
					if (dfi.m_Key == dk)
						dp.m_Files.Remove(di);
				}
				if (lst)
				{
					for (int dj = lst.m_Files.Count() - 1; dj >= 0; dj--)
					{
						DZS_DzbotLootFile lfj = lst.m_Files[dj];
						if (lfj.m_Key == dk)
							lst.m_Files.Remove(dj);
					}
				}
			}
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
					if (lst)
					{
						DZS_DzbotLootFile lf = lst.ByKey(key);
						if (lf)
							lf.m_Rev = rev;
					}
				}
			}
		}
		string written = "";
		DZS_SrcNode wl = r.m_Json.Get("written");
		if (wl && wl.IsArr())
			written = wl.Count().ToString();
		DZS_Log.Info("dzbot: saved " + sv.m_SaveId + " after " + sv.m_Tries + " try/tries, written " + written + ", deleted" + deletedText + ", replayed " + r.Str("replayed") + ", revision " + dp.Revision());
		EndSave();
		PlateFor(sv.m_Folder);
		pp.ProviderSaved("", "");
	}
}
