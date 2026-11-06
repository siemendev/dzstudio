// The properties part of the project bar: fixed 280 px, rolls under its head.
class DZS_PropPanel : DZS_PanelContent
{
	static const int MODE_EMPTY = 0;
	static const int MODE_ONE = 1;
	static const int MODE_MANY = 2;

	DZS_PropertiesPlugin m_P;
	protected Widget m_Root;
	protected int m_Mode = -1;
	protected bool m_Dirty = true;
	protected string m_SelKey;
	protected string m_Id;
	protected ref DZS_PropObj m_Obj;

	// empty
	protected ImageWidget m_EmptyIcon;
	protected ref DZS_WrapText m_EmptyText;
	protected ref DZS_HitCall m_ClassCall;

	// one object
	protected Widget m_TileBg;
	protected ImageWidget m_TileIcon;
	protected TextWidget m_Name;
	protected TextWidget m_GroupText;
	protected ref DZS_Hit m_ClassHit;
	protected Widget m_ClassBg;
	protected TextWidget m_ClassText;
	protected ImageWidget m_WarnIcon;
	protected ref DZS_WrapText m_WarnText;
	protected bool m_WarnShown;
	protected ref array<TextWidget> m_RowLabels;
	protected ref array<ref DZS_NumberField> m_Fields;
	protected ref DZS_PropVariantRow m_Variant;
	protected Widget m_PluginArea;
	protected ref array<ref DZS_PropertyRow> m_PluginRows;
	protected ref DZS_Button m_Details;
	protected float m_Measured;
	protected bool m_HasGround;
	protected string m_MeasureKey;
	protected int m_MeasureWait;

	// drag in a field
	protected int m_DragField = -1;
	protected ref DZS_PropObj m_DragStart;
	protected float m_DragValue;
	protected float m_DragMeasured;
	protected int m_FocusSerial;

	// several objects
	protected TextWidget m_ManyHead;
	protected TextWidget m_ManyLocked;
	protected ref array<TextWidget> m_FactLabels;
	protected ref array<ref DZS_WrapText> m_FactValues;
	protected ref DZS_WrapText m_ManyHint;
	protected bool m_HiddenShown;

	void DZS_PropPanel(DZS_PropertiesPlugin p)
	{
		m_P = p;
		m_PluginRows = new array<ref DZS_PropertyRow>();
	}


	void MarkDirty() { m_Dirty = true; }

	override void Build(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_EmptyIcon = DZS_W.Icon(m_Root, "mouse-pointer-2", DZS_Color.Get("textQuiet"));
		m_EmptyText = new DZS_WrapText(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"), true);
		m_EmptyText.SetText(DZS_Texts.Pick("Nichts ausgewählt — ein Objekt in der Szene anklicken oder eine Auswahl ziehen", "Nothing selected — click an object in the scene or drag a selection"));

		m_TileBg = DZS_W.Panel(m_Root, DZS_Color.Get("well"));
		m_TileIcon = DZS_W.Icon(m_Root, "package", DZS_Color.Get("textQuiet"));
		m_Name = DZS_W.Text(m_Root, DZS_Font.READBOLD, 13, DZS_Color.Get("text"));
		m_GroupText = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		m_ClassCall = new DZS_HitCall();
		m_ClassCall.m_OnClick.Insert(OnClassClick);
		m_ClassHit = new DZS_Hit(m_Root, m_ClassCall, "properties.class");
		m_ClassHit.m_FocusKind = "";
		m_ClassBg = DZS_W.Panel(m_ClassHit.m_W, DZS_Color.Get("well"));
		m_ClassText = DZS_W.Text(m_ClassHit.m_W, DZS_Font.MONO, 11, DZS_Color.Get("text"));
		m_WarnIcon = DZS_W.Icon(m_Root, "triangle-alert", DZS_Color.Get("warning"));
		m_WarnText = new DZS_WrapText(m_Root, DZS_Font.READ, 12, DZS_Color.Get("warning"));

		m_RowLabels = new array<TextWidget>();
		array<string> rows = {DZS_Texts.Pick("Position", "Position"), DZS_Texts.Pick("Drehung", "Rotation"), DZS_Texts.Pick("Größe und Höhenversatz", "Size and height offset")};
		foreach (string r : rows)
			m_RowLabels.Insert(DZS_PropertiesApi.RowLabel(m_Root, r));
		m_Fields = new array<ref DZS_NumberField>();
		array<string> letters = {"X", "Y", "Z", "G", "N", "R", "×", "H"};
		array<string> colors = {"axisX", "axisY", "axisZ", "textMuted", "textMuted", "textMuted", "textMuted", "textMuted"};
		for (int i = 0; i < 8; i++)
		{
			DZS_NumberField f = new DZS_NumberField("properties.f" + i, letters[i], colors[i], 90);
			f.MarkChangesSelection();
			f.Build(m_Root);
			f.EnableDrag(DZS_PropertiesPlugin.FieldName(i));
			if (i >= 3 && i <= 5)
				f.EnableReset(DZS_Texts.Pick("Auf 0°", "To 0°"));
			if (i == DZS_PropertiesPlugin.F_SIZE)
				f.EnableReset(DZS_Texts.Pick("Auf 1", "To 1"));
			f.m_OnCommit.Insert(OnCommit);
			f.m_OnWheel.Insert(OnWheel);
			f.m_OnDrag.Insert(OnDrag);
			f.m_OnReset.Insert(OnReset);
			f.m_OnFocus.Insert(OnFocus);
			m_Fields.Insert(f);
		}
		m_Variant = new DZS_PropVariantRow(m_P, m_Root, false);
		m_PluginArea = DZS_W.Frame(m_Root);
		foreach (DZS_PropertyRow pr : m_P.m_Rows)
			AddPluginRow(pr);
		m_Details = new DZS_Button(m_Root, "side", "Details …", "", 28);
		m_Details.m_Hit.m_FocusKind = "";
		m_Details.m_OnClick.Insert(OnDetails);

		m_ManyHead = DZS_W.Text(m_Root, DZS_Font.READBOLD, 13, DZS_Color.Get("text"));
		m_ManyLocked = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_FactLabels = new array<TextWidget>();
		m_FactValues = new array<ref DZS_WrapText>();
		array<string> facts = {DZS_Texts.Pick("Arten", "Kinds"), DZS_Texts.Pick("Gruppen", "Groups"), DZS_Texts.Pick("Mitte", "Centre"), DZS_Texts.Pick("Ausdehnung", "Extent"), DZS_Texts.Pick("Ausgeblendet", "Hidden")};
		foreach (string fl : facts)
		{
			m_FactLabels.Insert(DZS_PropertiesApi.RowLabel(m_Root, fl));
			m_FactValues.Insert(new DZS_WrapText(m_Root, DZS_Font.READ, 13, DZS_Color.Get("text")));
		}
		m_ManyHint = new DZS_WrapText(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
		m_ManyHint.SetText(DZS_Texts.Pick("Lage, Drehung und Größe der ganzen Auswahl: Position … an der Auswahl", "Position, rotation and size of the whole selection: Position … at the selection"));
		ShowMode(MODE_EMPTY);
	}

	void AddPluginRow(DZS_PropertyRow row)
	{
		if (!m_PluginArea || m_PluginRows.Find(row) >= 0)
			return;
		row.Build(m_PluginArea);
		m_PluginRows.Insert(row);
	}

	protected void ShowMode(int mode)
	{
		if (mode == m_Mode)
			return;
		m_Mode = mode;
		bool empty = mode == MODE_EMPTY;
		bool one = mode == MODE_ONE;
		bool many = mode == MODE_MANY;
		m_EmptyIcon.Show(empty);
		m_EmptyText.Show(empty);
		m_TileBg.Show(one);
		m_TileIcon.Show(one);
		m_Name.Show(one);
		m_GroupText.Show(one);
		m_ClassHit.m_W.Show(one);
		m_WarnIcon.Show(one && m_WarnShown);
		m_WarnText.Show(one && m_WarnShown);
		foreach (TextWidget rl : m_RowLabels)
			rl.Show(one);
		foreach (DZS_NumberField f : m_Fields)
			f.Show(one);
		if (!one)
			m_Variant.Show(false);
		m_PluginArea.Show(one || many);
		m_Details.Show(one);
		m_ManyHead.Show(many);
		m_ManyLocked.Show(many);
		foreach (TextWidget l : m_FactLabels)
			l.Show(many);
		foreach (DZS_WrapText v : m_FactValues)
			v.Show(many);
		m_ManyHint.Show(many);
	}

	bool IsDragging() { return m_DragField >= 0; }

	void CancelDrag()
	{
		if (m_DragField >= 0)
			m_Fields[m_DragField].CancelDrag();
	}

	// ---- per frame ----

	override void Update(float dt)
	{
		if (!m_Root)
			return;
		array<string> ids = m_P.SelIds();
		DZS_Instance inst = m_P.Inst();
		string key = "";
		DZS_SelectionApi selApi = DZS_SelectionApi.Get();
		if (selApi)
			key = selApi.Signature();
		else
		{
			foreach (string sid : ids)
				key += sid + ",";
		}
		if (key != m_SelKey)
		{
			m_SelKey = key;
			m_Dirty = true;
			if (m_DragField >= 0)
				CancelDrag();
		}
		if (!inst || ids.Count() == 0 || (Skip() & 1) != 0)
		{
			ShowMode(MODE_EMPTY);
			return;
		}
		if (ids.Count() == 1)
		{
			ShowMode(MODE_ONE);
			UpdateOne(inst, ids[0]);
			foreach (DZS_NumberField f : m_Fields)
				f.Update(dt);
			m_Variant.Update(dt);
		}
		else
		{
			ShowMode(MODE_MANY);
			if (m_Dirty)
				UpdateMany(inst, ids);
		}
		foreach (DZS_PropertyRow pr : m_PluginRows)
			pr.Update(dt);
		m_Dirty = false;
	}

	// -dzsPropSkip=MASK (search for crashes): 1 no content, 2 no measuring key, 4 no field values, 8 no plugin rows,
	// 16 no texts on a new selection, 32 no height measuring
	static int Skip()
	{
		string v;
		if (!GetCLIParam("dzsPropSkip", v))
			return 0;
		return v.ToInt();
	}

	protected void UpdateOne(DZS_Instance inst, string id)
	{
		DZS_PropObj o = DZS_PropObj.Read(inst, id);
		if (!o)
			return;
		m_Id = id;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		vector vp, vy;
		float vs;
		DZS_PropObj shown = o.Copy();
		if (scene && scene.GetView(id, vp, vy, vs))
		{
			shown.m_Pos = vp;
			shown.m_Ypr = vy;
			shown.m_Scale = vs;
		}
		m_Obj = o;
		if (m_Dirty && (Skip() & 16) == 0)
		{
			string game = DZS_PropText.GameName(o.m_Name);
			string warn = DZS_PropText.Placeholder(o.m_Name);
			if (warn != "")
				game = o.m_Name;
			if (game == "")
				game = "—";
			DZS_W.FitText(m_Name, game, DZS_Scale.Px(240));
			DZS_W.SetIcon(m_TileIcon, DZS_PropText.Icon(o.m_Name));
			string gname;
			inst.GetField(o.m_Group, DZS_Obj.GROUP_NAME, gname);
			m_GroupText.SetText(DZS_W.Literal(gname + " · "));
			bool cut = DZS_W.FitText(m_ClassText, o.m_Name, DZS_Scale.Px(200) - DZS_W.TextWidth(m_GroupText));
			string tip = "";
			if (cut)
				tip = o.m_Name;
			m_ClassHit.SetTip(tip);
			m_WarnText.SetText(warn);
			m_WarnShown = warn != "";
			m_WarnIcon.Show(m_WarnShown);
			m_WarnText.Show(m_WarnShown);
			string lockTip = "";
			if (o.m_Locked)
				lockTip = m_P.LockedTip();
			bool busy = m_P.PhysicsRunning();
			for (int i = 0; i < 8; i++)
			{
				m_Fields[i].SetEnabled(!o.m_Locked && !busy);
				string ft = DZS_PropertiesPlugin.FieldName(i);
				if (o.m_Locked)
					ft = lockTip;
				m_Fields[i].SetFieldTip(ft);
			}
			string binding = m_P.Kernel().GetUi().TipBinding("properties.details");
			m_Details.SetCaps(binding);
			m_Details.SetEnabled(!o.m_Locked, lockTip);
			m_Details.SetTipAction("Details …", "properties.details");
			m_Variant.SetObject(inst, o);
		}
		if (m_Dirty)
			m_MeasureWait = 2;
		if (m_MeasureWait > 0)
		{
			m_MeasureWait--;
			m_MeasureKey = "";
		}
		if ((Skip() & 2) != 0)
			return;
		string mkp = DZS_Obj.Vec(shown.m_Pos[0], shown.m_Pos[1], shown.m_Pos[2]);
		string mky = DZS_Obj.Vec(shown.m_Ypr[0], shown.m_Ypr[1], shown.m_Ypr[2]);
		string mks = shown.m_Scale.ToString();
		string mk = mkp + mky + mks;
		if (m_MeasureWait == 0 && mk != m_MeasureKey && m_DragField != DZS_PropertiesPlugin.F_HEIGHT)
		{
			m_MeasureKey = mk;
			float h;
			if ((Skip() & 32) == 0)
				m_HasGround = m_P.Measure(id, h);
			m_Measured = h;
		}
		if ((Skip() & 4) != 0)
			return;
		for (int f = 0; f < 7; f++)
		{
			float v = shown.Value(f);
			m_Fields[f].ShowValue(DZS_PropText.Num(v, DZS_PropertiesPlugin.Decimals(f)));
		}
		if (m_DragField == DZS_PropertiesPlugin.F_HEIGHT)
			m_Fields[7].ShowValue(DZS_PropText.Num(m_DragValue, 3));
		else if (m_HasGround)
			m_Fields[7].ShowValue(DZS_PropText.Num(m_Measured, 3));
		else
			m_Fields[7].ShowValue("—");
		bool hOn = !o.m_Locked && !m_P.PhysicsRunning() && (m_HasGround || m_MeasureWait > 0);
		if (m_Fields[7].IsEnabled() != hOn)
			m_Fields[7].SetEnabled(hOn);
		for (int a = 3; a <= 5; a++)
			m_Fields[a].ShowReset(Math.AbsFloat(shown.Value(a)) >= 0.005);
		m_Fields[6].ShowReset(Math.AbsFloat(shown.m_Scale - 1) >= 0.0005);
	}

	protected void UpdateMany(DZS_Instance inst, array<string> ids)
	{
		int locked = 0;
		int hidden = 0;
		array<string> free = new array<string>();
		map<string, int> kinds = new map<string, int>();
		map<string, string> shownOf = new map<string, string>();
		map<string, bool> groups = new map<string, bool>();
		foreach (string id : ids)
		{
			bool isObject = inst.Has(id) && inst.GetKind(id) == DZS_KIND_OBJECT;
			if (isObject)
			{
				string oname;
				inst.GetField(id, DZS_Obj.NAME, oname);
				string ogroup = inst.GetParent(id);
				if (inst.IsLocked(id) || inst.IsLocked(ogroup))
					locked++;
				else
					free.Insert(id);
				if (inst.IsHidden(id) || inst.IsHidden(ogroup))
					hidden++;
				string cls = oname;
				cls.ToLower();
				kinds.Set(cls, kinds.Get(cls) + 1);
				if (!shownOf.Contains(cls))
				{
					string shown = DZS_PropText.GameName(oname);
					shownOf.Set(cls, shown);
				}
				groups.Set(ogroup, true);
			}
		}
		m_ManyHead.SetText(DZS_Texts.Pick(ids.Count().ToString() + " Objekte", ids.Count().ToString() + " objects"));
		string lockText = "";
		if (locked > 0)
			lockText = DZS_Texts.Pick("· " + locked + " gesperrt", "· " + locked + " locked");
		m_ManyLocked.SetText(lockText);
		// kinds: by count descending, equal counts by name
		array<string> names = new array<string>();
		for (int k = 0; k < kinds.Count(); k++)
			names.Insert(kinds.GetKey(k));
		for (int x = 0; x < names.Count(); x++)
		{
			for (int y = x + 1; y < names.Count(); y++)
			{
				int cx = kinds.Get(names[x]);
				int cy = kinds.Get(names[y]);
				string sx = shownOf.Get(names[x]);
				string sy = shownOf.Get(names[y]);
				if (cy > cx || (cy == cx && sy < sx))
				{
					string t = names[x];
					names[x] = names[y];
					names[y] = t;
				}
			}
		}
		string kindText = "";
		for (int n = 0; n < names.Count() && n < 4; n++)
		{
			if (n > 0)
				kindText += "\n";
			string kn = names[n];
			string ks = shownOf.Get(kn);
			int kc = kinds.Get(kn);
			kindText += ks + " ×" + kc;
		}
		if (names.Count() > 4)
			kindText += "\n" + DZS_Texts.Pick("+" + (names.Count() - 4) + " weitere Arten", "+" + (names.Count() - 4) + " more kinds");
		m_FactValues[0].SetText(kindText);
		// groups in the order of the group list
		array<string> order = inst.GetChildren(DZS_PROJECT_ID);
		string groupText = "";
		int gcount = 0;
		foreach (string g : order)
		{
			if (!groups.Contains(g))
				continue;
			string gname;
			inst.GetField(g, DZS_Obj.GROUP_NAME, gname);
			if (gcount > 0)
				groupText += ", ";
			groupText += gname;
			gcount++;
		}
		if (gcount > 3)
			groupText = DZS_Texts.Pick(gcount.ToString() + " Gruppen", gcount.ToString() + " groups");
		m_FactValues[1].SetText(groupText);
		vector mn, mx;
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (free.Count() > 0 && sel && sel.GetBounds(free, mn, mx))
		{
			vector c = (mn + mx) * 0.5;
			string c0 = DZS_PropText.Num(c[0], 2);
			string c1 = DZS_PropText.Num(c[1], 2);
			string c2 = DZS_PropText.Num(c[2], 2);
			string ct = c0 + " / " + c1 + " / " + c2;
			m_FactValues[2].SetText(ct);
			string e0 = DZS_PropText.Num(mx[0] - mn[0], 1);
			string e1 = DZS_PropText.Num(mx[2] - mn[2], 1);
			string e2 = DZS_PropText.Num(mx[1] - mn[1], 1);
			string et = e0 + " × " + e1 + " × " + e2;
			m_FactValues[3].SetText(et);
		}
		else
		{
			m_FactValues[2].SetText("—");
			m_FactValues[3].SetText("—");
		}
		m_FactValues[4].SetText(hidden.ToString());
		m_HiddenShown = hidden > 0;
		m_FactLabels[4].Show(m_HiddenShown);
		m_FactValues[4].Show(m_HiddenShown);
		DZS_Log.Info("properties: " + ids.Count() + " objects, kinds '" + kindText + "', groups '" + groupText + "', centre " + m_FactValues[2].GetText() + ", extent " + m_FactValues[3].GetText());
	}

	// ---- layout ----

	override float Layout(float width)
	{
		if (!m_Root)
			return 0;
		float pad = DZS_Scale.Px(12);
		float y = DZS_Scale.Px(10);
		float w = width - 2 * pad;
		DZS_W.Place(m_Root, 0, 0, width, DZS_Scale.Px(600));
		if (m_Mode == MODE_EMPTY || m_Mode < 0)
		{
			float isz = DZS_Scale.Px(24);
			DZS_W.Place(m_EmptyIcon, (width - isz) / 2, DZS_Scale.Px(60), isz, isz);
			m_EmptyText.Place(pad, DZS_Scale.Px(92), w);
			return DZS_Scale.Px(160);
		}
		if (m_Mode == MODE_ONE)
		{
			float tile = DZS_Scale.Px(32);
			DZS_W.Place(m_TileBg, pad, y, tile, tile);
			float ic = DZS_Scale.Px(18);
			DZS_W.Place(m_TileIcon, pad + (tile - ic) / 2, y + (tile - ic) / 2, ic, ic);
			float tx = pad + tile + DZS_Scale.Px(10);
			DZS_W.Place(m_Name, tx, y - DZS_Scale.Px(2), width - tx - pad, DZS_Scale.Px(18));
			float gw = DZS_W.TextWidth(m_GroupText);
			DZS_W.Place(m_GroupText, tx, y + DZS_Scale.Px(16), gw + 2, DZS_Scale.Px(18));
			float cw = DZS_W.TextWidth(m_ClassText) + DZS_Scale.Px(8);
			m_ClassHit.Place(tx + gw + DZS_Scale.Px(2), y + DZS_Scale.Px(17), cw, DZS_Scale.Px(16));
			DZS_W.Place(m_ClassBg, 0, 0, cw, DZS_Scale.Px(16));
			DZS_W.Place(m_ClassText, DZS_Scale.Px(4), 0, cw, DZS_Scale.Px(16));
			y += tile + DZS_Scale.Px(8);
			if (m_WarnShown)
			{
				float wi = DZS_Scale.Px(14);
				DZS_W.Place(m_WarnIcon, pad, y + DZS_Scale.Px(2), wi, wi);
				y += m_WarnText.Place(pad + wi + DZS_Scale.Px(6), y, w - wi - DZS_Scale.Px(6)) + DZS_Scale.Px(4);
			}
			float gap = DZS_Scale.Px(6);
			float fw3 = (w - 2 * gap) / 3;
			float fh = DZS_Scale.Px(28);
			for (int r = 0; r < 3; r++)
			{
				DZS_W.Place(m_RowLabels[r], pad, y, w, DZS_Scale.Px(14));
				y += DZS_Scale.Px(15);
				int count = 3;
				if (r == 2)
					count = 2;
				for (int c = 0; c < count; c++)
				{
					DZS_NumberField f = m_Fields[r * 3 + c];
					int wref = fw3 / DZS_Scale.Factor();
					f.SetWidthRef(wref);
					f.Place(pad + c * (fw3 + gap), y, fh);
				}
				y += fh + DZS_Scale.Px(4);
			}
			y += m_Variant.Layout(pad, y, w);
			float py = 0;
			DZS_Instance inst = m_P.Inst();
			array<string> ids = m_P.SelIds();
			foreach (DZS_PropertyRow pr : m_PluginRows)
			{
				bool on = inst && (Skip() & 8) == 0 && pr.AppliesTo(inst, ids);
				pr.Show(on);
				if (on)
					py += pr.Layout(inst, ids, py, w);
			}
			DZS_W.Place(m_PluginArea, pad, y, w, py);
			y += py;
			float bw = m_Details.GetWidth();
			m_Details.Place(width - pad - bw, y + DZS_Scale.Px(4));
			y += m_Details.GetHeight() + DZS_Scale.Px(12);
			return y;
		}
		// several objects
		float hw = DZS_W.TextWidth(m_ManyHead);
		DZS_W.Place(m_ManyHead, pad, y, hw + 2, DZS_Scale.Px(18));
		DZS_W.Place(m_ManyLocked, pad + hw + DZS_Scale.Px(6), y, w - hw, DZS_Scale.Px(18));
		y += DZS_Scale.Px(26);
		float pairH = 0;
		for (int i = 0; i < 5; i++)
		{
			if (i == 4 && !m_HiddenShown)
				continue;
			float colX = pad;
			float colW = w;
			if (i == 2 || i == 3)
			{
				colW = (w - DZS_Scale.Px(8)) / 2;
				if (i == 3)
					colX = pad + colW + DZS_Scale.Px(8);
			}
			DZS_W.Place(m_FactLabels[i], colX, y, colW, DZS_Scale.Px(14));
			float vh = m_FactValues[i].Place(colX, y + DZS_Scale.Px(15), colW);
			if (i == 2)
				pairH = vh;
			else
			{
				y += DZS_Scale.Px(15) + Math.Max(vh, pairH) + DZS_Scale.Px(8);
				pairH = 0;
			}
		}
		float ppy = 0;
		DZS_Instance minst = m_P.Inst();
		array<string> mids = m_P.SelIds();
		foreach (DZS_PropertyRow mpr : m_PluginRows)
		{
			bool mon = minst && mpr.AppliesTo(minst, mids);
			mpr.Show(mon);
			if (mon)
				ppy += mpr.Layout(minst, mids, ppy, w);
		}
		DZS_W.Place(m_PluginArea, pad, y, w, ppy);
		y += ppy;
		y += m_ManyHint.Place(pad, y, w);
		return y + DZS_Scale.Px(10);
	}

	static int CountLines(string s)
	{
		int n = 1;
		int len = s.Length();
		for (int i = 0; i < len; i++)
		{
			if (s.Get(i) == "\n")
				n++;
		}
		return n;
	}

	// ---- fields ----

	protected int IndexOf(DZS_NumberField f)
	{
		return m_Fields.Find(f);
	}

	void OnCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		int i = IndexOf(f);
		float v;
		if (i < 0 || !DZS_Num.Parse(typed, v))
		{
			r.m_Ok = false;
			return;
		}
		v = DZS_PropertiesPlugin.Clamp(i, DZS_PropText.Round(v, DZS_PropertiesPlugin.Decimals(i)));
		r.m_Ok = true;
		m_P.ApplyValue(m_Id, i, v);
		m_Dirty = true;
		m_MeasureKey = "";
	}

	void OnWheel(DZS_NumberField f, int wheel, bool fine)
	{
		int i = IndexOf(f);
		if (i < 0 || !m_Obj || m_Obj.m_Locked)
			return;
		float step = DZS_PropertiesPlugin.StepOf(i);
		if (fine)
			step = step * 0.1;
		float cur = m_Obj.Value(i);
		if (i == DZS_PropertiesPlugin.F_HEIGHT)
		{
			if (!m_HasGround)
				return;
			cur = m_Measured;
		}
		float v = DZS_PropertiesPlugin.Clamp(i, DZS_PropText.Round(cur + wheel * step, 4));
		string key = "properties.wheel:" + m_Id + ":" + i + ":" + m_FocusSerial;
		m_P.ApplyValue(m_Id, i, v, key);
		f.ForceValue(DZS_PropText.Num(v, DZS_PropertiesPlugin.Decimals(i)));
		m_MeasureKey = "";
	}

	void OnFocus(DZS_NumberField f, bool focused)
	{
		m_FocusSerial++;
	}

	void OnReset(DZS_NumberField f)
	{
		int i = IndexOf(f);
		if (i < 3 || i > 6)
			return;
		float v = 0;
		if (i == DZS_PropertiesPlugin.F_SIZE)
			v = 1;
		m_P.ApplyValue(m_Id, i, v);
		m_Dirty = true;
	}

	void OnDrag(DZS_NumberField f, int phase, float steps)
	{
		int i = IndexOf(f);
		if (i < 0)
			return;
		if (phase == DZS_NumberField.DRAG_START)
		{
			DZS_PropObj o = DZS_PropObj.Read(m_P.Inst(), m_Id);
			if (!o || o.m_Locked || m_P.PhysicsRunning())
			{
				f.CancelDrag();
				return;
			}
			if (i == DZS_PropertiesPlugin.F_HEIGHT && !m_HasGround)
			{
				f.CancelDrag();
				return;
			}
			m_DragField = i;
			m_DragStart = o;
			m_DragMeasured = m_Measured;
			m_DragValue = StartValue(i);
			DZS_Log.Info("properties: drag start field " + i + " value " + m_DragValue);
			return;
		}
		if (m_DragField != i || !m_DragStart)
			return;
		float v = DZS_PropertiesPlugin.Clamp(i, StartValue(i) + steps * DZS_PropertiesPlugin.StepOf(i));
		v = DZS_PropText.Round(v, DZS_PropertiesPlugin.Decimals(i) + 1);
		if (phase == DZS_NumberField.DRAG_MOVE)
		{
			m_DragValue = v;
			m_P.Preview(m_DragStart, i, v, m_DragMeasured);
			return;
		}
		m_DragField = -1;
		if (phase == DZS_NumberField.DRAG_CANCEL)
		{
			m_P.ClearPreview(m_Id);
			DZS_Log.Info("properties: drag cancelled field " + i);
			m_DragStart = null;
			m_MeasureKey = "";
			return;
		}
		float end = DZS_PropText.Round(v, DZS_PropertiesPlugin.Decimals(i));
		DZS_Log.Info("properties: drag end field " + i + " value " + end + " (" + steps + " steps)");
		if (!m_P.ApplyValue(m_Id, i, end))
			m_P.ClearPreview(m_Id);
		m_DragStart = null;
		m_MeasureKey = "";
	}

	protected float StartValue(int i)
	{
		if (i == DZS_PropertiesPlugin.F_HEIGHT)
			return m_DragMeasured;
		return m_DragStart.Value(i);
	}

	void OnDetails(DZS_Button b)
	{
		m_P.OpenDetails();
	}

	// click on the class name: copies it in its spelling, also when the object is locked
	void OnClassClick(DZS_Hit h)
	{
		if (m_Obj)
			m_P.CopyClassName(m_Obj.m_Name);
	}

	override void SetView(float width, float height) {}
}
