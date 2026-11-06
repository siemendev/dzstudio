// What the plugin knows of a server, a session and its participants.
class DZS_CollabServer : Managed
{
	string m_Url;
	string m_Name;
	string m_Token;
	string m_User;
	// "", "ok", "unreachable", "old", "newer"
	string m_State;
	bool m_AuthCode;
	bool m_AuthName;
	bool m_Users;
	bool m_InviteUser;
	bool m_Hosted;
	int m_MaxRequest = 4194304;
	string m_Notice;
	string m_MinClient;
	string m_RecClient;
	string m_UpdateUrl;
	bool m_Asked;
	// entry of a plugin (DZS_SyncServers): not removable; sign-in by that plugin when the server offers it
	string m_Fixed;
	bool m_AuthProvider;
}

class DZS_CollabPeer : Managed
{
	string m_Pid;
	string m_UserId;
	string m_Name;
	int m_Colour;
	string m_Role;
	bool m_CanInvite;
	bool m_CanSave;
	bool m_CanManage;
	string m_State = "active";
	int m_Idle;
	vector m_Pos;
	bool m_HasPos;
	int m_LastSeen;
	int m_Held;
	// "studio" or "agent" (servers with the capability agents); phase and status only for agents
	string m_Kind;
	string m_AgentName;
	string m_Phase;
	string m_Status;

	bool IsAgent() { return m_Kind == "agent"; }

	// How dzStudio names the participant: an agent as "‹agentName› · Agent of ‹user›".
	string Label()
	{
		if (!IsAgent())
			return m_Name;
		string of = DZS_Texts.Pick("Agent von " + m_Name, "Agent of " + m_Name);
		if (m_AgentName == "")
			return of;
		return m_AgentName + " · " + of;
	}

	string PhaseText()
	{
		string p = DZS_Texts.Pick("wartet auf Arbeit", "waiting for work");
		if (m_Phase == "working")
		{
			if (m_Status != "")
				return m_Status;
			return DZS_Texts.Pick("arbeitet", "working");
		}
		if (m_Phase == "review")
			p = DZS_Texts.Pick("wartet auf Prüfung", "waiting for review");
		if (m_Status != "")
			p += " — " + m_Status;
		return p;
	}
}

// One operation of the own instance on its way to the server.
class DZS_CollabOut : Managed
{
	string m_OpId;
	bool m_System;
	ref array<string> m_Pieces;
	int m_ClientSeq;
	string m_LiveId;
}

// A remote operation as poll delivered it.
class DZS_CollabIn : Managed
{
	int m_Seq;
	string m_Pid;
	string m_OpId;
	bool m_System;
	string m_LiveId;
	ref array<string> m_Pieces;
}

class DZS_CollabSession : Managed
{
	string m_Server;
	string m_ServerName;
	string m_Id;
	string m_Pid;
	string m_Kind = "local";
	string m_Role;
	int m_Colour;
	bool m_CanInvite;
	bool m_CanSave;
	bool m_CanManage;
	string m_Project;
	string m_Map;
	string m_HostName;
	// a project held by the server: its mirror folder (saving goes through the provider)
	string m_Folder;
	// snapshot upload of the host
	bool m_Ready;
	ref array<string> m_Snap;
	int m_SnapPart;
	int m_SnapParts;
	ref array<ref array<string>> m_SnapPartsList;
	// log
	int m_After;
	int m_Applied;
	int m_ChainStart;
	ref array<string> m_ChainBuf;
	ref array<string> m_PartBuf;
	ref array<ref DZS_CollabUnit> m_Units = new array<ref DZS_CollabUnit>();
	ref array<ref DZS_CollabUnit> m_InFlightUnits = new array<ref DZS_CollabUnit>();
	string m_Key;
	int m_CounterStart;
	bool m_Resuming;
	bool m_BannerClosed;
	bool m_TooLong;
	int m_Seq;
	int m_NextClientSeq = 1;
	ref array<ref DZS_CollabOut> m_Outbox = new array<ref DZS_CollabOut>();
	ref array<ref DZS_CollabOut> m_InFlight = new array<ref DZS_CollabOut>();
	ref map<string, bool> m_SentUnseen = new map<string, bool>();
	ref array<ref DZS_CollabIn> m_Held = new array<ref DZS_CollabIn>();
	// versions known
	int m_VParticipants;
	int m_VLocks;
	int m_VLive;
	int m_VNotices;
	int m_VSaving;
	ref array<ref DZS_CollabPeer> m_Peers = new array<ref DZS_CollabPeer>();
	bool m_PeersKnown;
	// locks: element -> holder participant id (others only) and lapsed holders
	ref map<string, string> m_LockOf = new map<string, string>();
	ref map<string, bool> m_Lapsed = new map<string, bool>();
	ref map<string, bool> m_Mine = new map<string, bool>();
	int m_LockSeq;
	ref array<string> m_Want = new array<string>();
	bool m_LockDirty;
	float m_LockDirtyAt;
	bool m_LockSent;
	// saving as the guests see it
	string m_SaveState = "none";
	int m_SaveUnsaved;
	int m_SavedAgo = -1;
	float m_SavedAgoAt;
	string m_SavedClock;
	int m_SavedSeq = -1;
	string m_SavedByUser;
	string m_SavedByName;
	// savedSeq when the own save failed: the failure shows until the server writes the next save
	int m_FailSeq = -2;
	// connection
	float m_DownSince = -1;
	bool m_Lost;
	bool m_Ended;
	string m_EndReason;
	string m_EndBy;
	// last saving state reported by the host (to report changes only)
	string m_ReportState;
	int m_ReportUnsaved = -1;

	DZS_CollabPeer Peer(string pid)
	{
		foreach (DZS_CollabPeer p : m_Peers)
		{
			if (p.m_Pid == pid)
				return p;
		}
		return null;
	}

	DZS_CollabPeer Me() { return Peer(m_Pid); }

	DZS_CollabPeer Host()
	{
		foreach (DZS_CollabPeer p : m_Peers)
		{
			if (p.m_Role == "host")
				return p;
		}
		return null;
	}

	bool IsHost() { return m_Role == "host"; }
	bool IsGuest() { return m_Kind == "local" && m_Role == "guest"; }
	bool IsHosted() { return m_Kind == "hosted"; }

	int Others()
	{
		int n = 0;
		foreach (DZS_CollabPeer p : m_Peers)
		{
			if (p.m_Pid != m_Pid)
				n++;
		}
		return n;
	}
}

class DZS_CollabColors
{
	// The colour row of the groups (blue, orange, turquoise, pink, yellow-green, yellow, slate, fuchsia).
	static int Get(int colour)
	{
		if (colour == 1) return ARGB(255, 0x60, 0xA5, 0xFA);
		if (colour == 2) return ARGB(255, 0xFB, 0x92, 0x3C);
		if (colour == 3) return ARGB(255, 0x2D, 0xD4, 0xBF);
		if (colour == 4) return ARGB(255, 0xF4, 0x72, 0xB6);
		if (colour == 5) return ARGB(255, 0xA3, 0xE6, 0x35);
		if (colour == 6) return ARGB(255, 0xFD, 0xE0, 0x47);
		if (colour == 7) return ARGB(255, 0x94, 0xA3, 0xB8);
		if (colour == 8) return ARGB(255, 0xE8, 0x79, 0xF9);
		return ARGB(255, 0x94, 0xA3, 0xB8);
	}
}

// A session as sessions/list names it.
class DZS_CollabListed : Managed
{
	string m_Id;
	string m_Kind;
	string m_Project;
	string m_Map;
	string m_Host;
	int m_Count;
	string m_RefProvider;
	string m_RefKey;
}
