// "Edit together": sessions on a server that speaks the collaboration protocol,
// through the sync slot of the kernel. The plugin sits in the sync slot; the server decides the
// order of operations, the locks and the colours.
class DZS_CollabLink : DZS_CollabListener
{
	DZS_CollabPlugin m_P;

	override void OnCollab(DZS_CollabCall call, DZS_CollabReply reply)
	{
		m_P.OnReply(call, reply);
	}
}

class DZS_CollabSlot : DZS_SyncSlot
{
	DZS_CollabPlugin m_P;

	override void OnOperation(array<string> text, DZS_Origin origin, string opId)
	{
		m_P.Outgoing(text, origin, opId);
	}

	override void OnRejected(DZS_SubmitResult result)
	{
		DZS_Log.Info("collab: remote operation not applied: " + result.m_ReasonEn);
		if (result.m_Status == DZS_SubmitStatus.UNKNOWN_TYPE)
			m_P.NeedsNewer();
	}
}

class DZS_CollabActions : DZS_ActionHandler
{
	DZS_CollabPlugin m_P;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		return m_P.ActionEnabled(actionId, reasonDe, reasonEn);
	}

	override void OnAction(string actionId)
	{
		m_P.OnAction(actionId);
	}

	override string GetActionLabel(string actionId)
	{
		return m_P.ActionLabel(actionId);
	}

	override bool IsActionListed(string actionId)
	{
		return m_P.ActionListed(actionId);
	}
}

class DZS_CollabRules : DZS_SaveRules
{
	DZS_CollabPlugin m_P;

	override bool StatusText(DZS_Instance inst, out string text, out string lamp) { return m_P.RuleStatus(inst, text, lamp); }
	override bool SaveBlocked(DZS_Instance inst, out string de, out string en) { return m_P.RuleSaveBlocked(inst, de, en); }
	override bool SaveAsBlocked(DZS_Instance inst, out string de, out string en) { return m_P.RuleSaveAsBlocked(inst, de, en); }
	override bool NoLocalWork(DZS_Instance inst) { return m_P.IsGuestOf(inst) || m_P.IsHostedOf(inst); }
	override bool NoAutoSave(DZS_Instance inst) { return m_P.IsHostedOf(inst); }
	override bool Unsaved(DZS_Instance inst, out bool unsaved) { return m_P.RuleUnsaved(inst, unsaved); }
	override float AutoSaveLatest(DZS_Instance inst)
	{
		if (m_P.IsHostOf(inst))
			return 60;
		return 0;
	}
	override bool InterceptLeave(string what, string arg, string arg2) { return m_P.InterceptLeave(what, arg, arg2); }
	override void Saved(DZS_Instance inst, bool ok) { m_P.HostSaved(inst, ok); }
	override void Saving(DZS_Instance inst) { m_P.HostSaving(inst); }
	override int StateSeq(DZS_Instance inst) { return m_P.StateSeqOf(inst); }
}

class DZS_CollabGuard : DZS_SelectGuard
{
	DZS_CollabPlugin m_P;

	override bool Blocks(string id) { return m_P.HeldByOther(id); }
	override string HolderName(string id) { return m_P.HolderName(id); }
	override int HolderColor(string id) { return m_P.HolderColor(id); }
	override void Held(array<string> ids, array<int> colors, array<bool> lapsed) { m_P.CollectHeld(ids, colors, lapsed); }
	override void SelectionChanged(array<string> ids) { m_P.WantLocks(ids); }
	override string WaitReason(array<string> ids) { return m_P.ClaimWait(ids); }
	override bool ClaimForStep(DZS_Instance inst, array<string> ids, bool redo) { return m_P.ClaimStep(inst, ids, redo); }
}

class DZS_CollabGate : DZS_EditGate
{
	DZS_CollabPlugin m_P;

	override bool ActionBlocked(string actionId, out string de, out string en) { return m_P.GateAction(actionId, de, en); }
	override bool ViewOnly(DZS_Instance inst, out string de, out string en) { return m_P.GateViewOnly(inst, de, en); }
	override int MaxStepBytes(DZS_Instance inst) { return m_P.MaxStep(inst); }
	override void StepRefused(DZS_Instance inst, int bytes, int limit) { m_P.StepTooLarge(bytes, limit); }
}

class DZS_CollabLive : DZS_LiveFeed
{
	DZS_CollabPlugin m_P;

	override void OnGesture(array<string> ids, array<vector> pos, array<vector> ypr, array<float> scale) { m_P.LiveGesture(ids, pos, ypr, scale); }
	override void OnEnd(bool commit) { m_P.LiveEnd(commit); }
}

class DZS_CollabBannerKnobs : DZS_MessageListener
{
	DZS_CollabPlugin m_P;

	override void OnMessageButton(string kind) { m_P.OnBannerKnob(kind); }
}

// Another participant's object as the display shows it while his gesture runs (200 ms gliding).
class DZS_CollabLiveObj : Managed
{
	string m_Id;
	string m_Pid;
	vector m_FromPos;
	vector m_FromYpr;
	float m_FromScale;
	vector m_ToPos;
	vector m_ToYpr;
	float m_ToScale;
	float m_T;
	bool m_Back;
}

// A piece of an operation on its way: a whole small one, a part (over 1 MB) or a link of a chain (over 3.5 MB).
class DZS_CollabUnit : Managed
{
	string m_OpId;
	bool m_System;
	string m_LiveId;
	int m_ChainIndex = -1;
	int m_ChainCount;
	int m_Part = -1;
	int m_Parts;
	ref array<string> m_Pieces;
	int m_Bytes;
	int m_ClientSeq;
}

[DZS_RegisterPlugin(DZS_CollabPlugin)]
class DZS_CollabPlugin : DZS_Plugin
{
	static const string PANEL = "collab.panel";
	static const string MSG_PEER = "collab.peer";
	static const string MSG_LOCK = "collab.lock";
	static const string MSG = "collab";
	static const float PART_BYTES = 1000000;

	ref array<ref DZS_CollabServer> m_Servers = new array<ref DZS_CollabServer>();
	ref DZS_CollabSession m_S;
	protected ref DZS_CollabNet m_Net;
	protected ref DZS_CollabLink m_Link;
	protected ref DZS_CollabSlot m_Slot;
	protected ref DZS_CollabActions m_Actions;
	protected ref DZS_CollabRules m_Rules;
	protected ref DZS_CollabGuard m_Guard;
	protected ref DZS_CollabGate m_Gate;
	protected ref DZS_CollabLive m_Live;
	protected ref DZS_CollabBannerKnobs m_Knobs;
	ref DZS_CollabPanel m_Panel;
	ref DZS_CollabPage m_Page;
	protected ref DZS_StatusText m_Segment;
	protected ref map<string, ref DZS_CollabNet> m_InfoNets = new map<string, ref DZS_CollabNet>();
	ref DZS_CollabInviteDialog m_InviteDialog;
	ref DZS_CollabCodeDialog m_CodeDialog;
	protected bool m_Starting;
	float m_Now;
	protected float m_PollRetryAt;
	protected bool m_LeftWas;
	protected bool m_World;
	// sign-in with a code
	string m_LoginServer;
	string m_LoginCode;
	string m_LoginUrl;
	string m_LoginState;
	protected float m_LoginNext;
	protected float m_LoginEnds;
	protected float m_LoginEvery = 3;
	// pending start (dialog open)
	string m_InviteCode;
	string m_InviteState;
	protected string m_LeaveWhat;
	protected string m_LeaveArg;
	protected string m_LeaveArg2;
	ref DZS_CollabHosted m_Hosted;
	ref DZS_CollabAttach m_Attach;

	override string GetName() { return "collab"; }

	ref DZS_CollabSaveTarget m_Target;
	// Save As target: a new server project being made from the open one
	protected ref DZS_CollabHostedProj m_Creating;

	DZS_Kernel Kernel() { return m_Kernel; }

	override void OnRegister()
	{
		DZS_Settings st = m_Kernel.GetSettings();
		st.Define("collab.servers", DZS_Json.Arr());
		st.Define("collab.tokens", DZS_Json.Obj());
		st.RegisterTab("collab", "Gemeinsam bearbeiten", "Edit together");
		m_Page = new DZS_CollabPage();
		m_Page.m_P = this;
		st.RegisterPage("collab", m_Page);
		m_Hosted = new DZS_CollabHosted(this);
		m_Attach = new DZS_CollabAttach(this);
		LoadServers();
		m_Link = new DZS_CollabLink();
		m_Link.m_P = this;
		m_Actions = new DZS_CollabActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterAction("collab", "collab.start", "Gemeinsam bearbeiten …", "Edit together …", null, m_Actions);
		ui.RegisterAction("collab", "collab.join", "Sitzung beitreten …", "Join session …", null, m_Actions);
		ui.RegisterAction("collab", "collab.invite", "Einladen …", "Invite …", null, m_Actions);
		ui.RegisterAction("collab", "collab.leave", "Sitzung verlassen", "Leave session", null, m_Actions);
		ui.RegisterAction("collab", "collab.end", "Sitzung beenden", "End session", null, m_Actions);
		ui.RegisterAction("collab", "collab.copy", "Kopie speichern …", "Save a copy …", null, m_Actions);
		ui.RegisterAction("collab", "collab.discard", "Ungespeicherte Änderungen verwerfen …", "Discard unsaved changes …", null, m_Actions);
		ui.SetWhen("collab.start", "scene");
		ui.SetWhen("collab.join", "scene");
		ui.SetWhen("collab.invite", "scene");
		ui.SetWhen("collab.leave", "scene");
		ui.SetWhen("collab.end", "scene");
		ui.SetWhen("collab.copy", "scene");
		ui.SetWhen("collab.discard", "scene");
		DZS_HomeButtons.Add("collab.join", "Beitreten …", "Join …", "users");
		ui.AddMenuEntry("project", "collab.start", 460);
		ui.AddMenuEntry("project", "collab.join", 461);
		ui.AddMenuEntry("project", "collab.invite", 462);
		ui.AddMenuEntry("project", "collab.leave", 463);
		ui.AddMenuEntry("project", "collab.end", 464);
		ui.AddMenuEntry("project", "collab.copy", 465);
		ui.AddMenuEntry("project", "collab.discard", 466);
		m_Panel = new DZS_CollabPanel();
		m_Panel.m_P = this;
		ui.RegisterPanel("collab", PANEL, "Gemeinsam", "Together", "right", "users", m_Panel, 980);
		ui.SetPanelShown(PANEL, false);
		m_Rules = new DZS_CollabRules();
		m_Rules.m_P = this;
		DZS_SaveRules.Set(m_Rules);
		m_Guard = new DZS_CollabGuard();
		m_Guard.m_P = this;
		DZS_SelectGuard.Set(m_Guard);
		m_Gate = new DZS_CollabGate();
		m_Gate.m_P = this;
		DZS_EditGate.Set(m_Gate);
		m_Live = new DZS_CollabLive();
		m_Live.m_P = this;
		DZS_LiveFeed.Set(m_Live);
		m_Knobs = new DZS_CollabBannerKnobs();
		m_Knobs.m_P = this;
		m_Target = new DZS_CollabSaveTarget();
		m_Target.m_P = this;
		DZS_SaveTargets.Set(m_Target);
		DZS_Log.Info("collab: plugin, " + m_Servers.Count() + " server(s)");
	}

	override void OnSessionStart(string world)
	{
		m_World = true;
		m_Segment = new DZS_StatusText("collab.segment", DZS_Font.MONO);
		m_Kernel.GetScreen().AddStatusLeft(m_Segment);
		m_Segment.SetText("");
		foreach (DZS_CollabServer sv : m_Servers)
			AskInfo(sv);
		map<string, string> carry = DZS_Handover.Carry();
		if (carry.Contains("sessionId"))
			FromCarry(carry);
	}

	override void OnSessionEnd()
	{
		m_World = false;
		if (m_S)
			EndLocally("world");
	}

	// ---- servers and sign-in ----

	protected void LoadServers()
	{
		m_Servers.Clear();
		DZS_Settings st = m_Kernel.GetSettings();
		DZS_Json list = st.Get("collab.servers");
		DZS_Json tokens = st.Get("collab.tokens");
		string cli;
		if (GetCLIParam("dzsCollabUrl", cli) && cli != "")
			AddServerEntry(DZS_CollabNet.Trim(cli), tokens);
		if (list && list.IsArr())
		{
			for (int i = 0; i < list.Count(); i++)
			{
				DZS_Json e = list.At(i);
				string url = e.GetStr("url");
				if (url != "" && !FindServer(url))
				{
					AddServerEntry(url, tokens);
					string known = e.GetStr("name");
					if (known != "")
						FindServer(url).m_Name = known;
				}
			}
		}
		m_DefaultUrl = st.GetString("collab.default");
		if (m_DefaultUrl == "" && m_Servers.Count() > 0)
			m_DefaultUrl = m_Servers[0].m_Url;
	}

	protected int m_FixedRev = -1;
	protected string m_DefaultUrl;

	// Entries of plugins stand at the top of the list; their token comes from the sign-in plugin when the server
	// offers "provider" with its name, else they cannot sign in at all.
	protected void SyncFixed()
	{
		if (m_FixedRev != DZS_SyncServers.Rev())
		{
			m_FixedRev = DZS_SyncServers.Rev();
			array<DZS_SyncServer> fx = DZS_SyncServers.Fixed();
			foreach (DZS_SyncServer f : fx)
			{
				string url = DZS_CollabNet.Trim(f.m_Url);
				DZS_CollabServer have = FindServer(url);
				if (!have)
				{
					have = new DZS_CollabServer();
					have.m_Url = url;
					have.m_Name = url;
					m_Hosted.Register(url);
					if (m_World)
						AskInfo(have);
				}
				else
					m_Servers.RemoveItem(have);
				int top = 0;
				while (top < m_Servers.Count() && m_Servers[top].m_Fixed != "")
					top++;
				m_Servers.InsertAt(have, top);
				have.m_Fixed = f.m_Provider;
				DZS_Log.Info("collab: fixed server " + url + " of " + f.m_Provider);
			}
		}
		foreach (DZS_CollabServer s : m_Servers)
		{
			if (s.m_Fixed == "")
				continue;
			if (!s.m_AuthProvider)
			{
				s.m_Token = "";
				s.m_User = "";
				continue;
			}
			DZS_AuthProvider ap = DZS_SyncServers.Auth(s.m_Fixed);
			string tok = "";
			string who = "";
			if (ap)
			{
				tok = ap.Token(s.m_Url);
				who = ap.UserName();
			}
			bool fresh = s.m_Token == "" && tok != "";
			s.m_Token = tok;
			s.m_User = who;
			if (fresh && s.m_Hosted && s.m_State == "ok")
				m_Hosted.Refresh(s.m_Url);
		}
	}

	protected void AddServerEntry(string url, DZS_Json tokens)
	{
		DZS_CollabServer s = new DZS_CollabServer();
		s.m_Url = url;
		s.m_Name = url;
		m_Hosted.Register(url);
		if (tokens && tokens.IsObj() && tokens.Has(url))
		{
			DZS_Json t = tokens.Get(url);
			s.m_Token = t.GetStr("token");
			s.m_User = t.GetStr("name");
		}
		m_Servers.Insert(s);
	}

	DZS_CollabServer FindServer(string url)
	{
		foreach (DZS_CollabServer s : m_Servers)
		{
			if (s.m_Url == url)
				return s;
		}
		return null;
	}

	DZS_CollabServer DefaultServer()
	{
		DZS_CollabServer s = FindServer(m_DefaultUrl);
		if (s)
			return s;
		if (m_Servers.Count() == 0)
			return null;
		return m_Servers[0];
	}

	bool IsDefault(DZS_CollabServer s)
	{
		return s && DefaultServer() == s;
	}

	// "Remove server": only entries added by hand; the token goes with it, no question.
	void RemoveServer(DZS_CollabServer s)
	{
		if (!s || s.m_Fixed != "")
			return;
		int i = m_Servers.Find(s);
		if (i < 0)
			return;
		m_Servers.RemoveOrdered(i);
		StoreServers();
		DZS_Log.Info("collab: server removed " + s.m_Url);
	}

	void MakeDefault(DZS_CollabServer s)
	{
		if (!s || IsDefault(s))
			return;
		m_DefaultUrl = s.m_Url;
		StoreServers();
	}

	void StoreServers()
	{
		DZS_Json list = DZS_Json.Arr();
		DZS_Json tokens = DZS_Json.Obj();
		foreach (DZS_CollabServer s : m_Servers)
		{
			if (s.m_Fixed != "")
				continue;
			DZS_Json e = DZS_Json.Obj();
			e.Set("url", DZS_Json.Str(s.m_Url));
			if (s.m_Name != s.m_Url)
				e.Set("name", DZS_Json.Str(s.m_Name));
			list.Add(e);
			if (s.m_Token != "" && !s.m_AuthProvider)
			{
				DZS_Json t = DZS_Json.Obj();
				t.Set("token", DZS_Json.Str(s.m_Token));
				t.Set("name", DZS_Json.Str(s.m_User));
				tokens.Set(s.m_Url, t);
			}
		}
		DZS_Settings st = m_Kernel.GetSettings();
		st.Set("collab.servers", list);
		st.Set("collab.tokens", tokens);
		DZS_CollabServer def = DefaultServer();
		if (def)
			st.Set("collab.default", DZS_Json.Str(def.m_Url));
		st.Flush();
	}

	protected DZS_CollabNet NetFor(string url)
	{
		DZS_CollabNet n = m_InfoNets.Get(url);
		if (n)
			return n;
		n = new DZS_CollabNet(url, m_Link);
		m_InfoNets.Set(url, n);
		return n;
	}

	void AskInfo(DZS_CollabServer s)
	{
		if (DZS_Version.IsBlocked(VersionKey(s.m_Url)))
			return;
		DZS_CollabNet n = NetFor(s.m_Url);
		n.Info("info", s);
	}

	// "Add server …": the server is taken only if it speaks protocol version 1.
	string m_AddUrl;
	string m_AddResult;

	void TryAdd(string url)
	{
		m_AddUrl = DZS_CollabNet.Trim(url);
		m_AddResult = "asking";
		DZS_CollabServer probe = new DZS_CollabServer();
		probe.m_Url = m_AddUrl;
		probe.m_Asked = true;
		DZS_CollabNet n = NetFor(m_AddUrl);
		n.Info("add", probe);
	}

	protected void OnInfo(DZS_CollabCall c, DZS_CollabReply r)
	{
		DZS_CollabServer s = DZS_CollabServer.Cast(c.m_Data);
		if (!s)
			return;
		if (!r.m_Framed)
			s.m_State = "unreachable";
		else if (!r.Ok())
			s.m_State = "unreachable";
		else
		{
			s.m_State = "old";
			DZS_SrcNode protos = r.N("protocols");
			if (protos && protos.IsArr())
			{
				foreach (DZS_SrcNode p : protos.m_Items)
				{
					int major = DZS_CollabJ.I(p, "major");
					if (major == 1)
						s.m_State = "ok";
					else if (major > 1 && s.m_State != "ok")
						s.m_State = "newer";
				}
			}
			DZS_SrcNode sv = r.N("server");
			string nm = DZS_CollabJ.S(sv, "name");
			if (nm != "" && nm != s.m_Name)
			{
				s.m_Name = nm;
				if (c.m_Tag == "info" && FindServer(s.m_Url) == s && s.m_Fixed == "")
					StoreServers();
			}
			s.m_Notice = r.S("notice");
			array<string> caps = DZS_CollabJ.Strings(r.N("capabilities"));
			s.m_Users = caps.Find("users") >= 0;
			s.m_InviteUser = caps.Find("invite.user") >= 0;
			s.m_Hosted = caps.Find("hosted") >= 0;
			s.m_AuthCode = false;
			s.m_AuthName = false;
			s.m_AuthProvider = false;
			DZS_SrcNode auth = r.N("auth");
			if (auth && auth.IsArr())
			{
				foreach (DZS_SrcNode a : auth.m_Items)
				{
					string m = DZS_CollabJ.S(a, "method");
					if (m == "provider" && s.m_Fixed != "" && DZS_CollabJ.S(a, "provider") == s.m_Fixed)
						s.m_AuthProvider = true;
					if (m == "code")
						s.m_AuthCode = true;
					else if (m == "name")
						s.m_AuthName = true;
				}
			}
			DZS_SrcNode lim = r.N("limits");
			int mr = DZS_CollabJ.I(lim, "maxRequestBytes");
			if (mr > 0)
				s.m_MaxRequest = mr;
			s.m_MinClient = r.S("minClientVersion");
			s.m_RecClient = r.S("recommendedClientVersion");
			s.m_UpdateUrl = r.S("updateUrl");
			if (DZS_Version.Below(s.m_MinClient))
				DZS_Version.MarkOutdated(VersionKey(s.m_Url), s.m_Name, s.m_MinClient, s.m_UpdateUrl);
			else if (s.m_RecClient != "")
				DZS_Version.Recommended(s.m_RecClient, s.m_UpdateUrl);
		}
		DZS_Log.Info("collab: info " + s.m_Url + " → " + s.m_State + " (" + s.m_Name + ")");
		if (c.m_Tag == "info" && s.m_State == "ok" && s.m_Hosted && s.m_Token != "")
			m_Hosted.Refresh(s.m_Url);
		if (c.m_Tag == "add")
		{
			if (!r.m_Framed)
				m_AddResult = "nothing";
			else if (s.m_State == "newer")
				m_AddResult = "newer";
			else if (s.m_State != "ok")
				m_AddResult = "nothing";
			else if (FindServer(s.m_Url))
				m_AddResult = "known";
			else
			{
				m_Servers.Insert(s);
				m_Hosted.Register(s.m_Url);
				StoreServers();
				m_AddResult = "added";
			}
		}
	}

	static string VersionKey(string url)
	{
		return "collab:" + url;
	}

	static string ClientJson()
	{
		return "\"client\":{\"name\":\"dzstudio\",\"version\":" + DZS_CollabJ.Q(DZS_Version.Get()) + "}";
	}

	// Before signing in, starting or joining: a server that refuses this version gets the dialog instead of a request.
	bool VersionStop(DZS_CollabServer s)
	{
		if (!s)
			return false;
		return DZS_Version.ShowIfBlocked(VersionKey(s.m_Url));
	}

	protected void OutdatedReply(string url, string name, DZS_CollabReply r)
	{
		DZS_Version.Outdated(VersionKey(url), name, r.ErrS("minVersion"), r.ErrS("updateUrl"), true);
	}

	string LampText(DZS_CollabServer s, out string tone)
	{
		tone = "";
		if (DZS_Version.IsBlocked(VersionKey(s.m_Url)))
		{
			tone = "danger";
			return DZS_Texts.Pick("dzStudio veraltet", "dzStudio out of date");
		}
		if (s.m_State == "unreachable")
		{
			tone = "danger";
			return DZS_Texts.Pick("nicht erreichbar", "not reachable");
		}
		if (s.m_State == "old" || s.m_State == "newer")
		{
			tone = "danger";
			return DZS_Texts.Pick("Protokoll zu alt", "Protocol too old");
		}
		if (NoSignIn(s))
			return DZS_Texts.Pick("Anmeldung über " + s.m_Fixed + " gerade nicht möglich", "Sign-in via " + s.m_Fixed + " not possible right now");
		if (s.m_Token != "")
		{
			tone = "success";
			return DZS_Texts.Pick("angemeldet als " + s.m_User, "signed in as " + s.m_User);
		}
		return DZS_Texts.Pick("nicht angemeldet", "not signed in");
	}

	// A plugin's entry signs in only through that plugin.
	bool NoSignIn(DZS_CollabServer s)
	{
		return s.m_Fixed != "" && s.m_State == "ok" && !s.m_AuthProvider;
	}

	// "Sign in": the first way the server names that dzStudio speaks (code, else name).
	void SignIn(DZS_CollabServer s)
	{
		if (NoSignIn(s) || VersionStop(s))
			return;
		if (s.m_AuthProvider)
		{
			DZS_AuthProvider ap = DZS_SyncServers.Auth(s.m_Fixed);
			if (ap)
				ap.SignIn();
			return;
		}
		string pref;
		GetCLIParam("dzsCollabAuth", pref);
		if (s.m_AuthCode && pref != "name")
		{
			StartCodeLogin(s);
			return;
		}
		if (s.m_AuthName)
		{
			DZS_CollabNameDialog d = new DZS_CollabNameDialog(this, s);
			d.Open();
		}
	}

	void SignOut(DZS_CollabServer s)
	{
		if (s.m_AuthProvider)
		{
			DZS_AuthProvider ap = DZS_SyncServers.Auth(s.m_Fixed);
			if (ap)
				ap.SignOut();
			return;
		}
		s.m_Token = "";
		s.m_User = "";
		StoreServers();
	}

	void NameLogin(DZS_CollabServer s, string name)
	{
		string rq = DZS_CollabJ.Q(ReqId());
		string nq = DZS_CollabJ.Q(name);
		string body = "{\"protocol\":\"1.0\",\"requestId\":" + rq + ",\"name\":" + nq + "," + ClientJson() + "}";
		DZS_CollabNet n = NetFor(s.m_Url);
		n.Send("auth/name", "auth/name", body, s);
	}

	protected void StartCodeLogin(DZS_CollabServer s)
	{
		m_LoginServer = s.m_Url;
		m_LoginCode = "";
		m_LoginUrl = "";
		m_LoginState = "start";
		string body = "{\"protocol\":\"1.0\"," + ClientJson() + "}";
		DZS_CollabNet n = NetFor(s.m_Url);
		n.Send("auth/code/start", "auth/code/start", body, s);
		m_CodeDialog = new DZS_CollabCodeDialog(this, s);
		m_CodeDialog.Open();
	}

	void CancelCodeLogin()
	{
		m_LoginServer = "";
		m_LoginState = "";
	}

	protected void UpdateCodeLogin()
	{
		if (m_LoginServer == "" || m_LoginState != "waiting")
			return;
		if (m_Now >= m_LoginEnds)
		{
			m_LoginState = "expired";
			return;
		}
		if (m_Now < m_LoginNext)
			return;
		m_LoginNext = m_Now + m_LoginEvery;
		DZS_CollabServer s = FindServer(m_LoginServer);
		if (!s)
			return;
		DZS_CollabNet n = NetFor(s.m_Url);
		if (n.SendBusy() || n.Waiting() > 0)
			return;
		string body = "{\"protocol\":\"1.0\",\"loginCode\":" + DZS_CollabJ.Q(m_LoginCode) + "}";
		n.Send("auth/code/poll", "auth/code/poll", body, s);
	}

	protected void OnAuth(DZS_CollabCall c, DZS_CollabReply r)
	{
		DZS_CollabServer s = DZS_CollabServer.Cast(c.m_Data);
		if (!s)
			return;
		if (c.m_Tag == "auth/name")
		{
			if (r.Ok())
			{
				s.m_Token = r.S("token");
				s.m_User = r.S("name");
				StoreServers();
				if (s.m_Hosted && s.m_State == "ok")
					m_Hosted.Refresh(s.m_Url);
			}
			else if (r.Code() == "client_outdated")
				OutdatedReply(s.m_Url, s.m_Name, r);
			else
				Msg("danger", "Anmeldung abgelehnt", "Sign-in declined");
			return;
		}
		if (c.m_Tag == "auth/code/start")
		{
			if (!r.Ok())
			{
				m_LoginState = "failed";
				if (r.Code() == "client_outdated")
				{
					if (m_CodeDialog)
						m_CodeDialog.Close();
					CancelCodeLogin();
					OutdatedReply(s.m_Url, s.m_Name, r);
				}
				return;
			}
			m_LoginCode = r.S("loginCode");
			m_LoginUrl = r.S("url");
			int every = r.I("intervalSeconds");
			if (every < 2)
				every = 2;
			m_LoginEvery = every;
			m_LoginEnds = m_Now + r.I("expiresInSeconds");
			m_LoginNext = m_Now + every;
			m_LoginState = "waiting";
			return;
		}
		if (c.m_Tag == "auth/code/poll" && m_LoginState == "waiting")
		{
			if (!r.Ok())
				return;
			string state = r.S("state");
			if (state == "confirmed")
			{
				s.m_Token = r.S("token");
				s.m_User = r.S("name");
				StoreServers();
				if (s.m_Hosted && s.m_State == "ok")
					m_Hosted.Refresh(s.m_Url);
				m_LoginState = "confirmed";
				if (m_CodeDialog)
					m_CodeDialog.Close();
				m_CodeDialog = null;
			}
			else if (state == "expired")
				m_LoginState = "expired";
			else if (state == "denied")
				m_LoginState = "denied";
		}
	}

	static string FormatCode(string code)
	{
		if (code.Length() != 8)
			return code;
		string a = code.Substring(0, 4);
		string b = code.Substring(4, 4);
		return a + "-" + b;
	}

	static string ReqId()
	{
		DZS_Random.EnsureSeeded();
		string abc = "0123456789abcdefghijklmnopqrstuvwxyz";
		string s = "";
		for (int i = 0; i < 16; i++)
		{
			int k = Math.RandomInt(0, 36);
			s += abc.Substring(k, 1);
		}
		return s;
	}

	// ---- actions and menu ----

	DZS_Instance Shown() { return m_Kernel.GetShown(); }

	bool InSession() { return m_S != null; }

	bool ActionEnabled(string id, out string de, out string en)
	{
		if (id == "collab.start")
		{
			if (m_S)
			{
				de = "Die Sitzung läuft schon";
				en = "The session is already running";
				return false;
			}
			if (!Shown())
			{
				de = "Kein Projekt offen";
				en = "No project open";
				return false;
			}
			return !m_Starting;
		}
		if (id == "collab.join")
		{
			if (m_S)
			{
				de = "Erst die Sitzung verlassen";
				en = "Leave the session first";
				return false;
			}
			return true;
		}
		if (id == "collab.invite")
		{
			if (!m_S || !m_S.m_Ready)
				return false;
			if (!m_S.m_CanInvite)
			{
				de = "Nur wer Rechte am Projekt hat, lädt ein";
				en = "Only participants with rights to the project can invite";
				return false;
			}
			return true;
		}
		if (id == "collab.leave")
			return m_S != null && !m_S.IsHost();
		if (id == "collab.end")
			return m_S != null && m_S.IsHost();
		if (id == "collab.copy")
		{
			if (LootWaiting())
			{
				de = "Neue Loot-Dateien werden geholt …";
				en = "Fetching new loot files …";
				return false;
			}
			return m_S != null && m_S.m_Ready && Shown() != null;
		}
		if (id == "collab.discard")
			return m_S != null && m_S.m_Ready && m_S.IsHosted() && m_S.m_CanManage && m_S.m_SaveUnsaved > 0;
		return true;
	}

	// While a session runs, "Edit together …" is not shown; the session's entries stand in its place.
	bool ActionListed(string id)
	{
		if (id == "collab.start")
			return m_S == null;
		if (id == "collab.invite" || id == "collab.copy")
			return m_S != null;
		if (id == "collab.leave")
			return m_S != null && !m_S.IsHost();
		if (id == "collab.end")
			return m_S != null && m_S.IsHost();
		if (id == "collab.discard")
			return m_S != null && m_S.IsHosted() && m_S.m_CanManage;
		return true;
	}

	string ActionLabel(string id)
	{
		return "";
	}

	void OnAction(string id)
	{
		if (id == "collab.start")
		{
			if (m_S)
				return;
			DZS_CollabServer s = DefaultServer();
			if (!s)
			{
				m_Kernel.GetScreen().OpenSettings();
				return;
			}
			if (VersionStop(s))
				return;
			DZS_CollabStartDialog d = new DZS_CollabStartDialog(this, s);
			d.Open();
		}
		else if (id == "collab.invite")
			OpenInvite();
		else if (id == "collab.end")
			AskEnd("", "", "");
		else if (id == "collab.leave")
			LeaveSession();
		else if (id == "collab.join")
			OpenJoin();
		else if (id == "collab.copy")
		{
			DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
			if (pp)
				pp.OpenCopyDialog();
		}
		else if (id == "collab.discard")
			AskDiscardHosted();
	}

	void Msg(string tone, string de, string en, float seconds = 2.0, string kind = MSG)
	{
		string t = DZS_Texts.Pick(de, en);
		m_Kernel.GetScreen().ShowMessage(kind, tone, DZS_W.Literal(t), seconds);
	}

	// ---- starting a session of a local project ----

	void StartLocal(DZS_CollabServer s)
	{
		DZS_Instance inst = Shown();
		if (!inst || m_S || m_Starting)
			return;
		if (s.m_Token == "")
		{
			Msg("danger", "Nicht angemeldet — in den Einstellungen anmelden", "Not signed in — sign in in the settings", 3);
			return;
		}
		if (VersionStop(s))
			return;
		BeginStart(inst, s, ProjectName(), MapKey(), null);
	}

	// A project held by the server, just loaded: the session starts before anything else (no question, no invite).
	protected ref DZS_CollabHostedProj m_StartProj;

	void StartHosted(DZS_CollabHostedProj p)
	{
		DZS_Instance inst = Shown();
		DZS_CollabServer s = FindServer(p.m_Server);
		if (!inst || m_S || m_Starting || !s)
			return;
		if (s.m_Token == "")
		{
			HostedStartFailed(DZS_Texts.Pick("Nicht angemeldet — in den Einstellungen anmelden", "Not signed in — sign in in the settings"));
			return;
		}
		if (VersionStop(s))
		{
			HostedStartFailed("");
			return;
		}
		BeginStart(inst, s, p.m_Name, p.m_Map, p);
	}

	// Servers with a store that lets this user create a project.
	void SaveTargets(array<string> ids, array<string> names)
	{
		if (m_S || m_Starting)
			return;
		foreach (DZS_CollabServer s : m_Servers)
		{
			if (!s.m_Hosted || s.m_Token == "" || s.m_State != "ok")
				continue;
			m_Hosted.m_Creatable.Remove(s.m_Url);
			m_Hosted.Refresh(s.m_Url);
			ids.Insert(s.m_Url);
			names.Insert(s.m_Name);
		}
	}

	string SaveTargetState(string url)
	{
		if (m_Hosted.m_Loading.Contains(url))
			return "loading";
		if (m_Hosted.m_Errors.Get(url) != "")
			return "unreachable";
		if (m_Hosted.CreatableStore(url) == "")
			return "none";
		return "";
	}

	// Creating a server project: session/create hosted with a new hostedRef and an empty revision,
	// the own state as stand 0, then project/save under the new name; the open project moves there only on success.
	void SaveToServer(string url, string name)
	{
		DZS_Instance inst = Shown();
		DZS_CollabServer s = FindServer(url);
		if (!inst || !s || m_S || m_Starting)
			return;
		if (s.m_Token == "")
		{
			Msg("danger", "Nicht angemeldet — in den Einstellungen anmelden", "Not signed in — sign in in the settings", 3);
			return;
		}
		DZS_CollabHostedProj p = new DZS_CollabHostedProj();
		p.m_Server = url;
		p.m_RefProvider = m_Hosted.CreatableStore(url);
		if (p.m_RefProvider == "")
		{
			Msg("danger", "Dieser Server lässt dich kein Projekt anlegen", "This server does not let you create a project", 3);
			return;
		}
		DZS_Random.EnsureSeeded();
		string abc = "abcdefghijklmnopqrstuvwxyz0123456789";
		string key = "";
		for (int i = 0; i < 16; i++)
			key += abc.Get(Math.RandomInt(0, 36));
		string forced;
		if (GetCLIParam("dzsTestHostedKey", forced) && forced != "")
			key = forced;
		p.m_RefKey = key;
		p.m_Name = name;
		p.m_Map = MapKey();
		p.m_Rev = "";
		p.m_CanSave = true;
		p.m_Folder = DZS_CollabHosted.FolderOf(url, p.m_RefProvider, key);
		m_Creating = p;
		DZS_Log.Info("collab: new server project " + name + " at " + url + ", key " + key);
		BeginStart(inst, s, name, p.m_Map, p);
	}

	protected void CreateFailed(string why)
	{
		DZS_CollabHostedProj p = m_Creating;
		m_Creating = null;
		StartFailed("");
		if (why != "")
			m_Kernel.GetScreen().ShowMessage(MSG, "danger", DZS_W.Literal(why), 4);
		if (p)
			DZS_LocalStore.Delete(p.m_Folder);
		DZS_Log.Warn("collab: new server project not made: " + why);
	}

	// The first project/save of a new server project ended (Hosted.EndSave).
	void CreateSaveEnded(bool ok)
	{
		DZS_CollabHostedProj p = m_Creating;
		if (!p)
			return;
		m_Creating = null;
		if (ok)
		{
			DZS_Log.Info("collab: server project " + p.m_Name + " made, open in session " + m_S.m_Id);
			return;
		}
		if (m_S)
		{
			string body = "{" + SessHead() + ",\"requestId\":" + DZS_CollabJ.Q(ReqId()) + "}";
			DZS_CollabNet n = NetFor(m_S.m_Server);
			n.Send("session/end", "session/end", body);
		}
		EndLocally("server project not made");
		DZS_LocalStore.Delete(p.m_Folder);
	}

	bool IsCreating() { return m_Creating != null; }

	protected void BeginStart(DZS_Instance inst, DZS_CollabServer s, string project, string mapKey, DZS_CollabHostedProj p)
	{
		m_Starting = true;
		m_StartProj = p;
		m_S = new DZS_CollabSession();
		m_S.m_Server = s.m_Url;
		m_S.m_ServerName = s.m_Name;
		m_S.m_Project = project;
		m_S.m_Map = mapKey;
		if (p)
		{
			m_S.m_Kind = "hosted";
			m_S.m_Folder = p.m_Folder;
		}
		m_S.m_Snap = inst.ExportState();
		SplitParts(m_S.m_Snap);
		m_Slot = new DZS_CollabSlot();
		m_Slot.m_P = this;
		inst.AttachSync(m_Slot);
		m_Slot.SetNoExternalSystemOps(true);
		MarkCounter(inst);
		m_Net = new DZS_CollabNet(s.m_Url, m_Link);
		int bytes = DZS_Files.Length(m_S.m_Snap);
		string rq = DZS_CollabJ.Q(ReqId());
		string pq = DZS_CollabJ.Q(m_S.m_Project);
		string mq = DZS_CollabJ.Q(m_S.m_Map);
		string kq = DZS_CollabJ.Q(inst.GetSessionCode());
		string hd = ServerHead(s);
		string body = "{" + hd + ",\"requestId\":" + rq + "," + ClientJson();
		body += ",\"projectName\":" + pq + ",\"map\":" + mq;
		if (p)
			body += ",\"kind\":\"hosted\",\"hostedRef\":" + p.RefJson() + ",\"hostedRevision\":" + DZS_CollabJ.Q(p.m_Rev);
		else
			body += ",\"kind\":\"local\"";
		body += ",\"instanceKey\":" + kq;
		body += ",\"snapshotBytes\":" + bytes + ",\"snapshotParts\":" + m_S.m_SnapPartsList.Count() + "}";
		m_Net.Send("session/create", "session/create", body);
		m_StartFailAt = m_Now + 12;
		DZS_Log.Info("collab: start, state " + bytes + " bytes in " + m_S.m_SnapPartsList.Count() + " part(s)");
	}

	protected float m_StartFailAt;

	protected void SplitParts(array<string> chunks)
	{
		m_S.m_SnapPartsList = PartsOf(chunks);
	}

	static array<ref array<string>> PartsOf(array<string> chunks)
	{
		array<ref array<string>> parts = new array<ref array<string>>();
		array<string> cur = new array<string>();
		int size = 0;
		foreach (string c : chunks)
		{
			string q = DZS_CollabJ.Q(c);
			int qb = q.Length() + 1;
			if (size + qb > PART_BYTES && cur.Count() > 0)
			{
				parts.Insert(cur);
				cur = new array<string>();
				size = 0;
			}
			cur.Insert(c);
			size += qb;
		}
		parts.Insert(cur);
		return parts;
	}

	// ---- checkpoints (a state with seq > 0 when poll asks for it) ----

	protected bool m_CpWanted;
	protected bool m_CpBusy;
	protected int m_CpSeq;
	protected int m_CpPart;
	protected ref array<ref array<string>> m_CpParts;

	// the text of the state for a checkpoint, written over several frames (the state as it was at its start)
	protected ref DZS_StateSlicer m_CpSlicer;
	protected DZS_Instance m_CpInst;

	protected void TryCheckpoint()
	{
		if (m_CpSlicer)
		{
			StepCheckpoint();
			return;
		}
		if (!m_CpWanted || m_CpBusy || !m_S || !m_S.m_Ready || m_S.m_Ended || !m_Net)
			return;
		if (m_S.m_Held.Count() > 0 || m_S.m_SentUnseen.Count() > 0 || m_S.m_Units.Count() > 0 || m_S.m_InFlightUnits.Count() > 0 || m_S.m_ChainBuf || m_S.m_PartBuf)
			return;
		DZS_Instance inst = Shown();
		if (!inst)
			return;
		m_CpSlicer = inst.BeginSlices();
		m_CpInst = inst;
		m_CpSeq = m_S.m_Applied;
		m_CpBusy = true;
		StepCheckpoint();
	}

	protected void StepCheckpoint()
	{
		DZS_Instance inst = m_CpInst;
		if (!inst || inst != Shown() || !m_CpBusy || !m_S || m_S.m_Ended)
		{
			if (inst)
				inst.EndSlices(m_CpSlicer);
			m_CpSlicer = null;
			m_CpInst = null;
			m_CpBusy = false;
			DZS_Log.Info("collab: checkpoint text stopped");
			return;
		}
		if (!m_CpSlicer.Step(DZS_Jobs.SLICE_MS))
			return;
		array<string> state = m_CpSlicer.m_Chunks;
		inst.EndSlices(m_CpSlicer);
		m_CpSlicer = null;
		m_CpInst = null;
		m_CpParts = PartsOf(state);
		m_CpPart = 0;
		DZS_Log.Info("collab: checkpoint at seq " + m_CpSeq + ", " + DZS_Files.Length(state) + " bytes in " + m_CpParts.Count() + " part(s), text written over several frames");
		SendCpPart();
	}

	protected void SendCpPart()
	{
		array<string> pieces = m_CpParts[m_CpPart];
		string list = DZS_CollabJ.List(pieces);
		string body = "{" + SessHead() + ",\"seq\":" + m_CpSeq + ",\"part\":" + m_CpPart + ",\"parts\":" + m_CpParts.Count() + ",\"pieces\":" + list + "}";
		m_Net.Send("checkpoint/put", "snapshot/put", body);
	}

	protected void OnCpPut(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (!m_S || !m_CpBusy)
			return;
		if (!r.m_Framed)
		{
			Down();
			m_Net.Repeat(c, 1);
			return;
		}
		Up();
		if (!r.Ok())
		{
			DZS_Log.Warn("collab: checkpoint at seq " + m_CpSeq + " refused: " + r.Code() + " " + r.ErrS("message"));
			m_CpBusy = false;
			m_CpParts = null;
			m_CpWanted = false;
			return;
		}
		if (r.B("complete") || m_CpPart + 1 >= m_CpParts.Count())
		{
			DZS_Log.Info("collab: checkpoint at seq " + m_CpSeq + " complete");
			m_CpBusy = false;
			m_CpParts = null;
			m_CpWanted = false;
			return;
		}
		m_CpPart++;
		SendCpPart();
	}

	string ProjectName()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (pp)
			return pp.ProjectName();
		return "";
	}

	string MapKey()
	{
		string w = m_Kernel.GetWorld();
		w.ToLower();
		if (w == "chernarusplus")
			return "chernarus";
		if (w == "enoch")
			return "livonia";
		return w;
	}

	protected string ServerHead(DZS_CollabServer s)
	{
		return "\"protocol\":\"1.0\",\"token\":" + DZS_CollabJ.Q(s.m_Token);
	}

	protected string SessHead()
	{
		DZS_CollabServer s = FindServer(m_S.m_Server);
		string tok = "";
		if (s)
			tok = s.m_Token;
		string tq = DZS_CollabJ.Q(tok);
		string sq = DZS_CollabJ.Q(m_S.m_Id);
		string pq = DZS_CollabJ.Q(m_S.m_Pid);
		return "\"protocol\":\"1.0\",\"token\":" + tq + ",\"sessionId\":" + sq + ",\"participantId\":" + pq;
	}

	protected void OnCreated(DZS_CollabReply r)
	{
		if (!m_S)
			return;
		if (!r.Ok())
		{
			string why = DZS_Texts.Pick("Server nicht erreichbar", "Server not reachable");
			string code = r.Code();
			if (code == "client_outdated")
			{
				why = "";
				OutdatedReply(m_S.m_Server, m_S.m_ServerName, r);
			}
			else if (code == "auth_required" || code == "token_invalid" || code == "token_expired")
				why = DZS_Texts.Pick("Nicht angemeldet", "Not signed in");
			else if (r.m_Framed)
				why = DZS_Texts.Pick("Keine Sitzung möglich: ", "Cannot start a session: ") + r.ErrS("message");
			DZS_Log.Warn("collab: start failed: " + code + " " + r.ErrS("message"));
			DZS_CollabHostedProj sp = m_StartProj;
			if (sp && code == "already_running")
			{
				StartFailed("");
				DZS_Log.Info("collab: a session already runs for " + sp.m_Name + " — joining it");
				JoinHosted(sp, r.ErrS("sessionId"));
				return;
			}
			if (m_Creating)
			{
				CreateFailed(why);
				return;
			}
			if (sp)
			{
				StartFailed("");
				HostedStartFailed(why);
				return;
			}
			StartFailed(why);
			return;
		}
		m_S.m_Id = r.S("sessionId");
		m_S.m_Pid = r.S("participantId");
		m_S.m_Role = r.S("role");
		m_S.m_Colour = r.I("colour");
		m_S.m_CanInvite = r.B("canInvite");
		m_S.m_CanSave = r.B("canSave");
		m_S.m_CanManage = r.B("canManage");
		m_S.m_SnapPart = 0;
		SendSnapPart();
	}

	// Loading or starting the session of a server's project failed: opening is undone (it closes again).
	protected void HostedStartFailed(string why)
	{
		DZS_Log.Warn("collab: hosted project closed again: " + why);
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (pp)
		{
			pp.CloseProject(true);
			pp.ShowHome(true);
		}
		m_Kernel.GetScreen().ShowMessage(MSG, "danger", DZS_W.Literal(why), 4);
	}

	protected void StartFailed(string why)
	{
		m_Starting = false;
		m_StartProj = null;
		if (why != "")
			m_Kernel.GetScreen().ShowMessage(MSG, "danger", DZS_W.Literal(why), 3);
		DetachSlot();
		if (m_Net)
			m_Net.DropAll();
		m_Net = null;
		m_S = null;
	}

	protected void SendSnapPart()
	{
		int part = m_S.m_SnapPart;
		array<string> pieces = m_S.m_SnapPartsList[part];
		int parts = m_S.m_SnapPartsList.Count();
		string list = DZS_CollabJ.List(pieces);
		string body = "{" + SessHead() + ",\"seq\":0,\"part\":" + part + ",\"parts\":" + parts + ",\"pieces\":" + list + "}";
		m_Net.Send("snapshot/put", "snapshot/put", body);
	}

	protected void OnSnapPut(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (!m_S)
			return;
		if (!r.m_Framed)
		{
			m_Net.Repeat(c);
			return;
		}
		if (!r.Ok())
		{
			string sfail = DZS_Texts.Pick("Keine Sitzung möglich: ", "Cannot start a session: ") + r.ErrS("message");
			if (m_Creating)
			{
				CreateFailed(sfail);
				return;
			}
			if (m_StartProj)
			{
				StartFailed("");
				HostedStartFailed(sfail);
				return;
			}
			StartFailed(sfail);
			return;
		}
		if (r.B("complete") || m_S.m_SnapPart + 1 >= m_S.m_SnapPartsList.Count())
		{
			m_S.m_Ready = true;
			m_Starting = false;
			bool hosted = m_StartProj != null;
			m_StartProj = null;
			m_S.m_Snap = null;
			m_S.m_SnapPartsList = null;
			DZS_Log.Info("collab: session " + m_S.m_Id + " ready, participant " + m_S.m_Pid + ", colour " + m_S.m_Colour);
			m_Kernel.GetUi().SetPanelShown(PANEL, true);
			if (m_Panel)
				m_Panel.Unfold();
			WantLocks(SelectionNow());
			if (hosted)
			{
				m_S.m_HostName = m_S.m_ServerName;
				m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("Gemeinsam · ", "Together · ") + m_S.m_ServerName, "success");
				if (m_Creating)
				{
					m_Hosted.Remember(m_Creating);
					DZS_ProjectsPlugin cpp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
					if (!cpp || !cpp.SaveAsToProvider(m_Creating.m_Name, m_Creating.m_Folder))
						CreateSaveEnded(false);
				}
				return;
			}
			OpenInvite();
			return;
		}
		m_S.m_SnapPart++;
		SendSnapPart();
	}

	array<string> SelectionNow()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel)
			return new array<string>();
		return sel.GetSelection();
	}

	// ---- operations ----

	static const int MAX_OP = 3670016;
	static const int MAX_CHAIN = 16;

	void Outgoing(array<string> text, DZS_Origin origin, string opId)
	{
		if (!m_S)
			return;
		DZS_CollabOut o = new DZS_CollabOut();
		o.m_OpId = opId;
		o.m_System = origin == DZS_Origin.SYSTEM;
		o.m_Pieces = text;
		if (m_LiveCommit != "" && origin == DZS_Origin.LOCAL)
		{
			o.m_LiveId = m_LiveCommit;
			m_LiveCommit = "";
		}
		m_Attach.BeforeOperation(text);
		m_S.m_Outbox.Insert(o);
		Split(o);
	}

	// Chain and parts: over 3.5 MB links of at most 3.5 MB, each over 1 MB in
	// parts of at most 1 MB; counted as the server writes the pieces.
	protected void Split(DZS_CollabOut o)
	{
		array<ref array<string>> links = new array<ref array<string>>();
		array<string> cur = new array<string>();
		int size = 0;
		foreach (string pc : o.m_Pieces)
		{
			string q = DZS_CollabJ.Q(pc);
			int qb = q.Length() + 1;
			if (size + qb > MAX_OP && cur.Count() > 0)
			{
				links.Insert(cur);
				cur = new array<string>();
				size = 0;
			}
			cur.Insert(pc);
			size += qb;
		}
		links.Insert(cur);
		if (links.Count() > MAX_CHAIN)
		{
			DZS_Log.Error("collab: operation " + o.m_OpId + " has " + links.Count() + " links, more than " + MAX_CHAIN);
			return;
		}
		for (int li = 0; li < links.Count(); li++)
		{
			array<ref array<string>> parts = PartsOf(links[li]);
			for (int pi = 0; pi < parts.Count(); pi++)
			{
				DZS_CollabUnit u = new DZS_CollabUnit();
				u.m_OpId = o.m_OpId;
				u.m_System = o.m_System;
				u.m_LiveId = o.m_LiveId;
				if (links.Count() > 1)
				{
					u.m_ChainIndex = li;
					u.m_ChainCount = links.Count();
				}
				if (parts.Count() > 1)
				{
					u.m_Part = pi;
					u.m_Parts = parts.Count();
				}
				u.m_Pieces = parts[pi];
				u.m_Bytes = DZS_CollabJ.PiecesBytes(u.m_Pieces);
				m_S.m_Units.Insert(u);
			}
		}
		if (links.Count() > 1 || m_S.m_Units.Count() > 1)
			DZS_Log.Info("collab: operation " + o.m_OpId + " in " + links.Count() + " link(s), " + m_S.m_Units.Count() + " unit(s) waiting");
	}

	protected void SendOps()
	{
		if (!m_S || !m_S.m_Ready || m_S.m_Ended || m_S.m_Units.Count() == 0 || m_S.m_InFlightUnits.Count() > 0 || m_S.m_Lost || m_S.m_Resuming)
			return;
		if (m_Net.SendBusy() || m_Net.Waiting() > 0)
			return;
		string ops = "[";
		int n = 0;
		int bytes = 0;
		while (m_S.m_Units.Count() > 0 && n < 200)
		{
			DZS_CollabUnit u = m_S.m_Units[0];
			if (n > 0 && (u.m_Part >= 0 || bytes + u.m_Bytes > 1048576))
				break;
			m_S.m_Units.RemoveOrdered(0);
			if (u.m_Part <= 0)
			{
				u.m_ClientSeq = m_S.m_NextClientSeq;
				m_S.m_NextClientSeq++;
			}
			else
				u.m_ClientSeq = m_S.m_NextClientSeq - 1;
			m_S.m_InFlightUnits.Insert(u);
			m_S.m_SentUnseen.Set(u.m_OpId, true);
			RemoveOut(u.m_OpId);
			if (n > 0)
				ops += ",";
			string list = DZS_CollabJ.List(u.m_Pieces);
			ops += "{\"clientSeq\":" + u.m_ClientSeq + ",\"opId\":" + DZS_CollabJ.Q(u.m_OpId);
			if (u.m_System)
				ops += ",\"system\":true";
			if (u.m_LiveId != "")
				ops += ",\"liveId\":" + DZS_CollabJ.Q(u.m_LiveId);
			if (u.m_Part >= 0)
				ops += ",\"part\":" + u.m_Part + ",\"parts\":" + u.m_Parts;
			if (u.m_ChainIndex >= 0)
				ops += ",\"chain\":{\"index\":" + u.m_ChainIndex + ",\"count\":" + u.m_ChainCount + "}";
			ops += ",\"pieces\":" + list + "}";
			n++;
			bytes += u.m_Bytes;
			if (u.m_Part >= 0)
				break;
		}
		ops += "]";
		string body = "{" + SessHead() + ",\"ops\":" + ops + "}";
		m_Net.Send("ops/send", "ops/send", body);
	}

	protected void RemoveOut(string opId)
	{
		for (int i = m_S.m_Outbox.Count() - 1; i >= 0; i--)
		{
			if (m_S.m_Outbox[i].m_OpId == opId)
				m_S.m_Outbox.RemoveOrdered(i);
		}
	}

	protected void OnOpsSent(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (!m_S)
			return;
		if (!r.m_Framed)
		{
			Down();
			m_Net.Repeat(c, 1);
			return;
		}
		Up();
		if (!r.Ok())
		{
			string code = r.Code();
			if (code == "busy" || code == "internal" || code == "throttled")
			{
				m_Net.Repeat(c, 1);
				return;
			}
			DZS_Log.Warn("collab: ops/send " + code + " expected " + r.ErrS("expectedClientSeq"));
			if (code == "session_ended" || code == "not_participant")
				HandleGone(r);
			if (m_S)
				m_S.m_InFlightUnits.Clear();
			return;
		}
		m_S.m_InFlightUnits.Clear();
	}

	// Unsent own changes (for "‹n› changes will be lost").
	int Unsent()
	{
		if (!m_S)
			return 0;
		map<string, bool> ids = new map<string, bool>();
		foreach (DZS_CollabUnit u : m_S.m_Units)
			ids.Set(u.m_OpId, true);
		foreach (DZS_CollabUnit f : m_S.m_InFlightUnits)
			ids.Set(f.m_OpId, true);
		return ids.Count();
	}

	protected void StartPoll()
	{
		if (!m_S || !m_S.m_Ready || m_S.m_Ended || m_Net.PollOpen() || m_Now < m_PollRetryAt)
			return;
		vector pos = GetGame().GetCurrentCameraPosition();
		int idle = (DZS_Kernel.NowMs() - m_Kernel.GetLastInputMs()) / 1000;
		if (idle < 0)
			idle = 0;
		string body = "{" + SessHead() + ",\"after\":" + m_S.m_After + ",\"waitMs\":5000";
		body += ",\"versions\":{\"participants\":" + m_S.m_VParticipants + ",\"locks\":" + m_S.m_VLocks + ",\"live\":" + m_S.m_VLive;
		body += ",\"notices\":" + m_S.m_VNotices + ",\"saving\":" + m_S.m_VSaving + "}";
		body += ",\"presence\":{\"position\":" + DZS_CollabJ.Vec(pos) + ",\"idleSeconds\":" + idle + "}";
		if (m_S.IsHost())
		{
			string st;
			int unsaved;
			HostSavingState(st, unsaved);
			body += ",\"saving\":{\"state\":" + DZS_CollabJ.Q(st) + ",\"unsavedCount\":" + unsaved + "}";
		}
		body += "}";
		m_Net.Poll(body);
	}

	protected void HostSavingState(out string st, out int unsaved)
	{
		DZS_Instance inst = Shown();
		st = "saved";
		unsaved = 0;
		if (!inst)
			return;
		unsaved = inst.GetUnsavedCount();
		if (inst.IsSaving())
			st = "saving";
		else if (inst.IsSaveFailed())
			st = "failed";
		else if (inst.IsUnsaved())
			st = "unsaved";
	}

	protected void OnPoll(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (!m_S)
			return;
		if (!r.m_Framed)
		{
			Down();
			m_PollRetryAt = m_Now + 1;
			return;
		}
		if (!r.Ok())
		{
			string code = r.Code();
			if (code == "not_participant" && r.ErrS("reason") == "timeout")
			{
				Up();
				TooLong();
				return;
			}
			if (code == "cursor_too_old")
			{
				TooLong();
				return;
			}
			if (code == "session_ended" || code == "not_participant" || code == "session_unknown")
			{
				HandleGone(r);
				return;
			}
			DZS_Log.Warn("collab: poll error " + code + " " + r.ErrS("message"));
			m_PollRetryAt = m_Now + 1;
			return;
		}
		Up();
		m_S.m_Seq = r.I("seq");
		ReadNotices(r.N("notices"));
		ReadParticipants(r.N("participants"));
		ReadPresence(r.N("presence"));
		ReadLocks(r.N("locks"));
		ReadSaving(r.N("saving"));
		ReadOps(r.N("ops"));
		ReadLive(r.N("live"));
		if (r.B("checkpointWanted") && !m_CpWanted && !m_CpBusy)
		{
			m_CpWanted = true;
			DZS_Log.Info("collab: checkpoint wanted, cursor " + m_S.m_After);
		}
		TryCheckpoint();
		if (r.B("more"))
			m_PollRetryAt = 0;
	}

	protected void ReadOps(DZS_SrcNode ops)
	{
		if (!ops || !ops.IsArr() || ops.Count() == 0)
			return;
		array<ref DZS_CollabIn> foreign = new array<ref DZS_CollabIn>();
		foreach (DZS_CollabIn h : m_S.m_Held)
			foreign.Insert(h);
		m_S.m_Held.Clear();
		foreach (DZS_SrcNode o : ops.m_Items)
		{
			DZS_CollabIn inc = new DZS_CollabIn();
			inc.m_Seq = DZS_CollabJ.I(o, "seq");
			inc.m_Pid = DZS_CollabJ.S(o, "participantId");
			inc.m_OpId = DZS_CollabJ.S(o, "opId");
			inc.m_System = DZS_CollabJ.B(o, "system");
			inc.m_LiveId = DZS_CollabJ.S(o, "liveId");
			inc.m_Pieces = DZS_CollabJ.Strings(o.Get("pieces"));
			if (inc.m_Seq > m_S.m_After)
				m_S.m_After = inc.m_Seq;
			string parts = DZS_CollabJ.S(o, "parts");
			if (parts != "" && parts.ToInt() > 1)
			{
				int part = DZS_CollabJ.I(o, "part");
				if (part == 0 || !m_S.m_PartBuf)
					m_S.m_PartBuf = new array<string>();
				foreach (string pp : inc.m_Pieces)
					m_S.m_PartBuf.Insert(pp);
				if (part + 1 < parts.ToInt())
					continue;
				inc.m_Pieces = m_S.m_PartBuf;
				m_S.m_PartBuf = null;
			}
			DZS_SrcNode chain = o.Get("chain");
			if (chain && chain.IsObj())
			{
				int index = DZS_CollabJ.I(chain, "index");
				int count = DZS_CollabJ.I(chain, "count");
				if (index == 0)
				{
					m_S.m_ChainBuf = new array<string>();
					m_S.m_ChainStart = inc.m_Seq - 1;
				}
				if (m_S.m_ChainBuf)
				{
					foreach (string cp : inc.m_Pieces)
						m_S.m_ChainBuf.Insert(cp);
				}
				if (index + 1 < count)
					continue;
				if (m_S.m_ChainBuf)
					inc.m_Pieces = m_S.m_ChainBuf;
				m_S.m_ChainBuf = null;
				DZS_Log.Info("collab: chain of " + count + " links complete at seq " + inc.m_Seq + ", " + DZS_Files.Length(inc.m_Pieces) + " bytes");
			}
			m_S.m_Applied = inc.m_Seq;
			if (inc.m_Pid == m_S.m_Pid && m_S.m_SentUnseen.Contains(inc.m_OpId))
			{
				if (foreign.Count() > 0)
					ApplyForeign(foreign);
				foreign.Clear();
				m_Slot.Confirm(inc.m_OpId);
				m_S.m_SentUnseen.Remove(inc.m_OpId);
				continue;
			}
			foreign.Insert(inc);
		}
		if (foreign.Count() == 0)
			return;
		if (m_S.m_SentUnseen.Count() > 0)
		{
			foreach (DZS_CollabIn keep : foreign)
				m_S.m_Held.Insert(keep);
			return;
		}
		ApplyForeign(foreign);
	}

	protected void ApplyForeign(array<ref DZS_CollabIn> foreign)
	{
		array<ref array<string>> texts = new array<ref array<string>>();
		foreach (DZS_CollabIn f : foreign)
			texts.Insert(f.m_Pieces);
		array<string> dropped = m_Slot.Rebase(texts);
		foreach (string d : dropped)
		{
			RemoveOut(d);
			for (int i = m_S.m_Units.Count() - 1; i >= 0; i--)
			{
				if (m_S.m_Units[i].m_OpId == d)
					m_S.m_Units.RemoveOrdered(i);
			}
		}
		foreach (DZS_CollabIn lf : foreign)
		{
			if (lf.m_LiveId != "")
				LiveDone(lf.m_Pid, lf.m_LiveId);
		}
		if (m_S.IsHost())
		{
			foreach (DZS_CollabIn cf : foreign)
			{
				if (!cf.m_System)
					m_Slot.CountForeign(cf.m_Pieces);
			}
		}
		DZS_Log.Info("collab: applied " + foreign.Count() + " remote operation(s), cursor " + m_S.m_After + ", dropped own " + dropped.Count());
		ValidateSelection();
	}

	// Objects that vanished through another's change fall out of the own selection.
	protected void ValidateSelection()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		DZS_Instance inst = Shown();
		if (!sel || !inst)
			return;
		array<string> ids = sel.GetSelection();
		array<string> gone = new array<string>();
		foreach (string id : ids)
		{
			if (!inst.Has(id))
				gone.Insert(id);
		}
		if (gone.Count() > 0)
			sel.Remove(gone);
	}

	// ---- participants, presence, saving ----

	protected void ReadParticipants(DZS_SrcNode p)
	{
		if (!p || !p.IsObj())
			return;
		m_S.m_VParticipants = DZS_CollabJ.I(p, "version");
		DZS_SrcNode list = p.Get("list");
		if (!list || !list.IsArr())
			return;
		array<ref DZS_CollabPeer> next = new array<ref DZS_CollabPeer>();
		foreach (DZS_SrcNode e : list.m_Items)
		{
			string pid = DZS_CollabJ.S(e, "participantId");
			DZS_CollabPeer old = m_S.Peer(pid);
			DZS_CollabPeer np = new DZS_CollabPeer();
			if (old)
				np = old;
			np.m_Pid = pid;
			np.m_UserId = DZS_CollabJ.S(e, "userId");
			np.m_Name = DZS_CollabJ.S(e, "name");
			np.m_Colour = DZS_CollabJ.I(e, "colour");
			np.m_Role = DZS_CollabJ.S(e, "role");
			np.m_CanInvite = DZS_CollabJ.B(e, "canInvite");
			np.m_CanSave = DZS_CollabJ.B(e, "canSave");
			np.m_CanManage = DZS_CollabJ.B(e, "canManage");
			np.m_Kind = DZS_CollabJ.S(e, "kind");
			np.m_AgentName = DZS_CollabJ.S(e, "agentName");
			next.Insert(np);
			if (m_S.m_PeersKnown && !old && pid != m_S.m_Pid)
			{
				if (np.IsAgent())
					np.m_Phase = "waiting";
				Msg("", np.Label() + " ist beigetreten", np.Label() + " joined", 2, MSG_PEER);
			}
		}
		if (m_S.m_PeersKnown)
		{
			foreach (DZS_CollabPeer was : m_S.m_Peers)
			{
				bool still = false;
				foreach (DZS_CollabPeer nn : next)
				{
					if (nn.m_Pid == was.m_Pid)
						still = true;
				}
				if (!still && !m_Removed.Contains(was.m_Pid))
					Msg("", was.Label() + " hat die Sitzung verlassen", was.Label() + " left the session", 2, MSG_PEER);
			}
		}
		m_S.m_Peers = next;
		m_S.m_PeersKnown = true;
		DZS_CollabPeer me = m_S.Me();
		if (me)
		{
			m_S.m_Colour = me.m_Colour;
			m_S.m_CanInvite = me.m_CanInvite;
			m_S.m_CanManage = me.m_CanManage;
		}
		DZS_CollabPeer host = m_S.Host();
		if (host)
			m_S.m_HostName = host.m_Name;
		if (m_Panel)
			m_Panel.Dirty();
	}

	protected ref map<string, bool> m_Removed = new map<string, bool>();

	protected void ReadPresence(DZS_SrcNode list)
	{
		if (!list || !list.IsArr())
			return;
		foreach (DZS_SrcNode e : list.m_Items)
		{
			DZS_CollabPeer p = m_S.Peer(DZS_CollabJ.S(e, "participantId"));
			if (!p)
				continue;
			p.m_State = DZS_CollabJ.S(e, "state");
			p.m_Idle = DZS_CollabJ.I(e, "idleSeconds");
			p.m_LastSeen = DZS_CollabJ.I(e, "lastSeenSeconds");
			DZS_SrcNode pos = e.Get("position");
			if (pos && pos.IsArr() && pos.Count() >= 3)
			{
				p.m_Pos = DZS_CollabJ.V(e, "position");
				p.m_HasPos = true;
			}
			if (!p.IsAgent())
				continue;
			string phase = DZS_CollabJ.S(e, "phase");
			if (phase == "")
				phase = "waiting";
			if (phase == "review" && p.m_Phase != "" && p.m_Phase != "review")
				Msg("", p.Label() + " wartet auf Prüfung", p.Label() + " is waiting for review", 2, MSG_PEER);
			p.m_Phase = phase;
			p.m_Status = DZS_CollabJ.S(e, "status");
		}
	}

	protected void ReadSaving(DZS_SrcNode s)
	{
		if (!s || !s.IsObj())
			return;
		m_S.m_VSaving = DZS_CollabJ.I(s, "version");
		m_S.m_SaveState = DZS_CollabJ.S(s, "state");
		m_S.m_SaveUnsaved = DZS_CollabJ.I(s, "unsavedCount");
		if (m_S.IsHosted())
		{
			int was = m_S.m_SavedSeq;
			string seqText = DZS_CollabJ.S(s, "savedSeq");
			if (seqText != "")
				m_S.m_SavedSeq = seqText.ToInt();
			DZS_SrcNode by = s.Get("savedBy");
			m_S.m_SavedByUser = DZS_CollabJ.S(by, "userId");
			m_S.m_SavedByName = DZS_CollabJ.S(by, "name");
			DZS_Instance sinst = Shown();
			if (sinst && sinst.IsSaveFailed() && m_S.m_SavedSeq != m_S.m_FailSeq && m_S.m_SavedSeq != was)
				sinst.SetSaveFailed(false);
			DZS_Log.Info("collab: saving " + m_S.m_SaveState + ", " + m_S.m_SaveUnsaved + " unsaved, saved seq " + m_S.m_SavedSeq + " by " + m_S.m_SavedByName);
		}
		string ago = DZS_CollabJ.S(s, "savedSecondsAgo");
		if (ago == "")
			m_S.m_SavedAgo = -1;
		else
		{
			m_S.m_SavedAgo = ago.ToInt();
			int hh, mm, ss;
			GetHourMinuteSecond(hh, mm, ss);
			int t = hh * 3600 + mm * 60 + ss - m_S.m_SavedAgo;
			while (t < 0)
				t += 86400;
			int h2 = t / 3600;
			int m2 = (t / 60) % 60;
			m_S.m_SavedClock = h2.ToString() + ":" + m2.ToStringLen(2);
		}
	}

	protected void ReadNotices(DZS_SrcNode n)
	{
		if (!n || !n.IsObj())
			return;
		m_S.m_VNotices = DZS_CollabJ.I(n, "version");
		DZS_SrcNode list = n.Get("list");
		if (!list || !list.IsArr())
			return;
		foreach (DZS_SrcNode e : list.m_Items)
		{
			string kind = DZS_CollabJ.S(e, "kind");
			string by = DZS_CollabJ.S(e, "by");
			if (kind == "participant_removed")
			{
				string who = DZS_CollabJ.S(e, "name");
				string rpid = DZS_CollabJ.S(e, "participantId");
				DZS_CollabPeer gone = m_S.Peer(rpid);
				if (gone)
					who = gone.Label();
				m_Removed.Set(rpid, true);
				Msg("", who + " wurde entfernt", who + " was removed", 2, MSG_PEER);
			}
			else if (kind == "locks_released")
			{
				DZS_SelectionApi sel = DZS_SelectionApi.Get();
				if (sel)
				{
					array<string> none = new array<string>();
					sel.SetSelection(none);
				}
				Msg("", by + " hat deine Auswahl freigegeben", by + " released your selection", 2, MSG_LOCK);
			}
			else if (kind == "attachment_wanted")
				m_Attach.OnWanted(DZS_CollabJ.S(e, "checksum"));
			else if (kind == "locks_taken")
			{
				int cnt = DZS_CollabJ.I(e, "count");
				string objs = DZS_Issues.Plural(cnt, " Objekt", " Objekte");
				Msg("", DZS_Num.Count(cnt) + objs + " hat " + by + " übernommen, während du weg warst", by + " took over " + DZS_Num.Count(cnt) + " objects while you were away", 3, MSG_LOCK);
			}
		}
	}

	// ---- locks (claims) ----

	static const int MAX_LOCKS = 20000;

	void WantLocks(array<string> ids)
	{
		if (!m_S)
			return;
		if (ids.Count() > MAX_LOCKS)
		{
			array<string> extra = new array<string>();
			for (int i = MAX_LOCKS; i < ids.Count(); i++)
				extra.Insert(ids[i]);
			DZS_SelectionApi sel = DZS_SelectionApi.Get();
			if (sel)
			{
				OverLimit(MAX_LOCKS, extra.Count());
				sel.Remove(extra);
				return;
			}
		}
		m_S.m_Want.Clear();
		foreach (string id : ids)
			m_S.m_Want.Insert(id);
		m_S.m_LockDirty = true;
		m_S.m_LockDirtyAt = m_Now;
	}

	protected void OverLimit(int n, int m)
	{
		string de = "Höchstens " + DZS_Num.Count(n) + " Objekte in Bearbeitung — " + DZS_Num.Count(m) + DZS_Issues.Plural(m, " ist aus der Auswahl gefallen", " sind aus der Auswahl gefallen");
		string en = "At most " + DZS_Num.Count(n) + " objects being edited — " + DZS_Num.Count(m) + " left the selection";
		Msg("warning", de, en, 2, MSG_LOCK);
	}

	protected void SendLocks()
	{
		if (!m_S || !m_S.m_Ready || m_S.m_Ended || !m_S.m_LockDirty || m_Now - m_S.m_LockDirtyAt < 0.1)
			return;
		if (m_Net.SendBusy() || m_Net.Waiting() > 0)
			return;
		m_S.m_LockDirty = false;
		m_S.m_LockSeq++;
		string list = DZS_CollabJ.List(m_S.m_Want);
		string body = "{" + SessHead() + ",\"lockSeq\":" + m_S.m_LockSeq + ",\"elements\":" + list + "}";
		m_Net.Send("locks/set", "locks/set", body);
	}

	protected void OnLocksSet(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (!m_S)
			return;
		if (!r.m_Framed)
		{
			Down();
			m_Net.Repeat(c, 1);
			return;
		}
		Up();
		if (!r.Ok())
			return;
		array<string> granted = DZS_CollabJ.Strings(r.N("granted"));
		m_S.m_Mine.Clear();
		foreach (string g : granted)
			m_S.m_Mine.Set(g, true);
		DZS_SrcNode denied = r.N("denied");
		if (StepAnswer(r.I("lockSeq"), granted, denied))
			return;
		if (!denied || !denied.IsArr() || denied.Count() == 0)
			return;
		array<string> drop = new array<string>();
		array<string> over = new array<string>();
		string holder = "";
		foreach (DZS_SrcNode d : denied.m_Items)
		{
			string del = DZS_CollabJ.S(d, "element");
			string hp = DZS_CollabJ.S(d, "holder");
			if (hp == "")
			{
				over.Insert(del);
				continue;
			}
			drop.Insert(del);
			DZS_CollabPeer p = m_S.Peer(hp);
			if (p)
				holder = p.Label();
			m_S.m_LockOf.Set(del, hp);
		}
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel && over.Count() > 0)
		{
			sel.Remove(over);
			OverLimit(granted.Count(), over.Count());
		}
		if (drop.Count() == 0)
			return;
		if (sel)
			sel.Remove(drop);
		int n = drop.Count();
		string objs = DZS_Issues.Plural(n, " Objekt ist", " Objekte sind");
		string objsEn = DZS_Issues.Plural(n, " object is", " objects are");
		Msg("", holder + " war schneller — " + DZS_Num.Count(n) + objs + " bei " + holder + " in Bearbeitung", holder + " was quicker — " + DZS_Num.Count(n) + objsEn + " being edited by " + holder, 2, MSG_LOCK);
	}

	protected void ReadLocks(DZS_SrcNode l)
	{
		if (!l || !l.IsObj())
			return;
		m_S.m_VLocks = DZS_CollabJ.I(l, "version");
		m_S.m_LockOf.Clear();
		m_S.m_Lapsed.Clear();
		foreach (DZS_CollabPeer pp : m_S.m_Peers)
			pp.m_Held = 0;
		DZS_SrcNode holders = l.Get("holders");
		if (!holders || !holders.IsArr())
			return;
		array<string> lost = new array<string>();
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		foreach (DZS_SrcNode h : holders.m_Items)
		{
			string pid = DZS_CollabJ.S(h, "participantId");
			bool lapsed = DZS_CollabJ.B(h, "lapsed");
			array<string> els = DZS_CollabJ.Strings(h.Get("elements"));
			DZS_CollabPeer p = m_S.Peer(pid);
			if (p)
				p.m_Held = els.Count();
			if (pid == m_S.m_Pid)
				continue;
			if (lapsed)
				m_S.m_Lapsed.Set(pid, true);
			foreach (string e : els)
			{
				m_S.m_LockOf.Set(e, pid);
				if (sel && sel.IsSelected(e))
					lost.Insert(e);
			}
		}
		if (lost.Count() > 0 && sel)
			sel.Remove(lost);
		if (m_Panel)
			m_Panel.Dirty();
	}

	bool HeldByOther(string id)
	{
		if (!m_S)
			return false;
		string pid;
		if (!m_S.m_LockOf.Find(id, pid))
			return false;
		return !m_S.m_Lapsed.Contains(pid);
	}

	string HolderName(string id)
	{
		if (!m_S)
			return "";
		DZS_CollabPeer p = m_S.Peer(m_S.m_LockOf.Get(id));
		if (!p)
			return "?";
		return p.Label();
	}

	int HolderColor(string id)
	{
		if (!m_S)
			return 0;
		DZS_CollabPeer p = m_S.Peer(m_S.m_LockOf.Get(id));
		if (!p)
			return DZS_CollabColors.Get(0);
		return DZS_CollabColors.Get(p.m_Colour);
	}

	void CollectHeld(array<string> ids, array<int> colors, array<bool> lapsed)
	{
		if (!m_S)
			return;
		foreach (string id, string pid : m_S.m_LockOf)
		{
			DZS_CollabPeer p = m_S.Peer(pid);
			int col = DZS_CollabColors.Get(0);
			if (p)
				col = DZS_CollabColors.Get(p.m_Colour);
			ids.Insert(id);
			colors.Insert(col);
			bool lap = m_S.m_Lapsed.Contains(pid);
			lapsed.Insert(lap);
		}
	}

	// ---- connection ----

	protected void Down(float since = -1)
	{
		if (!m_S)
			return;
		if (since < 0)
			since = m_Now;
		if (m_S.m_DownSince < 0 || since < m_S.m_DownSince)
		{
			m_S.m_DownSince = since;
			DZS_Log.Info("collab: interruption begins");
		}
	}

	protected void Up()
	{
		if (!m_S || m_S.m_DownSince < 0)
			return;
		float secs = m_Now - m_S.m_DownSince;
		DZS_Log.Info("collab: interruption over after " + DZS_Num.Fixed(secs, 1) + " s");
		m_S.m_DownSince = -1;
		m_Kernel.GetScreen().SetStatusIcon("collab.down", false, "", "", "");
		m_Kernel.GetScreen().HideBanner("collab.lost");
		m_S.m_BannerClosed = false;
		if (m_S.m_Lost || secs >= 30)
		{
			m_S.m_Lost = false;
			Resume();
		}
	}

	// Returning: after 30 s or more, session/join with resume before anything collected goes out.
	protected void Resume()
	{
		if (!m_S || m_S.m_Resuming)
			return;
		m_S.m_Resuming = true;
		m_Net.m_Paused = true;
		string rq = DZS_CollabJ.Q(ReqId());
		string sq = DZS_CollabJ.Q(m_S.m_Id);
		string kq = DZS_CollabJ.Q(m_S.m_Key);
		string pq = DZS_CollabJ.Q(m_S.m_Pid);
		string body = "{" + TokenHeadOf(m_S.m_Server) + ",\"requestId\":" + rq + "," + ClientJson() + ",\"sessionId\":" + sq + ",\"instanceKey\":" + kq;
		body += ",\"resume\":{\"participantId\":" + pq + ",\"after\":" + m_S.m_Applied + "}}";
		m_Net.SendFirst("resume", "session/join", body);
		DZS_Log.Info("collab: resume after " + m_S.m_Applied);
	}

	protected void OnResumed(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (!m_S)
			return;
		if (!r.m_Framed)
		{
			Down();
			m_Net.Repeat(c, 1);
			return;
		}
		m_S.m_Resuming = false;
		if (!r.Ok())
		{
			string code = r.Code();
			DZS_Log.Info("collab: resume refused: " + code + " " + r.ErrS("reason"));
			if (code == "busy" || code == "internal" || code == "throttled")
			{
				m_S.m_Resuming = true;
				m_Net.Repeat(c, 1);
				return;
			}
			if (code == "cursor_too_old" || (code == "not_participant" && r.ErrS("reason") == "timeout"))
			{
				TooLong();
				return;
			}
			HandleGone(r);
			return;
		}
		string mode = r.S("mode");
		DZS_Log.Info("collab: resumed, mode " + mode + ", seq " + r.I("seq"));
		if (mode != "resume")
		{
			TooLong();
			return;
		}
		m_S.m_Role = r.S("role");
		m_S.m_Colour = r.I("colour");
		m_S.m_CanInvite = r.B("canInvite");
		m_S.m_CanSave = r.B("canSave");
		m_S.m_CanManage = r.B("canManage");
		m_S.m_After = m_S.m_Applied;
		m_S.m_ChainBuf = null;
		m_S.m_PartBuf = null;
		m_Net.m_Paused = false;
		m_Net.Pump();
		WantLocks(SelectionNow());
		Msg("success", "Verbindung wieder da", "Connection is back", 2);
	}

	// "The connection was gone too long": the server does not have the connection any more.
	protected void TooLong()
	{
		if (!m_S || m_S.m_TooLong)
			return;
		m_S.m_TooLong = true;
		m_S.m_Lost = true;
		m_Net.m_Paused = true;
		m_Kernel.GetScreen().HideBanner("collab.lost");
		DZS_CollabLostDialog d = new DZS_CollabLostDialog(this);
		d.Open();
	}

	// "Rejoin" after "gone too long": leave under the old participant, join anew.
	void Rejoin()
	{
		if (!m_S)
			return;
		string sid = m_S.m_Id;
		string proj = m_S.m_Project;
		string mk = m_S.m_Map;
		string host = m_S.m_HostName;
		string url = m_S.m_Server;
		string folder = m_S.m_Folder;
		string body = "{" + SessHead() + "}";
		DZS_CollabNet n = NetFor(m_S.m_Server);
		n.Send("session/leave", "session/leave", body);
		EndLocally("rejoin");
		m_RejoinFailed = false;
		JoinSession(sid, proj, mk, host, url, folder);
	}

	bool m_RejoinFailed;

	protected void WatchConnection()
	{
		if (!m_S || !m_S.m_Ready || m_S.m_TooLong)
			return;
		if (m_Net.PollOpen() && m_Net.PollAge() > 6)
			Down(m_Now - m_Net.PollAge() + 6);
		if (m_Net.SendBusy() && m_Net.SendAge() >= 3)
			Down(m_Now - m_Net.SendAge());
		if (m_S.m_DownSince < 0)
			return;
		float secs = m_Now - m_S.m_DownSince;
		if (secs >= 3)
		{
			int sn = secs;
			string de = "Verbindung unterbrochen seit " + sn + " s — deine Änderungen gehen raus, sobald sie zurück ist";
			string en = "Connection interrupted for " + sn + " s — your changes go out once it is back";
			m_Kernel.GetScreen().SetStatusIcon("collab.down", true, "wifi-off", DZS_Texts.Pick(de, en), "warning");
		}
		if (secs >= 30 && !m_S.m_Lost)
		{
			m_S.m_Lost = true;
			m_Net.m_Paused = true;
			DZS_Log.Info("collab: connection lost, view only");
			ShowLostBanner();
		}
	}

	protected void ShowLostBanner()
	{
		if (m_S.m_BannerClosed)
			return;
		array<string> tags = new array<string>();
		array<string> labels = new array<string>();
		tags.Insert("wait");
		labels.Insert(DZS_Texts.Pick("Weiter warten", "Keep waiting"));
		if (m_S.IsHost())
		{
			tags.Insert("alone");
			labels.Insert(DZS_Texts.Pick("Allein weiterarbeiten", "Continue alone"));
		}
		else
		{
			tags.Insert("copy");
			labels.Insert(DZS_Texts.Pick("Als eigene Kopie speichern …", "Save as own copy …"));
		}
		string t = DZS_Texts.Pick("Verbindung verloren — du kannst nur ansehen", "Connection lost — view only");
		m_Kernel.GetScreen().ShowBannerButtons("collab.lost", 50, "danger", DZS_W.Literal(t), tags, labels, m_Knobs);
	}

	void OnBannerKnob(string kind)
	{
		DZS_Log.Info("collab: banner knob " + kind);
		if (!m_S)
			return;
		if (kind == "collab.lost:wait")
		{
			m_S.m_BannerClosed = true;
			m_Kernel.GetScreen().HideBanner("collab.lost");
		}
		else if (kind == "collab.lost:alone")
		{
			m_Kernel.GetScreen().HideBanner("collab.lost");
			EndLocally("alone");
			Msg("", "Allein weiter — die Sitzung ist für dich beendet", "Continuing alone — the session is over for you", 3);
		}
		else if (kind == "collab.lost:copy")
		{
			m_Kernel.GetScreen().HideBanner("collab.lost");
			OwnCopy();
		}
	}

	// "Save as own copy …" out of a lost connection: leave the session, keep the own state (with the
	// unsent changes) as a local project.
	void OwnCopy()
	{
		if (!m_S)
			return;
		string body = "{" + SessHead() + "}";
		DZS_CollabNet n = NetFor(m_S.m_Server);
		n.Send("session/leave", "session/leave", body);
		EndLocally("copy");
		EndCopy();
	}

	// ---- view only and claims (claims, connection) ----

	bool IsLost() { return m_S && m_S.m_Lost; }

	// Very large steps: at most maxChainOps × maxOpBytes (56 MB) in the text form; -dzsCollabMaxStep=BYTES for tests.
	int MaxStep(DZS_Instance inst)
	{
		if (!m_S || inst != Shown())
			return 0;
		string dev;
		if (GetCLIParam("dzsCollabMaxStep", dev) && dev.ToInt() > 0)
			return dev.ToInt();
		return MAX_CHAIN * MAX_OP;
	}

	void StepTooLarge(int bytes, int limit)
	{
		float nf = Math.Ceil(bytes / 104857.6) / 10.0;
		float mf = Math.Floor(limit / 104857.6) / 10.0;
		string n = DZS_Num.Fixed(nf, 1);
		string m = DZS_Num.Fixed(mf, 1);
		Msg("warning", "Zu groß für eine gemeinsame Sitzung (" + n + " MB, höchstens " + m + " MB)", "Too large for a shared session (" + n + " MB, at most " + m + " MB)", 2, "history");
	}

	bool GateViewOnly(DZS_Instance inst, out string de, out string en)
	{
		if (!m_S || !m_S.m_Lost || inst != Shown())
			return false;
		de = "Verbindung verloren";
		en = "Connection lost";
		return true;
	}

	bool GateAction(string id, out string de, out string en)
	{
		if (!m_S || !m_S.m_Ready)
			return false;
		if (m_S.IsGuest() && id == "loot.toggle" && !m_Kernel.GetUi().IsActionChecked("loot.toggle"))
		{
			de = "Loot einschalten kann nur " + m_S.m_HostName;
			en = "Only " + m_S.m_HostName + " can turn loot on";
			return true;
		}
		if (!m_S.m_Lost)
			return false;
		array<string> free = {"placement.selectAll", "placement.selectSame", "placement.deselect", "placement.invert", "placement.focus", "placement.copy", "placement.copyText", "placement.tab", "placement.tool.select", "placement.tool.move", "placement.tool.rotate", "placement.tool.scale", "placement.toolCycle", "placement.followSlope", "placement.axis"};
		bool off = id == "core.undo" || id == "core.redo" || id == "projects.import" || id == "brush.toggle" || id == "brush.fromSelection" || id == "properties.physics" || id.IndexOf("loot.") == 0;
		if (id.IndexOf("placement.") == 0 && free.Find(id) < 0)
			off = true;
		if (!off)
			return false;
		de = "Verbindung verloren";
		en = "Connection lost";
		return true;
	}

	// New objects of this instance after the start belong to it at once.
	protected void MarkCounter(DZS_Instance inst)
	{
		string probe = inst.NewElementId();
		int dash = probe.LastIndexOf("-");
		string num = probe.Substring(dash + 1, probe.Length() - dash - 1);
		m_S.m_CounterStart = num.ToInt();
		if (m_S.m_Key == "")
			m_S.m_Key = inst.GetSessionCode();
	}

	protected bool OwnNew(string id)
	{
		DZS_Instance inst = Shown();
		if (!inst)
			return false;
		string pre = inst.GetSessionCode() + "-";
		if (id.IndexOf(pre) != 0)
			return false;
		string num = id.Substring(pre.Length(), id.Length() - pre.Length());
		return num.ToInt() > m_S.m_CounterStart;
	}

	bool Holds(string id)
	{
		return m_S && (m_S.m_Mine.Contains(id) || OwnNew(id));
	}

	string ClaimWait(array<string> ids)
	{
		if (!m_S || !m_S.m_Ready)
			return "";
		foreach (string id : ids)
		{
			if (Holds(id))
				continue;
			if (m_S.m_DownSince >= 0 && m_Now - m_S.m_DownSince >= 3)
				return DZS_Texts.Pick("Wartet auf Verbindung", "Waiting for the connection");
			return DZS_Texts.Pick("Wird belegt …", "Claiming …");
		}
		return "";
	}

	// Undo and redo: claim every existing object the step changes first.
	protected bool m_StepClaim;
	protected bool m_StepRedo;
	protected int m_StepSeq;
	protected ref array<string> m_StepIds;
	protected ref array<string> m_StepSel;

	bool ClaimStep(DZS_Instance inst, array<string> ids, bool redo)
	{
		if (!m_S || !m_S.m_Ready || inst != Shown())
			return false;
		array<string> need = new array<string>();
		foreach (string id : ids)
		{
			if (!Holds(id))
				need.Insert(id);
		}
		if (need.Count() == 0)
			return false;
		string what = StepThing(inst, redo);
		if (m_S.m_DownSince >= 0 && m_Now - m_S.m_DownSince >= 3)
		{
			StepRefused(redo, DZS_Texts.Pick("Verbindung unterbrochen", "Connection interrupted"));
			return true;
		}
		foreach (string nid : need)
		{
			if (HeldByOther(nid))
			{
				string who = HolderName(nid);
				StepRefused(redo, DZS_Texts.Pick(who + " bearbeitet " + what, who + " is editing " + what));
				return true;
			}
		}
		if (m_StepClaim)
			return true;
		m_StepClaim = true;
		m_StepRedo = redo;
		m_StepIds = need;
		m_StepSel = SelectionNow();
		m_S.m_Want.Clear();
		foreach (string sid : m_StepSel)
			m_S.m_Want.Insert(sid);
		foreach (string add : need)
		{
			if (m_S.m_Want.Find(add) < 0)
				m_S.m_Want.Insert(add);
		}
		m_S.m_LockDirty = true;
		m_S.m_LockDirtyAt = m_Now - 1;
		m_StepSeq = m_S.m_LockSeq + 1;
		DZS_Log.Info("collab: undo/redo waits for the claim of " + need.Count() + " object(s)");
		return true;
	}

	protected string StepThing(DZS_Instance inst, bool redo)
	{
		string name = inst.GetUndoName(!DZS_Texts.IsGerman());
		if (redo)
			name = inst.GetRedoName(!DZS_Texts.IsGerman());
		int sp = name.LastIndexOf(" ");
		if (sp > 0)
			return name.Substring(0, sp);
		array<string> ids = inst.StepObjects(redo);
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (ids.Count() == 1 && cat)
		{
			string cls;
			inst.GetField(ids[0], DZS_Obj.NAME, cls);
			string shown = cat.ShownName(cls);
			if (shown != "")
				return shown;
		}
		if (ids.Count() > 1)
			return ids.Count().ToString() + DZS_Texts.Pick(" Objekte", " objects");
		return name;
	}

	protected void StepRefused(bool redo, string why)
	{
		string de = "Rückgängig gerade nicht möglich: ";
		string en = "Undo not possible right now: ";
		if (redo)
		{
			de = "Wiederholen gerade nicht möglich: ";
			en = "Redo not possible right now: ";
		}
		Msg("warning", de + why, en + why, 2, "history");
	}

	// The answer to the claim of an undo/redo: all granted → the counter-operation, else the old selection again.
	protected bool StepAnswer(int lockSeq, array<string> granted, DZS_SrcNode denied)
	{
		if (!m_StepClaim || lockSeq < m_StepSeq)
			return false;
		m_StepClaim = false;
		DZS_Instance inst = Shown();
		bool all = true;
		foreach (string id : m_StepIds)
		{
			if (granted.Find(id) < 0)
				all = false;
		}
		if (all && inst)
		{
			DZS_Log.Info("collab: claim for undo/redo granted");
			if (m_StepRedo)
				inst.Redo();
			else
				inst.Undo();
			WantLocks(SelectionNow());
			return true;
		}
		string who = "?";
		if (denied && denied.IsArr() && denied.Count() > 0)
		{
			DZS_CollabPeer p = m_S.Peer(DZS_CollabJ.S(denied.m_Items[0], "holder"));
			if (p)
				who = p.Label();
		}
		if (inst)
			StepRefused(m_StepRedo, DZS_Texts.Pick(who + " bearbeitet " + StepThing(inst, m_StepRedo), who + " is editing " + StepThing(inst, m_StepRedo)));
		WantLocks(m_StepSel);
		return true;
	}

	// ---- live moving ----

	protected string m_LiveId;
	protected int m_LiveN;
	protected int m_LiveCount;
	string m_LiveCommit;
	protected bool m_LiveDirty;
	protected float m_LiveSentAt = -10;
	protected ref array<string> m_LiveIds;
	protected ref array<vector> m_LivePos;
	protected ref array<vector> m_LiveYpr;
	protected ref array<float> m_LiveScale;
	protected ref map<string, ref DZS_CollabLiveObj> m_LiveObjs = new map<string, ref DZS_CollabLiveObj>();
	protected ref map<string, string> m_LiveOf = new map<string, string>();
	protected ref map<string, string> m_LiveDone = new map<string, string>();

	void LiveGesture(array<string> ids, array<vector> pos, array<vector> ypr, array<float> scale)
	{
		if (!m_S || !m_S.m_Ready || m_S.m_Lost)
			return;
		if (m_LiveId == "")
		{
			m_LiveCount++;
			m_LiveId = m_S.m_Key + "-L" + m_LiveCount;
			m_LiveN = 0;
		}
		m_LiveIds = new array<string>();
		m_LivePos = new array<vector>();
		m_LiveYpr = new array<vector>();
		m_LiveScale = new array<float>();
		for (int i = 0; i < ids.Count(); i++)
		{
			if (!Holds(ids[i]))
				continue;
			m_LiveIds.Insert(ids[i]);
			m_LivePos.Insert(pos[i]);
			m_LiveYpr.Insert(ypr[i]);
			m_LiveScale.Insert(scale[i]);
		}
		m_LiveDirty = m_LiveIds.Count() > 0;
	}

	void LiveEnd(bool commit)
	{
		if (m_LiveId == "" || !m_S || !m_Net)
		{
			m_LiveId = "";
			return;
		}
		string how = "cancel";
		if (commit)
		{
			how = "commit";
			m_LiveCommit = m_LiveId;
		}
		if (m_LiveN > 0)
		{
			string body = "{" + SessHead() + ",\"liveId\":" + DZS_CollabJ.Q(m_LiveId) + ",\"n\":" + (m_LiveN + 1) + ",\"mode\":\"end\",\"end\":\"" + how + "\"}";
			m_Net.Send("live", "live/put", body);
		}
		else
			m_LiveCommit = "";
		DZS_Log.Info("collab: live " + m_LiveId + " end " + how + " after " + m_LiveN);
		m_LiveId = "";
		m_LiveDirty = false;
	}

	protected void SendLive()
	{
		if (!m_LiveDirty || m_LiveId == "" || m_Now - m_LiveSentAt < 0.2 || !m_S || m_S.m_Lost)
			return;
		m_LiveDirty = false;
		m_LiveSentAt = m_Now;
		m_LiveN++;
		vector tfPivot, tfMove;
		float tfYaw;
		if (m_LiveIds.Count() > 100 && LiveAsWhole(tfPivot, tfMove, tfYaw))
		{
			string tf = "{\"pivot\":" + DZS_CollabJ.Vec(tfPivot) + ",\"translate\":" + DZS_CollabJ.Vec(tfMove) + ",\"yaw\":" + DZS_CollabJ.Num(tfYaw) + "}";
			string tbody = "{" + SessHead() + ",\"liveId\":" + DZS_CollabJ.Q(m_LiveId) + ",\"n\":" + m_LiveN + ",\"mode\":\"transform\",\"transform\":" + tf + "}";
			m_Net.Send("live", "live/put", tbody);
			if (m_LiveN <= 3 || m_LiveN % 10 == 0)
				DZS_Log.Info("collab: live " + m_LiveId + " n " + m_LiveN + " transform of " + m_LiveIds.Count() + " pivot " + tfPivot.ToString() + " move " + tfMove.ToString() + " yaw " + tfYaw + ", " + tbody.Length() + " bytes");
			return;
		}
		string vals = "[";
		int n = Math.Min(100, m_LiveIds.Count());
		for (int i = 0; i < n; i++)
		{
			if (i > 0)
				vals += ",";
			vals += "{\"element\":" + DZS_CollabJ.Q(m_LiveIds[i]) + ",\"position\":" + DZS_CollabJ.Vec(m_LivePos[i]) + ",\"ypr\":" + DZS_CollabJ.Vec(m_LiveYpr[i]) + ",\"scale\":" + DZS_CollabJ.Num(m_LiveScale[i]) + "}";
		}
		vals += "]";
		string body = "{" + SessHead() + ",\"liveId\":" + DZS_CollabJ.Q(m_LiveId) + ",\"n\":" + m_LiveN + ",\"mode\":\"values\",\"values\":" + vals + "}";
		m_Net.Send("live", "live/put", body);
	}

	// More than 100 objects as a whole: pivot = middle of the north/east/up box around the objects as the gesture
	// began (fixed for the gesture); false if the gesture does not move all of them alike.
	protected string m_LivePivotFor;
	protected vector m_LivePivot;
	protected ref map<string, vector> m_LiveBasePos = new map<string, vector>();
	protected ref map<string, vector> m_LiveBaseYpr = new map<string, vector>();
	protected ref map<string, float> m_LiveBaseScale = new map<string, float>();

	protected bool LiveAsWhole(out vector pivot, out vector move, out float yaw)
	{
		DZS_Instance inst = Shown();
		if (!inst)
			return false;
		if (m_LivePivotFor != m_LiveId)
		{
			m_LivePivotFor = m_LiveId;
			m_LiveBasePos.Clear();
			m_LiveBaseYpr.Clear();
			m_LiveBaseScale.Clear();
			vector lo = "1000000000 1000000000 1000000000";
			vector hi = "-1000000000 -1000000000 -1000000000";
			array<vector> corners = new array<vector>();
			foreach (string bid : m_LiveIds)
			{
				if (!inst.Has(bid))
					continue;
				DZS_PlaceObj b = DZS_PlaceObj.FromStateQuick(inst, bid);
				m_LiveBasePos.Set(bid, b.m_Pos);
				m_LiveBaseYpr.Set(bid, b.m_Ypr);
				m_LiveBaseScale.Set(bid, b.m_Scale);
				b.Corners(corners);
				foreach (vector c : corners)
				{
					for (int k = 0; k < 3; k++)
					{
						lo[k] = Math.Min(lo[k], c[k]);
						hi[k] = Math.Max(hi[k], c[k]);
					}
				}
			}
			m_LivePivot = (lo + hi) * 0.5;
		}
		pivot = m_LivePivot;
		string first = m_LiveIds[0];
		if (!m_LiveBasePos.Contains(first))
			return false;
		vector fb = m_LiveBaseYpr.Get(first);
		vector fy = m_LiveYpr[0];
		yaw = DZS_CameraPlugin.AngleDiff(fb[0], fy[0]);
		float a = -yaw * Math.DEG2RAD;
		float ca = Math.Cos(a);
		float sa = Math.Sin(a);
		vector rel0 = m_LiveBasePos.Get(first) - pivot;
		vector rot0 = Vector(rel0[0] * ca - rel0[2] * sa, rel0[1], rel0[0] * sa + rel0[2] * ca);
		move = m_LivePos[0] - pivot - rot0;
		for (int i = 0; i < m_LiveIds.Count(); i++)
		{
			string id = m_LiveIds[i];
			if (!m_LiveBasePos.Contains(id))
				return false;
			vector by = m_LiveBaseYpr.Get(id);
			vector ny = m_LiveYpr[i];
			if (Math.AbsFloat(DZS_CameraPlugin.AngleDiff(by[0] + yaw, ny[0])) > 0.01 || Math.AbsFloat(ny[1] - by[1]) > 0.01 || Math.AbsFloat(ny[2] - by[2]) > 0.01)
				return false;
			if (Math.AbsFloat(m_LiveScale[i] - m_LiveBaseScale.Get(id)) > 0.0001)
				return false;
			vector rel = m_LiveBasePos.Get(id) - pivot;
			vector want = pivot + Vector(rel[0] * ca - rel[2] * sa, rel[1], rel[0] * sa + rel[2] * ca) + move;
			if (vector.Distance(want, m_LivePos[i]) > 0.01)
				return false;
		}
		return true;
	}

	protected void ReadLive(DZS_SrcNode l)
	{
		if (!l || !l.IsObj())
			return;
		m_S.m_VLive = DZS_CollabJ.I(l, "version");
		DZS_SrcNode states = l.Get("states");
		map<string, bool> seen = new map<string, bool>();
		DZS_Instance inst = Shown();
		if (states && states.IsArr() && inst)
		{
			foreach (DZS_SrcNode st : states.m_Items)
			{
				string pid = DZS_CollabJ.S(st, "participantId");
				if (pid == m_S.m_Pid)
					continue;
				string lid = DZS_CollabJ.S(st, "liveId");
				if (m_LiveDone.Get(pid) == lid)
					continue;
				seen.Set(pid, true);
				m_LiveOf.Set(pid, lid);
				string mode = DZS_CollabJ.S(st, "mode");
				if (mode == "values")
				{
					DZS_SrcNode vals = st.Get("values");
					if (!vals || !vals.IsArr())
						continue;
					foreach (DZS_SrcNode v : vals.m_Items)
					{
						string id = DZS_CollabJ.S(v, "element");
						if (!inst.Has(id))
							continue;
						vector ypr = DZS_CollabJ.V(v, "ypr");
						DZS_SrcNode scn = v.Get("scale");
						float sc = DZS_CollabJ.F(scn);
						LiveTarget(inst, pid, id, DZS_CollabJ.V(v, "position"), ypr, sc);
					}
				}
				else if (mode == "transform")
				{
					DZS_SrcNode tf = st.Get("transform");
					vector pivot = DZS_CollabJ.V(tf, "pivot");
					vector tr = DZS_CollabJ.V(tf, "translate");
					DZS_SrcNode yn = tf.Get("yaw");
					float yaw = DZS_CollabJ.F(yn);
					foreach (string eid, string holder : m_S.m_LockOf)
					{
						if (holder != pid || !inst.Has(eid))
							continue;
						DZS_PlaceObj o = DZS_PlaceObj.FromStateQuick(inst, eid);
						vector rel = o.m_Pos - pivot;
						float a = -yaw * Math.DEG2RAD;
						vector rot = Vector(rel[0] * Math.Cos(a) - rel[2] * Math.Sin(a), rel[1], rel[0] * Math.Sin(a) + rel[2] * Math.Cos(a));
						vector np = pivot + rot + tr;
						vector ny = o.m_Ypr;
						ny[0] = ny[0] + yaw;
						LiveTarget(inst, pid, eid, np, ny, o.m_Scale);
					}
				}
			}
		}
		foreach (string lkey, DZS_CollabLiveObj lo : m_LiveObjs)
		{
			if (!seen.Contains(lo.m_Pid) && !lo.m_Back && inst && inst.Has(lo.m_Id))
			{
				DZS_PlaceObj back = DZS_PlaceObj.FromStateQuick(inst, lo.m_Id);
				LiveTarget(inst, lo.m_Pid, lo.m_Id, back.m_Pos, back.m_Ypr, back.m_Scale);
				lo.m_Back = true;
			}
		}
	}

	protected void LiveTarget(DZS_Instance inst, string pid, string id, vector pos, vector ypr, float scale)
	{
		DZS_CollabLiveObj lo = m_LiveObjs.Get(id);
		if (!lo)
		{
			lo = new DZS_CollabLiveObj();
			lo.m_Id = id;
			DZS_PlaceObj o = DZS_PlaceObj.FromStateQuick(inst, id);
			lo.m_ToPos = o.m_Pos;
			lo.m_ToYpr = o.m_Ypr;
			lo.m_ToScale = o.m_Scale;
			lo.m_T = 1;
			m_LiveObjs.Set(id, lo);
		}
		float t = Math.Clamp(lo.m_T, 0, 1);
		lo.m_FromPos = lo.m_FromPos + (lo.m_ToPos - lo.m_FromPos) * t;
		lo.m_FromYpr = lo.m_FromYpr + (lo.m_ToYpr - lo.m_FromYpr) * t;
		lo.m_FromScale = lo.m_FromScale + (lo.m_ToScale - lo.m_FromScale) * t;
		if (lo.m_T >= 1)
		{
			lo.m_FromPos = lo.m_ToPos;
			lo.m_FromYpr = lo.m_ToYpr;
			lo.m_FromScale = lo.m_ToScale;
		}
		if (lo.m_T >= 1 && !lo.m_Back)
			DZS_Log.Info("collab: live of " + pid + " " + id + " from " + lo.m_FromPos.ToString() + " to " + pos.ToString() + " ypr " + ypr.ToString() + " scale " + scale);
		lo.m_Pid = pid;
		lo.m_ToPos = pos;
		lo.m_ToYpr = ypr;
		lo.m_ToScale = scale;
		lo.m_T = 0;
		lo.m_Back = false;
	}

	protected void GlideLive(float dt)
	{
		if (m_LiveObjs.Count() == 0)
			return;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (!scene)
			return;
		array<string> done = new array<string>();
		foreach (string id, DZS_CollabLiveObj lo : m_LiveObjs)
		{
			if (lo.m_T >= 1 && lo.m_Back)
			{
				scene.ClearView(id);
				done.Insert(id);
				continue;
			}
			if (lo.m_T >= 1)
				continue;
			lo.m_T += dt / 0.2;
			float t = Math.Clamp(lo.m_T, 0, 1);
			vector p = lo.m_FromPos + (lo.m_ToPos - lo.m_FromPos) * t;
			vector y = lo.m_FromYpr + (lo.m_ToYpr - lo.m_FromYpr) * t;
			float sc = lo.m_FromScale + (lo.m_ToScale - lo.m_FromScale) * t;
			scene.SetView(id, p, y, sc);
		}
		foreach (string d : done)
			m_LiveObjs.Remove(d);
	}

	// The operation of a gesture is applied: its objects stand on their end values, no more gliding.
	protected void LiveDone(string pid, string liveId)
	{
		m_LiveDone.Set(pid, liveId);
		array<string> drop = new array<string>();
		foreach (string id, DZS_CollabLiveObj lo : m_LiveObjs)
		{
			if (lo.m_Pid == pid)
				drop.Insert(id);
		}
		DZS_SceneApi scene = DZS_SceneApi.Get();
		foreach (string d : drop)
		{
			m_LiveObjs.Remove(d);
			if (scene)
				scene.ClearView(d);
		}
		DZS_Log.Info("collab: live " + liveId + " of " + pid + " done, " + drop.Count() + " object(s) on their end values");
	}

	protected void SegmentClick()
	{
		bool left = (GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) != 0;
		bool edge = left && !m_LeftWas;
		m_LeftWas = left;
		if (!edge || !m_Segment || !m_Segment.m_Frame || !m_S || !m_S.m_Ready || !m_Segment.m_Frame.IsVisible())
			return;
		float x, y, w, h;
		m_Segment.m_Frame.GetScreenPos(x, y);
		m_Segment.m_Frame.GetScreenSize(w, h);
		int mx, my;
		GetMousePos(mx, my);
		if (mx < x || mx > x + w || my < y || my > y + h)
			return;
		m_Kernel.GetScreen().UnfoldPanel(PANEL);
	}

	// ---- session end, leave ----

	protected void HandleGone(DZS_CollabReply r)
	{
		string code = r.Code();
		string reason = r.ErrS("reason");
		string by = r.ErrS("by");
		DZS_Log.Info("collab: session gone: " + code + " " + reason + " " + by);
		if (m_S.IsHosted())
		{
			m_ReopenFolder = m_S.m_Folder;
			bool mayReopen = m_S.m_CanSave;
			EndLocally("gone");
			string hde = "Die Sitzung wurde vom Server beendet";
			string hen = "The server ended the session";
			if (reason == "discarded")
			{
				hde = by + " hat die ungespeicherten Änderungen verworfen";
				hen = by + " discarded the unsaved changes";
			}
			else if (code == "not_participant" && reason == "removed")
			{
				hde = by + " hat dich aus der Sitzung entfernt";
				hen = by + " removed you from the session";
			}
			DZS_CollabEndDialog hd = new DZS_CollabEndDialog(this, DZS_Texts.Pick(hde, hen), true, mayReopen);
			hd.Open();
			return;
		}
		if (m_S.IsHost())
		{
			if (code == "session_ended" && (reason == "host_ended" || reason == "host_left"))
			{
				EndLocally("ended");
				return;
			}
			EndLocally("server");
			Msg("", "Die Sitzung wurde vom Server beendet", "The server ended the session", 2);
			return;
		}
		string hostName = m_S.m_HostName;
		EndLocally("gone");
		string de = "Die Sitzung wurde vom Server beendet";
		string en = "The server ended the session";
		if (code == "not_participant" && reason == "removed")
		{
			de = by + " hat dich aus der Sitzung entfernt";
			en = by + " removed you from the session";
		}
		else if (reason == "host_ended" || reason == "host_left")
		{
			de = hostName + " hat die Sitzung beendet";
			en = hostName + " ended the session";
		}
		else if (reason == "host_gone")
		{
			de = "Die Sitzung ist zu Ende — " + hostName + " ist nicht mehr erreichbar";
			en = "The session is over — " + hostName + " can no longer be reached";
		}
		string title = DZS_Texts.Pick(de, en);
		DZS_CollabEndDialog d = new DZS_CollabEndDialog(this, title);
		d.Open();
	}

	// An operation of a type this version does not know: the state would drift, so leave (after the rebase that found it).
	protected bool m_NeedsNewer;

	void NeedsNewer()
	{
		if (m_S)
			m_NeedsNewer = true;
	}

	protected void LeaveForNewer()
	{
		m_NeedsNewer = false;
		if (!m_S)
			return;
		DZS_Log.Warn("collab: unknown operation type in session " + m_S.m_Id + " — leaving");
		string body = "{" + SessHead() + "}";
		DZS_CollabNet n = NetFor(m_S.m_Server);
		n.Send("session/leave", "session/leave", body);
		bool host = m_S.IsHost();
		EndLocally("newer version needed");
		string de = "Diese Sitzung braucht eine neuere dzStudio-Version";
		string en = "This session needs a newer dzStudio version";
		if (host)
		{
			Msg("danger", de, en, 4);
			return;
		}
		DZS_CollabEndDialog d = new DZS_CollabEndDialog(this, DZS_Texts.Pick(de, en));
		d.Open();
	}

	// After the end of a session as a guest: keep the own state as a local project, or go to the main menu.
	void EndCopy()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp)
			return;
		if (m_CopyNoLoot)
		{
			m_CopyNoLoot = false;
			DZS_Log.Info("collab: own copy without loot (loot texts missing)");
			pp.OpenSaveAsDialog("Kopie ohne Loot gespeichert — Loot-Dateien fehlten", "Copy saved without loot — loot files were missing", true);
			return;
		}
		pp.OpenSaveAsDialog();
	}

	// "Discard" (with a question): Cancel and Esc lead back to the dialog it came from.
	protected string m_EndTitle;

	protected bool m_EndReopen;
	protected bool m_EndCanReopen;

	void AskDiscard(string title, bool reopen = false, bool canReopen = true)
	{
		m_EndTitle = title;
		m_EndReopen = reopen;
		m_EndCanReopen = canReopen;
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Ungespeicherte Änderungen aller Teilnehmer verwerfen?", "Discard everyone’s unsaved changes?");
		string a1 = DZS_Texts.Pick("Das lässt sich nicht rückgängig machen.", "This can’t be undone.");
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(k1, t1, a1);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("discard", DZS_Texts.Pick("Verwerfen", "Discard"), "danger");
		c.m_OnResult.Insert(OnDiscardAnswer);
		c.Open();
	}

	void OnDiscardAnswer(DZS_Dialog d, string result)
	{
		if (result == "discard")
		{
			EndDiscard();
			return;
		}
		DZS_CollabEndDialog back = new DZS_CollabEndDialog(this, m_EndTitle, m_EndReopen, m_EndCanReopen);
		back.Open();
	}

	// "Rejoin": with unsent changes first "‹n› changes will be lost".
	void AskRejoin()
	{
		int n = Unsent();
		if (n == 0)
		{
			Rejoin();
			return;
		}
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Neu beitreten", "Join again");
		string a1 = DZS_Texts.Pick(DZS_Num.Count(n) + DZS_Issues.Plural(n, " Änderung geht verloren", " Änderungen gehen verloren"), DZS_Num.Count(n) + DZS_Issues.Plural(n, " change will be lost", " changes will be lost"));
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(k1, t1, a1);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("rejoin", t1, "danger");
		c.m_OnResult.Insert(OnRejoinAnswer);
		c.Open();
	}

	void OnRejoinAnswer(DZS_Dialog d, string result)
	{
		if (result == "rejoin")
		{
			Rejoin();
			return;
		}
		DZS_CollabLostDialog back = new DZS_CollabLostDialog(this);
		back.Open();
	}

	// "Reopen": the server's project again from its store (joins a running session or starts a new one).
	string m_ReopenFolder;

	void Reopen()
	{
		string folder = m_ReopenFolder;
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || folder == "")
			return;
		DZS_Log.Info("collab: reopen " + folder);
		pp.CloseProject(true);
		pp.RequestOpen(folder);
	}

	// Project › "Discard unsaved changes …" (server project, for those allowed to manage it).
	void AskDiscardHosted()
	{
		if (!m_S || !m_S.IsHosted() || !m_S.m_CanManage)
			return;
		int n = m_S.m_SaveUnsaved;
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Ungespeicherte Änderungen verwerfen", "Discard unsaved changes");
		string de = "Damit gehen " + DZS_Num.Count(n) + DZS_Issues.Plural(n, " ungespeicherte Änderung", " ungespeicherte Änderungen") + " der Sitzung für alle verloren";
		string en = "This loses " + DZS_Num.Count(n) + DZS_Issues.Plural(n, " unsaved change", " unsaved changes") + " of the session for everyone";
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(k1, t1, DZS_Texts.Pick(de, en));
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("discard", DZS_Texts.Pick("Verwerfen", "Discard"), "danger");
		c.m_OnResult.Insert(OnDiscardHostedAnswer);
		c.Open();
	}

	void OnDiscardHostedAnswer(DZS_Dialog d, string result)
	{
		if (result != "discard" || !m_S || !m_Net)
			return;
		string rq = DZS_CollabJ.Q(ReqId());
		m_Net.Send("hosted/end", "session/end", "{" + SessHead() + ",\"requestId\":" + rq + "}");
	}

	protected void OnDiscarded(DZS_CollabReply r)
	{
		if (!m_S)
			return;
		if (!r.Ok())
		{
			Msg("danger", "Verwerfen nicht möglich: " + r.Code(), "Cannot discard: " + r.Code(), 3);
			return;
		}
		m_ReopenFolder = m_S.m_Folder;
		EndLocally("discarded");
		Reopen();
	}

	// project/save goes in the session's send channel once everything own is confirmed.
	bool AllConfirmed()
	{
		if (!m_S || !m_S.m_Ready)
			return false;
		return m_S.m_Held.Count() == 0 && m_S.m_SentUnseen.Count() == 0 && m_S.m_Units.Count() == 0 && m_S.m_InFlightUnits.Count() == 0 && !m_S.m_ChainBuf && !m_S.m_PartBuf;
	}

	string SessionHead() { return SessHead(); }

	void SendInSession(string tag, string path, string body, Managed data = null)
	{
		if (m_Net)
			m_Net.Send(tag, path, body, data);
	}

	// Parts of an attachment still waiting in the send channel are dropped once the server has it complete.
	void DropQueued(string tag, string sum)
	{
		if (!m_Net)
			return;
		for (int i = m_Net.m_Queue.Count() - 1; i >= 0; i--)
		{
			DZS_CollabCall q = m_Net.m_Queue[i];
			DZS_CollabAttachMark mk = DZS_CollabAttachMark.Cast(q.m_Data);
			if (q.m_Tag == tag && mk && mk.m_Sum == sum)
				m_Net.m_Queue.RemoveOrdered(i);
		}
	}

	// Who brought the newest loot files: the host of a local project, else who saved last.
	string LootSource()
	{
		if (!m_S)
			return "";
		if (!m_S.IsHosted())
			return m_S.m_HostName;
		if (m_S.m_SavedByName != "")
			return m_S.m_SavedByName;
		return m_S.m_ServerName;
	}

	bool LootWaiting() { return m_Attach && m_Attach.m_MissingSince >= 0; }

	void RepeatInSession(DZS_CollabCall c, float after)
	{
		if (m_Net)
			m_Net.Repeat(c, after);
	}

	void SavedInSession(int seq)
	{
		if (m_S)
			m_S.m_FailSeq = -2;
	}

	void SaveFailedInSession()
	{
		if (m_S)
			m_S.m_FailSeq = m_S.m_SavedSeq;
	}

	void EndDiscard()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp)
			return;
		pp.CloseProject(true);
		pp.ShowHome(true);
	}

	// ---- joining ----

	ref DZS_CollabJoinDialog m_JoinDialog;
	string m_JoinHint;
	bool m_Joining;
	ref array<ref DZS_CollabListed> m_Listed = new array<ref DZS_CollabListed>();
	protected float m_ListAt;
	protected ref array<ref array<string>> m_SnapIn;
	protected ref DZS_CollabSession m_JoinS;
	protected int m_SnapSeq;
	protected int m_SnapParts;

	void OpenJoin()
	{
		if (m_S)
			return;
		m_JoinHint = "";
		m_Listed.Clear();
		m_ListAt = 0;
		m_JoinDialog = new DZS_CollabJoinDialog(this);
		m_JoinDialog.Open();
	}

	protected DZS_CollabNet JoinNet()
	{
		DZS_CollabServer s = DefaultServer();
		if (!s)
			return null;
		return NetFor(s.m_Url);
	}

	protected string TokenHeadOf(string url)
	{
		DZS_CollabServer s = FindServer(url);
		string tok = "";
		if (s)
			tok = s.m_Token;
		string tq = DZS_CollabJ.Q(tok);
		return "\"protocol\":\"1.0\",\"token\":" + tq;
	}

	protected string TokenHead()
	{
		DZS_CollabServer s = DefaultServer();
		string tok = "";
		if (s)
			tok = s.m_Token;
		string tq = DZS_CollabJ.Q(tok);
		return "\"protocol\":\"1.0\",\"token\":" + tq;
	}

	protected void UpdateJoinList()
	{
		if (!m_JoinDialog || m_JoinDialog.m_Closed || m_Now < m_ListAt)
			return;
		DZS_CollabNet n = JoinNet();
		if (!n || n.SendBusy() || n.Waiting() > 0)
			return;
		m_ListAt = m_Now + 10;
		string body = "{" + TokenHead() + "}";
		n.Send("sessions/list", "sessions/list", body);
	}

	protected void OnList(DZS_CollabReply r)
	{
		if (!r.Ok())
			return;
		m_Listed.Clear();
		DZS_SrcNode list = r.N("sessions");
		if (!list || !list.IsArr())
			return;
		foreach (DZS_SrcNode e : list.m_Items)
		{
			DZS_CollabListed l = new DZS_CollabListed();
			l.m_Id = DZS_CollabJ.S(e, "sessionId");
			l.m_Kind = DZS_CollabJ.S(e, "kind");
			l.m_Project = DZS_CollabJ.S(e, "projectName");
			l.m_Map = DZS_CollabJ.S(e, "map");
			l.m_Host = DZS_CollabJ.S(e.Get("host"), "name");
			l.m_Count = DZS_CollabJ.I(e, "participants");
			DZS_SrcNode href = e.Get("hostedRef");
			l.m_RefProvider = DZS_CollabJ.S(href, "provider");
			l.m_RefKey = DZS_CollabJ.S(href, "key");
			if (l.m_Kind == "hosted")
			{
				DZS_CollabServer ls = DefaultServer();
				if (ls)
					l.m_Host = ls.m_Name;
			}
			m_Listed.Insert(l);
		}
	}

	static string NormalCode(string typed)
	{
		string t = typed;
		t.ToUpper();
		string res = "";
		for (int i = 0; i < t.Length(); i++)
		{
			string ch = t.Substring(i, 1);
			if (ch == " " || ch == "-")
				continue;
			res += ch;
		}
		return res;
	}

	void Redeem(string typed)
	{
		DZS_CollabNet n = JoinNet();
		if (!n || m_Joining)
			return;
		string code = NormalCode(typed);
		m_JoinHint = "";
		string rq = DZS_CollabJ.Q(ReqId());
		string cq = DZS_CollabJ.Q(code);
		string body = "{" + TokenHead() + ",\"requestId\":" + rq + ",\"code\":" + cq + "}";
		n.Send("invite/redeem", "invite/redeem", body);
	}

	protected void OnRedeem(DZS_CollabReply r)
	{
		if (!r.Ok())
		{
			string code = r.Code();
			if (code == "invite_invalid" || code == "invite_expired" || code == "invite_used")
				m_JoinHint = DZS_Texts.Pick("Dieser Code gilt nicht (mehr) — frag nach einem neuen", "This code is not valid (any more) — ask for a new one");
			else if (code == "session_ended" || code == "session_unknown")
				m_JoinHint = DZS_Texts.Pick("Diese Sitzung ist schon vorbei", "This session is already over");
			else if (code == "throttled")
				m_JoinHint = DZS_Texts.Pick("Zu viele Versuche — warte eine Minute", "Too many attempts — wait a minute");
			else if (!r.m_Framed)
				m_JoinHint = DZS_Texts.Pick("Server nicht erreichbar", "Server not reachable");
			else
				m_JoinHint = DZS_Texts.Pick("Beitreten nicht möglich: ", "Cannot join: ") + r.ErrS("message");
			return;
		}
		string host = DZS_CollabJ.S(r.N("host"), "name");
		string jsid = r.S("sessionId");
		string jproj = r.S("projectName");
		string jmap = r.S("map");
		JoinSession(jsid, jproj, jmap, host);
	}

	void JoinListed(DZS_CollabListed l)
	{
		DZS_CollabServer s = DefaultServer();
		if (!s)
			return;
		string folder = "";
		if (l.m_Kind == "hosted" && l.m_RefKey != "")
		{
			folder = DZS_CollabHosted.FolderOf(s.m_Url, l.m_RefProvider, l.m_RefKey);
			DZS_CollabHostedProj p = new DZS_CollabHostedProj();
			p.m_Folder = folder;
			p.m_Server = s.m_Url;
			p.m_RefProvider = l.m_RefProvider;
			p.m_RefKey = l.m_RefKey;
			p.m_Name = l.m_Project;
			p.m_Map = l.m_Map;
			m_Hosted.Remember(p);
		}
		JoinSession(l.m_Id, l.m_Project, l.m_Map, l.m_Host, s.m_Url, folder);
	}

	// Opening a project held by the server while its session runs: join it.
	protected bool m_JoinFromOpen;

	void JoinHosted(DZS_CollabHostedProj p, string sid)
	{
		m_Hosted.Remember(p);
		JoinSession(sid, p.m_Name, p.m_Map, "", p.m_Server, p.m_Folder);
		m_JoinFromOpen = m_Joining;
	}

	void JoinSession(string sid, string project, string mapKey, string host, string url = "", string folder = "")
	{
		DZS_CollabServer s = DefaultServer();
		if (url != "")
			s = FindServer(url);
		if (!s || m_Joining || VersionStop(s))
			return;
		DZS_CollabNet n = NetFor(s.m_Url);
		if (!DZS_Maps.Available(mapKey))
		{
			string mn = DZS_Maps.Display(mapKey);
			m_JoinHint = DZS_Texts.Pick(mn + " ist nicht verfügbar", mn + " is not available");
			return;
		}
		m_Joining = true;
		m_JoinS = new DZS_CollabSession();
		m_JoinS.m_Server = s.m_Url;
		m_JoinS.m_ServerName = s.m_Name;
		m_JoinS.m_Id = sid;
		m_JoinS.m_Project = project;
		m_JoinS.m_Map = mapKey;
		m_JoinS.m_HostName = host;
		m_JoinS.m_Folder = folder;
		string rq = DZS_CollabJ.Q(ReqId());
		string sq = DZS_CollabJ.Q(sid);
		m_JoinS.m_Key = DZS_Random.Code(10);
		string kq = DZS_CollabJ.Q(m_JoinS.m_Key);
		string body = "{" + TokenHeadOf(s.m_Url) + ",\"requestId\":" + rq + "," + ClientJson() + ",\"sessionId\":" + sq + ",\"instanceKey\":" + kq + "}";
		n.Send("session/join", "session/join", body);
	}

	protected void OnJoined(DZS_CollabReply r)
	{
		if (!m_JoinS)
			return;
		if (!r.Ok())
		{
			m_Joining = false;
			string code = r.Code();
			if (code == "client_outdated")
			{
				bool fromOpen = m_JoinFromOpen;
				string ourl = m_JoinS.m_Server;
				string oname = m_JoinS.m_ServerName;
				m_JoinFromOpen = false;
				m_JoinS = null;
				m_JoinHint = "";
				DZS_ProjectsPlugin opp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
				if (fromOpen && opp && !Shown())
					opp.ShowHome(true);
				else if (!fromOpen && !m_JoinDialog)
				{
					DZS_CollabLostDialog stay = new DZS_CollabLostDialog(this);
					stay.Open();
				}
				OutdatedReply(ourl, oname, r);
				return;
			}
			if (code == "session_full")
				m_JoinHint = DZS_Texts.Pick("Sitzung ist voll", "Session is full");
			else
				m_JoinHint = DZS_Texts.Pick("Beitreten nicht möglich: ", "Cannot join: ") + r.ErrS("message");
			bool openHosted = m_JoinFromOpen;
			m_JoinFromOpen = false;
			m_JoinS = null;
			if (openHosted)
			{
				Msg("danger", "Beitreten nicht möglich: " + r.ErrS("message"), "Cannot join: " + r.ErrS("message"), 4);
				DZS_ProjectsPlugin hpp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
				if (hpp && !Shown())
					hpp.ShowHome(true);
				return;
			}
			if (!m_JoinDialog)
			{
				Msg("danger", "Beitreten nicht möglich", "Cannot join", 3);
				DZS_CollabLostDialog again = new DZS_CollabLostDialog(this);
				again.Open();
			}
			return;
		}
		m_JoinS.m_Pid = r.S("participantId");
		m_JoinS.m_Colour = r.I("colour");
		m_JoinS.m_Kind = r.S("kind");
		m_JoinS.m_Role = r.S("role");
		m_JoinS.m_CanInvite = r.B("canInvite");
		m_JoinS.m_CanSave = r.B("canSave");
		m_JoinS.m_CanManage = r.B("canManage");
		DZS_SrcNode snap = r.N("snapshot");
		m_SnapSeq = DZS_CollabJ.I(snap, "seq");
		m_SnapParts = DZS_CollabJ.I(snap, "parts");
		m_SnapIn = new array<ref array<string>>();
		AskSnapPart(0);
	}

	protected void AskSnapPart(int part)
	{
		DZS_CollabNet n = NetFor(m_JoinS.m_Server);
		string sq = DZS_CollabJ.Q(m_JoinS.m_Id);
		string pq = DZS_CollabJ.Q(m_JoinS.m_Pid);
		string body = "{" + TokenHeadOf(m_JoinS.m_Server) + ",\"sessionId\":" + sq + ",\"participantId\":" + pq + ",\"seq\":" + m_SnapSeq + ",\"part\":" + part + "}";
		n.Send("snapshot/get", "snapshot/get", body);
	}

	protected void OnSnapGet(DZS_CollabReply r)
	{
		if (!m_JoinS)
			return;
		if (!r.Ok())
		{
			m_Joining = false;
			m_JoinHint = DZS_Texts.Pick("Beitreten nicht möglich: ", "Cannot join: ") + r.Code();
			m_JoinS = null;
			return;
		}
		array<string> pieces = DZS_CollabJ.Strings(r.N("pieces"));
		m_SnapIn.Insert(pieces);
		if (m_SnapIn.Count() < m_SnapParts)
		{
			AskSnapPart(m_SnapIn.Count());
			return;
		}
		array<string> chunks = new array<string>();
		foreach (array<string> part : m_SnapIn)
		{
			foreach (string c : part)
				chunks.Insert(c);
		}
		m_SnapIn = null;
		DZS_Log.Info("collab: joined " + m_JoinS.m_Id + " as " + m_JoinS.m_Pid + " (" + m_JoinS.m_Role + "), state " + DZS_Files.Length(chunks) + " bytes, seq " + m_SnapSeq);
		m_JoinS.m_After = m_SnapSeq;
		m_JoinS.m_Applied = m_SnapSeq;
		m_S = m_JoinS;
		m_JoinS = null;
		m_Joining = false;
		m_JoinFromOpen = false;
		if (m_JoinDialog)
			m_JoinDialog.Close();
		m_JoinDialog = null;
		ToCarry();
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		string plate = DZS_Texts.Pick("Gemeinsam · ", "Together · ") + m_S.m_HostName;
		if (m_S.IsHosted())
		{
			plate = DZS_Texts.Pick("Gemeinsam · ", "Together · ") + m_S.m_ServerName;
			m_S.m_HostName = m_S.m_ServerName;
		}
		else
			m_S.m_Folder = "";
		pp.OpenShared(m_S.m_Project, m_S.m_Map, chunks, plate, m_S.m_Folder);
	}

	// Plain texts that survive the reload of the mission module when the session's map is loaded first.
	protected void ToCarry()
	{
		map<string, string> c = DZS_Handover.Carry();
		c.Clear();
		c.Set("server", m_S.m_Server);
		c.Set("serverName", m_S.m_ServerName);
		c.Set("sessionId", m_S.m_Id);
		c.Set("participantId", m_S.m_Pid);
		c.Set("kind", m_S.m_Kind);
		c.Set("role", m_S.m_Role);
		string col = m_S.m_Colour.ToString();
		c.Set("colour", col);
		c.Set("canInvite", Flag(m_S.m_CanInvite));
		c.Set("canSave", Flag(m_S.m_CanSave));
		c.Set("canManage", Flag(m_S.m_CanManage));
		c.Set("project", m_S.m_Project);
		c.Set("map", m_S.m_Map);
		c.Set("host", m_S.m_HostName);
		string after = m_S.m_After.ToString();
		c.Set("after", after);
		c.Set("key", m_S.m_Key);
		c.Set("folder", m_S.m_Folder);
	}

	static string Flag(bool b)
	{
		if (b)
			return "1";
		return "0";
	}

	protected void FromCarry(map<string, string> c)
	{
		m_S = new DZS_CollabSession();
		m_S.m_Server = c.Get("server");
		m_S.m_ServerName = c.Get("serverName");
		m_S.m_Id = c.Get("sessionId");
		m_S.m_Pid = c.Get("participantId");
		m_S.m_Kind = c.Get("kind");
		m_S.m_Role = c.Get("role");
		string col = c.Get("colour");
		m_S.m_Colour = col.ToInt();
		m_S.m_CanInvite = c.Get("canInvite") == "1";
		m_S.m_CanSave = c.Get("canSave") == "1";
		m_S.m_CanManage = c.Get("canManage") == "1";
		m_S.m_Project = c.Get("project");
		m_S.m_Map = c.Get("map");
		m_S.m_HostName = c.Get("host");
		string after = c.Get("after");
		m_S.m_After = after.ToInt();
		m_S.m_Applied = m_S.m_After;
		m_S.m_Key = c.Get("key");
		m_S.m_Folder = c.Get("folder");
		DZS_Log.Info("collab: session " + m_S.m_Id + " carried over the world change");
	}

	// The shared project is open: hook into the sync slot and start polling from the snapshot's sequence number.
	override void OnPluginEvent(string topic, Managed payload)
	{
		if (topic != "projects.sharedOpened" || !m_S || m_S.m_Ready)
			return;
		DZS_Handover.Carry().Clear();
		DZS_Instance inst = Shown();
		if (!inst)
			return;
		m_Slot = new DZS_CollabSlot();
		m_Slot.m_P = this;
		inst.AttachSync(m_Slot);
		m_Slot.SetNoExternalSystemOps(true);
		MarkCounter(inst);
		m_Net = new DZS_CollabNet(m_S.m_Server, m_Link);
		m_S.m_Ready = true;
		m_PollRetryAt = 0;
		m_Kernel.GetUi().SetPanelShown(PANEL, true);
		m_Kernel.GetScreen().SetProviderPlate(DZS_Texts.Pick("Gemeinsam · ", "Together · ") + m_S.m_HostName, "success");
		DZS_Log.Info("collab: rights invite " + m_S.m_CanInvite + ", save " + m_S.m_CanSave + ", manage " + m_S.m_CanManage);
		DZS_Log.Info("collab: guest in session " + m_S.m_Id + ", cursor " + m_S.m_After);
	}

	void AskEnd(string what, string arg, string arg2)
	{
		if (!m_S)
			return;
		m_LeaveWhat = what;
		m_LeaveArg = arg;
		m_LeaveArg2 = arg2;
		int n = m_S.Others();
		if (n == 0)
		{
			EndSession();
			return;
		}
		string de = "Damit endet die Sitzung für " + n + DZS_Issues.Plural(n, " anderen", " andere");
		string en = "This ends the session for " + n + DZS_Issues.Plural(n, " other", " others");
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Sitzung beenden", "End session");
		string a1 = DZS_Texts.Pick(de, en);
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(k1, t1, a1);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("end", t1, "danger");
		c.m_OnResult.Insert(OnEndAnswer);
		c.Open();
	}

	void OnEndAnswer(DZS_Dialog d, string result)
	{
		if (result != "end")
		{
			m_LeaveWhat = "";
			return;
		}
		EndSession();
	}

	protected void EndSession()
	{
		if (!m_S)
			return;
		string body = "{" + SessHead() + "}";
		DZS_CollabNet n = NetFor(m_S.m_Server);
		n.Send("session/end", "session/end", body);
		DZS_Log.Info("collab: session " + m_S.m_Id + " ended by the host");
		EndLocally("ended");
		ContinueLeave();
	}

	// Leave: unsent own changes go out first (at most 10 s, knob "Leaving …"), else the question.
	float m_LeaveWaitT = -1;

	void LeaveSession()
	{
		if (!m_S || m_LeaveWaitT >= 0)
			return;
		int unsent = Unsent();
		if (unsent > 0)
		{
			DZS_Log.Info("collab: leaving, " + unsent + " unsent change(s) go out first");
			m_LeaveWaitT = 0;
			return;
		}
		LeaveNow();
	}

	protected void UpdateLeaveWait(float dt)
	{
		if (m_LeaveWaitT < 0)
			return;
		if (!m_S)
		{
			m_LeaveWaitT = -1;
			return;
		}
		m_LeaveWaitT += dt;
		int unsent = Unsent();
		if (unsent == 0)
		{
			DZS_Log.Info("collab: everything sent after " + DZS_Num.Fixed(m_LeaveWaitT, 1) + " s, leaving");
			LeaveNow();
			return;
		}
		if (m_LeaveWaitT < 10)
			return;
		m_LeaveWaitT = -1;
		DZS_Log.Info("collab: " + unsent + " change(s) did not reach the server in 10 s");
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick(DZS_Num.Count(unsent) + DZS_Issues.Plural(unsent, " Änderung hat den Server nicht erreicht und geht verloren, wenn du jetzt gehst.", " Änderungen haben den Server nicht erreicht und gehen verloren, wenn du jetzt gehst."), DZS_Num.Count(unsent) + DZS_Issues.Plural(unsent, " change did not reach the server and will be lost if you leave now.", " changes did not reach the server and will be lost if you leave now."));
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(k1, DZS_Texts.Pick("Sitzung verlassen", "Leave session"), t1);
		c.AddFootButton("leave", DZS_Texts.Pick("Trotzdem verlassen", "Leave anyway"), "danger");
		c.AddFootButton("copy", DZS_Texts.Pick("Als eigene Kopie speichern …", "Save as own copy …"), "side");
		c.AddFootButton("wait", DZS_Texts.Pick("Weiter warten", "Keep waiting"), "main");
		c.m_OnResult.Insert(OnUnsentLeaveAnswer);
		c.Open();
	}

	bool IsLeaving() { return m_LeaveWaitT >= 0; }

	void OnUnsentLeaveAnswer(DZS_Dialog d, string result)
	{
		if (!m_S)
			return;
		if (result == "leave")
		{
			LeaveNow();
			return;
		}
		if (result == "copy")
		{
			string what = m_LeaveWhat;
			m_LeaveWhat = "";
			EndLocally("left");
			DZS_Log.Info("collab: left with unsent changes, own copy (then " + what + ")");
			EndCopy();
			return;
		}
		if (result == "wait")
		{
			DZS_Log.Info("collab: keep waiting for " + Unsent() + " change(s)");
			m_LeaveWaitT = 0;
			return;
		}
		m_LeaveWhat = "";
	}

	protected void LeaveNow()
	{
		m_LeaveWaitT = -1;
		if (!m_S)
			return;
		string body = "{" + SessHead() + "}";
		DZS_CollabNet n = NetFor(m_S.m_Server);
		n.Send("session/leave", "session/leave", body);
		bool guest = m_S.IsGuest();
		bool hosted = m_S.IsHosted();
		int unsaved = m_S.m_SaveUnsaved;
		EndLocally("left");
		if (hosted && unsaved > 0)
			Msg("", DZS_Num.Count(unsaved) + DZS_Issues.Plural(unsaved, " ungespeicherte Änderung bleibt in der Sitzung", " ungespeicherte Änderungen bleiben in der Sitzung"), DZS_Num.Count(unsaved) + DZS_Issues.Plural(unsaved, " unsaved change stays in the session", " unsaved changes stay in the session"), 2);
		if (m_LeaveWhat == "" && (guest || hosted))
		{
			EndDiscard();
			return;
		}
		if (guest || hosted)
		{
			DZS_ProjectsPlugin lpp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
			if (lpp)
				lpp.CloseProject(true);
		}
		ContinueLeave();
	}

	protected void ContinueLeave()
	{
		string what = m_LeaveWhat;
		m_LeaveWhat = "";
		if (what == "")
			return;
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (pp)
			pp.Leave(what, m_LeaveArg, m_LeaveArg2);
	}

	bool InterceptLeave(string what, string arg, string arg2)
	{
		if (!m_S)
			return false;
		if (m_S.IsHost())
		{
			AskEnd(what, arg, arg2);
			return true;
		}
		m_LeaveWhat = what;
		m_LeaveArg = arg;
		m_LeaveArg2 = arg2;
		LeaveSession();
		return true;
	}

	protected void DetachSlot()
	{
		m_CpWanted = false;
		m_CpBusy = false;
		m_CpParts = null;
		if (m_Slot)
			m_Slot.Detach();
		m_Slot = null;
	}

	// "Save as own copy …" cannot fetch a missing loot text any more once the session is over.
	protected bool m_CopyNoLoot;

	protected void EndLocally(string why)
	{
		m_CopyNoLoot = LootWaiting();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (scene)
		{
			foreach (string lid, DZS_CollabLiveObj lo : m_LiveObjs)
				scene.ClearView(lid);
		}
		m_LiveObjs.Clear();
		m_LiveId = "";
		m_LiveCommit = "";
		m_StepClaim = false;
		if (m_World)
			m_Kernel.GetScreen().HideBanner("collab.lost");
		DetachSlot();
		if (m_Net)
			m_Net.DropAll();
		m_Net = null;
		m_S = null;
		m_Starting = false;
		m_Attach.Reset();
		if (m_World)
		{
			m_Kernel.GetUi().SetPanelShown(PANEL, false);
			m_Kernel.GetScreen().SetStatusIcon("collab.down", false, "", "", "");
		}
		if (m_InviteDialog)
			m_InviteDialog.Close();
		m_InviteDialog = null;
		DZS_Log.Info("collab: session over (" + why + ")");
	}

	// ---- invite ----

	void OpenInvite()
	{
		if (!m_S || !m_S.m_CanInvite)
			return;
		m_InviteCode = "";
		NewCode();
		m_InviteDialog = new DZS_CollabInviteDialog(this);
		m_InviteDialog.Open();
	}

	void NewCode()
	{
		if (!m_S)
			return;
		m_InviteState = "asking";
		string body = "{" + SessHead() + ",\"requestId\":" + DZS_CollabJ.Q(ReqId()) + "}";
		m_Net.Send("invite/create", "invite/create", body);
	}

	protected void OnInvite(DZS_CollabReply r)
	{
		if (!r.Ok())
		{
			m_InviteState = "failed";
			return;
		}
		m_InviteCode = FormatCode(r.S("code"));
		m_InviteState = "ok";
		DZS_Log.Info("collab: invite code " + m_InviteCode);
	}

	// ---- participant list knobs ----

	void JumpTo(DZS_CollabPeer p)
	{
		if (!p || !p.m_HasPos)
			return;
		DZS_CameraPlugin cam = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
		if (!cam)
			return;
		cam.FlyTo(cam.ClampTyped(p.m_Pos), cam.GetYaw(), cam.GetPitch());
		DZS_Log.Info("collab: jump to " + p.m_Name + " at " + p.m_Pos);
	}

	void ReleaseOf(DZS_CollabPeer p)
	{
		if (!m_S || !p)
			return;
		string rq = DZS_CollabJ.Q(ReqId());
		string tq = DZS_CollabJ.Q(p.m_Pid);
		string body = "{" + SessHead() + ",\"requestId\":" + rq + ",\"target\":" + tq + "}";
		m_Net.Send("locks/release", "locks/release", body);
	}

	protected string m_RemoveTarget;

	void AskRemove(DZS_CollabPeer p)
	{
		if (!m_S || !p)
			return;
		m_RemoveTarget = p.m_Pid;
		string de = p.Label() + " aus der Sitzung entfernen?";
		string en = "Remove " + p.Label() + " from the session?";
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Entfernen", "Remove");
		string a1 = DZS_Texts.Pick(de, en);
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(k1, t1, a1);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("remove", t1, "danger");
		c.m_OnResult.Insert(OnRemoveAnswer);
		c.Open();
	}

	void OnRemoveAnswer(DZS_Dialog d, string result)
	{
		if (result != "remove" || !m_S)
			return;
		string body = "{" + SessHead() + ",\"target\":" + DZS_CollabJ.Q(m_RemoveTarget) + "}";
		m_Net.Send("participant/remove", "participant/remove", body);
	}

	// ---- save rules ----

	bool IsHostOf(DZS_Instance inst) { return m_S && m_S.m_Ready && m_S.IsHost() && inst == Shown(); }
	bool IsGuestOf(DZS_Instance inst) { return m_S && m_S.IsGuest() && inst == Shown(); }
	bool IsHostedOf(DZS_Instance inst) { return m_S && m_S.IsHosted() && inst == Shown(); }

	bool RuleUnsaved(DZS_Instance inst, out bool unsaved)
	{
		if (!IsHostedOf(inst) || !m_S.m_Ready)
			return false;
		unsaved = m_S.m_SaveUnsaved > 0 || !AllConfirmed() || inst.IsSaveFailed();
		return true;
	}

	// Server project: everyone shows the same count — the operations after the last save, whoever made them.
	protected bool HostedStatus(DZS_Instance inst, out string text, out string lamp)
	{
		lamp = "";
		if (!m_S.m_Ready || inst.IsSaving())
			return false;
		if (inst.IsSaveFailed())
			return false;
		if (m_S.m_SaveState == "saving")
		{
			text = DZS_Texts.Pick("Speichert …", "Saving …");
			return true;
		}
		int n = m_S.m_SaveUnsaved;
		if (n > 0)
		{
			text = DZS_Texts.Pick(DZS_Num.Count(n) + DZS_Issues.Plural(n, " ungespeicherte Änderung", " ungespeicherte Änderungen"), DZS_Num.Count(n) + DZS_Issues.Plural(n, " unsaved change", " unsaved changes"));
			return true;
		}
		DZS_CollabPeer me = m_S.Me();
		bool mine = me && m_S.m_SavedByUser != "" && me.m_UserId == m_S.m_SavedByUser;
		string clock = "";
		if (m_S.m_SavedAgo >= 0)
			clock = " " + m_S.m_SavedClock;
		if (m_S.m_SavedByName == "")
			text = DZS_Texts.Pick("Gespeichert", "Saved");
		else if (mine)
			text = DZS_Texts.Pick("Gespeichert", "Saved") + clock;
		else
			text = DZS_Texts.Pick("Gespeichert von " + m_S.m_SavedByName + clock, "Saved by " + m_S.m_SavedByName + clock);
		return true;
	}

	bool RuleStatus(DZS_Instance inst, out string text, out string lamp)
	{
		if (IsHostedOf(inst))
			return HostedStatus(inst, text, lamp);
		if (!IsGuestOf(inst))
			return false;
		string host = m_S.m_HostName;
		lamp = "";
		if (m_S.m_SaveState == "saving")
			text = DZS_Texts.Pick(host + " speichert …", host + " is saving …");
		else if (m_S.m_SaveState == "failed")
		{
			text = DZS_Texts.Pick("Speichern bei " + host + " fehlgeschlagen", "Saving at " + host + " failed");
			lamp = "danger";
		}
		else if (m_S.m_SaveState == "unsaved" && m_S.m_SaveUnsaved > 0)
		{
			int n = m_S.m_SaveUnsaved;
			string de = DZS_Num.Count(n) + DZS_Issues.Plural(n, " ungespeicherte Änderung bei ", " ungespeicherte Änderungen bei ") + host;
			string en = DZS_Num.Count(n) + DZS_Issues.Plural(n, " unsaved change at ", " unsaved changes at ") + host;
			text = DZS_Texts.Pick(de, en);
		}
		else if (m_S.m_SavedAgo >= 0)
			text = DZS_Texts.Pick("Gespeichert von " + host + " " + m_S.m_SavedClock, "Saved by " + host + " " + m_S.m_SavedClock);
		else
			text = DZS_Texts.Pick("Gespeichert von " + host, "Saved by " + host);
		return true;
	}

	bool RuleSaveBlocked(DZS_Instance inst, out string de, out string en)
	{
		if (IsHostedOf(inst))
		{
			if (!m_S.m_Ready)
			{
				de = "Wartet auf die Sitzung";
				en = "Waiting for the session";
				return true;
			}
			if (LootWaiting())
			{
				de = "Neue Loot-Dateien werden geholt …";
				en = "Fetching new loot files …";
				return true;
			}
			if (m_S.m_CanSave)
				return false;
			de = "Speichern dürfen nur Teilnehmer mit Rechten am Projekt";
			en = "Only participants with rights to the project can save";
			return true;
		}
		if (!IsGuestOf(inst) || !m_S.m_Ready)
			return false;
		de = "Speichern kann nur " + m_S.m_HostName;
		en = "Only " + m_S.m_HostName + " can save";
		return true;
	}

	bool RuleSaveAsBlocked(DZS_Instance inst, out string de, out string en)
	{
		if (!m_S || inst != Shown())
			return false;
		de = "In einer gemeinsamen Sitzung: Projekt › Kopie speichern …";
		en = "In a shared session: Project › Save a copy …";
		return true;
	}

	void HostSaved(DZS_Instance inst, bool ok)
	{
		if (IsHostOf(inst))
			DZS_Log.Info("collab: host saved, ok " + ok + ", unsaved " + inst.GetUnsavedCount());
	}

	void HostSaving(DZS_Instance inst) {}

	// ---- replies ----

	void OnReply(DZS_CollabCall c, DZS_CollabReply r)
	{
		string tag = c.m_Tag;
		if (tag == "info" || tag == "add")
			OnInfo(c, r);
		else if (tag == "auth/name" || tag == "auth/code/start" || tag == "auth/code/poll")
			OnAuth(c, r);
		else if (tag == "session/create")
			OnCreated(r);
		else if (tag == "snapshot/put")
			OnSnapPut(c, r);
		else if (tag == "checkpoint/put")
			OnCpPut(c, r);
		else if (tag == "poll")
			OnPoll(c, r);
		else if (tag == "ops/send")
			OnOpsSent(c, r);
		else if (tag == "locks/set")
			OnLocksSet(c, r);
		else if (tag == "invite/create")
			OnInvite(r);
		else if (tag == "sessions/list")
			OnList(r);
		else if (tag == "invite/redeem")
			OnRedeem(r);
		else if (tag == "session/join")
			OnJoined(r);
		else if (tag == "snapshot/get")
			OnSnapGet(r);
		else if (tag == "resume")
			OnResumed(c, r);
		else if (tag == "hosted/save")
			m_Hosted.OnSaved(c, r);
		else if (tag == "attach/put")
			m_Attach.OnPut(c, r);
		else if (tag == "attach/get")
			m_Attach.OnGet(c, r);
		else if (tag == "attach/want")
		{
			if (!r.Ok())
				DZS_Log.Warn("collab: attachment/want " + r.Code());
		}
		else if (tag == "hosted/end")
			OnDiscarded(r);
		else if (tag == "live")
		{
			if (r.m_Framed && m_S)
				Up();
		}
		else if (tag == "participant/remove" || tag == "locks/release")
		{
			if (!r.Ok())
				DZS_Log.Warn("collab: " + tag + " " + r.Code());
		}
	}

	protected int m_OpCount;
	protected int m_DumpedCount = -1;
	protected float m_DumpAt;

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		m_OpCount++;
	}

	// -dzsCollabDump (developer): the shown project state as text in $profile:dzstudio\collab-state.json after changes.
	protected void Dump()
	{
		string arg;
		if (!GetCLIParam("dzsCollabDump", arg) || m_OpCount == m_DumpedCount || m_Now < m_DumpAt)
			return;
		DZS_Instance inst = Shown();
		if (!inst)
			return;
		m_DumpAt = m_Now + 1;
		m_DumpedCount = m_OpCount;
		array<string> chunks = inst.ExportState();
		DZS_Files.Write("$profile:dzstudio\\collab-state.json", chunks);
	}

	protected void StartTimeout()
	{
		string why = DZS_Texts.Pick("Server nicht erreichbar", "Server not reachable");
		if (m_Creating)
		{
			CreateFailed(why);
			return;
		}
		bool hosted = m_StartProj != null;
		StartFailed(why);
		if (hosted)
			HostedStartFailed(why);
	}

	override void OnUpdate(float dt)
	{
		m_Now += dt;
		if (m_NeedsNewer)
			LeaveForNewer();
		SyncFixed();
		PublishPeople();
		UpdateLeaveWait(dt);
		if (m_Hosted)
			m_Hosted.Update(dt);
		Dump();
		UpdateMapMarks();
		SegmentClick();
		UpdateCodeLogin();
		UpdateJoinList();
		if (!m_S)
		{
			UpdateSegment();
			return;
		}
		if (m_Starting && !m_S.m_Ready && m_Now > m_StartFailAt && m_Net && !m_Net.SendBusy())
		{
			StartTimeout();
			UpdateSegment();
			return;
		}
		if (m_Starting && !m_S.m_Ready && m_Now > m_StartFailAt)
		{
			StartTimeout();
			UpdateSegment();
			return;
		}
		if (m_S && m_S.m_Ready)
		{
			if (m_Net)
				m_Net.Pump();
			SendLocks();
			SendLive();
			SendOps();
			m_Attach.Update(dt);
			TryCheckpoint();
			StartPoll();
			WatchConnection();
			GlideLive(dt);
		}
		UpdateSegment();
	}

	protected void UpdateSegment()
	{
		if (!m_Segment)
			return;
		if (!m_S || !m_S.m_Ready)
		{
			m_Segment.SetText("");
			m_Segment.SetLamp("");
			return;
		}
		int n = m_S.m_Peers.Count();
		if (n == 0)
			n = 1;
		m_Segment.SetText(DZS_Texts.Pick("Gemeinsam · ", "Together · ") + n);
		string lamp = "success";
		if (m_S.m_DownSince >= 0 && m_Now - m_S.m_DownSince >= 30)
			lamp = "danger";
		else if (m_S.m_DownSince >= 0 && m_Now - m_S.m_DownSince >= 3)
			lamp = "warning";
		m_Segment.SetLamp(lamp);
	}

	// On the map: every other participant as a mark at the place the list names; a ring without connection.
	protected ref DZS_CollabMapOwner m_MapOwner;
	protected string m_MarkSig;

	protected void UpdateMapMarks()
	{
		string sig = "";
		array<ref DZS_MapMark> marks = new array<ref DZS_MapMark>();
		if (m_S && m_S.m_Ready)
		{
			foreach (DZS_CollabPeer p : m_S.m_Peers)
			{
				if (p.m_Pid == m_S.m_Pid || !p.m_HasPos)
					continue;
				DZS_MapMark m = new DZS_MapMark();
				m.m_Id = p.m_Pid;
				m.m_X = p.m_Pos[0];
				m.m_Z = p.m_Pos[2];
				m.m_Color = DZS_CollabColors.Get(p.m_Colour);
				m.m_Ring = p.m_State == "disconnected";
				m.m_Diamond = p.IsAgent();
				m.m_Tip = p.Label();
				string sub = PeerSub(p);
				if (sub != "")
					m.m_Tip += " · " + sub;
				marks.Insert(m);
				sig += p.m_Pid + "|" + m.m_X + "|" + m.m_Z + "|" + m.m_Color + "|" + m.m_Ring + "|" + m.m_Diamond + "|" + m.m_Tip + ";";
			}
		}
		if (sig == m_MarkSig)
			return;
		m_MarkSig = sig;
		if (marks.Count() == 0)
		{
			DZS_MapMarks.Clear("collab");
			return;
		}
		if (!m_MapOwner)
		{
			m_MapOwner = new DZS_CollabMapOwner();
			m_MapOwner.m_P = this;
		}
		DZS_MapMarks.Set("collab", marks, m_MapOwner);
	}

	int StateSeqOf(DZS_Instance inst)
	{
		if (!m_S || inst != Shown())
			return -1;
		return m_S.m_Applied;
	}

	void JumpToPid(string pid)
	{
		if (!m_S)
			return;
		foreach (DZS_CollabPeer p : m_S.m_Peers)
		{
			if (p.m_Pid == pid)
				JumpTo(p);
		}
	}

	// Text for the participant list: place and distance, or the state.
	string PeerSub(DZS_CollabPeer p)
	{
		if (p.m_Pid == m_S.m_Pid)
		{
			if (p.m_Held > 0)
				return p.m_Held.ToString() + DZS_Texts.Pick(" belegt", " held");
			return "";
		}
		if (p.m_State == "disconnected")
			return DZS_Texts.Pick("keine Verbindung", "no connection");
		if (p.IsAgent())
		{
			string ph = p.PhaseText();
			if (p.m_Held > 0)
				ph += " · " + p.m_Held + DZS_Texts.Pick(" belegt", " held");
			return ph;
		}
		if (p.m_State == "idle")
		{
			int mins = p.m_Idle / 60;
			return DZS_Texts.Pick("inaktiv seit " + mins + " min", "idle for " + mins + " min");
		}
		if (!p.m_HasPos)
			return "";
		int x = Math.Round(p.m_Pos[0]);
		int z = Math.Round(p.m_Pos[2]);
		vector cam = GetGame().GetCurrentCameraPosition();
		vector a = p.m_Pos;
		a[1] = 0;
		cam[1] = 0;
		float d = vector.Distance(a, cam);
		string dist;
		if (d >= 1000)
		{
			float km = d / 1000.0;
			dist = DZS_Num.Fixed(km, 1) + " km";
		}
		else
		{
			int dm = Math.Round(d);
			dist = dm.ToString() + " m";
		}
		string s = DZS_Texts.Pick("bei ", "at ") + x + " · " + z + " · " + dist;
		if (p.m_Held > 0)
			s += " · " + p.m_Held + DZS_Texts.Pick(" belegt", " held");
		return s;
	}

	string PeerWhere(DZS_CollabPeer p)
	{
		if (!p.m_HasPos)
			return DZS_Texts.Pick("Ort noch unbekannt", "Location not known yet");
		int x = Math.Round(p.m_Pos[0]);
		int z = Math.Round(p.m_Pos[2]);
		return DZS_Texts.Pick("bei ", "at ") + x + " · " + z;
	}

	protected void PublishPeople()
	{
		if (!m_S || !m_S.m_Ready || m_S.m_Ended)
		{
			DZS_SessionPeople.Clear();
			return;
		}
		array<ref DZS_SessionMember> list = new array<ref DZS_SessionMember>();
		array<DZS_CollabPeer> order = DZS_CollabPanel.Ordered(m_S);
		foreach (DZS_CollabPeer p : order)
		{
			if (p.IsAgent())
				continue;
			DZS_SessionMember m = new DZS_SessionMember();
			m.m_Pid = p.m_Pid;
			m.m_UserId = p.m_UserId;
			m.m_Name = p.m_Name;
			m.m_Color = DZS_CollabColors.Get(p.m_Colour);
			list.Insert(m);
		}
		DZS_SessionPeople.Publish(m_S.m_Server, m_S.m_Pid, list);
	}
}

class DZS_CollabMapOwner : DZS_MapMarkOwner
{
	DZS_CollabPlugin m_P;

	override void OnMapMarkAction(string markId) { m_P.JumpToPid(markId); }
}

class DZS_CollabSaveTarget : DZS_SaveTarget
{
	DZS_CollabPlugin m_P;

	override void Targets(array<string> ids, array<string> names) { m_P.SaveTargets(ids, names); }
	override string TargetState(string id) { return m_P.SaveTargetState(id); }
	override void SaveAs(string id, string name) { m_P.SaveToServer(id, name); }
}
