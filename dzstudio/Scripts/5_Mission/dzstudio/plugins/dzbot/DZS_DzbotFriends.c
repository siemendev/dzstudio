// dzbot friends: the own friends list over the editor
// endpoints /editor/friends and /editor/friends/set, shown in the plugin's settings tab and, in a session on the dzbot
// server, in the dialog "Friends" with "Add as friend".
class DZS_DzbotFriend : Managed
{
	int m_Id;
	string m_Name;
	bool m_Always;
	// switch sent, answer pending: shown in its new position, off
	bool m_Busy;
}

class DZS_DzbotFriendSet : Managed
{
	int m_Id;
	bool m_Add;
	bool m_Always;
	bool m_Was;
	int m_StartMs;
}

class DZS_DzbotHeldSet : Managed
{
	ref DZS_DzbotReply m_R;
	ref DZS_DzbotFriendSet m_Q;
	int m_DueMs;
}

class DZS_DzbotFriends : Managed
{
	static const int MAX = 200;

	DZS_DzbotPlugin m_P;
	// "", "loading", "ok", "failed"
	string m_State;
	ref array<ref DZS_DzbotFriend> m_List = new array<ref DZS_DzbotFriend>();
	int m_Max = MAX;
	string m_ManageUrl;
	// line under the list after a failed change
	string m_Note;
	ref map<int, bool> m_Adding = new map<int, bool>();
	// "Adding …" stays at least this long; earlier answers wait in m_Held
	static const int ADD_MIN_MS = 400;
	ref array<ref DZS_DzbotHeldSet> m_Held = new array<ref DZS_DzbotHeldSet>();
	int m_Rev;

	void Tick()
	{
		if (m_Held.Count() == 0)
			return;
		int now = DZS_Kernel.NowMs();
		for (int i = m_Held.Count() - 1; i >= 0; i--)
		{
			DZS_DzbotHeldSet h = m_Held[i];
			if (now < h.m_DueMs)
				continue;
			DZS_DzbotReply hr = h.m_R;
			DZS_DzbotFriendSet hq = h.m_Q;
			m_Held.Remove(i);
			Apply(hr, hq);
		}
	}

	void Load()
	{
		if (!m_P.IsConnected())
		{
			m_State = "";
			return;
		}
		if (m_State == "loading")
			return;
		m_State = "loading";
		m_Note = "";
		m_Rev++;
		m_P.AuthPost("friends", "friends", "/editor/friends", "");
	}

	DZS_DzbotFriend Find(int id)
	{
		foreach (DZS_DzbotFriend f : m_List)
		{
			if (f.m_Id == id)
				return f;
		}
		return null;
	}

	bool Full() { return m_State == "ok" && m_List.Count() >= m_Max; }

	void SetAlways(int id, bool on)
	{
		DZS_DzbotFriend f = Find(id);
		if (!f || f.m_Busy)
			return;
		DZS_DzbotFriendSet q = new DZS_DzbotFriendSet();
		q.m_Id = id;
		q.m_Always = on;
		q.m_Was = f.m_Always;
		f.m_Always = on;
		f.m_Busy = true;
		m_Note = "";
		m_Rev++;
		string fields = "\"id\":" + id + ",\"alwaysJoin\":false";
		if (on)
			fields = "\"id\":" + id + ",\"alwaysJoin\":true";
		DZS_Log.Info("dzbot: friends/set " + id + " alwaysJoin " + on);
		m_P.AuthPost("friends", "friends.set", "/editor/friends/set", fields, null, q);
	}

	void Add(int id)
	{
		if (Find(id) || m_Adding.Contains(id))
			return;
		DZS_DzbotFriendSet q = new DZS_DzbotFriendSet();
		q.m_Id = id;
		q.m_Add = true;
		q.m_StartMs = DZS_Kernel.NowMs();
		m_Adding.Set(id, true);
		m_Rev++;
		DZS_Log.Info("dzbot: friends/set " + id + " (add)");
		m_P.AuthPost("friends", "friends.set", "/editor/friends/set", "\"id\":" + id, null, q);
	}

	void OnList(DZS_DzbotReply r)
	{
		m_Rev++;
		if (!r.m_Framed || !r.Ok())
		{
			m_State = "failed";
			DZS_Log.Warn("dzbot: friends not loaded: " + r.m_Fail + r.Error());
			return;
		}
		m_State = "ok";
		m_List.Clear();
		int mx = r.Int("max");
		if (mx > 0)
			m_Max = mx;
		m_ManageUrl = r.Str("manageUrl");
		DZS_SrcNode list = r.m_Json.Get("friends");
		if (list && list.IsArr())
		{
			foreach (DZS_SrcNode n : list.m_Items)
			{
				DZS_DzbotFriend f = Take(n);
				if (f)
					m_List.Insert(f);
			}
		}
		string names = "";
		foreach (DZS_DzbotFriend lf : m_List)
			names += " " + lf.m_Id + ":" + lf.m_Name + ":" + lf.m_Always;
		DZS_Log.Info("dzbot: friends " + m_List.Count() + " of " + m_Max + names);
	}

	protected static DZS_DzbotFriend Take(DZS_SrcNode n)
	{
		if (!n || !n.IsObj())
			return null;
		DZS_SrcNode id = n.Get("id");
		DZS_SrcNode nm = n.Get("name");
		DZS_SrcNode aj = n.Get("alwaysJoin");
		if (!id)
			return null;
		DZS_DzbotFriend f = new DZS_DzbotFriend();
		f.m_Id = id.m_Str.ToInt();
		if (nm && nm.IsStr())
			f.m_Name = nm.m_Str;
		f.m_Always = false;
		if (aj && aj.IsBool())
			f.m_Always = aj.m_Bool;
		return f;
	}

	void OnSet(DZS_DzbotReply r, DZS_DzbotFriendSet q)
	{
		if (q && q.m_Add)
		{
			int due = q.m_StartMs + ADD_MIN_MS;
			if (DZS_Kernel.NowMs() < due)
			{
				DZS_DzbotHeldSet h = new DZS_DzbotHeldSet();
				h.m_R = r;
				h.m_Q = q;
				h.m_DueMs = due;
				m_Held.Insert(h);
				return;
			}
		}
		Apply(r, q);
	}

	protected void Apply(DZS_DzbotReply r, DZS_DzbotFriendSet q)
	{
		m_Rev++;
		if (!q)
			return;
		if (q.m_Add)
			DZS_Log.Info("dzbot: friends/set " + q.m_Id + " add shown " + (DZS_Kernel.NowMs() - q.m_StartMs) + " ms");
		if (q.m_Add)
			m_Adding.Remove(q.m_Id);
		DZS_DzbotFriend f = Find(q.m_Id);
		if (r.m_Framed && r.Ok())
		{
			DZS_DzbotFriend got = Take(r.m_Json.Get("friend"));
			if (got)
			{
				if (f)
				{
					f.m_Always = got.m_Always;
					f.m_Name = got.m_Name;
					f.m_Busy = false;
				}
				else
					InsertSorted(got);
			}
			DZS_Log.Info("dzbot: friends/set " + q.m_Id + " done, added " + r.Str("added"));
			return;
		}
		if (f && !q.m_Add)
		{
			f.m_Always = q.m_Was;
			f.m_Busy = false;
		}
		string de, en;
		Reason(r, de, en);
		string t = DZS_Texts.Pick("Freundesliste nicht geändert: " + de, "Friends list not changed: " + en);
		m_Note = t;
		DZS_Log.Warn("dzbot: friends/set " + q.m_Id + " failed: " + r.m_Fail + r.Error());
		m_P.Kernel().GetScreen().ShowMessage("dzbot.friends", "", DZS_W.Literal(t), 2.0);
	}

	static void Reason(DZS_DzbotReply r, out string de, out string en)
	{
		string err = r.Error();
		if (!r.m_Framed)
		{
			de = "dzbot nicht erreichbar";
			en = "dzbot not reachable";
		}
		else if (err == "friends_full")
		{
			de = "Höchstens 200 Freunde — auf dzbot.de aufräumen";
			en = "At most 200 friends — tidy up on dzbot.de";
		}
		else if (err == "rate_limited")
		{
			de = "Zu viele neue Freunde — versuch es später";
			en = "Too many new friends — try again later";
		}
		else if (err == "not_found")
		{
			de = "Dieses Konto gibt es bei dzbot nicht mehr";
			en = "This account no longer exists at dzbot";
		}
		else if (err == "bad_request")
		{
			de = "dzbot hat die Anfrage abgelehnt";
			en = "dzbot rejected the request";
		}
		else
		{
			de = r.Message();
			en = r.Message();
		}
	}

	protected void InsertSorted(DZS_DzbotFriend f)
	{
		string key = f.m_Name;
		key.ToLower();
		int at = 0;
		while (at < m_List.Count())
		{
			string other = m_List[at].m_Name;
			other.ToLower();
			if (other > key)
				break;
			at++;
		}
		m_List.InsertAt(f, at);
	}

	void OpenManage()
	{
		if (m_ManageUrl != "")
			GetGame().OpenURL(m_ManageUrl);
	}
}

// A sentence broken into lines measured in its font, for a width known only at layout time.
class DZS_DzbotWrap : Managed
{
	protected Widget m_Parent;
	protected string m_Role;
	protected float m_Size;
	protected int m_Color;
	protected string m_Text;
	protected float m_Width = -1;
	protected ref array<TextWidget> m_Lines = new array<TextWidget>();
	protected bool m_Hidden;

	void DZS_DzbotWrap(Widget parent, string role, float size, int color, string text)
	{
		m_Parent = parent;
		m_Role = role;
		m_Size = size;
		m_Color = color;
		m_Text = text;
	}

	protected TextWidget NewLine()
	{
		TextWidget t = DZS_W.Text(m_Parent, m_Role, m_Size, m_Color);
		t.Show(!m_Hidden);
		m_Lines.Insert(t);
		return t;
	}

	void SetText(string text)
	{
		if (text == m_Text)
			return;
		m_Text = text;
		m_Width = -1;
	}

	void Show(bool s)
	{
		m_Hidden = !s;
		foreach (TextWidget t : m_Lines)
			t.Show(s);
	}

	float Place(float x, float y, float width, float lineH)
	{
		if (width != m_Width)
		{
			m_Width = width;
			foreach (TextWidget old : m_Lines)
				old.Unlink();
			m_Lines.Clear();
			array<string> words = new array<string>();
			m_Text.Split(" ", words);
			TextWidget cur = NewLine();
			string line = "";
			foreach (string w : words)
			{
				string tryLine = w;
				if (line != "")
					tryLine = line + " " + w;
				cur.SetText(tryLine);
				if (line != "" && DZS_W.TextWidth(cur) > width)
				{
					cur.SetText(line);
					cur = NewLine();
					cur.SetText(w);
					line = w;
				}
				else
					line = tryLine;
			}
		}
		float yy = y;
		foreach (TextWidget t : m_Lines)
		{
			DZS_W.Place(t, x, yy, width, lineH);
			yy += lineH;
		}
		return yy - y;
	}

	void Destroy()
	{
		foreach (TextWidget t : m_Lines)
			t.Unlink();
		m_Lines.Clear();
	}
}

// One row of the friends list in the settings tab: name, switch "may always join".
class DZS_DzbotFriendRow : DZS_HitListener
{
	DZS_DzbotFriends m_F;
	int m_Id;
	ref DZS_Hit m_Hit;
	TextWidget m_Name;
	TextWidget m_Label;
	ref DZS_Toggle m_Toggle;

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			Flip();
	}

	void Flip()
	{
		DZS_DzbotFriend f = m_F.Find(m_Id);
		if (f && !f.m_Busy)
			m_F.SetAlways(m_Id, !f.m_Always);
	}

	void OnToggle(DZS_Toggle t, bool on)
	{
		DZS_DzbotFriend f = m_F.Find(m_Id);
		if (f && !f.m_Busy)
			m_F.SetAlways(m_Id, on);
	}

	void Destroy()
	{
		if (m_Toggle)
			m_Toggle.m_Hit.Destroy();
		if (m_Hit)
			m_Hit.Destroy();
	}
}

// Settings › dzbot: connection line, then "Friends for editing together".
class DZS_DzbotPage : DZS_SettingsPage
{
	DZS_DzbotPlugin m_P;
	protected Widget m_Root;
	protected TextWidget m_Status;
	protected ref DZS_Button m_Connect;
	protected TextWidget m_Head;
	protected ref DZS_DzbotWrap m_Lead;
	protected TextWidget m_Info;
	protected TextWidget m_Count;
	protected TextWidget m_Note;
	protected ref DZS_Button m_Manage;
	protected ref array<ref DZS_DzbotFriendRow> m_Rows = new array<ref DZS_DzbotFriendRow>();
	protected int m_Rev = -1;
	protected bool m_Connected;
	protected DZS_Dialog m_Dialog;

	override void Build(Widget page, DZS_Dialog dialog)
	{
		m_Dialog = dialog;
		m_Root = DZS_W.Frame(page);
		m_Status = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_Connect = new DZS_Button(m_Root, "side", "");
		m_Connect.m_OnClick.Insert(OnConnect);
		m_Head = DZS_W.Text(m_Root, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Head.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Freunde für gemeinsames Bearbeiten", "Friends for editing together")));
		m_Lead = new DZS_DzbotWrap(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textMuted"), DZS_Texts.Pick("Deine Freunde stehen beim Einladen zur Auswahl. Wer ‚darf immer beitreten‘ hat, kommt ohne Einladung in Sitzungen, die du mit einem lokalen Projekt startest.", "Your friends are listed when you invite. Anyone with ‘may always join’ can join sessions you start with a local project without an invitation."));
		m_Info = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Count = DZS_W.Text(m_Root, DZS_Font.MONO, 11, DZS_Color.Get("textQuiet"));
		m_Note = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("danger"));
		m_Manage = new DZS_Button(m_Root, "side", DZS_Texts.Pick("Auf dzbot.de bearbeiten", "Edit on dzbot.de"));
		m_Manage.m_OnClick.Insert(OnManage);
		m_Connected = m_P.IsConnected();
		m_P.m_Friends.Load();
	}

	void OnConnect(DZS_Button b)
	{
		if (m_P.IsConnected())
			m_P.AskDisconnect();
		else
			m_P.StartConnect();
	}

	void OnManage(DZS_Button b) { m_P.m_Friends.OpenManage(); }

	protected void BuildRows()
	{
		foreach (DZS_DzbotFriendRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		DZS_DzbotFriends fr = m_P.m_Friends;
		if (!m_P.IsConnected() || fr.m_State != "ok")
			return;
		foreach (DZS_DzbotFriend f : fr.m_List)
		{
			DZS_DzbotFriendRow row = new DZS_DzbotFriendRow();
			row.m_F = fr;
			row.m_Id = f.m_Id;
			row.m_Hit = new DZS_Hit(m_Root, row, "friend");
			row.m_Name = DZS_W.Text(row.m_Hit.m_W, DZS_Font.READ, 13, DZS_Color.Get("text"));
			row.m_Label = DZS_W.Text(row.m_Hit.m_W, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
			row.m_Label.SetText(DZS_Texts.Pick("darf immer beitreten", "may always join"));
			row.m_Toggle = new DZS_Toggle(row.m_Hit.m_W);
			row.m_Toggle.m_OnChange.Insert(row.OnToggle);
			m_Rows.Insert(row);
		}
	}

	override void Update(float dt)
	{
		bool conn = m_P.IsConnected();
		if (conn != m_Connected)
		{
			m_Connected = conn;
			m_P.m_Friends.Load();
		}
		if (conn)
		{
			string who = m_P.m_UserName;
			m_Status.SetText(DZS_W.Literal(DZS_Texts.Pick("Verbunden als ", "Connected as ") + who));
			m_Connect.SetLabel(DZS_Texts.Pick("Trennen", "Disconnect"));
		}
		else
		{
			m_Status.SetText(DZS_Texts.Pick("Nicht angemeldet", "Not signed in"));
			m_Connect.SetLabel(DZS_Texts.Pick("Verbinden …", "Connect …"));
		}
		DZS_DzbotFriends fr = m_P.m_Friends;
		if (fr.m_Rev != m_Rev)
		{
			bool rebuild = m_Rows.Count() != fr.m_List.Count() || !conn || fr.m_State != "ok";
			if (!rebuild)
			{
				for (int k = 0; k < m_Rows.Count(); k++)
				{
					if (m_Rows[k].m_Id != fr.m_List[k].m_Id)
						rebuild = true;
				}
			}
			m_Rev = fr.m_Rev;
			if (rebuild)
			{
				BuildRows();
				if (m_Dialog)
					m_Dialog.Layout();
			}
		}
		string info = "";
		if (!conn)
			info = DZS_Texts.Pick("Mit dzbot verbinden, um deine Freunde zu sehen", "Connect with dzbot to see your friends");
		else if (fr.m_State == "loading")
			info = DZS_Texts.Pick("Lädt …", "Loading …");
		else if (fr.m_State == "failed")
			info = DZS_Texts.Pick("Freunde nicht geladen: dzbot nicht erreichbar", "Friends not loaded: dzbot not reachable");
		else if (fr.m_State == "ok" && fr.m_List.Count() == 0)
			info = DZS_Texts.Pick("Noch keine Freunde", "No friends yet");
		m_Info.SetText(DZS_W.Literal(info));
		m_Info.Show(info != "");
		bool listed = conn && fr.m_State == "ok";
		string cnt = "";
		if (listed)
			cnt = DZS_Num.Count(fr.m_List.Count()) + DZS_Texts.Pick(" von ", " of ") + DZS_Num.Count(fr.m_Max);
		m_Count.SetText(DZS_W.Literal(cnt));
		m_Count.Show(listed);
		m_Manage.Show(listed && fr.m_ManageUrl != "");
		m_Note.SetText(DZS_W.Literal(fr.m_Note));
		foreach (DZS_DzbotFriendRow row : m_Rows)
		{
			DZS_DzbotFriend f = fr.Find(row.m_Id);
			if (!f)
				continue;
			row.m_Name.SetText(DZS_W.Literal(f.m_Name));
			if (row.m_Toggle.IsOn() != f.m_Always)
				row.m_Toggle.SetOn(f.m_Always);
			row.m_Toggle.m_Hit.SetEnabled(!f.m_Busy);
			row.m_Hit.SetEnabled(!f.m_Busy);
			float a = 1.0;
			if (f.m_Busy)
				a = 0.45;
			row.m_Toggle.m_Hit.m_W.SetAlpha(a);
		}
	}

	override float Layout(float width)
	{
		if (!m_Root)
			return 0;
		float y = 0;
		float cw = m_Connect.GetWidth();
		DZS_W.Place(m_Status, 0, DZS_Scale.Px(4), width - cw - DZS_Scale.Px(12), DZS_Scale.Px(20));
		m_Connect.Place(width - cw, 0);
		y += DZS_Scale.Px(44);
		DZS_W.Place(m_Head, 0, y, width, DZS_Scale.Px(14));
		y += DZS_Scale.Px(20);
		y += m_Lead.Place(0, y, width, DZS_Scale.Px(17)) + DZS_Scale.Px(8);
		if (m_Info.IsVisible())
		{
			DZS_W.Place(m_Info, 0, y, width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(28);
		}
		float rowH = DZS_Scale.Px(34);
		foreach (DZS_DzbotFriendRow row : m_Rows)
		{
			row.m_Hit.Place(0, y, width, rowH);
			DZS_W.Place(row.m_Name, DZS_Scale.Px(4), DZS_Scale.Px(7), width * 0.5, DZS_Scale.Px(20));
			float tw = DZS_Toggle.Width();
			row.m_Toggle.Place(width - tw - DZS_Scale.Px(4), (rowH - DZS_Toggle.Height()) * 0.5);
			float lw = DZS_Scale.Px(170);
			DZS_W.Place(row.m_Label, width - tw - DZS_Scale.Px(14) - lw, DZS_Scale.Px(8), lw, DZS_Scale.Px(18));
			y += rowH;
		}
		if (m_Rows.Count() > 0)
			y += DZS_Scale.Px(6);
		DZS_W.Place(m_Note, 0, y, width, DZS_Scale.Px(18));
		if (m_P.m_Friends.m_Note != "")
			y += DZS_Scale.Px(24);
		DZS_W.Place(m_Count, 0, y + DZS_Scale.Px(8), width * 0.4, DZS_Scale.Px(16));
		float mw = m_Manage.GetWidth();
		m_Manage.Place(width - mw, y);
		y += DZS_Scale.Px(40);
		DZS_W.Place(m_Root, 0, 0, width, y);
		return y;
	}

	override void Destroy()
	{
		foreach (DZS_DzbotFriendRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		if (m_Connect)
			m_Connect.Destroy();
		if (m_Manage)
			m_Manage.Destroy();
		if (m_Root)
			m_Root.Unlink();
		m_Root = null;
	}
}

// One participant in the dialog "Friends".
class DZS_DzbotPeerRow : DZS_HitListener
{
	DZS_DzbotFriendsDialog m_D;
	string m_Pid;
	int m_Id;
	ref DZS_Hit m_Hit;
	Widget m_Swatch;
	TextWidget m_Name;
	TextWidget m_Mark;
	TextWidget m_Label;
	ref DZS_Toggle m_Toggle;
	ref DZS_Button m_Add;
	ref array<Widget> m_Blocks = new array<Widget>();

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_D.Flip(m_Id);
	}

	void OnToggle(DZS_Toggle t, bool on) { m_D.SetAlways(m_Id, on); }
	void OnAdd(DZS_Button b) { m_D.AddFriend(m_Id); }

	void Destroy()
	{
		if (m_Toggle)
			m_Toggle.m_Hit.Destroy();
		if (m_Add)
			m_Add.Destroy();
		if (m_Hit)
			m_Hit.Destroy();
	}
}

// "Friends …" in a session on the dzbot server: the other participants with "Add as friend" or the switch.
class DZS_DzbotFriendsDialog : DZS_Dialog
{
	DZS_DzbotPlugin m_P;
	protected Widget m_In;
	protected ref DZS_DzbotWrap m_Lead;
	protected TextWidget m_Empty;
	protected ref array<ref DZS_DzbotPeerRow> m_Rows = new array<ref DZS_DzbotPeerRow>();
	protected int m_PeopleRev = -1;
	protected int m_FriendsRev = -1;
	protected float m_T;

	void DZS_DzbotFriendsDialog(DZS_DzbotPlugin p)
	{
		m_P = p;
		Setup(SMALL, DZS_Texts.Pick("Gemeinsam", "Together"), DZS_Texts.Pick("Freunde", "Friends"), false);
		AddFootButton("manage", DZS_Texts.Pick("Auf dzbot.de bearbeiten", "Edit on dzbot.de"), "side");
		AddFootButton("close", DZS_Texts.Get("core.dialog.closeButton"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_In = inner;
		m_Lead = new DZS_DzbotWrap(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"), DZS_Texts.Pick("Deine Freunde stehen beim Einladen zur Auswahl. Wer ‚darf immer beitreten‘ hat, kommt ohne Einladung in Sitzungen, die du mit einem lokalen Projekt startest.", "Your friends are listed when you invite. Anyone with ‘may always join’ can join sessions you start with a local project without an invitation."));
		m_Empty = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Empty.SetText(DZS_Texts.Pick("Noch niemand außer dir in der Sitzung.", "No one else is in the session yet."));
		m_P.m_Friends.Load();
		Rebuild();
		AddFootFocus();
	}

	void Flip(int id)
	{
		DZS_DzbotFriend f = m_P.m_Friends.Find(id);
		if (f && !f.m_Busy)
			m_P.m_Friends.SetAlways(id, !f.m_Always);
	}

	void SetAlways(int id, bool on)
	{
		DZS_DzbotFriend f = m_P.m_Friends.Find(id);
		if (f && !f.m_Busy)
			m_P.m_Friends.SetAlways(id, on);
	}

	void AddFriend(int id)
	{
		if (id > 0)
			m_P.m_Friends.Add(id);
	}

	protected void Rebuild()
	{
		foreach (DZS_DzbotPeerRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		m_PeopleRev = DZS_SessionPeople.Rev();
		m_FriendsRev = m_P.m_Friends.m_Rev;
		string me = DZS_SessionPeople.Me();
		array<DZS_SessionMember> people = DZS_SessionPeople.Members();
		foreach (DZS_SessionMember m : people)
		{
			if (m.m_Pid == me)
				continue;
			DZS_DzbotPeerRow row = new DZS_DzbotPeerRow();
			row.m_D = this;
			row.m_Pid = m.m_Pid;
			row.m_Id = m_P.AccountOf(m.m_UserId);
			row.m_Hit = new DZS_Hit(m_In, row, "peer");
			row.m_Swatch = DZS_W.Panel(row.m_Hit.m_W, m.m_Color);
			row.m_Name = DZS_W.Text(row.m_Hit.m_W, DZS_Font.READ, 13, DZS_Color.Get("text"));
			row.m_Name.SetText(DZS_W.Literal(m.m_Name));
			row.m_Mark = DZS_W.Text(row.m_Hit.m_W, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
			row.m_Mark.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Freund", "friend")));
			row.m_Label = DZS_W.Text(row.m_Hit.m_W, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
			row.m_Label.SetText(DZS_Texts.Pick("darf immer beitreten", "may always join"));
			row.m_Toggle = new DZS_Toggle(row.m_Hit.m_W);
			row.m_Toggle.m_OnChange.Insert(row.OnToggle);
			row.m_Add = new DZS_Button(row.m_Hit.m_W, "ghost", DZS_Texts.Pick("Zum Freund machen", "Add as friend"), "", 24);
			row.m_Add.m_OnClick.Insert(row.OnAdd);
			for (int i = 0; i < 4; i++)
				row.m_Blocks.Insert(DZS_W.Panel(row.m_Hit.m_W, DZS_Color.Get("blockEmpty")));
			m_Rows.Insert(row);
		}
		ClearFocusables();
		foreach (DZS_DzbotPeerRow r2 : m_Rows)
		{
			AddFocusHit(r2.m_Add.m_Hit);
			AddFocusHit(r2.m_Toggle.m_Hit);
		}
		AddFootFocus();
		Refresh();
	}

	protected void Refresh()
	{
		DZS_DzbotFriends fr = m_P.m_Friends;
		foreach (DZS_DzbotPeerRow row : m_Rows)
		{
			DZS_DzbotFriend f = fr.Find(row.m_Id);
			bool adding = fr.m_Adding.Contains(row.m_Id);
			bool known = f != null;
			row.m_Mark.Show(known);
			row.m_Toggle.m_Hit.m_W.Show(known);
			row.m_Label.Show(known);
			row.m_Add.Show(!known);
			row.m_Hit.SetEnabled(known && !f.m_Busy);
			if (known)
			{
				if (row.m_Toggle.IsOn() != f.m_Always)
					row.m_Toggle.SetOn(f.m_Always);
				row.m_Toggle.m_Hit.SetEnabled(!f.m_Busy);
				float a = 1.0;
				if (f.m_Busy)
					a = 0.45;
				row.m_Toggle.m_Hit.m_W.SetAlpha(a);
			}
			else
			{
				if (adding)
					row.m_Add.SetLabel(DZS_Texts.Pick("Wird hinzugefügt …", "Adding …"));
				else
					row.m_Add.SetLabel(DZS_Texts.Pick("Zum Freund machen", "Add as friend"));
				string why = "";
				bool on = !adding && fr.m_State == "ok" && row.m_Id > 0;
				if (fr.Full())
				{
					on = false;
					why = DZS_Texts.Pick("Höchstens 200 Freunde — auf dzbot.de aufräumen", "At most 200 friends — tidy up on dzbot.de");
				}
				else if (fr.m_State == "failed")
					why = DZS_Texts.Pick("Freunde nicht geladen: dzbot nicht erreichbar", "Friends not loaded: dzbot not reachable");
				else if (row.m_Id <= 0)
					why = DZS_Texts.Pick("Kein dzbot-Konto", "No dzbot account");
				row.m_Add.SetEnabled(on, why);
			}
			foreach (Widget b : row.m_Blocks)
				b.Show(adding);
		}
		m_Empty.Show(m_Rows.Count() == 0);
		DZS_Button mg = FootButton("manage");
		if (mg)
			mg.SetEnabled(fr.m_ManageUrl != "", DZS_Texts.Pick("Freunde nicht geladen", "Friends not loaded"));
	}

	override void UpdateContent(float dt)
	{
		m_T += dt;
		if (m_PeopleRev != DZS_SessionPeople.Rev())
		{
			Rebuild();
			Layout();
		}
		else if (m_FriendsRev != m_P.m_Friends.m_Rev)
		{
			m_FriendsRev = m_P.m_Friends.m_Rev;
			Refresh();
			Layout();
		}
		float bs = DZS_Scale.Px(8);
		int lit = Math.Floor(m_T / 0.2);
		lit = lit % 4;
		foreach (DZS_DzbotPeerRow row : m_Rows)
		{
			for (int i = 0; i < row.m_Blocks.Count(); i++)
			{
				if (i == lit)
					row.m_Blocks[i].SetColor(DZS_Color.Get("accent"));
				else
					row.m_Blocks[i].SetColor(DZS_Color.Get("blockEmpty"));
			}
		}
		if (!DZS_SessionPeople.Server() || DZS_SessionPeople.Server() == "")
			Close();
	}

	override float LayoutContent(float width)
	{
		float y = m_Lead.Place(0, 0, width, DZS_Scale.Px(19)) + DZS_Scale.Px(10);
		if (m_Rows.Count() == 0)
		{
			DZS_W.Place(m_Empty, 0, y, width, DZS_Scale.Px(20));
			return y + DZS_Scale.Px(28);
		}
		float rowH = DZS_Scale.Px(36);
		float sw = DZS_Scale.Px(12);
		foreach (DZS_DzbotPeerRow row : m_Rows)
		{
			row.m_Hit.Place(0, y, width, rowH);
			DZS_W.Place(row.m_Swatch, 0, DZS_Scale.Px(12), sw, sw);
			DZS_W.Place(row.m_Name, sw + DZS_Scale.Px(8), DZS_Scale.Px(8), DZS_Scale.Px(140), DZS_Scale.Px(20));
			float nw = DZS_W.TextWidth(row.m_Name);
			DZS_W.Place(row.m_Mark, sw + DZS_Scale.Px(16) + nw, DZS_Scale.Px(11), DZS_Scale.Px(80), DZS_Scale.Px(16));
			float tw = DZS_Toggle.Width();
			row.m_Toggle.Place(width - tw, (rowH - DZS_Toggle.Height()) * 0.5);
			float lw = DZS_Scale.Px(150);
			DZS_W.Place(row.m_Label, width - tw - DZS_Scale.Px(10) - lw, DZS_Scale.Px(9), lw, DZS_Scale.Px(18));
			float aw = row.m_Add.GetWidth();
			row.m_Add.Place(width - aw, DZS_Scale.Px(6));
			float bx = width - aw - DZS_Scale.Px(8) - 4 * DZS_Scale.Px(12);
			for (int i = 0; i < row.m_Blocks.Count(); i++)
				DZS_W.Place(row.m_Blocks[i], bx + i * DZS_Scale.Px(12), DZS_Scale.Px(14), DZS_Scale.Px(8), DZS_Scale.Px(8));
			y += rowH;
		}
		return y + DZS_Scale.Px(4);
	}

	override void OnFoot(string tag)
	{
		if (tag == "manage")
		{
			m_P.m_Friends.OpenManage();
			return;
		}
		Close();
	}

	override void OnClosed()
	{
		foreach (DZS_DzbotPeerRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		if (m_P.m_FriendsDialog == this)
			m_P.m_FriendsDialog = null;
	}
}
