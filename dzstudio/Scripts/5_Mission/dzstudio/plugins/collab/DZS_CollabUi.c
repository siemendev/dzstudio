// The parts of "Gemeinsam bearbeiten" the user sees: section "Gemeinsam", settings page, dialogs.

// One row of the participant list: colour field, name, marks, place, knobs.
class DZS_CollabRow : Managed
{
	string m_Pid;
	Widget m_Swatch;
	TextWidget m_Name;
	TextWidget m_Mark;
	TextWidget m_Sub;
	ref DZS_Button m_Jump;
	ref DZS_Button m_Release;
	ref DZS_Button m_Remove;

	void Destroy()
	{
		if (m_Jump)
			m_Jump.Destroy();
		if (m_Release)
			m_Release.Destroy();
		if (m_Remove)
			m_Remove.Destroy();
		if (m_Swatch)
			m_Swatch.Unlink();
		if (m_Name)
			m_Name.Unlink();
		if (m_Mark)
			m_Mark.Unlink();
		if (m_Sub)
			m_Sub.Unlink();
	}
}

class DZS_CollabPanel : DZS_PanelContent
{
	DZS_CollabPlugin m_P;
	protected Widget m_Root;
	protected ref array<ref DZS_CollabRow> m_Rows = new array<ref DZS_CollabRow>();
	protected ref DZS_Button m_Invite;
	protected ref DZS_Button m_Leave;
	protected ref array<ref DZS_Button> m_Knobs = new array<ref DZS_Button>();
	protected bool m_Dirty = true;
	protected string m_Sig;
	protected float m_Tick;

	override void Build(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_Invite = new DZS_Button(m_Root, "side", DZS_Texts.Pick("Einladen …", "Invite …"));
		m_Invite.m_OnClick.Insert(OnInvite);
		m_Leave = new DZS_Button(m_Root, "side", DZS_Texts.Pick("Sitzung beenden", "End session"));
		m_Leave.m_OnClick.Insert(OnLeave);
	}

	void Dirty() { m_Dirty = true; }
	void Unfold() {}

	void OnInvite(DZS_Button b) { m_P.OpenInvite(); }

	void OnKnob(DZS_Button b) { m_P.Kernel().GetUi().Trigger(b.m_Tag); }

	protected string ListedKnobs()
	{
		string r = "";
		DZS_UiRegistry ui = m_P.Kernel().GetUi();
		array<DZS_HomeButton> all = DZS_SessionKnobs.All();
		foreach (DZS_HomeButton k : all)
		{
			if (ui.IsActionListed(k.m_Action))
				r += k.m_Action + ",";
		}
		return r;
	}

	void OnLeave(DZS_Button b)
	{
		if (!m_P.m_S)
			return;
		if (m_P.m_S.IsHost())
			m_P.AskEnd("", "", "");
		else
			m_P.LeaveSession();
	}

	void OnRowKnob(DZS_Button b)
	{
		DZS_CollabSession s = m_P.m_S;
		if (!s)
			return;
		string tag = b.m_Tag;
		int bar = tag.IndexOf("|");
		if (bar < 0)
			return;
		string what = tag.Substring(0, bar);
		string pid = tag.Substring(bar + 1, tag.Length() - bar - 1);
		DZS_CollabPeer p = s.Peer(pid);
		if (!p)
			return;
		if (what == "jump")
			m_P.JumpTo(p);
		else if (what == "release")
			m_P.ReleaseOf(p);
		else if (what == "remove")
			m_P.AskRemove(p);
	}

	protected void Rebuild()
	{
		foreach (DZS_CollabRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		foreach (DZS_Button kb : m_Knobs)
			kb.Destroy();
		m_Knobs.Clear();
		DZS_CollabSession s = m_P.m_S;
		if (!s)
			return;
		DZS_UiRegistry ui = m_P.Kernel().GetUi();
		array<DZS_HomeButton> knobs = DZS_SessionKnobs.All();
		foreach (DZS_HomeButton k : knobs)
		{
			if (!ui.IsActionListed(k.m_Action))
				continue;
			DZS_Button nb = new DZS_Button(m_Root, "side", DZS_Texts.Pick(k.m_De, k.m_En));
			nb.m_Tag = k.m_Action;
			nb.m_OnClick.Insert(OnKnob);
			m_Knobs.Insert(nb);
		}
		array<DZS_CollabPeer> order = Ordered(s);
		foreach (DZS_CollabPeer p : order)
		{
			DZS_CollabRow row = new DZS_CollabRow();
			row.m_Pid = p.m_Pid;
			row.m_Swatch = DZS_W.Panel(m_Root, DZS_CollabColors.Get(p.m_Colour));
			row.m_Name = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("text"));
			row.m_Mark = DZS_W.Text(m_Root, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
			row.m_Sub = DZS_W.Text(m_Root, DZS_Font.MONO, 11, DZS_Color.Get("textMuted"));
			if (p.m_Pid != s.m_Pid)
			{
				row.m_Jump = new DZS_Button(m_Root, "ghost", DZS_Texts.Pick("Hinspringen", "Go there"), "", 22);
				row.m_Jump.m_Tag = "jump|" + p.m_Pid;
				row.m_Jump.m_OnClick.Insert(OnRowKnob);
				if (s.m_CanManage && p.m_Role != "host")
				{
					row.m_Release = new DZS_Button(m_Root, "ghost", DZS_Texts.Pick("Belegung freigeben", "Release selection"), "", 22);
					row.m_Release.m_Tag = "release|" + p.m_Pid;
					row.m_Release.m_OnClick.Insert(OnRowKnob);
					row.m_Remove = new DZS_Button(m_Root, "ghost", DZS_Texts.Pick("Entfernen", "Remove"), "", 22);
					row.m_Remove.m_Tag = "remove|" + p.m_Pid;
					row.m_Remove.m_OnClick.Insert(OnRowKnob);
				}
			}
			m_Rows.Insert(row);
		}
		m_Dirty = false;
	}

	// Oneself first, then in order of joining.
	static array<DZS_CollabPeer> Ordered(DZS_CollabSession s)
	{
		array<DZS_CollabPeer> r = new array<DZS_CollabPeer>();
		DZS_CollabPeer me = s.Me();
		if (me)
			r.Insert(me);
		foreach (DZS_CollabPeer p : s.m_Peers)
		{
			if (p != me)
				r.Insert(p);
		}
		return r;
	}

	override void Update(float dt)
	{
		DZS_CollabSession s = m_P.m_S;
		if (!s)
			return;
		string sig = "";
		foreach (DZS_CollabPeer p : s.m_Peers)
			sig += p.m_Pid + p.m_Colour + p.m_Role + ",";
		if (s.m_CanManage)
			sig += "m";
		sig += ListedKnobs();
		if (sig != m_Sig)
		{
			m_Sig = sig;
			m_Dirty = true;
		}
		if (m_Dirty)
			Rebuild();
		m_Tick -= dt;
		if (m_Tick > 0)
			return;
		m_Tick = 0.5;
		foreach (DZS_CollabRow row : m_Rows)
		{
			DZS_CollabPeer rp = s.Peer(row.m_Pid);
			if (!rp)
				continue;
			row.m_Name.SetText(DZS_W.Literal(rp.m_Name));
			string mark = "";
			if (rp.m_Pid == s.m_Pid)
				mark = DZS_Texts.Pick("du", "you");
			if (s.m_Kind == "local" && rp.m_Role == "host")
			{
				if (mark != "")
					mark += " · ";
				mark += DZS_Texts.Pick("Gastgeber", "host");
			}
			else if (s.m_Kind == "hosted" && !rp.m_CanInvite)
			{
				if (mark != "")
					mark += " · ";
				mark += DZS_Texts.Pick("Gast", "Guest");
			}
			row.m_Mark.SetText(DZS_Texts.Upper(mark));
			string sub = m_P.PeerSub(rp);
			row.m_Sub.SetText(DZS_W.Literal(sub));
			if (row.m_Release)
				row.m_Release.SetEnabled(rp.m_Held > 0, DZS_Texts.Pick("Belegt nichts", "Holds nothing"));
		}
		m_Invite.SetEnabled(s.m_CanInvite, DZS_Texts.Pick("Nur wer Rechte am Projekt hat, lädt ein", "Only participants with rights to the project can invite"));
		if (s.IsHost())
			m_Leave.SetLabel(DZS_Texts.Pick("Sitzung beenden", "End session"));
		else if (m_P.IsLeaving())
			m_Leave.SetLabel(DZS_Texts.Pick("Verlässt …", "Leaving …"));
		else
			m_Leave.SetLabel(DZS_Texts.Pick("Verlassen", "Leave"));
		m_Leave.SetEnabled(!m_P.IsLeaving());
	}

	override float Layout(float width)
	{
		if (!m_Root)
			return 0;
		float pad = DZS_Scale.Px(10);
		float y = DZS_Scale.Px(6);
		float rowH = DZS_Scale.Px(44);
		float sw = DZS_Scale.Px(12);
		foreach (DZS_CollabRow row : m_Rows)
		{
			DZS_W.Place(row.m_Swatch, pad, y + DZS_Scale.Px(4), sw, sw);
			float nx = pad + sw + DZS_Scale.Px(8);
			DZS_W.Place(row.m_Name, nx, y, DZS_Scale.Px(110), DZS_Scale.Px(20));
			float nw = DZS_W.TextWidth(row.m_Name);
			DZS_W.Place(row.m_Mark, nx + nw + DZS_Scale.Px(8), y + DZS_Scale.Px(3), DZS_Scale.Px(120), DZS_Scale.Px(16));
			DZS_W.Place(row.m_Sub, nx, y + DZS_Scale.Px(20), width - nx - pad, DZS_Scale.Px(16));
			float bx = nx;
			float by = y + DZS_Scale.Px(40);
			if (row.m_Jump)
			{
				row.m_Jump.Place(bx, by);
				bx += row.m_Jump.GetWidth() + DZS_Scale.Px(4);
			}
			if (row.m_Remove)
				row.m_Remove.Place(bx, by);
			if (row.m_Release)
				row.m_Release.Place(nx, by + DZS_Scale.Px(26));
			y += rowH;
			if (row.m_Jump)
				y += DZS_Scale.Px(28);
			if (row.m_Release)
				y += DZS_Scale.Px(26);
		}
		y += DZS_Scale.Px(4);
		if (m_Knobs.Count() > 0)
		{
			float kx = width - pad;
			for (int ki = m_Knobs.Count() - 1; ki >= 0; ki--)
			{
				kx -= m_Knobs[ki].GetWidth();
				m_Knobs[ki].Place(kx, y);
				kx -= DZS_Scale.Px(8);
			}
			y += DZS_Scale.Px(34);
		}
		float lw = m_Leave.GetWidth();
		m_Leave.Place(width - pad - lw, y);
		float iw = m_Invite.GetWidth();
		m_Invite.Place(width - pad - lw - DZS_Scale.Px(8) - iw, y);
		y += DZS_Scale.Px(36);
		DZS_W.Place(m_Root, 0, 0, width, y);
		return y;
	}
}

// Settings › "Gemeinsam bearbeiten": the known servers with lamp and sign-in.
class DZS_CollabServerRow : DZS_HitListener
{
	DZS_CollabPage m_Page;
	string m_Url;
	ref DZS_Hit m_Hit;
	Widget m_Bg;
	TextWidget m_Name;
	TextWidget m_Addr;
	Widget m_Lamp;
	TextWidget m_State;
	ref DZS_Button m_Sign;
	ref DZS_Button m_Remove;

	override void OnHitClick(DZS_Hit h, int button)
	{
		m_Page.MakeDefault(m_Url);
	}

	void Destroy()
	{
		if (m_Sign)
			m_Sign.Destroy();
		if (m_Remove)
			m_Remove.Destroy();
		if (m_Hit)
			m_Hit.Destroy();
	}
}

class DZS_CollabPage : DZS_SettingsPage
{
	DZS_CollabPlugin m_P;
	protected Widget m_Root;
	protected DZS_Dialog m_Dialog;
	protected TextWidget m_Lead;
	protected ref array<ref DZS_CollabServerRow> m_Rows = new array<ref DZS_CollabServerRow>();
	protected ref DZS_Button m_Add;
	protected TextWidget m_Notice;
	protected string m_Sig;

	override void Build(Widget page, DZS_Dialog dialog)
	{
		m_Dialog = dialog;
		m_Root = DZS_W.Frame(page);
		m_Lead = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Lead.SetText(DZS_Texts.Pick("Ein Klick auf eine Zeile macht sie zum Standard.", "A click on a row makes it the default."));
		m_Add = new DZS_Button(m_Root, "side", DZS_Texts.Pick("Server hinzufügen …", "Add server …"));
		m_Add.m_OnClick.Insert(OnAdd);
		m_Notice = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
		BuildRows();
	}

	protected void BuildRows()
	{
		foreach (DZS_CollabServerRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		m_Sig = "";
		foreach (DZS_CollabServer s : m_P.m_Servers)
		{
			m_Sig += s.m_Url + ",";
			DZS_CollabServerRow row = new DZS_CollabServerRow();
			row.m_Page = this;
			row.m_Url = s.m_Url;
			row.m_Hit = new DZS_Hit(m_Root, row, "server");
			row.m_Bg = DZS_W.Panel(row.m_Hit.m_W, DZS_Color.Get("surfaceRaised"));
			row.m_Name = DZS_W.Text(row.m_Hit.m_W, DZS_Font.READBOLD, 14, DZS_Color.Get("text"));
			row.m_Addr = DZS_W.Text(row.m_Hit.m_W, DZS_Font.MONO, 11, DZS_Color.Get("textQuiet"));
			row.m_Lamp = DZS_W.Panel(row.m_Hit.m_W, DZS_Color.Get("textQuiet"));
			row.m_State = DZS_W.Text(row.m_Hit.m_W, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
			row.m_Sign = new DZS_Button(row.m_Hit.m_W, "side", "");
			row.m_Sign.m_Tag = s.m_Url;
			row.m_Sign.m_OnClick.Insert(OnSign);
			if (s.m_Fixed == "")
			{
				row.m_Remove = new DZS_Button(row.m_Hit.m_W, "ghost", DZS_Texts.Pick("Server entfernen", "Remove server"), "", 22);
				row.m_Remove.m_Tag = s.m_Url;
				row.m_Remove.m_OnClick.Insert(OnRemove);
			}
			m_Rows.Insert(row);
		}
	}

	void MakeDefault(string url)
	{
		DZS_CollabServer s = m_P.FindServer(url);
		if (s)
			m_P.MakeDefault(s);
	}

	void OnSign(DZS_Button b)
	{
		DZS_CollabServer s = m_P.FindServer(b.m_Tag);
		if (!s)
			return;
		if (s.m_Token != "")
			m_P.SignOut(s);
		else
			m_P.SignIn(s);
	}

	void OnRemove(DZS_Button b)
	{
		m_P.RemoveServer(m_P.FindServer(b.m_Tag));
	}

	void OnAdd(DZS_Button b)
	{
		DZS_CollabAddDialog d = new DZS_CollabAddDialog(m_P);
		d.Open();
	}

	override void Update(float dt)
	{
		string sig = "";
		foreach (DZS_CollabServer s0 : m_P.m_Servers)
			sig += s0.m_Url + ",";
		if (sig != m_Sig)
			BuildRows();
		string notice = "";
		for (int i = 0; i < m_Rows.Count() && i < m_P.m_Servers.Count(); i++)
		{
			DZS_CollabServerRow row = m_Rows[i];
			DZS_CollabServer s = m_P.m_Servers[i];
			string nm = s.m_Name;
			if (m_P.IsDefault(s))
				nm += DZS_Texts.Pick("  · Standard", "  · default");
			row.m_Name.SetText(DZS_W.Literal(nm));
			row.m_Addr.SetText(DZS_W.Literal(s.m_Url));
			string tone;
			string lamp = m_P.LampText(s, tone);
			row.m_State.SetText(DZS_W.Literal(lamp));
			string col = "textQuiet";
			if (tone == "success")
				col = "success";
			else if (tone == "danger")
				col = "danger";
			row.m_Lamp.SetColor(DZS_Color.Get(col));
			if (s.m_Token != "")
				row.m_Sign.SetLabel(DZS_Texts.Pick("Abmelden", "Sign out"));
			else
				row.m_Sign.SetLabel(DZS_Texts.Pick("Anmelden", "Sign in"));
			row.m_Sign.SetEnabled(s.m_State == "ok" || s.m_Token != "", DZS_Texts.Pick("Server nicht erreichbar", "Server not reachable"));
			row.m_Sign.Show(!m_P.NoSignIn(s));
			if (m_P.IsDefault(s) && s.m_Notice != "")
				notice = s.m_Notice;
		}
		m_Notice.SetText(DZS_W.Literal(notice));
	}

	override float Layout(float width)
	{
		if (!m_Root)
			return 0;
		float y = 0;
		DZS_W.Place(m_Lead, 0, y, width, DZS_Scale.Px(20));
		y += DZS_Scale.Px(28);
		float rowH = DZS_Scale.Px(56);
		foreach (DZS_CollabServerRow row : m_Rows)
		{
			row.m_Hit.Place(0, y, width, rowH);
			DZS_W.Place(row.m_Bg, 0, 0, width, rowH);
			DZS_W.Place(row.m_Name, DZS_Scale.Px(12), DZS_Scale.Px(8), width * 0.4, DZS_Scale.Px(20));
			DZS_W.Place(row.m_Addr, DZS_Scale.Px(12), DZS_Scale.Px(30), width * 0.4, DZS_Scale.Px(16));
			float sw = row.m_Sign.GetWidth();
			float rx = width - DZS_Scale.Px(12);
			if (row.m_Remove)
			{
				float rw = row.m_Remove.GetWidth();
				rx -= rw;
				row.m_Remove.Place(rx, DZS_Scale.Px(17));
				rx -= DZS_Scale.Px(8);
				sw += rw + DZS_Scale.Px(8);
			}
			row.m_Sign.Place(rx - row.m_Sign.GetWidth(), DZS_Scale.Px(14));
			float lx = width * 0.42;
			DZS_W.Place(row.m_Lamp, lx, DZS_Scale.Px(24), DZS_Scale.Px(8), DZS_Scale.Px(8));
			DZS_W.Place(row.m_State, lx + DZS_Scale.Px(14), DZS_Scale.Px(18), width - lx - sw - DZS_Scale.Px(40), DZS_Scale.Px(20));
			y += rowH + DZS_Scale.Px(6);
		}
		y += DZS_Scale.Px(4);
		m_Add.Place(0, y);
		y += DZS_Scale.Px(40);
		DZS_W.Place(m_Notice, 0, y, width, DZS_Scale.Px(18));
		y += DZS_Scale.Px(24);
		DZS_W.Place(m_Root, 0, 0, width, y);
		return y;
	}

	override void Destroy()
	{
		foreach (DZS_CollabServerRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		if (m_Add)
			m_Add.Destroy();
		if (m_Root)
			m_Root.Unlink();
		m_Root = null;
	}
}

// "Server hinzufügen …": address field; the server is asked at once and taken only with protocol 1.
class DZS_CollabAddDialog : DZS_Dialog
{
	DZS_CollabPlugin m_P;
	protected ref DZS_TextField m_Field;
	protected TextWidget m_Label;
	protected TextWidget m_Info;
	protected bool m_Asked;

	void DZS_CollabAddDialog(DZS_CollabPlugin p)
	{
		m_P = p;
		string k1 = DZS_Texts.Pick("Gemeinsam bearbeiten", "Edit together");
		string t1 = DZS_Texts.Pick("Server hinzufügen", "Add server");
		Setup(SMALL, k1, t1, true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("add", DZS_Texts.Pick("Hinzufügen", "Add"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Label = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Adresse", "Address")));
		m_Field = new DZS_TextField("collab.address", "https://…/dzcollab/", "textMuted", 372);
		m_Field.Build(inner);
		m_Info = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		AddFocusField(m_Field);
		AddFootFocus();
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Label, 0, 0, width, DZS_Scale.Px(14));
		m_Field.Place(0, DZS_Scale.Px(18), DZS_Scale.Px(28));
		DZS_W.Place(m_Info, 0, DZS_Scale.Px(56), width, DZS_Scale.Px(40));
		return DZS_Scale.Px(96);
	}

	override void UpdateContent(float dt)
	{
		m_Field.Refresh();
		if (!m_Asked)
			return;
		string res = m_P.m_AddResult;
		if (res == "asking")
			m_Info.SetText(DZS_Texts.Pick("Fragt den Server …", "Asking the server …"));
		else if (res == "newer")
			m_Info.SetText(DZS_Texts.Pick("Dieser Server spricht eine neuere Version — dzStudio aktualisieren", "This server speaks a newer version — update dzStudio"));
		else if (res == "nothing")
			m_Info.SetText(DZS_Texts.Pick("Kein Server für gemeinsames Bearbeiten unter dieser Adresse", "No server for editing together at this address"));
		else if (res == "known")
			m_Info.SetText(DZS_Texts.Pick("Dieser Server steht schon in der Liste", "This server is already listed"));
		else if (res == "added")
			Close();
	}

	override void OnFoot(string tag)
	{
		if (tag != "add")
		{
			Close();
			return;
		}
		string url = m_Field.GetText();
		if (url == "")
			return;
		m_Asked = true;
		m_P.TryAdd(url);
	}
}

// Sign-in with a display name only (servers in the own network).
class DZS_CollabNameDialog : DZS_NameDialog
{
	DZS_CollabPlugin m_P;
	DZS_CollabServer m_S;

	void DZS_CollabNameDialog(DZS_CollabPlugin p, DZS_CollabServer s)
	{
		m_P = p;
		m_S = s;
		string k1 = DZS_Texts.Pick("Gemeinsam bearbeiten", "Edit together");
		string t0 = DZS_Texts.Pick("Anmelden bei ", "Sign in to ");
		string t1 = t0 + s.m_Name;
		string i1 = DZS_Texts.Pick("Unter diesem Namen sehen dich die anderen.", "The others see you under this name.");
		string b1 = DZS_Texts.Pick("Anmelden", "Sign in");
		SetupName(k1, t1, "", i1, "signin", b1);
	}

	override void OnFoot(string tag)
	{
		if (tag == "signin")
		{
			string name = m_Field.GetText();
			if (name == "")
				return;
			m_P.NameLogin(m_S, name);
		}
		Close();
	}
}

// Sign-in with a code confirmed on the server's page.
class DZS_CollabCodeDialog : DZS_Dialog
{
	DZS_CollabPlugin m_P;
	protected ref DZS_SignInCode m_Body;
	protected bool m_Opened;

	void DZS_CollabCodeDialog(DZS_CollabPlugin p, DZS_CollabServer s)
	{
		m_P = p;
		string k1 = DZS_Texts.Pick("Gemeinsam bearbeiten", "Edit together");
		string t0 = DZS_Texts.Pick("Anmelden bei ", "Sign in to ");
		string t1 = t0 + s.m_Name;
		Setup(MEDIUM, k1, t1, false);
		AddFootButton("close", DZS_Texts.Get("core.dialog.closeButton"), "side");
		AddFootButton("browser", DZS_Texts.Pick("Im Browser öffnen", "Open in browser"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Body = new DZS_SignInCode(inner, "");
		AddFootFocus();
	}

	override void UpdateContent(float dt)
	{
		string st = m_P.m_LoginState;
		string t = DZS_Texts.Pick("Wartet auf Bestätigung …", "Waiting for confirmation …");
		bool waiting = true;
		if (st == "expired")
			t = DZS_Texts.Pick("Code abgelaufen — erneut anmelden", "Code expired — sign in again");
		else if (st == "denied")
			t = DZS_Texts.Pick("Anmeldung abgelehnt", "Sign-in declined");
		else if (st == "failed")
			t = DZS_Texts.Pick("Server nicht erreichbar", "Server not reachable");
		if (st == "expired" || st == "denied" || st == "failed")
			waiting = false;
		m_Body.Set(m_P.m_LoginCode, m_P.m_LoginUrl, t, waiting, m_Opened);
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
			if (m_P.m_LoginUrl != "")
			{
				GetGame().OpenURL(m_P.m_LoginUrl);
				m_Opened = true;
				Layout();
			}
			return;
		}
		m_P.CancelCodeLogin();
		Close();
	}

	override void Cancel()
	{
		m_P.CancelCodeLogin();
		super.Cancel();
	}
}

// Projekt › "Gemeinsam bearbeiten …" for a local project: starts a session.
class DZS_CollabStartDialog : DZS_Dialog
{
	DZS_CollabPlugin m_P;
	DZS_CollabServer m_S;
	protected TextWidget m_Facts;
	protected TextWidget m_Text;
	protected bool m_Started;

	void DZS_CollabStartDialog(DZS_CollabPlugin p, DZS_CollabServer s)
	{
		m_P = p;
		m_S = s;
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Gemeinsam bearbeiten", "Edit together");
		Setup(MEDIUM, k1, t1, true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("start", DZS_Texts.Pick("Sitzung starten", "Start session"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Facts = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
		string mapName = DZS_Screen.MapName(DZS_Kernel.Get().GetWorld());
		string f1 = DZS_Texts.Pick("Projekt: ", "Project: ");
		string f2 = DZS_Texts.Pick("Karte: ", "Map: ");
		string pn = m_P.ProjectName();
		string facts = "Server: " + m_S.m_Name + "   ·   " + f1 + pn + "   ·   " + f2 + mapName;
		m_Facts.SetText(DZS_W.Literal(facts));
		m_Text = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Text.SetText(DZS_Texts.Pick("Wer Rechte an diesem Projekt hat, kann direkt beitreten. Andere lädst du ein.", "Anyone with rights to this project can join directly. Others you invite."));
		AddFootFocus();
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Facts, 0, 0, width, DZS_Scale.Px(22));
		DZS_W.Place(m_Text, 0, DZS_Scale.Px(32), width, DZS_Scale.Px(20));
		return DZS_Scale.Px(56);
	}

	override void UpdateContent(float dt)
	{
		if (!m_Started)
			return;
		if (m_P.m_S && m_P.m_S.m_Ready)
		{
			Close();
			return;
		}
		if (!m_P.m_S)
			Close();
	}

	override void OnFoot(string tag)
	{
		if (tag != "start")
		{
			Close();
			return;
		}
		if (m_Started)
			return;
		m_Started = true;
		DZS_Button b = FootButton("start");
		b.SetLabel(DZS_Texts.Pick("Startet …", "Starting …"));
		b.SetEnabled(false, "");
		m_P.StartLocal(m_S);
	}
}

// "Einladen": the code, big, with "Kopieren" and "Neuer Code".
class DZS_CollabInviteDialog : DZS_Dialog
{
	DZS_CollabPlugin m_P;
	protected TextWidget m_Code;
	protected TextWidget m_Text;
	protected ref DZS_Button m_Copy;

	void DZS_CollabInviteDialog(DZS_CollabPlugin p)
	{
		m_P = p;
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Einladen", "Invite");
		Setup(SMALL, k1, t1, false);
		AddFootButton("new", DZS_Texts.Pick("Neuer Code", "New code"), "side");
		AddFootButton("close", DZS_Texts.Get("core.dialog.closeButton"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Code = DZS_W.Text(inner, DZS_Font.DISPLAY, 40, DZS_Color.Get("text"));
		m_Copy = new DZS_Button(inner, "side", DZS_Texts.Pick("Kopieren", "Copy"));
		m_Copy.m_OnClick.Insert(OnCopy);
		m_Text = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Text.SetText(DZS_Texts.Pick("Gilt für eine Person, nur für diese Sitzung, höchstens 24 Stunden.", "Valid for one person, only for this session, at most 24 hours."));
		AddFocusHit(m_Copy.m_Hit);
		AddFootFocus();
	}

	void OnCopy(DZS_Button b)
	{
		if (m_P.m_InviteCode != "")
			GetGame().CopyToClipboard(m_P.m_InviteCode);
	}

	override void UpdateContent(float dt)
	{
		string c = m_P.m_InviteCode;
		if (c == "")
			c = "…";
		m_Code.SetText(DZS_W.Literal(c));
		m_Copy.SetEnabled(m_P.m_InviteCode != "", "");
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Code, 0, 0, width, DZS_Scale.Px(52));
		float cw = m_Copy.GetWidth();
		m_Copy.Place(width - cw, DZS_Scale.Px(12));
		DZS_W.Place(m_Text, 0, DZS_Scale.Px(60), width, DZS_Scale.Px(20));
		return DZS_Scale.Px(84);
	}

	override void OnFoot(string tag)
	{
		if (tag == "new")
		{
			m_P.NewCode();
			return;
		}
		Close();
	}

	override void OnClosed()
	{
		if (m_P.m_InviteDialog == this)
			m_P.m_InviteDialog = null;
	}
}

// "Sitzung beitreten": the invite code above, the sessions the server lists below (Beitreten).
class DZS_CollabJoinRow : Managed
{
	string m_Id;
	TextWidget m_Name;
	TextWidget m_Sub;
	ref DZS_Button m_Join;
}

class DZS_CollabJoinDialog : DZS_Dialog
{
	DZS_CollabPlugin m_P;
	protected ref DZS_TextField m_Field;
	protected TextWidget m_Label;
	protected TextWidget m_Hint;
	protected TextWidget m_ListLabel;
	protected TextWidget m_Empty;
	protected ref DZS_Button m_Go;
	protected Widget m_Inner2;
	protected ref array<ref DZS_CollabJoinRow> m_Rows = new array<ref DZS_CollabJoinRow>();
	protected string m_Sig = "-";

	void DZS_CollabJoinDialog(DZS_CollabPlugin p)
	{
		m_P = p;
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		string t1 = DZS_Texts.Pick("Sitzung beitreten", "Join session");
		Setup(MEDIUM, k1, t1, true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
	}

	override void BuildContent(Widget inner)
	{
		m_Inner2 = inner;
		m_Label = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Einladungscode", "Invite code")));
		m_Field = new DZS_TextField("collab.code", "XXXX-XXXX", "textMuted", 240);
		m_Field.Build(inner);
		m_Field.m_OnCommit.Insert(OnCommit);
		m_Go = new DZS_Button(inner, "main", DZS_Texts.Pick("Beitreten", "Join"));
		m_Go.m_OnClick.Insert(OnGo);
		m_Hint = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("warning"));
		m_ListLabel = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_ListLabel.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Sitzungen, denen du beitreten kannst", "Sessions you can join")));
		m_Empty = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Empty.SetText(DZS_Texts.Pick("Gerade keine Sitzung — mit einem Einladungscode trittst du trotzdem bei.", "No session right now — with an invite code you can join anyway."));
		AddFocusField(m_Field);
		AddFocusHit(m_Go.m_Hit);
		AddFootFocus();
	}

	void OnCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		r.m_Ok = true;
		if (typed != "")
			m_P.Redeem(typed);
	}

	void OnGo(DZS_Button b)
	{
		string t = m_Field.GetText();
		if (t != "")
			m_P.Redeem(t);
	}

	void OnRow(DZS_Button b)
	{
		foreach (DZS_CollabListed l : m_P.m_Listed)
		{
			if (l.m_Id == b.m_Tag)
				m_P.JoinListed(l);
		}
	}

	override void UpdateContent(float dt)
	{
		m_Field.Refresh();
		m_Hint.SetText(DZS_W.Literal(m_P.m_JoinHint));
		if (m_P.m_Joining)
			m_Go.SetLabel(DZS_Texts.Pick("Tritt bei …", "Joining …"));
		else
			m_Go.SetLabel(DZS_Texts.Pick("Beitreten", "Join"));
		string sig = "";
		foreach (DZS_CollabListed l0 : m_P.m_Listed)
			sig += l0.m_Id + l0.m_Count + ",";
		if (sig == m_Sig)
			return;
		m_Sig = sig;
		foreach (DZS_CollabJoinRow old : m_Rows)
		{
			old.m_Name.Unlink();
			old.m_Sub.Unlink();
			old.m_Join.Destroy();
		}
		m_Rows.Clear();
		foreach (DZS_CollabListed l : m_P.m_Listed)
		{
			DZS_CollabJoinRow row = new DZS_CollabJoinRow();
			row.m_Id = l.m_Id;
			row.m_Name = DZS_W.Text(m_Inner2, DZS_Font.READBOLD, 14, DZS_Color.Get("text"));
			row.m_Name.SetText(DZS_W.Literal(l.m_Project));
			row.m_Sub = DZS_W.Text(m_Inner2, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
			string mn = DZS_Maps.Display(l.m_Map);
			string tn = DZS_Texts.Pick(" Teilnehmer", DZS_Issues.Plural(l.m_Count, " participant", " participants"));
			string sub = l.m_Host + " · " + mn + " · " + l.m_Count + tn;
			row.m_Sub.SetText(DZS_W.Literal(sub));
			row.m_Join = new DZS_Button(m_Inner2, "side", DZS_Texts.Pick("Beitreten", "Join"));
			row.m_Join.m_Tag = l.m_Id;
			row.m_Join.m_OnClick.Insert(OnRow);
			if (!DZS_Maps.Available(l.m_Map))
				row.m_Join.SetEnabled(false, DZS_Texts.Pick(mn + " ist nicht verfügbar", mn + " is not available"));
			m_Rows.Insert(row);
		}
		Layout();
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Label, 0, 0, width, DZS_Scale.Px(14));
		m_Field.Place(0, DZS_Scale.Px(18), DZS_Scale.Px(28));
		float gw = m_Go.GetWidth();
		m_Go.Place(m_Field.GetWidth() + DZS_Scale.Px(10), DZS_Scale.Px(18));
		DZS_W.Place(m_Hint, 0, DZS_Scale.Px(54), width, DZS_Scale.Px(20));
		float y = DZS_Scale.Px(88);
		DZS_W.Place(m_ListLabel, 0, y, width, DZS_Scale.Px(14));
		y += DZS_Scale.Px(22);
		m_Empty.Show(m_Rows.Count() == 0);
		if (m_Rows.Count() == 0)
		{
			DZS_W.Place(m_Empty, 0, y, width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(28);
		}
		foreach (DZS_CollabJoinRow row : m_Rows)
		{
			DZS_W.Place(row.m_Name, 0, y, width * 0.7, DZS_Scale.Px(20));
			DZS_W.Place(row.m_Sub, 0, y + DZS_Scale.Px(20), width * 0.7, DZS_Scale.Px(18));
			float bw = row.m_Join.GetWidth();
			row.m_Join.Place(width - bw, y + DZS_Scale.Px(6));
			y += DZS_Scale.Px(48);
		}
		return y;
	}

	override void OnClosed()
	{
		if (m_P.m_JoinDialog == this)
			m_P.m_JoinDialog = null;
	}
}

// Shown to a guest when the session ended: no close knob, Esc does nothing.
class DZS_CollabEndDialog : DZS_Dialog
{
	DZS_CollabPlugin m_P;
	protected TextWidget m_Text;
	protected string m_EndTitle;
	protected bool m_Reopen;
	protected bool m_CanReopen;

	// reopen: a project held by the server — „Neu öffnen“ instead of „Verwerfen“; without the right to save it is off
	// and „Verwerfen“ stands beside it.
	void DZS_CollabEndDialog(DZS_CollabPlugin p, string title, bool reopen = false, bool canReopen = true)
	{
		m_P = p;
		m_EndTitle = title;
		m_Reopen = reopen;
		m_CanReopen = canReopen;
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		Setup(SMALL, k1, title, true);
		if (reopen)
		{
			if (!canReopen)
				AddFootButton("discard", DZS_Texts.Pick("Verwerfen", "Discard"), "danger");
			AddFootButton("copy", DZS_Texts.Pick("Als eigene Kopie speichern …", "Save as own copy …"), "side");
			DZS_Button ro = AddFootButton("reopen", DZS_Texts.Pick("Neu öffnen", "Reopen"), "main");
			if (!canReopen)
				ro.SetEnabled(false, DZS_Texts.Pick("Nur wer speichern darf, öffnet es ohne laufende Sitzung", "Only users who may save can open it without a running session"));
		}
		else
		{
			AddFootButton("discard", DZS_Texts.Pick("Verwerfen", "Discard"), "danger");
			AddFootButton("copy", DZS_Texts.Pick("Als eigene Kopie speichern …", "Save as own copy …"), "main");
		}
		m_CloseX.m_Hit.m_W.Show(false);
	}

	override void BuildContent(Widget inner)
	{
		m_Text = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_Text.SetText(DZS_Texts.Pick("Dein Stand ist noch da. Speichere ihn als eigene Kopie, sonst ist er weg.", "Your state is still here. Save it as your own copy, otherwise it is gone."));
		AddFootFocus();
	}

	override float BoxWidth()
	{
		return DZS_Scale.Px(560);
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Text, 0, 0, width, DZS_Scale.Px(24));
		return DZS_Scale.Px(28);
	}

	override void Cancel() {}

	override void OnFoot(string tag)
	{
		if (tag == "copy")
		{
			Close();
			m_P.EndCopy();
			return;
		}
		if (tag == "reopen")
		{
			Close();
			m_P.Reopen();
			return;
		}
		Close();
		m_P.AskDiscard(m_EndTitle, m_Reopen, m_CanReopen);
	}
}

// „Die Verbindung war zu lange weg“.
class DZS_CollabLostDialog : DZS_Dialog
{
	DZS_CollabPlugin m_P;
	protected TextWidget m_Text;

	void DZS_CollabLostDialog(DZS_CollabPlugin p)
	{
		m_P = p;
		string k1 = DZS_Texts.Pick("Gemeinsam", "Together");
		Setup(SMALL, k1, DZS_Texts.Pick("Die Verbindung war zu lange weg", "The connection was gone too long"), true);
		AddFootButton("copy", DZS_Texts.Pick("Als eigene Kopie speichern …", "Save as own copy …"), "side");
		AddFootButton("rejoin", DZS_Texts.Pick("Neu beitreten", "Join again"), "main");
		m_CloseX.m_Hit.m_W.Show(false);
	}

	override void BuildContent(Widget inner)
	{
		m_Text = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_Text.SetText(DZS_Texts.Pick("Was den Server nicht erreicht hat, ist nur noch bei dir.", "What did not reach the server is only with you now."));
		AddFootFocus();
	}

	override float BoxWidth()
	{
		return DZS_Scale.Px(560);
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Text, 0, 0, width, DZS_Scale.Px(24));
		return DZS_Scale.Px(28);
	}

	override void Cancel() {}

	override void OnFoot(string tag)
	{
		Close();
		if (tag == "copy")
		{
			m_P.OwnCopy();
			return;
		}
		m_P.AskRejoin();
	}
}
