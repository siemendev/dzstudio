// Position der Auswahl: X, Y, Z of the pivot, "Drehen um" and "Größe ×" for several objects.
class DZS_PositionDialog : DZS_Dialog
{
	protected DZS_PlacementPlugin m_P;
	protected ref array<ref DZS_NumberField> m_Fields;
	protected ref array<TextWidget> m_Labels;
	ref array<float> m_Values;
	float m_MinF;
	float m_MaxF;
	bool m_ScaleOff;
	protected TextWidget m_Note;

	void DZS_PositionDialog(DZS_PlacementPlugin p, vector pivot, float minF, float maxF, bool scaleOff)
	{
		m_P = p;
		m_MinF = minF;
		m_MaxF = maxF;
		m_ScaleOff = scaleOff;
		m_Values = new array<float>();
		m_Values.Insert(pivot[0]);
		m_Values.Insert(pivot[1]);
		m_Values.Insert(pivot[2]);
		m_Values.Insert(0);
		m_Values.Insert(1);
		string kicker = DZS_Texts.Pick("Auswahl", "Selection");
		string title = DZS_Texts.Pick("Position der Auswahl", "Position of the selection");
		Setup(SMALL, kicker, title, false);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("apply", DZS_Texts.Pick("Übernehmen", "Apply"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Fields = new array<ref DZS_NumberField>();
		m_Labels = new array<TextWidget>();
		array<string> letters = {"X", "Y", "Z", "°", "×"};
		array<string> colors = {"axisX", "axisY", "axisZ", "textMuted", "textMuted"};
		string turn = DZS_Texts.Pick("Drehen um", "Turn by");
		string size = DZS_Texts.Pick("Größe ×", "Size ×");
		array<string> names = {"X", "Y", "Z", turn, size};
		for (int i = 0; i < 5; i++)
		{
			TextWidget l = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
			l.SetText(DZS_Texts.Upper(names[i]));
			m_Labels.Insert(l);
			DZS_NumberField f = new DZS_NumberField("placement.pos" + i, letters[i], colors[i], 160);
			f.Build(inner);
			f.m_OnCommit.Insert(OnFieldCommit);
			m_Fields.Insert(f);
			AddFocusField(f);
		}
		m_Note = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		if (m_ScaleOff)
			m_Fields[4].SetEnabled(false);
		AddFootFocus();
	}

	void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		int i = m_Fields.Find(f);
		if (typed == "")
		{
			r.m_Ok = true;
			return;
		}
		float v;
		if (i < 0 || !DZS_Num.Parse(typed, v))
		{
			r.m_Ok = false;
			return;
		}
		m_Values[i] = v;
		r.m_Ok = true;
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		for (int i = 0; i < 5; i++)
		{
			DZS_W.Place(m_Labels[i], 0, y, DZS_Scale.Px(120), DZS_Scale.Px(28));
			m_Fields[i].Place(DZS_Scale.Px(130), y, DZS_Scale.Px(28));
			int dec = 3;
			if (i == 3)
				dec = 2;
			m_Fields[i].ShowValue(DZS_Num.Fixed(m_Values[i], dec));
			y += DZS_Scale.Px(34);
		}
		DZS_W.Place(m_Note, 0, y, width, DZS_Scale.Px(20));
		return y + DZS_Scale.Px(22);
	}

	override void UpdateContent(float dt)
	{
		DZS_Button main = FootButton("apply");
		string note = "";
		bool ok = true;
		if (m_ScaleOff)
			note = DZS_Texts.Pick("Objekte mit Größe außerhalb 0,05–20 — Größe × bleibt 1", "Objects with size outside 0.05–20 — size × stays 1");
		else if (m_Values[4] < m_MinF - 0.00001 || m_Values[4] > m_MaxF + 0.00001)
		{
			ok = false;
			note = DZS_Texts.Pick("Möglich: ×", "Possible: ×") + DZS_Num.Fixed(m_MinF, 3) + " – ×" + DZS_Num.Fixed(m_MaxF, 3);
		}
		m_Note.SetText(note);
		if (main && main.m_Hit.m_Enabled != ok)
			main.SetEnabled(ok, note);
	}

	override void OnFoot(string tag)
	{
		if (tag == "apply")
		{
			if (!FootButton("apply").m_Hit.m_Enabled)
				return;
			Close();
			m_P.ApplyPosition(Vector(m_Values[0], m_Values[1], m_Values[2]), m_Values[3], m_Values[4]);
			return;
		}
		Close();
	}
}
