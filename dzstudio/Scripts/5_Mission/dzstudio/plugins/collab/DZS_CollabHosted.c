// Projects held by a server without a provider plugin:
// a tab per server in the project browser, loading
// into a mirror folder, and saving through the session. Such a project is always open in a session.
class DZS_CollabHostedProj : Managed
{
	string m_Folder;
	string m_Server;
	string m_RefProvider;
	string m_RefKey;
	string m_Name;
	string m_Map;
	string m_Rev;
	bool m_CanSave;
	string m_Session;
	// the store's name of the manifest ("manifest", or what the server listed for it)
	string m_Manifest = "manifest";
	ref map<string, string> m_FileRev = new map<string, string>();

	string RefJson()
	{
		return "{\"provider\":" + DZS_CollabJ.Q(m_RefProvider) + ",\"key\":" + DZS_CollabJ.Q(m_RefKey) + "}";
	}
}

class DZS_CollabHostedProvider : DZS_StoreProvider
{
	DZS_CollabHosted m_H;
	string m_Url;

	override string Id() { return "collab:" + m_Url; }
	override bool Owns(string folder) { return folder != "" && folder.IndexOf(DZS_CollabHosted.PrefixOf(m_Url)) == 0; }
	override string Label(string folder) { return m_H.ServerName(m_Url); }
	override void Fetch(string folder) { m_H.Fetch(folder); }
	override void Save(string folder, array<ref DZS_SaveFile> files) { m_H.Save(folder, files); }
	override bool SaveRunning(string folder) { return m_H.SaveRunning(folder); }
	override void Opened(string folder) { m_H.Opened(folder); }
	override string TabLabel() { return m_H.TabOf(m_Url); }
	override void RefreshRows() { m_H.Refresh(m_Url); }
	override bool RowsLoading() { return m_H.m_Loading.Contains(m_Url); }
	override string RowsEmptyText() { return m_H.EmptyText(m_Url); }
	override void Rows(array<ref DZS_ProjectInfo> into) { m_H.Rows(m_Url, into); }
	override string RowMeta(DZS_ProjectInfo p) { return m_H.RowMeta(p); }
	override string RowMark(DZS_ProjectInfo p) { return m_H.RowMark(p); }
	override bool RowOff(DZS_ProjectInfo p, out string de, out string en) { return m_H.RowOff(p, de, en); }
	override string Address() { return m_Url; }
	override void NoRename(out string de, out string en)
	{
		de = "Umbenennen und Löschen gibt es bei Projekten eines Servers nicht";
		en = "Projects of a server cannot be renamed or deleted here";
	}
}

class DZS_CollabHostedLink : DZS_CollabListener
{
	DZS_CollabHosted m_H;

	override void OnCollab(DZS_CollabCall call, DZS_CollabReply reply) { m_H.OnReply(call, reply); }
}

// A save on its way: checksums in slices per frame, then one project/save in the session's send channel.
class DZS_CollabHostedSave : Managed
{
	string m_Folder;
	ref array<string> m_Names = new array<string>();
	ref array<ref array<string>> m_Pieces = new array<ref array<string>>();
	ref array<string> m_Sums = new array<string>();
	ref array<string> m_Deleted = new array<string>();
	ref DZS_Sha256 m_Hash;
	int m_At;
	float m_Started;
	float m_SentAt = -1;
	string m_ReqId;
	int m_Seq;
	bool m_Force;
	string m_Body;
}

class DZS_CollabHosted : Managed
{
	static const string PREFIX = "@srv-";
	static const float HASH_MS = 4;

	DZS_CollabPlugin m_P;
	ref DZS_CollabHostedLink m_Link;
	// a save shown as failed after 30 s that still repeats in the send channel until 10 min after its first sending
	ref DZS_CollabHostedSave m_Late;
	ref map<string, ref DZS_CollabNet> m_Nets = new map<string, ref DZS_CollabNet>();
	ref map<string, ref DZS_CollabHostedProj> m_Projects = new map<string, ref DZS_CollabHostedProj>();
	ref map<string, ref array<string>> m_Order = new map<string, ref array<string>>();
	ref map<string, bool> m_Loading = new map<string, bool>();
	ref map<string, string> m_Errors = new map<string, string>();
	ref map<string, string> m_Creatable = new map<string, string>();
	ref map<string, ref DZS_CollabHostedProvider> m_Providers = new map<string, ref DZS_CollabHostedProvider>();
	// opening
	string m_FetchFolder;
	bool m_FetchRestarted;
	ref array<ref DZS_CollabHostedFile> m_Files;
	int m_FileAt;
	ref array<string> m_FileChunks;
	string m_StartFolder;
	// saving
	ref DZS_CollabHostedSave m_Save;
	float m_Now;

	void DZS_CollabHosted(DZS_CollabPlugin p)
	{
		m_P = p;
		m_Link = new DZS_CollabHostedLink();
		m_Link.m_H = this;
	}

	static string PrefixOf(string url)
	{
		int h = 7;
		for (int i = 0; i < url.Length(); i++)
			h = (h * 31 + url.Get(i).ToAscii()) & 0xFFFFFF;
		return PREFIX + h.ToString() + "-";
	}

	static string FolderOf(string url, string provider, string key)
	{
		string raw = provider + "-" + key;
		string clean = "";
		for (int i = 0; i < raw.Length() && clean.Length() < 60; i++)
		{
			string c = raw.Get(i);
			int a = c.ToAscii();
			bool ok = (a >= 48 && a <= 57) || (a >= 65 && a <= 90) || (a >= 97 && a <= 122) || c == "-" || c == "_";
			if (ok)
				clean += c;
			else
				clean += "_";
		}
		return PrefixOf(url) + clean;
	}

	void Register(string url)
	{
		if (m_Providers.Contains(url))
			return;
		DZS_CollabHostedProvider p = new DZS_CollabHostedProvider();
		p.m_H = this;
		p.m_Url = url;
		m_Providers.Set(url, p);
		array<ref DZS_StoreProvider> all = DZS_StoreProvider.All();
		for (int i = all.Count() - 1; i >= 0; i--)
		{
			DZS_CollabHostedProvider old = DZS_CollabHostedProvider.Cast(all[i]);
			if (old && old.m_Url == url)
				all.RemoveOrdered(i);
		}
		DZS_StoreProvider.Register(p);
	}

	DZS_CollabHostedProvider ProviderFor(string url) { return m_Providers.Get(url); }

	// The provider name of a server's store as its project/list names it ("" = none listed yet).
	// New server project: the first entry of project/list's stores with canCreate; "" = none or
	// not listed yet.
	string CreatableStore(string url)
	{
		return m_Creatable.Get(url);
	}

	bool StoresKnown(string url)
	{
		return m_Creatable.Contains(url);
	}

	void Remember(DZS_CollabHostedProj p)
	{
		m_Projects.Set(p.m_Folder, p);
		WriteRef(p);
	}

	protected DZS_CollabNet NetFor(string url)
	{
		DZS_CollabNet n = m_Nets.Get(url);
		if (n)
			return n;
		n = new DZS_CollabNet(url, m_Link);
		m_Nets.Set(url, n);
		return n;
	}

	string ServerName(string url)
	{
		DZS_CollabServer s = m_P.FindServer(url);
		if (s)
			return s.m_Name;
		return url;
	}

	protected string AuthHead(string url)
	{
		DZS_CollabServer s = m_P.FindServer(url);
		string tok = "";
		if (s)
			tok = s.m_Token;
		return "\"protocol\":\"1.0\",\"token\":" + DZS_CollabJ.Q(tok);
	}

	DZS_ProjectsPlugin Projects() { return DZS_ProjectsPlugin.Cast(m_P.Kernel().GetPlugin("projects")); }

	DZS_CollabHostedProj ProjectOf(string folder)
	{
		DZS_CollabHostedProj p = m_Projects.Get(folder);
		if (p)
			return p;
		array<string> chunks = DZS_Files.Read(DZS_LocalStore.Path(folder, "hosted.ref"));
		if (!chunks)
			return null;
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
			return null;
		DZS_SrcNode r = doc.m_Root;
		p = new DZS_CollabHostedProj();
		p.m_Folder = folder;
		p.m_Server = DZS_CollabJ.S(r, "server");
		p.m_RefProvider = DZS_CollabJ.S(r, "provider");
		p.m_RefKey = DZS_CollabJ.S(r, "key");
		p.m_Name = DZS_CollabJ.S(r, "name");
		p.m_Map = DZS_CollabJ.S(r, "map");
		p.m_Manifest = DZS_CollabJ.S(r, "manifest");
		if (p.m_Manifest == "")
			p.m_Manifest = "manifest";
		if (p.m_Server == "" || p.m_RefKey == "")
			return null;
		m_Projects.Set(folder, p);
		return p;
	}

	protected void WriteRef(DZS_CollabHostedProj p)
	{
		string t = "{\"server\":" + DZS_CollabJ.Q(p.m_Server) + ",\"provider\":" + DZS_CollabJ.Q(p.m_RefProvider);
		t += ",\"key\":" + DZS_CollabJ.Q(p.m_RefKey) + ",\"name\":" + DZS_CollabJ.Q(p.m_Name) + ",\"map\":" + DZS_CollabJ.Q(p.m_Map);
		t += ",\"manifest\":" + DZS_CollabJ.Q(p.m_Manifest) + "}\n";
		array<string> one = new array<string>();
		one.Insert(t);
		DZS_Files.MakeDirs(DZS_LocalStore.Dir(p.m_Folder));
		DZS_Files.Write(DZS_LocalStore.Path(p.m_Folder, "hosted.ref"), one);
	}

	// ---- the tab ----

	string TabOf(string url)
	{
		DZS_CollabServer s = m_P.FindServer(url);
		if (!s || !s.m_Hosted)
			return "";
		DZS_AuthProvider ap = DZS_SyncServers.Auth(s.m_Fixed);
		if (ap && ap.TabLabel() != "")
			return ap.TabLabel();
		return s.m_Name;
	}

	string EmptyText(string url)
	{
		return m_Errors.Get(url);
	}

	void Refresh(string url)
	{
		DZS_CollabServer s = m_P.FindServer(url);
		m_Errors.Set(url, "");
		if (!s)
			return;
		if (DZS_Version.IsBlocked(DZS_CollabPlugin.VersionKey(url)))
		{
			m_Errors.Set(url, DZS_Texts.Pick("dzStudio ist veraltet", "dzStudio is out of date"));
			return;
		}
		if (s.m_Token == "")
		{
			m_Errors.Set(url, DZS_Texts.Pick("Nicht angemeldet — in den Einstellungen anmelden", "Not signed in — sign in in the settings"));
			array<string> none = new array<string>();
			m_Order.Set(url, none);
			return;
		}
		m_Loading.Set(url, true);
		array<string> fresh = new array<string>();
		m_Order.Set(url, fresh);
		AskPage(url, "");
	}

	protected void AskPage(string url, string mark)
	{
		string fail;
		if (GetCLIParam("dzsTestListFail", fail) && FileExist("$profile:dzstudio\\test-listfail"))
		{
			m_Loading.Remove(url);
			m_Errors.Set(url, DZS_Texts.Pick(ServerName(url) + " nicht erreichbar", ServerName(url) + " not reachable"));
			DZS_Log.Info("collab: project/list at " + url + " failed (test switch)");
			return;
		}
		string body = "{" + AuthHead(url);
		if (mark != "")
			body += ",\"after\":" + DZS_CollabJ.Q(mark);
		body += "}";
		NetFor(url).Send("hosted/list", "project/list", body);
	}

	protected void OnList(DZS_CollabCall c, DZS_CollabReply r)
	{
		string url = c.m_Net.m_Base;
		if (!r.Ok())
		{
			m_Loading.Remove(url);
			string code = r.Code();
			if (code == "auth_required" || code == "token_invalid" || code == "token_expired")
				m_Errors.Set(url, DZS_Texts.Pick("Nicht angemeldet — in den Einstellungen anmelden", "Not signed in — sign in in the settings"));
			else if (code == "bad_request")
			{
				m_Loading.Set(url, true);
				array<string> again = new array<string>();
				m_Order.Set(url, again);
				AskPage(url, "");
			}
			else
				m_Errors.Set(url, DZS_Texts.Pick(ServerName(url) + " nicht erreichbar", ServerName(url) + " not reachable"));
			return;
		}
		array<string> order = m_Order.Get(url);
		DZS_SrcNode stores = r.N("stores");
		if (stores && stores.IsArr())
		{
			string creatable = "";
			foreach (DZS_SrcNode st : stores.m_Items)
			{
				string sp = DZS_CollabJ.S(st, "provider");
				if (creatable == "" && sp != "" && DZS_CollabJ.B(st, "canCreate"))
					creatable = sp;
			}
			m_Creatable.Set(url, creatable);
			DZS_Log.Info("collab: stores at " + url + ", creatable: " + creatable);
		}
		DZS_SrcNode list = r.N("projects");
		if (list && list.IsArr())
		{
			foreach (DZS_SrcNode e : list.m_Items)
			{
				DZS_SrcNode href = e.Get("hostedRef");
				string prov = DZS_CollabJ.S(href, "provider");
				string key = DZS_CollabJ.S(href, "key");
				string folder = FolderOf(url, prov, key);
				if (order.Find(folder) >= 0)
					continue;
				DZS_CollabHostedProj p = m_Projects.Get(folder);
				if (!p)
				{
					p = new DZS_CollabHostedProj();
					m_Projects.Set(folder, p);
				}
				p.m_Folder = folder;
				p.m_Server = url;
				p.m_RefProvider = prov;
				p.m_RefKey = key;
				p.m_Name = DZS_CollabJ.S(e, "projectName");
				p.m_Map = DZS_CollabJ.S(e, "map");
				p.m_Rev = DZS_CollabJ.S(e, "hostedRevision");
				p.m_CanSave = DZS_CollabJ.B(e, "canSave");
				p.m_Session = DZS_CollabJ.S(e, "sessionId");
				order.Insert(folder);
			}
		}
		string next = r.S("next");
		if (next != "")
		{
			AskPage(url, next);
			return;
		}
		m_Loading.Remove(url);
		DZS_Log.Info("collab: " + order.Count() + " project(s) at " + url);
	}

	void Rows(string url, array<ref DZS_ProjectInfo> into)
	{
		array<string> order = m_Order.Get(url);
		if (!order)
			return;
		foreach (string f : order)
		{
			DZS_CollabHostedProj p = m_Projects.Get(f);
			if (!p)
				continue;
			DZS_ProjectInfo i = new DZS_ProjectInfo();
			i.m_Folder = f;
			i.m_Name = p.m_Name;
			i.m_MapKey = p.m_Map;
			i.m_Status = "ok";
			into.Insert(i);
		}
	}

	string RowMeta(DZS_ProjectInfo i)
	{
		string mapName = DZS_Texts.Pick("unbekannt", "unknown");
		if (i.m_MapKey != "")
			mapName = DZS_Maps.Display(i.m_MapKey);
		string saved = "—";
		DZS_ProjectsPlugin pp = Projects();
		if (pp)
		{
			string st = DZS_ProjectsPlugin.ShowStamp(pp.RecentStamp(i.m_Folder, "saved"));
			if (st != "")
				saved = st;
		}
		return mapName + " · —" + DZS_Texts.Pick(" Gruppen · gespeichert ", " groups · saved ") + saved;
	}

	string RowMark(DZS_ProjectInfo i)
	{
		DZS_CollabHostedProj p = m_Projects.Get(i.m_Folder);
		if (p && p.m_Session != "")
			return DZS_Texts.Pick("Sitzung", "Session");
		return "";
	}

	bool RowOff(DZS_ProjectInfo i, out string de, out string en)
	{
		DZS_CollabHostedProj p = m_Projects.Get(i.m_Folder);
		if (!p || p.m_Session != "" || p.m_CanSave)
			return false;
		de = "Nur wer speichern darf, öffnet es ohne laufende Sitzung";
		en = "Only users who may save can open it without a running session";
		return true;
	}

	// ---- opening ----

	void Fetch(string folder)
	{
		DZS_CollabHostedProj p = ProjectOf(folder);
		DZS_ProjectsPlugin pp = Projects();
		if (!p)
		{
			pp.Fetched(folder, false, "Projekt nicht gefunden", "Project not found");
			return;
		}
		m_FetchFolder = folder;
		m_FetchRestarted = false;
		AskHead(p);
	}

	protected void AskHead(DZS_CollabHostedProj p)
	{
		string body = "{" + AuthHead(p.m_Server) + ",\"hostedRef\":" + p.RefJson() + "}";
		NetFor(p.m_Server).Send("hosted/head", "project/load", body);
	}

	protected void FetchFailed(string de, string en)
	{
		string folder = m_FetchFolder;
		m_FetchFolder = "";
		m_Files = null;
		m_FileChunks = null;
		DZS_Log.Warn("collab: opening " + folder + " failed: " + de);
		Projects().Fetched(folder, false, de, en);
	}

	protected void OnHead(DZS_CollabReply r)
	{
		DZS_CollabHostedProj p = ProjectOf(m_FetchFolder);
		if (!p)
			return;
		if (!r.Ok())
		{
			string code = r.Code();
			if (!r.m_Framed)
				FetchFailed("Server nicht erreichbar", "Server not reachable");
			else if (code == "project_unknown" || code == "not_allowed")
				FetchFailed("Projekt nicht gefunden", "Project not found");
			else
				FetchFailed("Öffnen nicht möglich: " + r.ErrS("message"), "Cannot open: " + r.ErrS("message"));
			return;
		}
		p.m_Name = r.S("projectName");
		p.m_Map = r.S("map");
		p.m_CanSave = r.B("canSave");
		p.m_Session = r.S("sessionId");
		p.m_Rev = r.S("hostedRevision");
		if (p.m_Session != "")
		{
			string folder = m_FetchFolder;
			m_FetchFolder = "";
			WriteRef(p);
			DZS_Log.Info("collab: " + p.m_Name + " has session " + p.m_Session + " — joining instead of loading");
			m_P.JoinHosted(p, p.m_Session);
			return;
		}
		if (!p.m_CanSave)
		{
			FetchFailed("Nur wer speichern darf, öffnet es ohne laufende Sitzung", "Only users who may save can open it without a running session");
			return;
		}
		if (!DZS_Maps.Available(p.m_Map))
		{
			string mn = DZS_Maps.Display(p.m_Map);
			FetchFailed(mn + " ist nicht verfügbar", mn + " is not available");
			return;
		}
		m_Files = new array<ref DZS_CollabHostedFile>();
		p.m_FileRev.Clear();
		p.m_Manifest = "manifest";
		DZS_SrcNode files = r.N("files");
		if (files && files.IsArr())
		{
			foreach (DZS_SrcNode f : files.m_Items)
			{
				DZS_CollabHostedFile hf = new DZS_CollabHostedFile();
				hf.m_Name = DZS_CollabJ.S(f, "name");
				hf.m_Parts = DZS_CollabJ.I(f, "parts");
				if (hf.m_Parts < 1)
					hf.m_Parts = 1;
				hf.m_Rel = hf.m_Name;
				if (hf.m_Name == "manifest" || hf.m_Name == DZS_ProjectFile.NAME || hf.m_Name == DZS_LegacyNames.PROJECT_FILE)
				{
					hf.m_Rel = DZS_ProjectFile.NAME;
					p.m_Manifest = hf.m_Name;
				}
				else if (DZS_LocalStore.InvalidRel(hf.m_Rel))
				{
					DZS_Log.Warn("collab: file name " + hf.m_Name + " cannot be a local path — left out");
					continue;
				}
				string rv = DZS_CollabJ.S(f, "revision");
				p.m_FileRev.Set(hf.m_Name, rv);
				m_Files.Insert(hf);
			}
		}
		DZS_Log.Info("collab: loading " + p.m_Name + " revision " + p.m_Rev + ", " + m_Files.Count() + " file(s)");
		m_FileAt = 0;
		m_FileChunks = new array<string>();
		DZS_Files.MakeDirs(DZS_LocalStore.Dir(p.m_Folder));
		NextFilePart(p, 0);
	}

	protected void NextFilePart(DZS_CollabHostedProj p, int part)
	{
		if (m_FileAt >= m_Files.Count())
		{
			FetchDone(p);
			return;
		}
		DZS_CollabHostedFile hf = m_Files[m_FileAt];
		string body = "{" + AuthHead(p.m_Server) + ",\"hostedRef\":" + p.RefJson() + ",\"hostedRevision\":" + DZS_CollabJ.Q(p.m_Rev);
		body += ",\"file\":" + DZS_CollabJ.Q(hf.m_Name) + ",\"part\":" + part + "}";
		NetFor(p.m_Server).Send("hosted/file", "project/load", body);
	}

	protected void OnFile(DZS_CollabReply r)
	{
		DZS_CollabHostedProj p = ProjectOf(m_FetchFolder);
		if (!p || !m_Files)
			return;
		if (!r.Ok())
		{
			if (r.Code() == "revision_unknown" && !m_FetchRestarted)
			{
				m_FetchRestarted = true;
				AskHead(p);
				return;
			}
			if (!r.m_Framed)
				FetchFailed("Server nicht erreichbar", "Server not reachable");
			else
				FetchFailed("Öffnen nicht möglich: " + r.Code(), "Cannot open: " + r.Code());
			return;
		}
		array<string> pieces = DZS_CollabJ.Strings(r.N("pieces"));
		foreach (string pc : pieces)
			m_FileChunks.Insert(pc);
		int part = r.I("part");
		int parts = r.I("parts");
		if (part + 1 < parts)
		{
			NextFilePart(p, part + 1);
			return;
		}
		DZS_CollabHostedFile hf = m_Files[m_FileAt];
		string path = DZS_LocalStore.Path(p.m_Folder, hf.m_Rel);
		if (hf.m_Rel == DZS_ProjectFile.NAME)
		{
			// name and map come from the server, the camera stays; an older store keeps only format, version and project
			string view = DZS_ProjectFile.ViewRawOf(DZS_Files.Read(path));
			array<string> own = DZS_ProjectFile.RewriteV2(m_FileChunks, p.m_Name, p.m_Map, view, "", true);
			string ppart = RawOf(m_FileChunks, "project");
			if (own)
				m_FileChunks = own;
			else if (ppart != "")
				m_FileChunks = DZS_ProjectFile.Compose(p.m_Name, p.m_Map, ppart, view);
		}
		int slash = hf.m_Rel.LastIndexOf("/");
		if (slash > 0)
			DZS_Files.MakeDirs(DZS_LocalStore.Path(p.m_Folder, hf.m_Rel.Substring(0, slash)));
		DZS_Files.Write(path, m_FileChunks);
		hf.m_Bytes = DZS_Files.Length(m_FileChunks);
		m_FileChunks = new array<string>();
		m_FileAt++;
		NextFilePart(p, 0);
	}

	protected void FetchDone(DZS_CollabHostedProj p)
	{
		array<string> keep = new array<string>();
		foreach (DZS_CollabHostedFile hf : m_Files)
			keep.Insert(hf.m_Rel);
		array<string> names;
		array<bool> dirs;
		DZS_Files.List(DZS_LocalStore.Dir(p.m_Folder), names, dirs);
		for (int i = 0; i < names.Count(); i++)
		{
			string n = names[i];
			string low = n;
			low.ToLower();
			if (dirs[i] || low.IndexOf(".json") < 0 || keep.Find(n) >= 0)
				continue;
			DeleteFile(DZS_LocalStore.Path(p.m_Folder, n));
		}
		WriteRef(p);
		string folder = m_FetchFolder;
		m_FetchFolder = "";
		DZS_Log.Info("collab: loaded " + m_Files.Count() + " file(s) of " + p.m_Name + " into " + folder);
		m_Files = null;
		m_FileChunks = null;
		Projects().Fetched(folder, true, "", "");
	}

	// The loaded project is open (or a joined one opened from the session's state): name it as the server does and, when
	// no session is running yet, start one before anything else happens.
	void Opened(string folder)
	{
		DZS_CollabHostedProj p = ProjectOf(folder);
		DZS_ProjectsPlugin pp = Projects();
		if (p && pp && pp.m_Open)
			pp.m_Open.m_Name = p.m_Name;
		if (m_P.m_S)
			return;
		m_StartFolder = folder;
	}

	// ---- saving ----

	void Save(string folder, array<ref DZS_SaveFile> files)
	{
		DZS_ProjectsPlugin pp = Projects();
		DZS_CollabHostedProj p = ProjectOf(folder);
		DZS_CollabSession s = m_P.m_S;
		if (!p || !s || !s.IsHosted() || s.m_Folder != folder)
		{
			pp.ProviderSaved("Nicht in der Sitzung dieses Projekts", "Not in the session of this project");
			return;
		}
		DZS_CollabHostedSave sv = new DZS_CollabHostedSave();
		sv.m_Folder = folder;
		sv.m_Started = m_Now;
		sv.m_ReqId = DZS_CollabPlugin.ReqId();
		foreach (DZS_SaveFile f : files)
		{
			if (f.m_Delete)
			{
				if (p.m_FileRev.Contains(f.m_Rel))
					sv.m_Deleted.Insert(f.m_Rel);
				continue;
			}
			if (f.m_Rel == DZS_ProjectFile.NAME)
			{
				sv.m_Names.Insert(p.m_Manifest);
				sv.m_Pieces.Insert(ManifestOf(f.m_Chunks));
				continue;
			}
			sv.m_Names.Insert(f.m_Rel);
			sv.m_Pieces.Insert(f.m_Chunks);
		}
		// the project file holds the whole project: files an older save stored beside it go
		for (int k = 0; k < p.m_FileRev.Count(); k++)
		{
			string stored = p.m_FileRev.GetKey(k);
			if (stored == p.m_Manifest || sv.m_Names.Find(stored) >= 0 || sv.m_Deleted.Find(stored) >= 0)
				continue;
			sv.m_Deleted.Insert(stored);
		}
		m_Save = sv;
		DZS_Log.Info("collab: save of " + p.m_Name + ", " + sv.m_Names.Count() + " file(s), " + sv.m_Deleted.Count() + " deleted; waiting for own operations");
	}

	static string RawOf(array<string> chunks, string key)
	{
		if (!chunks)
			return "";
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
			return "";
		DZS_SrcNode n = doc.m_Root.Get(key);
		if (!n || !n.IsObj())
			return "";
		return doc.Raw(n);
	}

	// The manifest in the store: the project file (an older one with only format, version and project).
	protected array<string> ManifestOf(array<string> chunks)
	{
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
			return chunks;
		DZS_SrcNode part = doc.m_Root.Get("project");
		if (!part || !part.IsObj())
			return chunks;
		string fmt = DZS_CollabJ.S(doc.m_Root, "format");
		int ver = DZS_CollabJ.I(doc.m_Root, "version");
		string t = "{\n    \"format\": " + DZS_CollabJ.Q(fmt) + ",\n    \"version\": " + ver + ",\n    \"project\": " + doc.Raw(part) + "\n}\n";
		array<string> res = new array<string>();
		res.Insert(t);
		return res;
	}

	void Update(float dt)
	{
		m_Now += dt;
		foreach (string u, DZS_CollabNet n : m_Nets)
			n.Pump();
		if (m_StartFolder != "")
		{
			DZS_ProjectsPlugin pp = Projects();
			if (pp && pp.m_Open && pp.m_Open.m_Folder == m_StartFolder && m_P.Shown())
			{
				string f = m_StartFolder;
				m_StartFolder = "";
				DZS_CollabHostedProj p = ProjectOf(f);
				if (p)
					m_P.StartHosted(p);
			}
		}
		UpdateSave();
	}

	protected void UpdateSave()
	{
		DZS_CollabHostedSave sv = m_Save;
		if (!sv || sv.m_Body != "")
		{
			if (sv && sv.m_SentAt >= 0 && m_Now - sv.m_SentAt > 30 && !sv.m_Force)
				GoLate();
			return;
		}
		if (!m_P.m_S)
		{
			EndSave("Verbindung unterbrochen", "Connection interrupted");
			return;
		}
		if (!m_P.AllConfirmed())
		{
			if (m_Now - sv.m_Started > 10)
				EndSave("Verbindung unterbrochen", "Connection interrupted");
			return;
		}
		while (sv.m_At < sv.m_Pieces.Count())
		{
			if (!sv.m_Hash)
				sv.m_Hash = new DZS_Sha256();
			if (!sv.m_Hash.Feed(sv.m_Pieces[sv.m_At], HASH_MS))
				return;
			sv.m_Sums.Insert(sv.m_Hash.Finish());
			sv.m_Hash = null;
			sv.m_At++;
		}
		if (!m_P.AllConfirmed())
			return;
		sv.m_Seq = m_P.m_S.m_Applied;
		BuildBody(sv);
		DZS_CollabServer s = m_P.FindServer(m_P.m_S.m_Server);
		int limit = 4194304;
		if (s && s.m_MaxRequest > 0)
			limit = s.m_MaxRequest;
		int size = sv.m_Body.Length();
		if (size > limit)
		{
			float mbf = Math.Ceil(size / 1048576.0);
			int mb = mbf;
			int lmb = limit / 1048576;
			string de = "Projekt zu groß für den Server (" + mb + " MB, höchstens " + lmb + " MB)";
			string en = "Project too large for the server (" + mb + " MB, at most " + lmb + " MB)";
			DZS_Log.Warn("collab: save not sent, " + size + " bytes over " + limit);
			EndSave(de, en);
			return;
		}
		SendSave(sv);
	}

	protected void BuildBody(DZS_CollabHostedSave sv)
	{
		string list = "";
		for (int i = 0; i < sv.m_Names.Count(); i++)
		{
			if (list != "")
				list += ",";
			list += "{\"name\":" + DZS_CollabJ.Q(sv.m_Names[i]) + ",\"pieces\":" + DZS_CollabJ.List(sv.m_Pieces[i]) + ",\"checksum\":" + DZS_CollabJ.Q(sv.m_Sums[i]) + "}";
		}
		string dl = "";
		foreach (string d : sv.m_Deleted)
		{
			if (dl != "")
				dl += ",";
			dl += DZS_CollabJ.Q(d);
		}
		string body = "{" + m_P.SessionHead() + ",\"requestId\":" + DZS_CollabJ.Q(sv.m_ReqId) + ",\"seq\":" + sv.m_Seq;
		body += ",\"files\":[" + list + "],\"deleted\":[" + dl + "]";
		if (sv.m_Force)
			body += ",\"force\":true";
		body += "}";
		sv.m_Body = body;
	}

	protected void SendSave(DZS_CollabHostedSave sv)
	{
		sv.m_SentAt = m_Now;
		DZS_Log.Info("collab: project/save at seq " + sv.m_Seq + ", " + sv.m_Names.Count() + " file(s), " + sv.m_Body.Length() + " bytes");
		m_P.SendInSession("hosted/save", "project/save", sv.m_Body);
	}

	bool SaveRunning(string folder)
	{
		return m_Late && m_Late.m_Folder == folder;
	}

	protected void GoLate()
	{
		if (!m_P.IsCreating())
			m_Late = m_Save;
		DZS_Log.Info("collab: no answer to project/save for 30 s, shown as failed, repeating in the background");
		EndSave("Verbindung unterbrochen", "Connection interrupted");
	}

	void OnSaved(DZS_CollabCall c, DZS_CollabReply r)
	{
		DZS_CollabHostedSave sv = m_Save;
		bool late = false;
		if (!sv || (m_Late && c.m_Body == m_Late.m_Body))
		{
			sv = m_Late;
			late = true;
		}
		if (!sv)
			return;
		if (!r.m_Framed)
		{
			float age = m_Now - sv.m_SentAt;
			if (age <= 600)
			{
				if (!late && age > 30 && !sv.m_Force)
					GoLate();
				m_P.RepeatInSession(c, 1);
				return;
			}
			if (late)
			{
				m_Late = null;
				DZS_Log.Warn("collab: project/save given up after 10 min");
				Projects().ProviderLateGivenUp();
				return;
			}
			EndSave("Verbindung unterbrochen", "Connection interrupted");
			return;
		}
		if (late)
		{
			OnLateSaved(c, r, sv);
			return;
		}
		if (!r.Ok())
		{
			string code = r.Code();
			if (code == "busy" || code == "internal")
			{
				m_P.RepeatInSession(c, 1);
				return;
			}
			if (code == "checksum_mismatch")
			{
				string name = "";
				DZS_SrcNode mm = r.m_J.Get("error");
				if (mm)
					mm = mm.Get("mismatches");
				if (mm && mm.IsArr() && mm.Count() > 0)
					name = DZS_CollabJ.S(mm.m_Items[0], "name");
				if (name == "manifest" || name == DZS_ProjectFile.NAME || name == DZS_LegacyNames.PROJECT_FILE)
					name = "Manifest";
				EndSave(name + " kam beim Server nicht richtig an", name + " did not reach the server intact");
				return;
			}
			if (code == "store_changed" && m_P.IsCreating())
			{
				EndSave("Name schon vergeben, bitte noch einmal", "Name already taken, please try again");
				return;
			}
			if (code == "store_changed")
			{
				AskStoreChanged(r);
				return;
			}
			if (code == "not_allowed")
			{
				EndSave("Speichern dürfen nur Teilnehmer mit Rechten am Projekt", "Only participants with rights to the project can save");
				return;
			}
			if (code == "too_large")
			{
				EndSave("Projekt zu groß für den Server", "Project too large for the server");
				return;
			}
			EndSave(code + " " + r.ErrS("message"), code + " " + r.ErrS("message"));
			return;
		}
		DZS_CollabHostedProj p = ProjectOf(sv.m_Folder);
		bool superseded = r.B("superseded");
		if (superseded && m_P.IsCreating())
		{
			// the server counts the empty revision of a new project as saved at seq 0: nothing was stored
			DZS_Log.Warn("collab: first save of the new server project superseded at seq " + sv.m_Seq + ", nothing stored");
			EndSave("der Server hat nichts abgelegt", "the server stored nothing");
			return;
		}
		if (p)
		{
			p.m_Rev = r.S("hostedRevision");
			DZS_SrcNode revs = r.N("revisions");
			if (revs && revs.IsArr())
			{
				foreach (DZS_SrcNode e : revs.m_Items)
					p.m_FileRev.Set(DZS_CollabJ.S(e, "name"), DZS_CollabJ.S(e, "revision"));
			}
			foreach (string d : sv.m_Deleted)
				p.m_FileRev.Remove(d);
		}
		DZS_Log.Info("collab: saved at seq " + r.I("savedSeq") + ", revision " + r.S("hostedRevision") + ", superseded " + superseded);
		m_P.SavedInSession(r.I("savedSeq"));
		EndSave("", "", superseded);
	}

	protected void OnLateSaved(DZS_CollabCall c, DZS_CollabReply r, DZS_CollabHostedSave sv)
	{
		if (!r.Ok() && (r.Code() == "busy" || r.Code() == "internal") && m_Now - sv.m_SentAt <= 600)
		{
			m_P.RepeatInSession(c, 1);
			return;
		}
		m_Late = null;
		if (!r.Ok())
		{
			DZS_Log.Warn("collab: late answer to project/save: " + r.Code());
			return;
		}
		DZS_CollabHostedProj p = ProjectOf(sv.m_Folder);
		bool superseded = r.B("superseded");
		if (p)
		{
			p.m_Rev = r.S("hostedRevision");
			DZS_SrcNode revs = r.N("revisions");
			if (revs && revs.IsArr())
			{
				foreach (DZS_SrcNode e : revs.m_Items)
					p.m_FileRev.Set(DZS_CollabJ.S(e, "name"), DZS_CollabJ.S(e, "revision"));
			}
			foreach (string d : sv.m_Deleted)
				p.m_FileRev.Remove(d);
		}
		DZS_Log.Info("collab: late answer, saved at seq " + r.I("savedSeq") + ", revision " + r.S("hostedRevision") + ", superseded " + superseded);
		m_P.SavedInSession(r.I("savedSeq"));
		if (!Projects().ProviderSavedLate(superseded))
			DZS_Log.Warn("collab: late save not applied to the project (another save is running)");
	}

	protected void EndSave(string de, string en, bool noFollow = false)
	{
		DZS_CollabHostedSave sv = m_Save;
		m_Save = null;
		if (!sv)
			return;
		if (de != "")
		{
			DZS_Log.Warn("collab: save failed: " + de);
			m_P.SaveFailedInSession();
		}
		bool creating = m_P.IsCreating();
		Projects().ProviderSaved(de, en, noFollow);
		if (creating)
			m_P.CreateSaveEnded(de == "");
	}

	protected void AskStoreChanged(DZS_CollabReply r)
	{
		string files = "";
		DZS_SrcNode err = r.m_J.Get("error");
		DZS_SrcNode ch;
		if (err)
			ch = err.Get("changed");
		if (ch && ch.IsArr())
		{
			foreach (DZS_SrcNode e : ch.m_Items)
			{
				if (files != "")
					files += ", ";
				files += e.m_Str;
			}
		}
		string name = m_P.ProjectName();
		string k = DZS_Texts.Pick("Speichern", "Save");
		string t = DZS_Texts.Pick("Außerhalb geändert", "Changed outside");
		string a = DZS_Texts.Pick(name + " wurde außerhalb der Sitzung geändert (" + files + "). Speichern überschreibt das.", name + " was changed outside the session (" + files + "). Saving overwrites that.");
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(k, t, a);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("force", DZS_Texts.Pick("Trotzdem speichern", "Save anyway"), "danger");
		c.m_OnResult.Insert(OnStoreChanged);
		c.Open();
		DZS_Log.Info("collab: store changed outside: " + files);
	}

	void OnStoreChanged(DZS_Dialog d, string result)
	{
		DZS_CollabHostedSave sv = m_Save;
		if (!sv)
			return;
		if (result != "force")
		{
			m_Save = null;
			Projects().ProviderCancelled();
			return;
		}
		sv.m_Force = true;
		sv.m_ReqId = DZS_CollabPlugin.ReqId();
		BuildBody(sv);
		SendSave(sv);
	}

	void OnReply(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (c.m_Tag == "hosted/list")
			OnList(c, r);
		else if (c.m_Tag == "hosted/head")
			OnHead(r);
		else if (c.m_Tag == "hosted/file")
			OnFile(r);
	}
}

class DZS_CollabHostedFile : Managed
{
	string m_Name;
	string m_Rel;
	int m_Parts;
	int m_Bytes;
}
