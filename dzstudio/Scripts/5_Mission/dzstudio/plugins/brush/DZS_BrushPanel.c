// Brush section: Setzen · Radieren, set drop-down, kinds, radius and density, Untergrund.
class DZS_BrushPanel : DZS_PanelContent
{
	DZS_BrushPlugin m_P;
	protected Widget m_Root;
	protected ref DZS_Chip m_Set;
	protected ref DZS_Chip m_Erase;
	protected ref DZS_Button m_More;
	protected ref DZS_Button m_Edit;
	protected ref DZS_BrushNameHit m_NameHit;
	protected TextWidget m_Name;
	protected Widget m_NameBg;
	protected ref DZS_Border m_NameBorder;
	protected ImageWidget m_Chevron;
	protected ImageWidget m_Warn;
	protected TextWidget m_Kinds;
	protected TextWidget m_RadiusLabel;
	protected TextWidget m_DensityLabel;
	ref DZS_Slider m_Radius;
	ref DZS_Slider m_Density;
	ref DZS_NumberField m_RadiusField;
	ref DZS_NumberField m_DensityField;
	protected TextWidget m_GroundLabel;
	protected ref DZS_Chip m_GroundTerrain;
	protected ref DZS_Chip m_GroundAll;

	void DZS_BrushPanel(DZS_BrushPlugin p)
	{
		m_P = p;
	}

	override void Build(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_Set = new DZS_Chip(m_Root, "set", DZS_Texts.Get("brush.mode.set"));
		m_Set.m_OnClick.Insert(OnMode);
		m_Erase = new DZS_Chip(m_Root, "erase", DZS_Texts.Get("brush.mode.erase"));
		m_Erase.m_OnClick.Insert(OnMode);
		m_More = new DZS_Button(m_Root, "ghost", "", "ellipsis", 28);
		m_More.m_Hit.m_FocusKind = "";
		m_More.m_OnClick.Insert(OnMore);
		m_Edit = new DZS_Button(m_Root, "side", DZS_Texts.Get("brush.own.edit"), "", 28);
		m_Edit.m_OnClick.Insert(OnEdit);
		m_NameHit = new DZS_BrushNameHit(m_Root, this);
		m_NameBg = DZS_W.Panel(m_NameHit.m_Hit.m_W, DZS_Color.Get("well"));
		m_NameBorder = new DZS_Border(m_NameHit.m_Hit.m_W, DZS_Color.Get("line"));
		m_Chevron = DZS_W.Icon(m_NameHit.m_Hit.m_W, "chevron-down", DZS_Color.Get("textMuted"));
		m_Name = DZS_W.Text(m_NameHit.m_Hit.m_W, DZS_Font.READBOLD, 13, DZS_Color.Get("text"));
		m_Warn = DZS_W.Icon(m_NameHit.m_Hit.m_W, "triangle-alert", DZS_Color.Get("warning"));
		m_Kinds = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"), true);
		m_RadiusLabel = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_RadiusLabel.SetText(DZS_Texts.Get("brush.radius"));
		m_DensityLabel = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_DensityLabel.SetText(DZS_Texts.Get("brush.density"));
		m_Radius = new DZS_Slider(m_Root, DZS_BrushPlugin.R_MIN, DZS_BrushPlugin.R_MAX);
		m_Radius.m_OnChange.Insert(OnRadiusSlide);
		m_Density = new DZS_Slider(m_Root, DZS_BrushPlugin.D_MIN, DZS_BrushPlugin.D_MAX);
		m_Density.m_OnChange.Insert(OnDensitySlide);
		m_RadiusField = new DZS_NumberField("brush.radius", "", "textMuted", 56);
		m_RadiusField.Build(m_Root);
		m_RadiusField.m_OnCommit.Insert(OnRadiusCommit);
		m_RadiusField.m_OnWheel.Insert(OnRadiusWheel);
		m_DensityField = new DZS_NumberField("brush.density", "", "textMuted", 56);
		m_DensityField.Build(m_Root);
		m_DensityField.m_OnCommit.Insert(OnDensityCommit);
		m_DensityField.m_OnWheel.Insert(OnDensityWheel);
		m_GroundLabel = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_GroundLabel.SetText(DZS_Texts.Get("brush.ground"));
		m_GroundTerrain = new DZS_Chip(m_Root, "terrain", DZS_Texts.Get("brush.ground.terrain"));
		m_GroundTerrain.m_OnClick.Insert(OnGround);
		m_GroundAll = new DZS_Chip(m_Root, "all", DZS_Texts.Get("brush.ground.all"));
		m_GroundAll.m_OnClick.Insert(OnGround);
	}

	override float Layout(float width)
	{
		float pad = DZS_Scale.Px(12);
		float y = DZS_Scale.Px(10);
		float inner = width - 2 * pad;
		float half = (inner - DZS_Scale.Px(6)) / 2;
		DZS_W.Place(m_Root, 0, 0, width, DZS_Scale.Px(400));
		m_Set.SetOn(!m_P.m_EraseMode);
		m_Erase.SetOn(m_P.m_EraseMode);
		m_Set.Place(pad, y, half);
		m_Erase.Place(pad + half + DZS_Scale.Px(6), y, half);
		y += DZS_Chip.Height() + DZS_Scale.Px(10);
		float bh = DZS_Scale.Px(28);
		m_More.Place(width - pad - bh, y);
		float nx = pad;
		float nw = inner - bh - DZS_Scale.Px(4);
		DZS_BrushSet s = m_P.CurrentSet();
		string name = "—";
		bool warn = false;
		if (s)
		{
			name = s.Name();
			warn = s.Usable(false).Count() == 0;
		}
		m_Name.SetText(DZS_W.Literal(name));
		m_NameHit.m_Hit.Place(nx, y, nw, bh);
		DZS_W.Place(m_NameBg, 0, 0, nw, bh);
		m_NameBorder.Place(0, 0, nw, bh);
		float icon = DZS_Scale.Px(14);
		float chev = DZS_Scale.Px(16);
		float tx = DZS_Scale.Px(8);
		DZS_W.Place(m_Chevron, nw - chev - DZS_Scale.Px(6), (bh - chev) / 2, chev, chev);
		float tw = nw - tx - chev - DZS_Scale.Px(10);
		m_Warn.Show(warn);
		if (warn)
		{
			DZS_W.Place(m_Name, tx + icon + DZS_Scale.Px(4), 0, tw - icon - DZS_Scale.Px(4), bh);
			DZS_W.Place(m_Warn, tx, (bh - icon) / 2, icon, icon);
			m_NameHit.m_Hit.SetTip(DZS_Texts.Get("brush.set.noneInCatalog"));
		}
		else
		{
			DZS_W.Place(m_Name, tx, 0, tw, bh);
			m_NameHit.m_Hit.SetTip(m_P.KindsTip());
		}
		y += bh;
		int kinds = 0;
		if (s)
			kinds = s.Kinds();
		string kindsText = DZS_Texts.Fmt("brush.kinds", kinds.ToString());
		if (s && s.m_Bundled)
			kindsText = DZS_Texts.Fmt("brush.kindsBundled", kinds.ToString());
		m_Kinds.SetText(kindsText);
		DZS_W.Place(m_Kinds, pad, y, inner, DZS_Scale.Px(20));
		y += DZS_Scale.Px(28);
		float lw = DZS_Scale.Px(62);
		float rowH = DZS_Scale.Px(30);
		DZS_W.Place(m_RadiusLabel, pad, y, lw, rowH);
		if (!m_Radius.IsDragging())
			m_Radius.SetValue(m_P.m_Radius);
		m_Radius.Place(pad + lw, y + (rowH - DZS_Slider.Height()) / 2);
		m_RadiusField.ShowValue(DZS_Num.Fixed(m_P.m_Radius, 1));
		m_RadiusField.Place(width - pad - m_RadiusField.GetWidth(), y, rowH);
		y += rowH + DZS_Scale.Px(4);
		DZS_W.Place(m_DensityLabel, pad, y, lw, rowH);
		if (!m_Density.IsDragging())
			m_Density.SetValue(m_P.m_Density);
		m_Density.Place(pad + lw, y + (rowH - DZS_Slider.Height()) / 2);
		m_DensityField.ShowValue(DZS_Num.Fixed(m_P.m_Density, 1));
		m_DensityField.Place(width - pad - m_DensityField.GetWidth(), y, rowH);
		y += rowH + DZS_Scale.Px(10);
		DZS_W.Place(m_GroundLabel, pad, y, inner, DZS_Scale.Px(20));
		y += DZS_Scale.Px(22);
		m_GroundTerrain.SetOn(!m_P.m_OnObjects);
		m_GroundAll.SetOn(m_P.m_OnObjects);
		float gw1 = m_GroundTerrain.Width();
		m_GroundTerrain.Place(pad, y, gw1);
		m_GroundAll.Place(pad + gw1 + DZS_Scale.Px(6), y, Math.Min(m_GroundAll.Width(), inner - gw1 - DZS_Scale.Px(6)));
		y += DZS_Chip.Height() + DZS_Scale.Px(14);
		m_Edit.Place(pad, y);
		y += DZS_Scale.Px(40);
		return y;
	}

	override void Update(float dt)
	{
		m_Radius.Update();
		m_Density.Update();
		m_RadiusField.Update(dt);
		m_DensityField.Update(dt);
	}

	void OnMode(DZS_Chip c)
	{
		m_P.SetEraseMode(c.m_Key == "erase");
	}

	void OnMore(DZS_Button b)
	{
		m_P.OpenMoreMenu();
	}

	void OnEdit(DZS_Button b)
	{
		m_P.OpenEditDialog();
	}

	void OnNameClick()
	{
		float x, y, w, h;
		m_NameHit.m_Hit.m_W.GetScreenPos(x, y);
		m_NameHit.m_Hit.m_W.GetScreenSize(w, h);
		m_P.OpenSetList(x, y + h + DZS_Scale.Px(2), w);
	}

	void OnRadiusSlide(DZS_Slider s, float v)
	{
		m_P.SetRadius(v);
	}

	void OnDensitySlide(DZS_Slider s, float v)
	{
		m_P.SetDensity(v);
	}

	void OnRadiusCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = DZS_Num.Parse(typed, v);
		if (r.m_Ok)
			m_P.SetRadius(v);
	}

	void OnDensityCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = DZS_Num.Parse(typed, v);
		if (r.m_Ok)
			m_P.SetDensity(v);
	}

	void OnRadiusWheel(DZS_NumberField f, int steps, bool fine)
	{
		float step = 1;
		if (fine)
			step = 0.1;
		m_P.SetRadius(m_P.m_Radius + steps * step);
		f.ForceValue(DZS_Num.Fixed(m_P.m_Radius, 1));
	}

	void OnDensityWheel(DZS_NumberField f, int steps, bool fine)
	{
		float step = 0.5;
		if (fine)
			step = 0.1;
		m_P.SetDensity(m_P.m_Density + steps * step);
		f.ForceValue(DZS_Num.Fixed(m_P.m_Density, 1));
	}

	void OnGround(DZS_Chip c)
	{
		m_P.SetOnObjects(c.m_Key == "all");
	}
}

class DZS_BrushNameHit : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	DZS_BrushPanel m_Panel;

	void DZS_BrushNameHit(Widget parent, DZS_BrushPanel panel)
	{
		m_Panel = panel;
		m_Hit = new DZS_Hit(parent, this, "brush.setName");
		m_Hit.m_FocusKind = "";
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_Panel.OnNameClick();
	}
}
