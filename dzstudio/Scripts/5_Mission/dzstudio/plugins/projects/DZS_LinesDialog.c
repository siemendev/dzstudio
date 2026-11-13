// One cell of a dialog line; width in reference pixels, 0 = a share of the rest, -1 = its own width.
class DZS_RCell : Managed
{
	TextWidget m_Text;
	string m_Full;
	DZS_TextField m_Field;
	DZS_Button m_Button;
	DZS_Toggle m_Toggle;
	DZS_ProgressBar m_Bar;
	float m_W;
}

class DZS_RLine : Managed
{
	ref array<ref DZS_RCell> m_Cells = new array<ref DZS_RCell>();
	float m_H = 28;
	float m_Before;
}

// Dialog of lines of cells, placed left to right; knobs, switches and pickers report by tag (an extension's rules,
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
	// the project a dialog bound to an entity watches (Watch): a change of its state asks StateChanged
	protected DZS_Instance m_WatchInst;
	protected int m_WatchGen = -1;
	protected float m_WatchWait;

	protected void Watch(DZS_Instance inst)
	{
		m_WatchInst = inst;
		if (inst)
			m_WatchGen = inst.GetGen();
	}

	// Whether what the dialog edits is still in the project (by its identity, never by a place in a list).
	bool TargetAlive() { return true; }

	// The watched project changed (an own step, undo, a collab partner): a dialog whose entity is gone closes.
	void StateChanged()
	{
		if (!TargetAlive())
			Gone();
	}

	void Gone()
	{
		Close();
		GoneNotice();
	}

	static void GoneNotice(string de = "Das Element wurde gelöscht", string en = "This element was deleted")
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (k && k.GetScreen())
			k.GetScreen().ShowMessage("extensions.gone", "warning", DZS_Texts.Pick(de, en), 3.0);
		DZS_Log.Info("extensions: a dialog's element was deleted, dialog closed");
	}

	override void UpdateContent(float dt)
	{
		if (m_Closed)
			return;
		if (m_IndexPending)
		{
			IndexWait(dt);
			return;
		}
		if (!m_WatchInst)
			return;
		m_WatchWait -= dt;
		if (m_WatchWait > 0)
			return;
		m_WatchWait = 0.4;
		DZS_Kernel k = DZS_Kernel.Get();
		if (k && k.GetShown() != m_WatchInst)
		{
			Close();
			return;
		}
		int g = m_WatchInst.GetGen();
		if (g == m_WatchGen)
			return;
		// a dialog that reads the extension index asks once it is built for the new state
		if (!IndexNow())
			return;
		m_WatchGen = g;
		StateChanged();
	}

	// ---- the extension index (DZS_EntityExts.IndexReady) ----
	// A dialog that reads the index (UseIndex) never builds the whole project in one frame: the index is built over the
	// next frames. A dialog opened meanwhile shows "Building …" with the shared progress bar instead of its content and
	// is built again (Again) once it is there; an open one says so in its foot and carries out what was asked meanwhile
	// (a knob, a foot knob, a choice) afterwards, after StateChanged has checked its target.

	protected DZS_Instance m_IndexInst;
	// the content is the "Building …" placeholder
	protected bool m_IndexShell;
	protected bool m_IndexPending;
	// how long it has been waited for the index; the foot says "Building …" only after a moment (no flicker)
	protected float m_IndexWaited;
	protected bool m_IndexNote;
	protected ref DZS_ProgressBar m_IndexBar;
	protected ref array<string> m_DeferKinds = new array<string>();
	protected ref array<string> m_DeferTags = new array<string>();
	protected ref array<string> m_DeferWhats = new array<string>();
	protected ref array<DZS_Button> m_DeferKnobs = new array<DZS_Button>();

	protected void UseIndex(DZS_Instance inst)
	{
		m_IndexInst = inst;
	}

	static string BuildingText()
	{
		return DZS_Texts.Pick("Wird aufgebaut …", "Building …");
	}

	// Whether the index of the state as it is now is there; else it is being built (the foot says so after a moment)
	// and false.
	protected bool IndexNow()
	{
		if (!m_IndexInst)
			return true;
		if (DZS_EntityExts.IndexReady(m_IndexInst))
			return true;
		if (!m_IndexPending)
		{
			m_IndexPending = true;
			m_IndexWaited = 0;
		}
		return false;
	}

	// At the start of BuildContent of a dialog that reads the index: true when it is there; else the content is
	// "Building …" with the progress bar, false, and the dialog is built again (Again) once the index is there.
	protected bool IndexOrShell(DZS_Instance inst)
	{
		m_IndexInst = inst;
		if (!inst || DZS_EntityExts.IndexReady(inst))
			return true;
		m_IndexShell = true;
		m_IndexPending = true;
		Note(BuildingText(), "textMuted");
		DZS_RLine l = Line(6, 22);
		DZS_RCell c = new DZS_RCell();
		DZS_ProgressBar bar = new DZS_ProgressBar(m_Parent);
		m_IndexBar = bar;
		bar.SetShare(DZS_EntityExts.IndexShare());
		c.m_Bar = bar;
		c.m_W = -1;
		l.m_Cells.Insert(c);
		AddFootFocus();
		return false;
	}

	// The dialog built anew, as it is now (a dialog that shows "Building …" once the index is there).
	void Again() {}

	// Something that came while the index was built, carried out once it is there ("call": Resume(tag)).
	protected void Defer(string kind, string tag, string what = "", DZS_Button knob = null)
	{
		for (int i = 0; i < m_DeferKinds.Count(); i++)
		{
			if (kind == "call" && m_DeferKinds[i] == kind && m_DeferTags[i] == tag)
				return;
		}
		m_DeferKinds.Insert(kind);
		m_DeferTags.Insert(tag);
		m_DeferWhats.Insert(what);
		m_DeferKnobs.Insert(knob);
	}

	void Resume(string tag) {}

	protected void IndexWait(float dt)
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!m_IndexInst || (k && k.GetShown() != m_IndexInst))
		{
			Close();
			return;
		}
		if (!DZS_EntityExts.IndexReady(m_IndexInst))
		{
			m_IndexWaited += dt;
			if (m_IndexBar)
				m_IndexBar.SetShare(DZS_EntityExts.IndexShare());
			else if (!m_IndexNote && m_IndexWaited >= DZS_Jobs.SHOW_AFTER)
			{
				m_IndexNote = true;
				SetFootNote(BuildingText());
				Layout();
			}
			return;
		}
		m_IndexPending = false;
		if (m_IndexShell)
		{
			// the target checked again on the state the index was built for: it may have gone meanwhile
			if (TargetAlive())
				Again();
			else
				Gone();
			return;
		}
		if (m_IndexNote)
		{
			m_IndexNote = false;
			SetFootNote("");
			Layout();
		}
		if (m_WatchInst && m_WatchInst.GetGen() != m_WatchGen)
		{
			m_WatchGen = m_WatchInst.GetGen();
			StateChanged();
		}
		if (m_Closed || m_IndexPending)
			return;
		array<string> kinds = new array<string>();
		kinds.Copy(m_DeferKinds);
		array<string> tags = new array<string>();
		tags.Copy(m_DeferTags);
		array<string> whats = new array<string>();
		whats.Copy(m_DeferWhats);
		array<DZS_Button> knobs = new array<DZS_Button>();
		knobs.Copy(m_DeferKnobs);
		m_DeferKinds.Clear();
		m_DeferTags.Clear();
		m_DeferWhats.Clear();
		m_DeferKnobs.Clear();
		for (int i = 0; i < kinds.Count(); i++)
		{
			if (m_Closed)
				return;
			string kind = kinds[i];
			// the state changed again meanwhile: the rest waits for the next index
			if (m_IndexPending)
			{
				Defer(kind, tags[i], whats[i], knobs[i]);
				continue;
			}
			if (kind == "knob")
				Knobbed(knobs[i], tags[i]);
			else if (kind == "foot")
				OnFoot(tags[i]);
			else if (kind == "pick")
				Picked(whats[i], tags[i]);
			else if (kind == "call")
				Resume(tags[i]);
		}
	}

	// Foot knobs other than closing wait for the index; while the content is "Building …" they do nothing.
	override void OnFootClick(DZS_Button b)
	{
		if (b.m_Tag == "close" || b.m_Tag == "cancel" || IndexNow())
		{
			super.OnFootClick(b);
			return;
		}
		if (!m_IndexShell)
			Defer("foot", b.m_Tag);
	}

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
		if (c.m_Bar)
			return DZS_ProgressBar.Width();
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
				else if (pc.m_Bar)
					pc.m_Bar.Place(x, y + (h - DZS_ProgressBar.Height()) / 2);
				x += cw + gap;
			}
			y += h;
		}
		return y + DZS_Scale.Px(4);
	}

	void OnKnob(DZS_Button b)
	{
		if (!IndexNow())
		{
			if (!m_IndexShell)
				Defer("knob", b.m_Tag, "", b);
			return;
		}
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
		if (!IndexNow())
		{
			if (!m_IndexShell)
				Defer("pick", it.m_Tag, m_PickWhat);
			return;
		}
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
