// One cell of a dialog line; width in reference pixels, 0 = a share of the rest, -1 = its own width.
class DZS_RCell : Managed
{
	TextWidget m_Text;
	string m_Full;
	DZS_TextField m_Field;
	DZS_Button m_Button;
	DZS_Toggle m_Toggle;
	float m_W;
}

class DZS_RLine : Managed
{
	ref array<ref DZS_RCell> m_Cells = new array<ref DZS_RCell>();
	float m_H = 28;
	float m_Before;
}

// Dialog of lines of cells, placed left to right; knobs, switches and pickers report by tag (the dzbot rules,
// the generic extension fields).
class DZS_LinesDialog : DZS_Dialog
{
	protected Widget m_Parent;
	protected ref array<ref DZS_RLine> m_Lines = new array<ref DZS_RLine>();
	protected ref array<ref DZS_TextField> m_Fields = new array<ref DZS_TextField>();
	protected ref array<ref DZS_Button> m_Knobs = new array<ref DZS_Button>();
	protected ref array<ref DZS_Toggle> m_Toggles = new array<ref DZS_Toggle>();
	protected ref array<string> m_ToggleTags = new array<string>();
	protected string m_PickWhat;

	protected DZS_RLine Line(float before = 0, float height = 28)
	{
		DZS_RLine l = new DZS_RLine();
		l.m_Before = before;
		l.m_H = height;
		m_Lines.Insert(l);
		return l;
	}

	protected TextWidget Label(DZS_RLine l, string text, float w = -1, string tone = "text", float size = 13)
	{
		DZS_RCell c = new DZS_RCell();
		c.m_Text = DZS_W.Text(m_Parent, DZS_Font.READ, size, DZS_Color.Get(tone));
		c.m_Full = text;
		c.m_Text.SetText(DZS_W.Literal(text));
		c.m_W = w;
		l.m_Cells.Insert(c);
		return c.m_Text;
	}

	protected void Heading(string text, float before = 14)
	{
		DZS_RLine l = Line(before, 20);
		Label(l, DZS_Texts.Upper(text), 0, "textMuted", 12);
	}

	protected void Note(string text, string tone = "textQuiet")
	{
		DZS_RLine l = Line(0, 22);
		Label(l, text, 0, tone, 13);
	}

	protected DZS_TextField Field(DZS_RLine l, string text, string placeholder, float w = 0)
	{
		DZS_TextField f = new DZS_TextField("", placeholder, "textMuted", 120);
		f.Build(m_Parent);
		f.SetText(text);
		m_Fields.Insert(f);
		AddFocusField(f);
		DZS_RCell c = new DZS_RCell();
		c.m_Field = f;
		c.m_W = w;
		l.m_Cells.Insert(c);
		return f;
	}

	protected DZS_Button Knob(DZS_RLine l, string kind, string label, string tag, string icon = "")
	{
		DZS_Button b = new DZS_Button(m_Parent, kind, label, icon, 28);
		b.m_Tag = tag;
		b.m_OnClick.Insert(OnKnob);
		m_Knobs.Insert(b);
		AddFocusHit(b.m_Hit);
		DZS_RCell c = new DZS_RCell();
		c.m_Button = b;
		c.m_W = -1;
		l.m_Cells.Insert(c);
		return b;
	}

	protected DZS_Toggle Switch(DZS_RLine l, bool on, string tag)
	{
		DZS_Toggle t = new DZS_Toggle(m_Parent);
		t.SetOn(on);
		t.m_OnChange.Insert(OnSwitch);
		m_Toggles.Insert(t);
		m_ToggleTags.Insert(tag);
		AddFocusHit(t.m_Hit);
		DZS_RCell c = new DZS_RCell();
		c.m_Toggle = t;
		c.m_W = -1;
		l.m_Cells.Insert(c);
		return t;
	}

	protected float CellWidth(DZS_RCell c)
	{
		if (c.m_W > 0)
			return DZS_Scale.Px(c.m_W);
		if (c.m_Text)
			return DZS_W.TextWidth(c.m_Text) + DZS_Scale.Px(2);
		if (c.m_Button)
			return c.m_Button.GetWidth();
		if (c.m_Toggle)
			return DZS_Toggle.Width() + DZS_Scale.Px(4);
		return DZS_Scale.Px(120);
	}

	override float LayoutContent(float width)
	{
		float gap = DZS_Scale.Px(8);
		float y = 0;
		foreach (DZS_RLine l : m_Lines)
		{
			y += DZS_Scale.Px(l.m_Before);
			float h = DZS_Scale.Px(l.m_H);
			float fixedW = 0;
			int flex = 0;
			foreach (DZS_RCell c : l.m_Cells)
			{
				if (c.m_W == 0)
					flex++;
				else
					fixedW += CellWidth(c);
			}
			if (l.m_Cells.Count() > 1)
				fixedW += gap * (l.m_Cells.Count() - 1);
			float share = 0;
			if (flex > 0)
				share = Math.Max(DZS_Scale.Px(40), (width - fixedW) / flex);
			float x = 0;
			foreach (DZS_RCell pc : l.m_Cells)
			{
				float cw = share;
				if (pc.m_W != 0)
					cw = CellWidth(pc);
				if (pc.m_Text)
				{
					if (pc.m_W >= 0)
						DZS_W.FitText(pc.m_Text, pc.m_Full, cw);
					float th = DZS_W.TextHeight(pc.m_Text);
					DZS_W.Place(pc.m_Text, x, y + (h - th) / 2, cw, th);
				}
				else if (pc.m_Field)
				{
					int refW = cw / DZS_Scale.Factor();
					pc.m_Field.SetWidthRef(refW);
					pc.m_Field.Place(x, y + (h - pc.m_Field.Height()) / 2, pc.m_Field.Height());
				}
				else if (pc.m_Button)
					pc.m_Button.Place(x, y + (h - pc.m_Button.GetHeight()) / 2);
				else if (pc.m_Toggle)
					pc.m_Toggle.Place(x + DZS_Scale.Px(2), y + (h - DZS_Toggle.Height()) / 2);
				x += cw + gap;
			}
			y += h;
		}
		return y + DZS_Scale.Px(4);
	}

	void OnKnob(DZS_Button b)
	{
		Knobbed(b, b.m_Tag);
	}

	void OnSwitch(DZS_Toggle t, bool on)
	{
		int i = m_Toggles.Find(t);
		if (i >= 0)
			Switched(m_ToggleTags[i], on);
	}

	void Knobbed(DZS_Button b, string tag) {}
	void Switched(string tag, bool on) {}
	void Picked(string what, string tag) {}

	// A choice under a knob; the tag of the chosen entry comes back in Picked(what, tag).
	protected void Pick(DZS_Button at, array<ref DZS_MenuItem> items, string what)
	{
		if (items.Count() == 0)
			return;
		m_PickWhat = what;
		float px, py;
		at.GetWidget().GetScreenPos(px, py);
		DZS_Popup pop = DZS_Kernel.Get().GetScreen().OpenContextMenu(items, px, py + at.GetHeight() + DZS_Scale.Px(2));
		pop.m_OnChoose.Insert(OnPicked);
	}

	void OnPicked(DZS_Popup p, DZS_MenuItem it)
	{
		DZS_Kernel.Get().GetScreen().CloseMenu();
		if (!it.m_Enabled)
			return;
		Picked(m_PickWhat, it.m_Tag);
	}

	protected static DZS_MenuItem Item(string tag, string text)
	{
		DZS_MenuItem it = new DZS_MenuItem();
		it.m_Tag = tag;
		it.m_Text = text;
		return it;
	}
}
