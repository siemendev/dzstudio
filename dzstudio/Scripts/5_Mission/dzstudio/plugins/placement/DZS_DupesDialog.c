// Clean up duplicates: one row per set of twins with "Show", the dialog stays open.
class DZS_DupesDialog : DZS_Dialog
{
	protected ref array<string> m_Labels;
	protected ref array<TextWidget> m_Texts = new array<TextWidget>();
	protected ref array<ref DZS_Button> m_Show = new array<ref DZS_Button>();
	protected TextWidget m_Body;
	ref ScriptInvoker m_OnShow = new ScriptInvoker();

	void DZS_DupesDialog(string kicker, string title, array<string> labels)
	{
		m_Labels = labels;
		Setup(MEDIUM, kicker, title, true);
	}

	override void BuildContent(Widget inner)
	{
		m_Body = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("textQuiet"));
		m_Body.SetText(DZS_Texts.Pick("Je Zeile ein behaltenes Objekt mit den überzähligen, die genau darauf stehen.", "Each row: one kept object with the surplus ones standing right on it."));
		for (int i = 0; i < m_Labels.Count(); i++)
		{
			TextWidget t = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
			t.SetText(m_Labels[i]);
			m_Texts.Insert(t);
			DZS_Button b = new DZS_Button(inner, "side", DZS_Texts.Pick("Zeigen", "Show"));
			b.m_Tag = i.ToString();
			b.m_OnClick.Insert(OnShowClick);
			m_Show.Insert(b);
			AddFocusHit(b.m_Hit);
		}
		AddFootFocus();
	}

	void OnShowClick(DZS_Button b)
	{
		m_OnShow.Invoke(b.m_Tag.ToInt());
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		DZS_W.Place(m_Body, 0, y, width, DZS_Scale.Px(24));
		y += DZS_Scale.Px(34);
		for (int i = 0; i < m_Texts.Count(); i++)
		{
			DZS_Button b = m_Show[i];
			float rowH = Math.Max(DZS_Scale.Px(32), b.GetHeight() + DZS_Scale.Px(4));
			DZS_W.Place(m_Texts[i], 0, y + (rowH - DZS_Scale.Px(24)) / 2, width - b.GetWidth() - DZS_Scale.Px(12), DZS_Scale.Px(24));
			b.Place(width - b.GetWidth(), y + (rowH - b.GetHeight()) / 2);
			y += rowH;
		}
		return y;
	}
}
