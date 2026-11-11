// Inspector of the Spawns layer: the event of the selected point, or of the event chosen in the list — how many of it
// are on the map (with the base's value next to it), its classes with their maximum, and removing the point.
class DZS_SpawnsPanel : DZS_PanelContent
{
	protected DZS_SpawnsPlugin m_P;
	protected Widget m_Parent;
	protected Widget m_Root;
	protected int m_BuiltGen = -1;
	protected float m_Width;
	protected float m_H;
	protected float m_Y;
	protected ref array<ref DZS_Button> m_Buttons = new array<ref DZS_Button>();
	protected ref array<ref DZS_NumberField> m_Fields = new array<ref DZS_NumberField>();
	protected string m_Event;

	void DZS_SpawnsPanel(DZS_SpawnsPlugin p)
	{
		m_P = p;
	}

	override void Build(Widget parent)
	{
		m_Parent = parent;
	}

	override float Layout(float width)
	{
		bool changed = Math.AbsFloat(width - m_Width) > 0.5;
		m_Width = width;
		Rebuild(changed);
		return m_H;
	}

	override void Update(float dt)
	{
		if (m_BuiltGen != m_P.m_Gen)
			Rebuild(false);
		foreach (DZS_NumberField f : m_Fields)
			f.Update(dt);
	}

	protected bool AnyFocused()
	{
		foreach (DZS_NumberField f : m_Fields)
		{
			if (f.IsFocused() || f.IsDragging())
				return true;
		}
		return false;
	}

	protected void Clear()
	{
		foreach (DZS_Button b : m_Buttons)
			b.Destroy();
		foreach (DZS_NumberField f : m_Fields)
			f.Destroy();
		m_Buttons.Clear();
		m_Fields.Clear();
		if (m_Root)
			m_Root.Unlink();
		m_Root = null;
	}

	protected float Pad() { return DZS_Scale.Px(12); }

	protected TextWidget Line(string text, string color, float size, float h)
	{
		TextWidget t = DZS_W.Text(m_Root, DZS_Font.READ, size, DZS_Color.Get(color));
		t.SetText(DZS_W.Literal(text));
		DZS_W.Place(t, Pad(), m_Y, m_Width - 2 * Pad(), DZS_Scale.Px(h));
		m_Y += DZS_Scale.Px(h);
		return t;
	}

	protected void Rebuild(bool force)
	{
		if (!m_Parent || m_Width <= 0)
			return;
		if (!force && m_BuiltGen == m_P.m_Gen)
			return;
		if (AnyFocused() && !force)
			return;
		m_BuiltGen = m_P.m_Gen;
		Clear();
		m_Root = DZS_W.Frame(m_Parent);
		m_Y = DZS_Scale.Px(8);
		DZS_SpawnPoint p = m_P.Selected();
		if (p)
			EventView(p.m_Event, p);
		else if (m_P.m_SelEvent != "")
			EventView(m_P.m_SelEvent, null);
		else
			Summary();
		m_H = m_Y + DZS_Scale.Px(8);
		DZS_W.Place(m_Root, 0, 0, m_Width, m_H);
	}

	protected void Summary()
	{
		int pts = 0;
		int kept = 0;
		foreach (DZS_SpawnPoint sp : m_P.m_Points)
		{
			if (!sp.m_Gone)
				pts++;
			if (sp.m_Kept)
				kept++;
		}
		Line(DZS_Texts.Pick("Fahrzeug-Spawnpunkte", "Vehicle spawn points"), "textMuted", 12, 18);
		Line(m_P.m_Events.Count().ToString() + DZS_Texts.Pick(" Events · ", " events · ") + pts + DZS_Texts.Pick(" Punkte", " points"), "text", 14, 22);
		Line(DZS_Texts.Pick("Auto anklicken oder Event in der Liste wählen.", "Click a car or pick an event in the list."), "textQuiet", 13, 20);
		Line(DZS_Texts.Pick("Grün: eigene Punkte · Gelb: verschobene", "Green: own points · Yellow: moved"), "textQuiet", 13, 20);
		if (kept > 0)
			Line(DZS_Texts.Pick("Blau: außerhalb angelegt, nur sichtbar", "Blue: made outside, shown only"), "textQuiet", 13, 20);
	}

	protected void EventView(string ev, DZS_SpawnPoint p)
	{
		DZS_SpawnEvent e = m_P.m_Events.Get(ev);
		m_Event = ev;
		string kind = DZS_Texts.Pick("Event der Basis", "Base event");
		if (e && e.m_Elem != "")
			kind = DZS_Texts.Pick("Eigenes festes Fahrzeug", "Own fixed vehicle");
		Line(kind, "textMuted", 12, 18);
		Line(ev, "text", 15, 24);
		if (!p)
		{
			m_Y += DZS_Scale.Px(6);
			EventValues(ev, e);
			return;
		}
		string where = "x " + Math.Round(p.m_X) + " · z " + Math.Round(p.m_Z) + " · " + Math.Round(p.m_A) + "°";
		if (p.m_Kept)
		{
			Line(where, "textQuiet", 13, 20);
			Line(DZS_KeptBlocks.ReadOnlyTitle(), "textMuted", 13, 20);
			Line(DZS_KeptBlocks.ReadOnlyHint(), "textQuiet", 13, 20);
			return;
		}
		if (p.m_Own)
			where += DZS_Texts.Pick(" · eigener Punkt", " · own point");
		else if (p.m_Moved)
			where += DZS_Texts.Pick(" · verschoben", " · moved");
		Line(where, "textQuiet", 13, 20);
		int here = m_P.StackSize(p);
		if (here > 1)
		{
			string hereText = m_P.HereText(here);
			string next = DZS_Texts.Pick(" · Klick aufs Auto wechselt", " · click the car for the next");
			Line(hereText + next, "textQuiet", 13, 20);
		}
		m_Y += DZS_Scale.Px(6);
		EventValues(ev, e);
		m_Y += DZS_Scale.Px(8);
		DZS_Button rem = new DZS_Button(m_Root, "danger", DZS_Texts.Pick("Punkt entfernen", "Remove point"), "trash");
		rem.Place(Pad(), m_Y);
		rem.m_OnClick.Insert(OnRemove);
		m_Buttons.Insert(rem);
		m_Y += DZS_Scale.Px(32);
	}

	protected void EventValues(string ev, DZS_SpawnEvent e)
	{
		if (e)
		{
			string baseNote = "";
			if (e.m_Elem == "" && e.m_BaseNominal != "")
				baseNote = DZS_Texts.Pick("Basis ", "Base ") + e.m_BaseNominal;
			ValueRow(DZS_Texts.Pick("Anzahl auf der Karte", "Count on the map"), "nominal", e.m_Nominal, baseNote);
			if (e.m_Elem == "")
			{
				Line(DZS_Texts.Pick("Klassen – höchstens je Klasse", "Classes – at most per class"), "textMuted", 12, 20);
				for (int i = 0; i < e.m_Types.Count(); i++)
				{
					string note = "";
					if (e.m_BaseMax[i] != e.m_Max[i])
						note = DZS_Texts.Pick("Basis ", "Base ") + e.m_BaseMax[i];
					ValueRow(e.m_Types[i], "child:" + e.m_Types[i] + ":max", e.m_Max[i], note);
				}
			}
			else
				Line(e.m_Types[0], "textQuiet", 13, 20);
			Line(m_P.PointsOf(ev).ToString() + DZS_Texts.Pick(" Spawnpunkte", " spawn points"), "textQuiet", 13, 20);
		}
	}

	protected void ValueRow(string label, string field, string value, string note)
	{
		float rowH = DZS_Scale.Px(26);
		TextWidget l = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("text"));
		l.SetText(DZS_W.Literal(label));
		DZS_W.Place(l, Pad(), m_Y, m_Width * 0.5, rowH);
		DZS_NumberField f = new DZS_NumberField("spawns." + field, "", "textMuted", 70);
		f.Build(m_Root);
		f.m_Tag = field;
		f.m_OnCommit.Insert(OnValue);
		f.ShowValue(value);
		f.Place(m_Width - Pad() - f.GetWidth(), m_Y, rowH);
		m_Fields.Insert(f);
		if (note != "")
		{
			TextWidget n = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
			n.SetText(note);
			DZS_W.Place(n, m_Width - Pad() - f.GetWidth() - DZS_Scale.Px(70), m_Y, DZS_Scale.Px(64), rowH);
		}
		m_Y += rowH + DZS_Scale.Px(4);
	}

	void OnValue(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		string t = typed;
		t.Trim();
		float num;
		r.m_Ok = DZS_Num.Parse(t, num);
		if (!r.m_Ok)
			return;
		int n = Math.Clamp(Math.Round(num), 0, 999);
		m_P.SetEventValue(m_Event, f.m_Tag, n.ToString());
	}

	void OnRemove(DZS_Button b)
	{
		m_P.RemoveSelected();
	}
}
