// The collected list before saving and after opening.
// Result tags: "save" (go on), "cancel", "close", "show" (m_ShowId holds the object).
class DZS_CheckRow : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	DZS_Issue m_Issue;
	DZS_CheckListDialog m_Dialog;
	Widget m_Bg;
	ref DZS_Border m_Ring;
	ImageWidget m_Icon;
	TextWidget m_Sev;
	TextWidget m_What;
	TextWidget m_Why;
	ref DZS_Button m_Show;
	ref DZS_Button m_FixKnob;

	void DZS_CheckRow(DZS_CheckListDialog d, Widget parent, DZS_Issue issue)
	{
		m_Dialog = d;
		m_Issue = issue;
		m_Hit = new DZS_Hit(parent, this, "checkrow");
		m_Hit.m_FocusKind = "list";
		Widget f = m_Hit.m_W;
		m_Bg = DZS_W.Panel(f, DZS_Color.Get("surfaceRaised"));
		m_Ring = new DZS_Border(f, DZS_Color.Get("accent"));
		string tone = "info";
		string word = DZS_Texts.Pick("Hinweis", "Hint");
		if (issue.m_Severity == DZS_Issue.ERROR)
		{
			tone = "danger";
			word = DZS_Texts.Pick("Fehler", "Error");
		}
		else if (issue.m_Severity == DZS_Issue.WARNING)
		{
			tone = "warning";
			word = DZS_Texts.Pick("Warnung", "Warning");
		}
		m_Icon = DZS_W.Icon(f, DZS_Plate.ToneIcon(tone), DZS_Color.Get(tone));
		m_Sev = DZS_W.Text(f, DZS_Font.LABELBOLD, 11, DZS_Color.Get(tone));
		m_Sev.SetText(DZS_Texts.Upper(word));
		m_What = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("text"));
		string what = issue.m_What;
		if (issue.m_Group != "")
		{
			if (what != "")
				what += " · ";
			what += issue.m_Group;
		}
		if (issue.m_File != "")
		{
			if (what != "")
				what += " · ";
			what += issue.m_File;
		}
		m_What.SetText(what);
		m_Why = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Why.SetText(DZS_Texts.Pick(issue.m_De, issue.m_En));
		if (issue.m_ObjectId != "")
		{
			m_Show = new DZS_Button(f, "ghost", DZS_Texts.Pick("Zeigen", "Show"));
			m_Show.m_Hit.m_FocusKind = "";
			m_Show.m_OnClick.Insert(OnShowClick);
		}
		if (issue.m_Fix)
		{
			m_FixKnob = new DZS_Button(f, "secondary", DZS_Texts.Pick(issue.m_Fix.m_De, issue.m_Fix.m_En));
			m_FixKnob.m_Hit.m_FocusKind = "";
			m_FixKnob.m_OnClick.Insert(OnFixClick);
		}
		Refresh();
	}

	void OnFixClick(DZS_Button b)
	{
		m_Dialog.FixIssue(m_Issue);
	}

	void OnShowClick(DZS_Button b)
	{
		m_Dialog.ShowIssue(m_Issue);
	}

	override void OnHitActivate(DZS_Hit h)
	{
		if (m_Issue.m_ObjectId != "")
			m_Dialog.ShowIssue(m_Issue);
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	void Refresh()
	{
		m_Bg.Show(m_Hit.m_Hover);
		m_Ring.Show(m_Hit.m_Focus);
	}

	float Place(float y, float w)
	{
		float h = DZS_Scale.Px(48);
		m_Hit.Place(0, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		m_Ring.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		float icon = DZS_Scale.Px(16);
		DZS_W.Place(m_Icon, DZS_Scale.Px(8), DZS_Scale.Px(6), icon, icon);
		DZS_W.Place(m_Sev, DZS_Scale.Px(30), DZS_Scale.Px(6), DZS_Scale.Px(90), DZS_Scale.Px(16));
		float right = 0;
		if (m_Show)
		{
			right = m_Show.GetWidth() + DZS_Scale.Px(8);
			m_Show.Place(w - right, (h - m_Show.GetHeight()) / 2);
		}
		if (m_FixKnob)
		{
			right += m_FixKnob.GetWidth() + DZS_Scale.Px(8);
			m_FixKnob.Place(w - right, (h - m_FixKnob.GetHeight()) / 2);
		}
		DZS_W.Place(m_What, DZS_Scale.Px(120), DZS_Scale.Px(4), w - DZS_Scale.Px(128) - right, DZS_Scale.Px(20));
		DZS_W.Place(m_Why, DZS_Scale.Px(30), DZS_Scale.Px(25), w - DZS_Scale.Px(38) - right, DZS_Scale.Px(20));
		return h;
	}
}

class DZS_CheckListDialog : DZS_Dialog
{
	protected ref DZS_Issues m_Issues;
	protected string m_Mode;
	protected ref array<ref DZS_CheckRow> m_Rows;
	protected ref DZS_Toggle m_Confirm;
	protected TextWidget m_ConfirmText;
	protected ref DZS_Hit m_ConfirmHit;
	string m_ShowId;

	// mode: "save" (question before writing) or "read" (after opening or with hints only)
	void DZS_CheckListDialog(DZS_Issues issues, string mode, string kickerDe, string kickerEn)
	{
		m_Issues = issues;
		m_Mode = mode;
		m_Rows = new array<ref DZS_CheckRow>();
		string tx129 = DZS_Texts.Pick(kickerDe, kickerEn);
		string tx130 = issues.Title();
		Setup(MEDIUM, tx129, tx130, mode != "read");
		if (mode == "read")
			AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "side");
		else
		{
			AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
			if (!issues.HasBlocking())
			{
				string kind = "main";
				if (issues.HasConfirmable())
					kind = "danger";
				string goDe = "Trotzdem speichern";
				string goEn = "Save anyway";
				if (mode == "import")
				{
					goDe = "Trotzdem importieren";
					goEn = "Import anyway";
				}
				DZS_Button go = AddFootButton("save", DZS_Texts.Pick(goDe, goEn), kind);
				go.m_Hit.m_Primary = true;
				if (issues.HasConfirmable())
					go.SetEnabled(false, DZS_Texts.Pick("Erst den Schalter umlegen", "Flip the switch first"));
			}
		}
	}

	override void BuildContent(Widget inner)
	{
		array<DZS_Issue> sorted = m_Issues.Sorted();
		foreach (DZS_Issue i : sorted)
			m_Rows.Insert(new DZS_CheckRow(this, inner, i));
		if (m_Mode == "save" && m_Issues.HasConfirmable() && !m_Issues.HasBlocking())
		{
			m_Confirm = new DZS_Toggle(inner);
			m_Confirm.m_OnChange.Insert(OnConfirmFlip);
			m_ConfirmText = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
			m_ConfirmText.SetText(DZS_Texts.Pick("Ich habe die Fehler gelesen und will trotzdem speichern", "I have read the errors and want to save anyway"));
		}
		foreach (DZS_CheckRow r : m_Rows)
			AddFocusHit(r.m_Hit);
		if (m_Confirm)
			AddFocusHit(m_Confirm.m_Hit);
		AddFootFocus();
	}

	void OnConfirmFlip(DZS_Toggle t, bool on)
	{
		DZS_Button go = FootButton("save");
		if (go)
			go.SetEnabled(on, DZS_Texts.Pick("Erst den Schalter umlegen", "Flip the switch first"));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		foreach (DZS_CheckRow r : m_Rows)
			y += r.Place(y, width) + DZS_Scale.Px(4);
		if (m_Confirm)
		{
			y += DZS_Scale.Px(8);
			m_Confirm.Place(0, y + DZS_Scale.Px(3));
			DZS_W.Place(m_ConfirmText, DZS_Toggle.Width() + DZS_Scale.Px(8), y, width - DZS_Toggle.Width() - DZS_Scale.Px(8), DZS_Scale.Px(20));
			y += DZS_Scale.Px(24);
		}
		return y;
	}

	// Anfangsfokus: "Trotzdem speichern" with warnings; first entry with errors and when reading.
	override void InitialFocus()
	{
		DZS_Button go = FootButton("save");
		if (go && go.m_Hit.m_Enabled)
		{
			SetFocusToHit(go.m_Hit);
			return;
		}
		if (m_Rows.Count() > 0)
		{
			SetFocusToHit(m_Rows[0].m_Hit);
			return;
		}
		super.InitialFocus();
	}

	// The fix changed the project: the list is closed with "fixed" and the caller builds it again (a save plans anew).
	void FixIssue(DZS_Issue i)
	{
		if (!i.m_Fix || !i.m_Fix.Apply())
			return;
		DZS_Log.Info("check list: fixed " + i.m_What);
		m_OnResult.Invoke(this, "fixed");
		Close();
	}

	void ShowIssue(DZS_Issue i)
	{
		m_ShowId = i.m_ObjectId;
		m_OnResult.Invoke(this, "show");
		Close();
	}

	override void Cancel()
	{
		if (m_Mode == "read")
			m_OnResult.Invoke(this, "close");
		else
			m_OnResult.Invoke(this, "cancel");
		Close();
	}
}
