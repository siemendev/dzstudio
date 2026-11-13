// Project › Upload to dzbot …: the open local project becomes a dzbot project mod — a new one, or one of the user's
// whose draft it replaces. Unsaved changes are saved locally first; the project file is then sent as the mod's
// dzstudio/project.json through the dzbot save. The local file stays as it is; on success the mod opens in its place.
class DZS_DzbotUpload : DZS_MessageListener
{
	DZS_DzbotPlugin m_P;
	ref DZS_DzbotUploadDialog m_Dialog;
	// "", "saving" (the local project), "creating", "loading" (the mod's project file), "sending"
	string m_State;
	string m_Error;
	protected bool m_Hooked;
	protected string m_Folder;
	protected string m_MapKey;
	// the create key of one dialog: a repeat after a failure gets the same mod back
	protected string m_Key;
	protected bool m_New;
	protected string m_Name;
	protected string m_Owner;
	protected bool m_Multi;
	protected string m_Ref;
	protected ref array<string> m_Chunks;
	protected string m_DoneUrl;

	void DZS_DzbotUpload(DZS_DzbotPlugin p)
	{
		m_P = p;
	}

	bool Busy() { return m_State != ""; }

	string MapKey() { return m_MapKey; }

	// A local project is open in the scene.
	bool LocalOpen()
	{
		DZS_ProjectsPlugin pp = m_P.Projects();
		if (!pp || !pp.m_Open || pp.IsHome() || m_P.Kernel().GetShown() == null)
			return false;
		return pp.m_Open.m_Provider == "local";
	}

	bool CanStart(out string de, out string en)
	{
		if (!LocalOpen())
		{
			de = "Nur in einem lokalen Projekt";
			en = "Only in a local project";
			return false;
		}
		if (m_P.Outdated())
		{
			de = "dzStudio ist veraltet";
			en = "dzStudio is out of date";
			return false;
		}
		if (!m_P.IsConnected())
		{
			de = "Erst mit dzbot verbinden — Projekt › Mit dzbot verbinden …";
			en = "Connect with dzbot first — Project › Connect with dzbot …";
			return false;
		}
		if (Busy())
		{
			de = "Wird gerade hochgeladen";
			en = "Uploading right now";
			return false;
		}
		return true;
	}

	void OpenDialog()
	{
		string de, en;
		if (m_Dialog || !CanStart(de, en))
			return;
		DZS_ProjectsPlugin pp = m_P.Projects();
		m_Folder = pp.m_Open.m_Folder;
		m_MapKey = pp.m_Open.m_MapKey;
		m_Key = DZS_Random.Code(16);
		m_Ref = "";
		m_Error = "";
		m_Dialog = new DZS_DzbotUploadDialog(this, pp.m_Open.m_Name);
		m_Dialog.Open();
		m_P.RefreshProjects();
		DZS_Log.Info("dzbot: upload dialog for " + m_Folder + " on " + m_MapKey);
	}

	void DialogClosed(DZS_DzbotUploadDialog d)
	{
		if (m_Dialog == d)
			m_Dialog = null;
	}

	// Who may own a new mod, the user first; before the list names any, the user alone.
	array<ref DZS_DzbotOwner> Owners()
	{
		array<ref DZS_DzbotOwner> r = new array<ref DZS_DzbotOwner>();
		foreach (DZS_DzbotOwner ow : m_P.m_Owners)
			r.Insert(ow);
		if (r.Count() == 0)
		{
			DZS_DzbotOwner me = new DZS_DzbotOwner();
			me.m_Id = "me";
			me.m_Name = DZS_Texts.Pick("Ich", "Me");
			me.m_Kind = "creator";
			r.Insert(me);
		}
		return r;
	}

	// The user's project mods that apply to the project's map (no file mods, no placements).
	array<DZS_DzbotProject> Targets()
	{
		array<DZS_DzbotProject> r = new array<DZS_DzbotProject>();
		array<string> order = m_P.ListOrder();
		foreach (string pref : order)
		{
			DZS_DzbotProject dp = m_P.m_Projects.Get(pref);
			if (dp && DZS_DzbotPlugin.IsProjectMod(dp) && dp.m_Maps.Find(m_MapKey) >= 0)
				r.Insert(dp);
		}
		return r;
	}

	// isNew: a new mod named name for owner; else the project mod pref.
	void Start(bool isNew, string name, string owner, bool multi, string pref)
	{
		if (Busy())
			return;
		m_Error = "";
		DZS_ProjectsPlugin pp = m_P.Projects();
		if (!LocalOpen() || pp.m_Open.m_Folder != m_Folder)
		{
			Fail("Das Projekt ist nicht mehr offen", "The project is no longer open");
			return;
		}
		if (!m_P.IsConnected())
		{
			Fail("Nicht mit dzbot verbunden", "Not connected with dzbot");
			return;
		}
		m_New = isNew;
		m_Name = name;
		m_Owner = owner;
		m_Multi = multi;
		if (!isNew)
			m_Ref = pref;
		DZS_Log.Info("dzbot: upload " + m_Folder + ", new " + isNew + ", owner " + owner + ", target " + pref);
		array<string> chunks = DZS_Files.Read(DZS_LocalStore.File(m_Folder));
		DZS_Instance inst = m_P.Kernel().GetShown();
		if (!inst.IsUnsaved() && chunks && DZS_ProjectFile.IsV2(chunks))
		{
			Send(chunks);
			return;
		}
		if (pp.IsSaving())
		{
			Fail("Das Projekt wird gerade gespeichert — gleich noch einmal", "The project is being saved — try again in a moment");
			return;
		}
		if (!m_Hooked)
		{
			pp.m_SaveNotified.Insert(OnLocalSaved);
			m_Hooked = true;
		}
		m_State = "saving";
		pp.StartSave(false, "notify");
		if (!pp.IsSaving() && m_State == "saving")
		{
			m_State = "";
			Fail("Erst speichern", "Save first");
		}
	}

	void OnLocalSaved(bool saved)
	{
		if (m_State != "saving")
			return;
		m_State = "";
		if (!saved)
		{
			Fail("Lokal nicht gespeichert — nichts hochgeladen", "Not saved locally — nothing uploaded");
			return;
		}
		array<string> chunks = DZS_Files.Read(DZS_LocalStore.File(m_Folder));
		if (!chunks || !DZS_ProjectFile.IsV2(chunks))
		{
			Fail("Die Projektdatei ist nicht lesbar", "The project file is not readable");
			return;
		}
		Send(chunks);
	}

	protected void Send(array<string> chunks)
	{
		m_Chunks = chunks;
		if (!m_New)
		{
			SendLoad();
			return;
		}
		m_State = "creating";
		string multi = "false";
		if (m_Multi)
			multi = "true";
		string fields = "\"name\":" + DZS_DzbotFrame.Q(m_Name) + ",\"map\":" + DZS_DzbotFrame.Q(m_MapKey);
		fields += ",\"owner\":" + DZS_DzbotFrame.Q(m_Owner) + ",\"multiInstance\":" + multi + ",\"key\":" + DZS_DzbotFrame.Q(m_Key);
		m_P.AuthPost("main", "upload.create", "/editor/project/create", fields);
	}

	// The mod's project file (key and revision) without opening it: the upload replaces it on purpose.
	protected void SendLoad()
	{
		m_State = "loading";
		m_P.AuthPost("main", "upload.load", "/editor/project/load", "\"project\":" + DZS_DzbotFrame.Q(m_Ref));
	}

	void OnCreated(DZS_DzbotReply r)
	{
		if (m_State != "creating")
			return;
		m_State = "";
		if (!Answered(r))
			return;
		DZS_SrcNode pj = r.m_Json.Get("project");
		DZS_DzbotProject dp = null;
		if (pj && pj.IsObj())
			dp = m_P.TakeProject(pj);
		if (!dp)
		{
			Fail("dzbot hat keine Mod zurückgegeben", "dzbot returned no mod");
			return;
		}
		m_Ref = dp.m_Ref;
		DZS_Log.Info("dzbot: upload created " + m_Ref + " with key " + m_Key);
		SendLoad();
	}

	void OnLoaded(DZS_DzbotReply r)
	{
		if (m_State != "loading")
			return;
		m_State = "";
		if (!Answered(r))
			return;
		DZS_SrcNode pj = r.m_Json.Get("project");
		DZS_DzbotProject dp = null;
		if (pj && pj.IsObj())
			dp = m_P.TakeProject(pj);
		if (!dp || !DZS_DzbotPlugin.IsProjectMod(dp))
		{
			Fail("Das ist keine Projekt-Mod", "This is not a project mod");
			return;
		}
		DZS_SrcNode sess = r.m_Json.Get("session");
		if (sess && !sess.IsNull())
		{
			Fail("Für diese Mod läuft eine gemeinsame Sitzung", "A shared session runs for this mod");
			return;
		}
		dp.m_ProjectFile = m_P.ProjectFileOf(r);
		m_State = "sending";
		if (!m_P.SaveUpload(dp, m_Chunks, m_MapKey))
		{
			m_State = "";
			Fail("dzbot speichert gerade — gleich noch einmal", "dzbot is saving right now — try again in a moment");
		}
	}

	// false (and the failure shown) for a reply that is no success.
	protected bool Answered(DZS_DzbotReply r)
	{
		if (!r.m_Framed)
		{
			Fail("dzbot nicht erreichbar", "dzbot not reachable");
			return false;
		}
		if (r.Ok())
			return true;
		string err = r.Error();
		string m = r.Message();
		if (m != "")
			Fail("dzbot: " + m, "dzbot: " + m);
		else if (err == "not_allowed")
			Fail("dzbot: dafür fehlt dir die Berechtigung", "dzbot: you are not allowed to do that");
		else if (err == "pro_required")
			Fail("dzbot: dafür braucht es dzbot Pro", "dzbot: this needs dzbot Pro");
		else
			Fail("dzbot: " + err, "dzbot: " + err);
		return false;
	}

	// The end of the dzbot save (DZS_DzbotPlugin.SaveUpload): "" = saved.
	void Saved(string de, string en)
	{
		if (m_State != "sending")
			return;
		m_State = "";
		m_Chunks = null;
		if (de != "")
		{
			Fail(de, en);
			return;
		}
		DZS_DzbotProject dp = m_P.m_Projects.Get(m_Ref);
		string name = m_Name;
		m_DoneUrl = "";
		if (dp)
		{
			name = dp.m_Name;
			m_DoneUrl = dp.m_ManageUrl;
		}
		DZS_Log.Info("dzbot: uploaded " + m_Folder + " into " + m_Ref + ", manage " + m_DoneUrl);
		if (m_Dialog)
			m_Dialog.Close();
		DZS_ProjectsPlugin pp = m_P.Projects();
		if (pp)
			pp.RequestOpen(DZS_DzbotPlugin.FolderOf(m_Ref));
		string t = DZS_Texts.Pick("Hochgeladen: „" + name + "“ ist jetzt eine dzbot-Mod.", "Uploaded: “" + name + "” is now a dzbot mod.");
		if (!m_New)
			t = DZS_Texts.Pick("Hochgeladen: der Entwurf von „" + name + "“ ist ersetzt.", "Uploaded: the draft of “" + name + "” is replaced.");
		DZS_Screen scr = m_P.Kernel().GetScreen();
		if (m_DoneUrl != "")
			scr.ShowMessageWithButton("dzbot.upload", "success", DZS_W.Literal(t), 15.0, DZS_Texts.Pick("In dzbot fertig einrichten", "Finish setting it up in dzbot"), this);
		else
			scr.ShowMessage("dzbot.upload", "success", DZS_W.Literal(t), 8.0);
	}

	override void OnMessageButton(string kind)
	{
		if (kind == "dzbot.upload" && m_DoneUrl != "")
			GetGame().OpenURL(m_DoneUrl);
	}

	// Nothing changes locally; the dialog shows why and can try again (a created mod is reused through the key).
	protected void Fail(string de, string en)
	{
		m_Chunks = null;
		m_Error = DZS_Texts.Pick(de, en);
		DZS_Log.Warn("dzbot: upload failed: " + m_Error);
		if (!m_Dialog)
			m_P.Kernel().GetScreen().ShowMessage("dzbot.upload", "danger", DZS_W.Literal(m_Error), 8.0);
	}

	string StateText()
	{
		if (m_State == "saving")
			return DZS_Texts.Pick("Speichert das Projekt lokal …", "Saving the project locally …");
		if (m_State == "creating")
			return DZS_Texts.Pick("Legt die Mod in dzbot an …", "Creating the mod in dzbot …");
		if (m_State == "loading")
			return DZS_Texts.Pick("Fragt dzbot nach dem Entwurf …", "Asking dzbot for the draft …");
		if (m_State == "sending")
			return DZS_Texts.Pick("Lädt hoch …", "Uploading …");
		return "";
	}
}

class DZS_DzbotUploadDialog : DZS_Dialog
{
	protected DZS_DzbotUpload m_U;
	protected Widget m_In;
	protected string m_ProjectName;
	protected bool m_New = true;
	protected int m_SeenRev;
	protected bool m_Loaded;
	protected ref DZS_Chip m_ModeNew;
	protected ref DZS_Chip m_ModeOld;
	protected TextWidget m_NameLabel;
	protected ref DZS_TextField m_Field;
	protected TextWidget m_OwnerLabel;
	protected ref array<ref DZS_Chip> m_OwnerChips = new array<ref DZS_Chip>();
	protected string m_Owner;
	protected TextWidget m_MapLabel;
	protected TextWidget m_MapText;
	protected ref DZS_Toggle m_Multi;
	protected TextWidget m_MultiText;
	protected ref DZS_DzbotWrap m_NewNote;
	protected TextWidget m_ListLabel;
	protected ref array<ref DZS_ListRow> m_Rows = new array<ref DZS_ListRow>();
	protected string m_Target;
	protected ref DZS_DzbotWrap m_Empty;
	protected ref DZS_DzbotWrap m_OldNote;
	protected TextWidget m_ListState;
	protected ref DZS_DzbotWrap m_Error;
	protected TextWidget m_Busy;

	void DZS_DzbotUploadDialog(DZS_DzbotUpload u, string projectName)
	{
		m_U = u;
		m_ProjectName = projectName;
		Setup(SMALL, "dzbot", DZS_Texts.Pick("Zu dzbot hochladen", "Upload to dzbot"), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("upload", DZS_Texts.Pick("Hochladen", "Upload"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_In = inner;
		m_SeenRev = m_U.m_P.m_ListRev;
		m_ModeNew = new DZS_Chip(inner, "new", DZS_Texts.Pick("Neue Mod", "New mod"));
		m_ModeNew.m_OnClick.Insert(OnMode);
		m_ModeOld = new DZS_Chip(inner, "existing", DZS_Texts.Pick("Bestehende aktualisieren", "Update existing"));
		m_ModeOld.m_OnClick.Insert(OnMode);
		m_NameLabel = NewLabel(DZS_Texts.Pick("Name", "Name"));
		m_Field = new DZS_TextField("dzbot.upload.name", m_ProjectName, "textMuted", 372);
		m_Field.Build(inner);
		m_Field.SetText(m_ProjectName);
		m_Field.m_OnCommit.Insert(OnFieldCommit);
		m_OwnerLabel = NewLabel(DZS_Texts.Pick("Besitzer", "Owner"));
		m_MapLabel = NewLabel(DZS_Texts.Pick("Karte", "Map"));
		m_MapText = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_MapText.SetText(DZS_Maps.Display(m_U.MapKey()) + DZS_Texts.Pick(" · aus dem Projekt", " · from the project"));
		m_Multi = new DZS_Toggle(inner);
		m_MultiText = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_MultiText.SetText(DZS_Texts.Pick("Läuft mehrfach", "Runs more than once"));
		m_NewNote = new DZS_DzbotWrap(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"), DZS_Texts.Pick("Privat, bis du sie in dzbot veröffentlichst.", "Private until you publish it in dzbot."));
		m_ListLabel = NewLabel(DZS_Texts.Pick("Deine Projekt-Mods auf ", "Your project mods on ") + DZS_Maps.Display(m_U.MapKey()));
		string mapName = DZS_Maps.Display(m_U.MapKey());
		m_Empty = new DZS_DzbotWrap(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"), DZS_Texts.Pick("Du hast noch keine Projekt-Mod auf " + mapName + ". Datei-Mods und Platzierungen lassen sich nicht ersetzen — lege mit „Neue Mod“ eine an.", "You have no project mod on " + mapName + " yet. File mods and placements cannot be replaced — create one with “New mod”."));
		m_OldNote = new DZS_DzbotWrap(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"), DZS_Texts.Pick("Ersetzt den Entwurf durch dieses Projekt. Die veröffentlichte Version bleibt, bis du neu veröffentlichst.", "Replaces the draft with this project. The published version stays until you publish again."));
		m_ListState = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Error = new DZS_DzbotWrap(inner, DZS_Font.READ, 13, DZS_Color.Get("danger"), "");
		m_Busy = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		BuildOwners();
		SetMode(true);
	}

	protected TextWidget NewLabel(string text)
	{
		TextWidget t = DZS_W.Text(m_In, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		t.SetText(DZS_Texts.Upper(text));
		return t;
	}

	void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		r.m_Ok = DZS_Names.ValidProjectName(typed);
	}

	void OnMode(DZS_Chip c)
	{
		if (!m_U.Busy())
			SetMode(c == m_ModeNew);
	}

	void SetMode(bool isNew)
	{
		m_New = isNew;
		m_ModeNew.SetOn(isNew);
		m_ModeOld.SetOn(!isNew);
		m_NameLabel.Show(isNew);
		m_Field.Show(isNew);
		m_OwnerLabel.Show(isNew);
		m_MapLabel.Show(isNew);
		m_MapText.Show(isNew);
		m_Multi.m_Hit.m_W.Show(isNew);
		m_MultiText.Show(isNew);
		m_NewNote.Show(isNew);
		m_ListLabel.Show(!isNew);
		m_OldNote.Show(!isNew);
		ShowRows();
		DZS_Button main = FootButton("upload");
		if (isNew)
			main.SetLabel(DZS_Texts.Pick("Hochladen", "Upload"));
		else
			main.SetLabel(DZS_Texts.Pick("Ersetzen", "Replace"));
		Refocus();
		DZS_Log.Info("dzbot: upload dialog mode " + Describe());
	}

	// One line for the log (and the in-game check): mode, list state, owners, targets, choice.
	string Describe()
	{
		string mode = "existing";
		if (m_New)
			mode = "new";
		return mode + ", loaded " + m_Loaded + ", owners " + m_OwnerChips.Count() + " (" + m_Owner + "), targets " + m_Rows.Count() + " (" + m_Target + "), state '" + m_U.m_State + "', error '" + m_U.m_Error + "'";
	}

	// The test hook: the n-th row of the list chosen (0 = first).
	void PickRow(int n)
	{
		if (n >= 0 && n < m_Rows.Count())
			OnRow(m_Rows[n]);
	}

	protected void BuildOwners()
	{
		foreach (DZS_Chip old : m_OwnerChips)
			old.m_Hit.Destroy();
		m_OwnerChips.Clear();
		array<ref DZS_DzbotOwner> owners = m_U.Owners();
		bool kept = false;
		foreach (DZS_DzbotOwner ow : owners)
		{
			DZS_Chip c = new DZS_Chip(m_In, ow.m_Id, ow.m_Name);
			c.m_OnClick.Insert(OnOwner);
			m_OwnerChips.Insert(c);
			if (ow.m_Id == m_Owner)
				kept = true;
		}
		if (!kept)
			m_Owner = owners[0].m_Id;
		foreach (DZS_Chip oc : m_OwnerChips)
		{
			oc.SetOn(oc.m_Key == m_Owner);
			oc.Show(m_New && m_Loaded);
		}
	}

	void OnOwner(DZS_Chip c)
	{
		if (m_U.Busy())
			return;
		m_Owner = c.m_Key;
		foreach (DZS_Chip oc : m_OwnerChips)
			oc.SetOn(oc == c);
	}

	protected void BuildRows()
	{
		foreach (DZS_ListRow old : m_Rows)
			old.Destroy();
		m_Rows.Clear();
		array<DZS_DzbotProject> targets = m_U.Targets();
		bool kept = false;
		foreach (DZS_DzbotProject dp : targets)
		{
			DZS_ListRow r = new DZS_ListRow(m_In, dp.m_Ref, dp.m_Name, dp.m_Owner, "");
			r.m_OnClick.Insert(OnRow);
			m_Rows.Insert(r);
			if (dp.m_Ref == m_Target)
				kept = true;
		}
		if (!kept)
			m_Target = "";
		ShowRows();
	}

	protected void ShowRows()
	{
		foreach (DZS_ListRow r : m_Rows)
		{
			r.m_Hit.m_W.Show(!m_New);
			r.SetSelected(r.m_Key == m_Target);
		}
		m_Empty.Show(!m_New && m_Loaded && m_Rows.Count() == 0);
		foreach (DZS_Chip oc : m_OwnerChips)
			oc.Show(m_New && m_Loaded);
	}

	void OnRow(DZS_ListRow r)
	{
		if (m_U.Busy())
			return;
		m_Target = r.m_Key;
		foreach (DZS_ListRow o : m_Rows)
			o.SetSelected(o == r);
	}

	protected void Refocus()
	{
		ClearFocusables();
		if (m_New)
		{
			AddFocusField(m_Field);
			AddFocusHit(m_Multi.m_Hit);
		}
		else
		{
			foreach (DZS_ListRow r : m_Rows)
				AddFocusHit(r.m_Hit);
		}
		AddFootFocus();
	}

	override void UpdateContent(float dt)
	{
		m_Field.Refresh();
		DZS_DzbotPlugin p = m_U.m_P;
		if (!m_Loaded && !p.m_ListLoading && p.m_ListRev != m_SeenRev)
		{
			m_Loaded = p.m_ListError == "";
			m_SeenRev = p.m_ListRev;
			if (m_Loaded)
			{
				BuildOwners();
				BuildRows();
				Refocus();
			}
			DZS_Log.Info("dzbot: upload dialog list " + Describe());
		}
		string listState = "";
		if (!m_Loaded)
		{
			listState = DZS_Texts.Pick("Lädt deine dzbot-Mods …", "Loading your dzbot mods …");
			if (!p.m_ListLoading && p.m_ListError != "")
				listState = p.m_ListError;
		}
		m_ListState.SetText(listState);
		m_ListState.Show(listState != "");
		m_Error.SetText(m_U.m_Error);
		m_Error.Show(m_U.m_Error != "" && !m_U.Busy());
		m_Busy.SetText(m_U.StateText());
		m_Busy.Show(m_U.Busy());
		bool busy = m_U.Busy();
		m_ModeNew.SetEnabled(!busy);
		m_ModeOld.SetEnabled(!busy);
		string why = "";
		if (busy)
			why = m_U.StateText();
		else if (!m_Loaded)
			why = listState;
		else if (m_New && !DZS_Names.ValidProjectName(Value()))
			why = DZS_Texts.Pick("Name: 1–60 Zeichen, nicht nur Leerzeichen", "Name: 1–60 characters, not only spaces");
		else if (!m_New && m_Target == "")
			why = DZS_Texts.Pick("Erst eine Mod wählen", "Choose a mod first");
		DZS_Button main = FootButton("upload");
		bool ok = why == "";
		if (main.m_Hit.m_Enabled != ok)
			main.SetEnabled(ok, why);
	}

	string Value()
	{
		string t = m_Field.GetText();
		if (t == "")
			return m_ProjectName;
		return t;
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		float gap = DZS_Scale.Px(8);
		m_ModeNew.Place(0, y);
		m_ModeOld.Place(m_ModeNew.Width() + gap, y);
		y += DZS_Chip.Height() + DZS_Scale.Px(16);
		if (m_New)
		{
			DZS_W.Place(m_NameLabel, 0, y, width, DZS_Scale.Px(14));
			m_Field.Place(0, y + DZS_Scale.Px(18), DZS_Scale.Px(28));
			y += DZS_Scale.Px(58);
			DZS_W.Place(m_OwnerLabel, 0, y, width, DZS_Scale.Px(14));
			y += DZS_Scale.Px(18);
			if (m_Loaded)
			{
				float x = 0;
				foreach (DZS_Chip oc : m_OwnerChips)
				{
					float cw = Math.Min(oc.Width(), width);
					if (x > 0 && x + cw > width)
					{
						x = 0;
						y += DZS_Chip.Height() + DZS_Scale.Px(4);
					}
					oc.Place(x, y, cw);
					x += cw + gap;
				}
				y += DZS_Chip.Height() + DZS_Scale.Px(14);
			}
			else
			{
				DZS_W.Place(m_ListState, 0, y, width, DZS_Scale.Px(20));
				y += DZS_Scale.Px(30);
			}
			DZS_W.Place(m_MapLabel, 0, y, width, DZS_Scale.Px(14));
			DZS_W.Place(m_MapText, 0, y + DZS_Scale.Px(18), width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(48);
			m_Multi.Place(0, y + DZS_Scale.Px(3));
			DZS_W.Place(m_MultiText, DZS_Toggle.Width() + DZS_Scale.Px(10), y, width - DZS_Toggle.Width() - DZS_Scale.Px(10), DZS_Scale.Px(20));
			y += DZS_Scale.Px(32);
			y += m_NewNote.Place(0, y, width, DZS_Scale.Px(19));
		}
		else
		{
			DZS_W.Place(m_ListLabel, 0, y, width, DZS_Scale.Px(14));
			y += DZS_Scale.Px(18);
			if (!m_Loaded)
			{
				DZS_W.Place(m_ListState, 0, y, width, DZS_Scale.Px(20));
				y += DZS_Scale.Px(30);
			}
			else if (m_Rows.Count() == 0)
				y += m_Empty.Place(0, y, width, DZS_Scale.Px(19)) + DZS_Scale.Px(10);
			else
			{
				float rowH = DZS_Scale.Px(48);
				foreach (DZS_ListRow r : m_Rows)
				{
					r.Place(0, y, width, rowH);
					y += rowH + DZS_Scale.Px(2);
				}
				y += DZS_Scale.Px(10);
			}
			y += m_OldNote.Place(0, y, width, DZS_Scale.Px(19));
		}
		if (m_U.Busy())
		{
			DZS_W.Place(m_Busy, 0, y + DZS_Scale.Px(10), width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(30);
		}
		else if (m_U.m_Error != "")
			y += DZS_Scale.Px(10) + m_Error.Place(0, y + DZS_Scale.Px(10), width, DZS_Scale.Px(19));
		return y + DZS_Scale.Px(4);
	}

	override void OnFoot(string tag)
	{
		if (tag != "upload")
		{
			Close();
			return;
		}
		if (m_U.Busy())
			return;
		if (m_New)
		{
			string name = Value();
			if (!DZS_Names.ValidProjectName(name) || !m_Loaded)
				return;
			m_U.Start(true, name, m_Owner, m_Multi.IsOn(), "");
			return;
		}
		if (m_Target != "")
			m_U.Start(false, "", "", false, m_Target);
	}

	override void OnClosed()
	{
		foreach (DZS_Chip oc : m_OwnerChips)
			oc.m_Hit.Destroy();
		m_OwnerChips.Clear();
		foreach (DZS_ListRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		m_ModeNew.m_Hit.Destroy();
		m_ModeOld.m_Hit.Destroy();
		m_Multi.m_Hit.Destroy();
		m_NewNote.Destroy();
		m_Empty.Destroy();
		m_OldNote.Destroy();
		m_Error.Destroy();
		m_U.DialogClosed(this);
	}
}
