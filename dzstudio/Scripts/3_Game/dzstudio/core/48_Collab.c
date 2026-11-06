// Public parts a sync plugin needs besides the sync slot.
// One plugin at a time installs each of them; without one everything works as alone.

// Save rules: who may save, what the status bar says about saving, and whether question and backup happen.
class DZS_SaveRules : Managed
{
	protected static ref DZS_SaveRules s_Rules;

	static void Set(DZS_SaveRules r) { s_Rules = r; }
	static DZS_SaveRules Get() { return s_Rules; }

	// Replaces the save state at the left of the status bar; lamp "" or a tone.
	bool StatusText(DZS_Instance inst, out string text, out string lamp) { return false; }
	// Save (Ctrl+S, knob, autosave) off with a reason.
	bool SaveBlocked(DZS_Instance inst, out string de, out string en) { return false; }
	bool SaveAsBlocked(DZS_Instance inst, out string de, out string en) { return false; }
	// No question about unsaved changes and no backup (the work lies elsewhere).
	bool NoLocalWork(DZS_Instance inst) { return false; }
	// Autosave at the latest this many seconds after the first unsaved change, even while changes go on (0 = off).
	float AutoSaveLatest(DZS_Instance inst) { return 0; }
	// Autosave has no effect for this instance (the work lies with the server); saving by hand stays.
	bool NoAutoSave(DZS_Instance inst) { return false; }
	// Whether there is anything to save, when the plugin knows it better than the own history (true = decided).
	bool Unsaved(DZS_Instance inst, out bool unsaved) { return false; }
	// Leaving the project (home, open, new, quit): true = the plugin asks first and calls leave again itself.
	bool InterceptLeave(string what, string arg, string arg2) { return false; }
	// After a save to the local folder: ok or failed.
	// The sequence number of the session the state stands at (-1 = no session).
	int StateSeq(DZS_Instance inst) { return -1; }
	void Saved(DZS_Instance inst, bool ok) {}
	void Saving(DZS_Instance inst) {}
}

// Selection filter: objects others hold cannot be selected; their frames and the pointer hint name the holder.
class DZS_SelectGuard : Managed
{
	protected static ref DZS_SelectGuard s_Guard;

	static void Set(DZS_SelectGuard g) { s_Guard = g; }
	static DZS_SelectGuard Get() { return s_Guard; }

	static bool IsBlocked(string id)
	{
		if (!s_Guard)
			return false;
		return s_Guard.Blocks(id);
	}

	bool Blocks(string id) { return false; }
	string HolderName(string id) { return ""; }
	// ARGB of the holder's colour.
	int HolderColor(string id) { return 0; }
	// Objects of others to draw: id, colour, lapsed (hollow dot only).
	void Held(array<string> ids, array<int> colors, array<bool> lapsed) {}
	// After every change of the own selection.
	void SelectionChanged(array<string> ids) {}
	// Changes to these objects wait for a claim: "" = go, else the reason for greying out.
	string WaitReason(array<string> ids) { return ""; }
	// Before undo/redo with the objects the step changes: false = go now; true = the guard took over (it calls
	// Undo/Redo of the instance itself once it holds them, or shows why not).
	bool ClaimForStep(DZS_Instance inst, array<string> ids, bool redo) { return false; }

	static string Waits(array<string> ids)
	{
		if (!s_Guard)
			return "";
		return s_Guard.WaitReason(ids);
	}
}

// Controls off while a change has to wait: the kernel asks the guard once per frame about the
// selection; every action, field and object-toolbar knob registered as "changes the selection" is off with this reason.
class DZS_ChangeWait
{
	protected static string s_Reason;

	static void Refresh(array<string> selection, DZS_Instance shown)
	{
		if (!selection || selection.Count() == 0)
		{
			s_Reason = "";
			return;
		}
		DZS_EditGate gate = DZS_EditGate.Get();
		string de, en;
		if (gate && shown && gate.ViewOnly(shown, de, en))
		{
			s_Reason = DZS_Texts.Pick(de, en);
			return;
		}
		s_Reason = DZS_SelectGuard.Waits(selection);
	}

	static string Reason() { return s_Reason; }
}

// Change gate while nothing may change (connection lost): actions and local operations off with a reason.
class DZS_EditGate : Managed
{
	protected static ref DZS_EditGate s_Gate;

	static void Set(DZS_EditGate g) { s_Gate = g; }
	static DZS_EditGate Get() { return s_Gate; }

	static bool ActionOff(string actionId, out string de, out string en)
	{
		if (!s_Gate)
			return false;
		return s_Gate.ActionBlocked(actionId, de, en);
	}

	static bool ChangesOff(DZS_Instance inst)
	{
		if (!s_Gate)
			return false;
		string de, en;
		return s_Gate.ViewOnly(inst, de, en);
	}

	bool ActionBlocked(string actionId, out string de, out string en) { return false; }
	bool ViewOnly(DZS_Instance inst, out string de, out string en) { return false; }
	// Step size: > 0 = the text form of a new step is measured before it happens, and a larger one is
	// refused (the gate says why to the user).
	int MaxStepBytes(DZS_Instance inst) { return 0; }
	void StepRefused(DZS_Instance inst, int bytes, int limit) {}
}

// Intermediate values: the plugin running a gesture reports its intermediate values;
// one listener (the sync plugin) gets them. End(true) comes right before the operation of the gesture is submitted.
class DZS_LiveFeed : Managed
{
	protected static ref DZS_LiveFeed s_Feed;

	static void Set(DZS_LiveFeed f) { s_Feed = f; }

	static void Report(array<string> ids, array<vector> pos, array<vector> ypr, array<float> scale)
	{
		if (s_Feed)
			s_Feed.OnGesture(ids, pos, ypr, scale);
	}

	static void End(bool commit)
	{
		if (s_Feed)
			s_Feed.OnEnd(commit);
	}

	void OnGesture(array<string> ids, array<vector> pos, array<vector> ypr, array<float> scale) {}
	void OnEnd(bool commit) {}
}

// Knobs of plugins in the top bar of the main menu, left of "Settings"; a click
// triggers the action.
class DZS_HomeButton : Managed
{
	string m_Action;
	string m_De;
	string m_En;
	string m_Icon;
}

class DZS_HomeButtons
{
	protected static ref array<ref DZS_HomeButton> s_List;

	static void Add(string actionId, string de, string en, string icon)
	{
		if (!s_List)
			s_List = new array<ref DZS_HomeButton>();
		foreach (DZS_HomeButton b : s_List)
		{
			if (b.m_Action == actionId)
				return;
		}
		DZS_HomeButton nb = new DZS_HomeButton();
		nb.m_Action = actionId;
		nb.m_De = de;
		nb.m_En = en;
		nb.m_Icon = icon;
		s_List.Insert(nb);
	}

	static array<DZS_HomeButton> All()
	{
		array<DZS_HomeButton> r = new array<DZS_HomeButton>();
		if (!s_List)
			return r;
		foreach (DZS_HomeButton b : s_List)
			r.Insert(b);
		return r;
	}
}

// Text for a checksum: the plugin that holds whole files outside the state
// (loot) names the files the state refers to, gives the text of a checksum, takes a text fetched from elsewhere (only if
// its checksum fits) and keeps its changing controls off with a reason while a text is missing.
class DZS_FileTexts : Managed
{
	protected static ref DZS_FileTexts s_Holder;

	static void Set(DZS_FileTexts h) { s_Holder = h; }
	static DZS_FileTexts Get() { return s_Holder; }

	void Named(DZS_Instance inst, array<string> names, array<string> sums) {}
	array<string> TextOf(string sum) { return null; }
	bool Take(DZS_Instance inst, string sum, array<string> chunks) { return false; }
	// "" = nothing missing; else changes that need the text are off with this reason
	void SetWaiting(string de, string en) {}
	// The checksum over a text, as the state names it.
	string SumOf(array<string> chunks) { return DZS_Sha256.Of(chunks); }
}

// Participants of the running session for other plugins: the sync plugin
// publishes server, own participant and members; m_Rev changes whenever any of it does.
class DZS_SessionMember : Managed
{
	string m_Pid;
	string m_UserId;
	string m_Name;
	int m_Color;
}

class DZS_SessionPeople
{
	protected static string s_Server;
	protected static string s_Me;
	protected static string s_Sig;
	protected static int s_Rev;
	protected static ref array<ref DZS_SessionMember> s_List;

	static void Publish(string server, string me, array<ref DZS_SessionMember> list)
	{
		string sig = server + "|" + me;
		if (list)
		{
			foreach (DZS_SessionMember m : list)
				sig += "|" + m.m_Pid + "," + m.m_UserId + "," + m.m_Name + "," + m.m_Color;
		}
		if (sig == s_Sig)
			return;
		s_Sig = sig;
		s_Server = server;
		s_Me = me;
		s_List = new array<ref DZS_SessionMember>();
		if (list)
		{
			foreach (DZS_SessionMember m2 : list)
				s_List.Insert(m2);
		}
		s_Rev++;
	}

	static void Clear() { Publish("", "", null); }
	static string Server() { return s_Server; }
	static string Me() { return s_Me; }
	static int Rev() { return s_Rev; }

	static array<DZS_SessionMember> Members()
	{
		array<DZS_SessionMember> r = new array<DZS_SessionMember>();
		if (!s_List)
			return r;
		foreach (DZS_SessionMember m : s_List)
			r.Insert(m);
		return r;
	}
}

// Knobs of plugins in the foot of the sync plugin's section, in a row of their own above its own knobs; shown while
// the action is listed (DZS_ActionHandler.IsActionListed), a click triggers it.
class DZS_SessionKnobs
{
	protected static ref array<ref DZS_HomeButton> s_List;

	static void Add(string actionId, string de, string en)
	{
		if (!s_List)
			s_List = new array<ref DZS_HomeButton>();
		foreach (DZS_HomeButton b : s_List)
		{
			if (b.m_Action == actionId)
				return;
		}
		DZS_HomeButton nb = new DZS_HomeButton();
		nb.m_Action = actionId;
		nb.m_De = de;
		nb.m_En = en;
		s_List.Insert(nb);
	}

	static array<DZS_HomeButton> All()
	{
		array<DZS_HomeButton> r = new array<DZS_HomeButton>();
		if (!s_List)
			return r;
		foreach (DZS_HomeButton b : s_List)
			r.Insert(b);
		return r;
	}
}

// A sign-in plugin for the sign-in method "provider": gives the sync plugin a token for a base
// address; signing in and out belong to the plugin.
class DZS_AuthProvider : Managed
{
	string Token(string baseUrl) { return ""; }
	string UserName() { return ""; }
	void SignIn() {}
	void SignOut() {}
}

// Server entries of plugins: fixed in the sync plugin's list, not removable.
class DZS_SyncServer : Managed
{
	string m_Url;
	string m_Provider;
}

class DZS_SyncServers
{
	protected static ref array<ref DZS_SyncServer> s_Fixed;
	// not owned: the plugin keeps its provider (mission-module objects must not outlive their module here)
	protected static ref map<string, DZS_AuthProvider> s_Auth;
	protected static int s_Rev;

	static void AddFixed(string url, string provider)
	{
		if (!s_Fixed)
			s_Fixed = new array<ref DZS_SyncServer>();
		foreach (DZS_SyncServer f : s_Fixed)
		{
			if (f.m_Url == url)
			{
				f.m_Provider = provider;
				return;
			}
		}
		DZS_SyncServer n = new DZS_SyncServer();
		n.m_Url = url;
		n.m_Provider = provider;
		s_Fixed.Insert(n);
		s_Rev++;
	}

	static void RegisterAuth(string provider, DZS_AuthProvider p)
	{
		if (!s_Auth)
			s_Auth = new map<string, DZS_AuthProvider>();
		s_Auth.Set(provider, p);
	}

	static DZS_AuthProvider Auth(string provider)
	{
		if (!s_Auth || provider == "")
			return null;
		return s_Auth.Get(provider);
	}

	static int Rev() { return s_Rev; }

	static array<DZS_SyncServer> Fixed()
	{
		array<DZS_SyncServer> r = new array<DZS_SyncServer>();
		if (!s_Fixed)
			return r;
		foreach (DZS_SyncServer f : s_Fixed)
			r.Insert(f);
		return r;
	}
}

// Targets of "Save as" besides "Local": the sync plugin names its
// servers and takes the save to one of them.
class DZS_SaveTarget : Managed
{
	void Targets(array<string> ids, array<string> names) {}
	// "" = ready, "loading" = still asking the server, "unreachable" = the list failed, "none" = the server lets this
	// user create nothing
	string TargetState(string id) { return ""; }
	void SaveAs(string id, string name) {}
}

class DZS_SaveTargets
{
	// not owned (see DZS_SyncServers)
	protected static DZS_SaveTarget s_T;

	static void Set(DZS_SaveTarget t) { s_T = t; }
	static DZS_SaveTarget Get() { return s_T; }
}
