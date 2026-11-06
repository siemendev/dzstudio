// Plate at the pointer: first line bold, up to four lines below with key caps.
class DZS_PointerHint : Managed
{
	protected Widget m_Frame;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected TextWidget m_Title;
	protected ref array<ref DZS_RichLine> m_Lines;
	protected string m_Sig;

	void DZS_PointerHint(Widget parent)
	{
		m_Frame = DZS_W.Frame(parent);
		m_Bg = DZS_W.Panel(m_Frame, DZS_Color.Get("plate"));
		m_Border = new DZS_Border(m_Frame, DZS_Color.Get("lineStrong"));
		m_Title = DZS_W.Text(m_Frame, DZS_Font.READBOLD, 13, DZS_Color.Get("text"));
		m_Lines = new array<ref DZS_RichLine>();
		for (int i = 0; i < 4; i++)
			m_Lines.Insert(new DZS_RichLine(m_Frame, DZS_Font.READ, 13, DZS_Color.Get("textMuted")));
		m_Frame.Show(false);
	}

	void Hide()
	{
		m_Frame.Show(false);
	}

	// At (x, y): right below the pointer with 20 px offset, or at the given point without offset.
	void Show(string title, array<string> lines, float x, float y, bool offset = true)
	{
		bool keys = DZS_Kernel.Get().GetSettings().GetBool("core.pointerHints", true);
		array<string> use = new array<string>();
		foreach (string l : lines)
		{
			if (keys || !l.Contains("["))
				use.Insert(l);
		}
		lines = use;
		keys = true;
		string sig = title;
		foreach (string l2 : lines)
			sig += "|" + l2;
		float pad = DZS_Scale.Px(8);
		float lineH = DZS_Font.WidgetHeight(DZS_Font.READ, 13) + DZS_Scale.Px(4);
		if (sig != m_Sig)
		{
			m_Sig = sig;
			m_Title.SetText(DZS_W.Literal(title));
			for (int i = 0; i < 4; i++)
			{
				string t = "";
				if (i < lines.Count() && keys)
					t = lines[i];
				m_Lines[i].Set(t);
				m_Lines[i].Show(t != "");
			}
		}
		float w = DZS_W.TextWidth(m_Title);
		int shown = 0;
		for (int k = 0; k < 4; k++)
		{
			if (k < lines.Count() && keys && lines[k] != "")
			{
				w = Math.Max(w, m_Lines[k].GetWidth());
				shown++;
			}
		}
		w += 2 * pad;
		float h = pad * 2 + lineH * (1 + shown);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		m_Border.Place(0, 0, w, h);
		DZS_W.Place(m_Title, pad, pad, w - pad, lineH);
		float ly = pad + lineH;
		for (int j = 0; j < 4; j++)
		{
			if (j < lines.Count() && keys && lines[j] != "")
			{
				m_Lines[j].Place(pad, ly + (lineH - m_Lines[j].GetHeight()) / 2);
				ly += lineH;
			}
		}
		int sw, sh;
		GetScreenSize(sw, sh);
		float off = 0;
		if (offset)
			off = DZS_Scale.Px(20);
		float px = x + off;
		float py = y + off;
		if (px + w > sw)
			px = x - off - w;
		if (py + h > sh)
			py = y - off - h;
		m_Frame.SetPos(px, py);
		m_Frame.SetSize(w, h);
		m_Frame.Show(true);
	}
}
