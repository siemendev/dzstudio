// A plugin shows its own building blocks on the Musterseite (DZS_SheetParts.Add(MyPart) in OnRegister).
class DZS_SheetPart : Managed
{
	string TitleDe() { return ""; }
	string TitleEn() { return ""; }
	void Build(Widget inner) {}
	// height used below y
	float Layout(float width, float y) { return 0; }
	void Update(float dt) {}
}

class DZS_SheetParts
{
	static ref array<typename> s_Types;

	static void Add(typename t)
	{
		if (!s_Types)
			s_Types = new array<typename>();
		foreach (typename have : s_Types)
		{
			if (have.ToString() == t.ToString())
				return;
		}
		s_Types.Insert(t);
	}

	// Types of a mission module are new after a world change; the kernel clears the list before plugins register again.
	static void Clear()
	{
		if (s_Types)
			s_Types.Clear();
	}
}

// Sample sheet: every kernel building block in its states; only with "developer": true.
class DZS_BlockSheet : DZS_Dialog
{
	protected ref array<TextWidget> m_Heads = new array<TextWidget>();
	protected ref array<ref DZS_Button> m_Buttons = new array<ref DZS_Button>();
	protected ref array<ref DZS_Toggle> m_Toggles = new array<ref DZS_Toggle>();
	protected ref DZS_Slider m_Slider;
	protected ref array<ref DZS_NumberField> m_Fields = new array<ref DZS_NumberField>();
	protected ref array<ref DZS_SearchField> m_Searches = new array<ref DZS_SearchField>();
	protected ref array<ref DZS_Chip> m_Chips = new array<ref DZS_Chip>();
	protected ref DZS_RichLine m_Caps;
	protected ref array<ref DZS_Plate> m_Plates = new array<ref DZS_Plate>();
	protected ref array<TextWidget> m_Notes = new array<TextWidget>();
	protected ref array<ref DZS_Button> m_Openers = new array<ref DZS_Button>();
	protected ref DZS_Popup m_Menu;
	protected ref array<ref DZS_StatusText> m_Lamps = new array<ref DZS_StatusText>();
	protected ref array<ref DZS_SheetPart> m_Parts = new array<ref DZS_SheetPart>();

	void DZS_BlockSheet()
	{
		Setup(LARGE, DZS_Texts.Pick("Entwickler", "Developer"), DZS_Texts.Pick("Musterseite der Bausteine", "Building blocks"), false);
		AddFootButton("close", DZS_Texts.Get("core.dialog.closeButton"), "main");
	}

	protected TextWidget Head(Widget inner, string de, string en)
	{
		TextWidget t = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		t.SetText(DZS_Texts.Upper("// " + DZS_Texts.Pick(de, en)));
		m_Heads.Insert(t);
		return t;
	}

	protected DZS_Button Btn(Widget inner, string kind, string label, string icon, bool enabled, bool on)
	{
		DZS_Button b = new DZS_Button(inner, kind, label, icon, 28);
		b.SetEnabled(enabled, DZS_Texts.Pick("Ausgegraut mit Grund", "Greyed out with a reason"));
		b.SetOn(on);
		b.SetTip(label, "Ctrl+S");
		m_Buttons.Insert(b);
		return b;
	}

	override void BuildContent(Widget inner)
	{
		Head(inner, "Knöpfe — normal, aus, an", "Buttons — normal, off, on");
		array<string> kinds = {"main", "secondary", "side", "ghost", "danger"};
		foreach (string k : kinds)
		{
			Btn(inner, k, k, "", true, false);
			Btn(inner, k, k, "", false, false);
		}
		Btn(inner, "ghost", "", "eye", true, false);
		Btn(inner, "ghost", "", "lock", true, true);
		Btn(inner, "ghost", "", "trash", false, false);

		Head(inner, "Schalter und Regler", "Switches and slider");
		for (int t = 0; t < 3; t++)
		{
			DZS_Toggle tg = new DZS_Toggle(inner);
			tg.SetOn(t == 0);
			if (t == 2)
				tg.m_Hit.SetEnabled(false);
			m_Toggles.Insert(tg);
		}
		m_Slider = new DZS_Slider(inner, 0, 100);
		m_Slider.SetValue(30);

		Head(inner, "Zahlenfelder und Suchfeld", "Number fields and search field");
		array<string> letters = {"X", "Y", "Z", "×"};
		array<string> colors = {"axisX", "axisY", "axisZ", "textMuted"};
		for (int f = 0; f < 4; f++)
		{
			DZS_NumberField nf = new DZS_NumberField("sheet.f" + f, letters[f], colors[f], 120);
			nf.Build(inner);
			nf.ForceValue("4600,500");
			if (f == 2)
				nf.EnableReset(DZS_Texts.Pick("Auf 0", "To 0"));
			if (f == 3)
				nf.SetEnabled(false);
			m_Fields.Insert(nf);
		}
		for (int s = 0; s < 2; s++)
		{
			DZS_SearchField sf = new DZS_SearchField("sheet.s" + s, DZS_Texts.Pick("Name oder Klassenname …", "Name or class name …"));
			sf.SetWidth(220);
			sf.Build(inner);
			if (s == 1)
				sf.SetText("Fass");
			m_Searches.Insert(sf);
		}

		Head(inner, "Kategorie-Knöpfe und Tastenkappen", "Category buttons and key caps");
		array<string> chipsDe = {"Alle", "Gebäude", "Militär", "Pflanzen"};
		array<string> chipsEn = {"All", "Buildings", "Military", "Plants"};
		array<string> chips = chipsDe;
		if (DZS_Texts.Pick("de", "en") == "en")
			chips = chipsEn;
		for (int c = 0; c < chips.Count(); c++)
		{
			DZS_Chip ch = new DZS_Chip(inner, "sheet.c" + c, chips[c]);
			ch.SetOn(c == 0);
			if (c == 3)
				ch.SetEnabled(false, DZS_Texts.Pick("Ausgegraut", "Greyed out"));
			m_Chips.Insert(ch);
		}
		m_Caps = new DZS_RichLine(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_Caps.Set(DZS_Texts.Pick("[Ctrl+S] speichern · [Esc] abbrechen · [Shift]+Klick mehrere", "[Ctrl+S] save · [Esc] cancel · [Shift]+click several"));

		Head(inner, "Meldungen (Platten) je Ton", "Messages (plates) per tone");
		array<string> tones = {"info", "success", "warning", "danger"};
		foreach (string tone : tones)
			m_Plates.Insert(new DZS_Plate(inner, tone, DZS_Texts.Pick("Meldung ", "Message ") + tone));

		Head(inner, "Leerer Zustand, Fakten, Text", "Empty state, facts, text");
		array<string> notes = {DZS_Texts.Pick("Nichts ausgewählt — ein Objekt in der Szene anklicken", "Nothing selected — click an object in the scene"), DZS_Texts.Pick("Maße  2,1 × 1,0 × 1,2 m", "Size  2.1 × 1.0 × 1.2 m"), DZS_Texts.Pick("Konsole: spawnt", "Console: spawns")};
		array<string> noteColors = {"textMuted", "text", "textQuiet"};
		for (int n = 0; n < notes.Count(); n++)
		{
			TextWidget nt = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get(noteColors[n]));
			nt.SetText(DZS_W.Literal(notes[n]));
			m_Notes.Insert(nt);
		}

		Head(inner, "Kontextmenü (Eintrag normal, mit Taste, ausgegraut)", "Context menu (entry normal, with key, greyed out)");
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		array<string> namesDe = {"Ausrichten", "Sperren", "Loot bearbeiten"};
		array<string> namesEn = {"Align", "Lock", "Edit loot"};
		array<string> names = namesDe;
		if (DZS_Texts.Pick("de", "en") == "en")
			names = namesEn;
		for (int mi = 0; mi < names.Count(); mi++)
		{
			DZS_MenuItem it = new DZS_MenuItem();
			it.m_Text = names[mi];
			it.m_Enabled = mi < 2;
			it.m_Reason = DZS_Texts.Pick("Loot ist aus", "Loot is off");
			if (mi == 1)
				it.m_Binding = "L";
			items.Insert(it);
		}
		m_Menu = new DZS_Popup(inner, items, "sheet");

		Head(inner, "Statuslampe je Farbe", "Status lamp per colour");
		array<string> lampTones = {"success", "warning", "danger", ""};
		array<string> lampDe = {"verbunden", "verbindet …", "getrennt", "nicht angemeldet"};
		array<string> lampEn = {"connected", "connecting …", "disconnected", "not signed in"};
		for (int li = 0; li < lampTones.Count(); li++)
		{
			DZS_StatusText lamp = new DZS_StatusText("sheet.lamp" + li, "label");
			lamp.Build(inner);
			lamp.SetText(DZS_Texts.Pick(lampDe[li], lampEn[li]));
			lamp.SetLamp(lampTones[li]);
			if (lampTones[li] == "")
				lamp.SetColorName("textQuiet");
			m_Lamps.Insert(lamp);
		}

		if (DZS_SheetParts.s_Types)
		{
			foreach (typename pt : DZS_SheetParts.s_Types)
			{
				DZS_SheetPart part = DZS_SheetPart.Cast(pt.Spawn());
				if (!part)
					continue;
				Head(inner, part.TitleDe(), part.TitleEn());
				part.Build(inner);
				m_Parts.Insert(part);
			}
		}

		Head(inner, "Öffnet: Bestätigung, Meldung, Kontextmenü, Prüfliste", "Opens: confirmation, message, context menu, check list");
		array<string> openTags = {"confirm", "message", "context", "checksave", "checkread"};
		array<string> openDe = {"Bestätigung …", "Meldung", "Kontextmenü …", "Prüfliste (Speichern) …", "Prüfliste (Lesen) …"};
		array<string> openEn = {"Confirmation …", "Message", "Context menu …", "Check list (save) …", "Check list (read) …"};
		for (int o = 0; o < openTags.Count(); o++)
		{
			DZS_Button ob = new DZS_Button(inner, "secondary", DZS_Texts.Pick(openDe[o], openEn[o]), "", 28);
			ob.m_Tag = openTags[o];
			ob.m_OnClick.Insert(OnOpener);
			m_Openers.Insert(ob);
		}
		AddFootFocus();
		DZS_Log.Info("block sheet open");
	}

	void OnOpener(DZS_Button b)
	{
		DZS_Screen scr = DZS_Kernel.Get().GetScreen();
		if (b.m_Tag == "confirm")
		{
			DZS_ConfirmDialog d = new DZS_ConfirmDialog(DZS_Texts.Pick("Muster", "Sample"), DZS_Texts.Pick("Bestätigung", "Confirmation"), DZS_Texts.Pick("So sieht eine Rückfrage aus.", "This is what a question looks like."));
			d.Open();
		}
		else if (b.m_Tag == "context")
		{
			float bx, by, bw, bh;
			b.GetWidget().GetScreenPos(bx, by);
			b.GetWidget().GetScreenSize(bw, bh);
			array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
			array<string> de = {"Ausrichten", "Sperren", "Loot bearbeiten"};
			array<string> en = {"Align", "Lock", "Edit loot"};
			for (int i = 0; i < de.Count(); i++)
			{
				DZS_MenuItem it = new DZS_MenuItem();
				it.m_Text = DZS_Texts.Pick(de[i], en[i]);
				it.m_Enabled = i < 2;
				it.m_Reason = DZS_Texts.Pick("Loot ist aus", "Loot is off");
				if (i == 1)
					it.m_Binding = "L";
				items.Insert(it);
			}
			DZS_Popup pop = scr.OpenContextMenu(items, bx, by + bh + DZS_Scale.Px(2));
			pop.m_OnChoose.Insert(OnSampleChoose);
			DZS_Log.Info("block sheet context menu open");
		}
		else if (b.m_Tag == "checksave" || b.m_Tag == "checkread")
		{
			DZS_Issues iss = new DZS_Issues();
			iss.Add(DZS_Issue.ERROR, "Klasse unbekannt — wird nicht geschrieben", "Unknown class — not written", "Land_Muster_X");
			iss.Add(DZS_Issue.WARNING, "Größe 1,4 — die Datei kennt keine Größe", "Scale 1.4 — the file has no scale", "Barrel_Blue");
			iss.Add(DZS_Issue.HINT, "customString leer", "customString empty", DZS_Texts.Pick("Fass 2", "Barrel 2"));
			string mode = "read";
			if (b.m_Tag == "checksave")
			{
				mode = "save";
				iss.m_Items[0].m_Confirmable = true;
			}
			DZS_CheckListDialog cl = new DZS_CheckListDialog(iss, mode, "Muster", "Sample");
			cl.Open();
		}
		else
			scr.ShowMessage("sheet.msg", "info", DZS_Texts.Pick("Meldung mit [Esc] Tastenkappe", "Message with [Esc] key cap"), 3);
	}

	void OnSampleChoose(DZS_Popup p, DZS_MenuItem it)
	{
		DZS_Log.Info("block sheet context menu choose " + it.m_Text);
		DZS_Kernel.Get().GetScreen().CloseMenu();
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		float gap = DZS_Scale.Px(8);
		float row = DZS_Scale.Px(36);
		int hi = 0;
		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		float x = 0;
		foreach (DZS_Button b : m_Buttons)
		{
			if (x + b.GetWidth() > width)
			{
				x = 0;
				y += row;
			}
			b.Place(x, y);
			x += b.GetWidth() + gap;
		}
		y += row + DZS_Scale.Px(8);

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		x = 0;
		foreach (DZS_Toggle tg : m_Toggles)
		{
			tg.Place(x, y + DZS_Scale.Px(6));
			x += DZS_Scale.Px(56);
		}
		m_Slider.Place(x + DZS_Scale.Px(16), y + DZS_Scale.Px(6));
		y += row + DZS_Scale.Px(8);

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		x = 0;
		foreach (DZS_NumberField nf : m_Fields)
		{
			nf.Place(x, y, DZS_Scale.Px(28));
			x += DZS_Scale.Px(132);
		}
		y += row;
		x = 0;
		foreach (DZS_SearchField sf : m_Searches)
		{
			sf.Place(x, y, DZS_Scale.Px(28));
			x += DZS_Scale.Px(236);
		}
		y += row + DZS_Scale.Px(8);

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		x = 0;
		foreach (DZS_Chip ch : m_Chips)
		{
			ch.Place(x, y);
			x += DZS_Scale.Px(100);
		}
		y += row;
		m_Caps.Place(0, y);
		y += row + DZS_Scale.Px(8);

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		x = 0;
		foreach (DZS_Plate pl : m_Plates)
		{
			pl.Place(x, y);
			x += pl.m_W + gap;
		}
		y += row + DZS_Scale.Px(8);

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		foreach (TextWidget nt : m_Notes)
		{
			DZS_W.Place(nt, 0, y, width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(24);
		}
		y += DZS_Scale.Px(8);

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		m_Menu.Open(0, y);
		y += DZS_Scale.Px(110);

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		hi++;
		y += DZS_Scale.Px(22);
		x = 0;
		foreach (DZS_StatusText lamp : m_Lamps)
		{
			lamp.Place(x, y, DZS_Scale.Px(20));
			x += lamp.GetWidth() + DZS_Scale.Px(24);
		}
		y += row;

		foreach (DZS_SheetPart part : m_Parts)
		{
			DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
			hi++;
			y += DZS_Scale.Px(22);
			y += part.Layout(width, y) + DZS_Scale.Px(8);
		}

		DZS_W.Place(m_Heads[hi], 0, y, width, DZS_Scale.Px(16));
		y += DZS_Scale.Px(22);
		x = 0;
		foreach (DZS_Button ob : m_Openers)
		{
			ob.Place(x, y);
			x += ob.GetWidth() + gap;
		}
		y += row;
		return y;
	}

	override void UpdateContent(float dt)
	{
		foreach (DZS_NumberField nf : m_Fields)
			nf.Update(dt);
		foreach (DZS_SearchField sf : m_Searches)
			sf.Update(dt);
		foreach (DZS_SheetPart part : m_Parts)
			part.Update(dt);
	}

	override void OnFoot(string tag)
	{
		Close();
	}
}
