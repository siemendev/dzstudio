// Content of a sign-in by code: the code large,
// the address short, "Copy code" / "Copy link", the state line with block indicator and the browser hint.
class DZS_SignInCode : Managed
{
	protected TextWidget m_Lead;
	protected TextWidget m_Code;
	protected TextWidget m_Url;
	protected TextWidget m_State;
	protected TextWidget m_Opened;
	protected ref DZS_Button m_CopyCode;
	protected ref DZS_Button m_CopyLink;
	protected ref array<Widget> m_Blocks = new array<Widget>();
	protected string m_ShownCode;
	protected string m_FullUrl;
	protected bool m_Waiting;
	protected float m_T;

	void DZS_SignInCode(Widget inner, string lead)
	{
		if (lead != "")
		{
			m_Lead = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
			m_Lead.SetText(DZS_W.Literal(lead));
		}
		m_Code = DZS_W.Text(inner, DZS_Font.DISPLAY, 32, DZS_Color.Get("text"));
		m_Url = DZS_W.Text(inner, DZS_Font.MONO, 12, DZS_Color.Get("textMuted"));
		m_CopyCode = new DZS_Button(inner, "secondary", DZS_Texts.Pick("Code kopieren", "Copy code"), "copy", 28);
		m_CopyCode.m_OnClick.Insert(OnCopyCode);
		m_CopyLink = new DZS_Button(inner, "secondary", DZS_Texts.Pick("Link kopieren", "Copy link"), "copy", 28);
		m_CopyLink.m_OnClick.Insert(OnCopyLink);
		m_State = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Opened = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_Opened.SetText(DZS_W.Literal(DZS_Texts.Pick("Im Browser geöffnet — mit Alt+Tab wechseln", "Opened in the browser — switch with Alt+Tab")));
		m_Opened.Show(false);
		for (int i = 0; i < 4; i++)
			m_Blocks.Insert(DZS_W.Panel(inner, DZS_Color.Get("blockEmpty")));
	}

	// Code as XXXX-XXXX when it comes as eight characters without a dash.
	static string Format(string code)
	{
		if (code.Length() == 8 && code.IndexOf("-") < 0)
			return code.Substring(0, 4) + "-" + code.Substring(4, 4);
		return code;
	}

	// Address without scheme and without the part from "?" on (dzbot.de/editor/connect).
	static string Short(string url)
	{
		string u = url;
		if (u.IndexOf("https://") == 0)
			u = u.Substring(8, u.Length() - 8);
		else if (u.IndexOf("http://") == 0)
			u = u.Substring(7, u.Length() - 7);
		int q = u.IndexOf("?");
		if (q >= 0)
			u = u.Substring(0, q);
		return u;
	}

	void Set(string code, string fullUrl, string state, bool waiting, bool opened)
	{
		m_ShownCode = Format(code);
		m_FullUrl = fullUrl;
		m_Waiting = waiting;
		string c = m_ShownCode;
		if (c == "")
			c = "…";
		m_Code.SetText(DZS_W.Literal(c));
		m_Url.SetText(DZS_W.Literal(Short(fullUrl)));
		m_State.SetText(DZS_W.Literal(state));
		m_Opened.Show(opened);
		m_CopyCode.SetEnabled(m_ShownCode != "", DZS_Texts.Pick("Noch kein Code", "No code yet"));
		m_CopyLink.SetEnabled(m_FullUrl != "", DZS_Texts.Pick("Noch kein Code", "No code yet"));
	}

	void Tick(float dt)
	{
		m_T += dt;
		int lit = m_T * 4;
		lit = lit % 4;
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			m_Blocks[i].Show(m_Waiting);
			if (i == lit)
				m_Blocks[i].SetColor(DZS_Color.Get("accent"));
			else
				m_Blocks[i].SetColor(DZS_Color.Get("blockEmpty"));
		}
	}

	float Layout(float width)
	{
		float y = 0;
		if (m_Lead)
		{
			DZS_W.Place(m_Lead, 0, y, width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(28);
		}
		DZS_W.Place(m_Code, 0, y, width, DZS_Scale.Px(44));
		y += DZS_Scale.Px(50);
		DZS_W.Place(m_Url, 0, y, width, DZS_Scale.Px(18));
		y += DZS_Scale.Px(28);
		m_CopyCode.Place(0, y);
		m_CopyLink.Place(m_CopyCode.GetWidth() + DZS_Scale.Px(8), y);
		y += DZS_Scale.Px(40);
		for (int i = 0; i < m_Blocks.Count(); i++)
			DZS_W.Place(m_Blocks[i], i * DZS_Scale.Px(12), y + DZS_Scale.Px(6), DZS_Scale.Px(8), DZS_Scale.Px(8));
		float bx = DZS_Scale.Px(56);
		DZS_W.Place(m_State, bx, y, width - bx, DZS_Scale.Px(20));
		y += DZS_Scale.Px(26);
		DZS_W.Place(m_Opened, 0, y, width, DZS_Scale.Px(20));
		y += DZS_Scale.Px(26);
		return y;
	}

	void OnCopyCode(DZS_Button b)
	{
		if (m_ShownCode == "")
			return;
		GetGame().CopyToClipboard(m_ShownCode);
		Copied("code");
	}

	void OnCopyLink(DZS_Button b)
	{
		if (m_FullUrl == "")
			return;
		GetGame().CopyToClipboard(m_FullUrl);
		Copied("link");
	}

	protected void Copied(string what)
	{
		DZS_Kernel.Get().GetScreen().ShowMessage("signin.copied", "success", DZS_Texts.Pick("Kopiert", "Copied"), 2);
		DZS_Log.Info("sign-in: " + what + " copied");
	}
}
