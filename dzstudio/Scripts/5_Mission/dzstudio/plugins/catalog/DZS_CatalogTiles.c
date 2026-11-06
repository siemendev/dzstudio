// Local entities of classes, made like the game's own item preview (at 0 0 0, no physics, no simulation), a few
// per frame: they give the size of a model (dimensions). The game draws its 3D preview widget only as a top-level
// widget and then under every surface of the editor, so tiles show the placeholder instead (see the build plan).
class DZS_CatPreviews : Managed
{
	static const int PER_FRAME = 2;
	static const int KEEP = 80;

	protected ref map<string, EntityAI> m_Made;
	protected ref array<string> m_Order;
	protected ref map<string, bool> m_Failed;
	protected int m_Budget;

	void DZS_CatPreviews()
	{
		m_Made = new map<string, EntityAI>();
		m_Order = new array<string>();
		m_Failed = new map<string, bool>();
	}

	void NewFrame()
	{
		m_Budget = PER_FRAME;
	}

	// The entity for a class, or null while it is not made yet (or cannot be made).
	EntityAI Get(DZS_CatEntry e)
	{
		if (!e || e.m_Root == "" || e.m_IsTemplate)
			return null;
		EntityAI ent = m_Made.Get(e.m_Low);
		if (ent)
		{
			int at = m_Order.Find(e.m_Low);
			if (at >= 0)
				m_Order.Remove(at);
			m_Order.Insert(e.m_Low);
			return ent;
		}
		if (m_Failed.Contains(e.m_Low) || m_Budget <= 0)
			return null;
		m_Budget--;
		ent = EntityAI.Cast(GetGame().CreateObjectEx(e.m_Key, "0 0 0", ECE_LOCAL | ECE_CREATEPHYSICS | ECE_TRACE));
		if (!ent)
		{
			m_Failed.Set(e.m_Low, true);
			return null;
		}
		dBodyDestroy(ent);
		ent.DisableSimulation(true);
		ent.SetAllowDamage(false);
		m_Made.Set(e.m_Low, ent);
		m_Order.Insert(e.m_Low);
		while (m_Order.Count() > KEEP)
		{
			string old = m_Order[0];
			m_Order.Remove(0);
			EntityAI o = m_Made.Get(old);
			m_Made.Remove(old);
			if (o)
				GetGame().ObjectDelete(o);
		}
		return ent;
	}

	bool Pending(DZS_CatEntry e)
	{
		return e && e.m_Root != "" && !e.m_IsTemplate && !m_Failed.Contains(e.m_Low) && !m_Made.Contains(e.m_Low);
	}

	void Clear()
	{
		foreach (string k, EntityAI ent : m_Made)
		{
			if (ent)
				GetGame().ObjectDelete(ent);
		}
		m_Made.Clear();
		m_Order.Clear();
	}
}

class DZS_CatTileOwner : Managed
{
	void OnTileClick(DZS_CatTile t, int button) {}
	void OnTileDouble(DZS_CatTile t) {}
	void OnTileStar(DZS_CatTile t) {}
	void OnTileDrag(DZS_CatTile t) {}
}

// Object tile: "row" (48 px, list), "small" (32 × 32, recent), "card" (grid of the full-screen search).
class DZS_CatTile : DZS_HitListener
{
	static const float DRAG_PX = 4;

	string m_Form;
	DZS_CatEntry m_Entry;
	DZS_CatTileOwner m_Owner;
	ref DZS_Hit m_Hit;
	ref DZS_Hit m_StarHit;
	protected Widget m_Bg;
	protected Widget m_Tint;
	protected Widget m_Bar;
	protected ref DZS_Border m_Ring;
	protected Widget m_ImgBg;
	protected ImageWidget m_Photo;
	protected ImageWidget m_Place;
	protected ImageWidget m_Warn;
	protected TextWidget m_Name;
	protected TextWidget m_Sub;
	protected TextWidget m_Mark;
	protected ImageWidget m_Star;
	protected Widget m_HitBg;
	protected TextWidget m_HitText;
	bool m_InHand;
	bool m_Marked;
	bool m_Fav;
	bool m_Off;
	protected string m_PhotoPath;
	protected int m_PressX;
	protected int m_PressY;
	protected bool m_Down;
	protected float m_LastClick = -10;
	protected float m_W;
	protected float m_H;
	protected float m_ImgSize;

	void DZS_CatTile(Widget parent, string form, DZS_CatTileOwner owner)
	{
		m_Form = form;
		m_Owner = owner;
		m_Hit = new DZS_Hit(parent, this, "catalog.tile");
		m_Hit.m_FocusKind = "";
		Widget f = m_Hit.m_W;
		m_Bg = DZS_W.Panel(f, DZS_Color.Get("surfaceRaised"));
		m_Tint = DZS_W.Panel(f, DZS_Color.Get("accentTint"));
		m_Bar = DZS_W.Panel(f, DZS_Color.Get("accent"));
		m_Ring = new DZS_Border(f, DZS_Color.Get("accent"));
		m_ImgBg = DZS_W.Panel(f, DZS_Color.Get("imageBg"));
		m_Photo = DZS_W.Icon(f, "image", ARGB(255, 255, 255, 255));
		m_Place = DZS_W.Icon(f, "package", DZS_Color.Get("textQuiet"));
		m_Warn = DZS_W.Icon(f, "triangle-alert", DZS_Color.Get("warning"));
		if (form != "small")
		{
			m_HitBg = DZS_W.Panel(f, DZS_Color.Get("accentTint"));
			m_Name = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("text"));
			m_HitText = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("accentText"));
			m_Sub = DZS_W.Text(f, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
			m_Mark = DZS_W.Text(f, DZS_Font.LABELBOLD, 9, DZS_Color.Get("accentText"), false, true);
			m_Mark.SetText(DZS_Texts.Upper(DZS_Texts.Get("catalog.inHand")));
		}
		if (form == "row")
		{
			m_StarHit = new DZS_Hit(f, this, "catalog.star");
			m_StarHit.m_FocusKind = "";
			m_StarHit.SetTip(DZS_Texts.Get("catalog.fav.tip"));
			m_Star = DZS_W.Icon(m_StarHit.m_W, "star", DZS_Color.Get("textQuiet"));
		}
	}

	void Destroy()
	{
		if (m_StarHit)
			m_StarHit.Destroy();
		m_Hit.Destroy();
	}

	void Show(bool s)
	{
		m_Hit.m_W.Show(s);
	}

	bool IsShown() { return m_Hit.m_W.IsVisible(); }

	void Bind(DZS_CatEntry e)
	{
		if (m_Entry != e)
		{
			m_Entry = e;
			m_PhotoPath = "";
		}
		if (!e)
			return;
		string tip = e.m_Key;
		if (e.m_IsTemplate)
			tip = e.m_Name;
		if (m_Form == "small")
			tip = e.m_Name;
		if (e.m_IsTemplate && e.m_TplMissing > 0)
			tip = DZS_Texts.Fmt("catalog.tpl.missing", e.m_TplMissing.ToString(), e.m_TplCount.ToString());
		if (m_Off)
			tip = DZS_Texts.Get("catalog.swap.noTemplate");
		m_Hit.SetTip(tip);
	}

	void SetPhoto(string path)
	{
		if (path == m_PhotoPath)
			return;
		m_PhotoPath = path;
		if (path != "")
		{
			m_Photo.LoadImageFile(0, path);
			m_Photo.SetImage(0);
		}
	}

	// x, y, width; the height follows from the form (card: picture three quarters of the width plus two lines).
	void Place(float x, float y, float w)
	{
		m_W = w;
		if (m_Form == "row")
			m_H = DZS_Scale.Px(48);
		else if (m_Form == "small")
			m_H = w;
		else
			m_H = w * 0.75 + DZS_Scale.Px(42);
		m_Hit.Place(x, y, m_W, m_H);
		Layout();
	}

	float Height() { return m_H; }

	protected void Layout()
	{
		DZS_W.Place(m_Bg, 0, 0, m_W, m_H);
		DZS_W.Place(m_Tint, 0, 0, m_W, m_H);
		float bar = DZS_Scale.Px(3);
		DZS_W.Place(m_Bar, 0, 0, bar, m_H);
		m_Ring.Place(0, 0, m_W, m_H, Math.Max(1, DZS_Scale.Px(1)));
		float ix, iy, iw, ih;
		if (m_Form == "row")
		{
			iw = DZS_Scale.Px(40);
			ih = iw;
			ix = DZS_Scale.Px(8);
			iy = (m_H - ih) / 2;
		}
		else if (m_Form == "small")
		{
			ix = 0;
			iy = 0;
			iw = m_W;
			ih = m_H;
		}
		else
		{
			ix = 0;
			iy = 0;
			iw = m_W;
			ih = m_W * 0.75;
		}
		m_ImgSize = Math.Min(iw, ih);
		DZS_W.Place(m_ImgBg, ix, iy, iw, ih);
		DZS_W.Place(m_Photo, ix, iy, iw, ih);
		float ps = Math.Min(iw, ih) * 0.5;
		DZS_W.Place(m_Place, ix + (iw - ps) / 2, iy + (ih - ps) / 2, ps, ps);
		float ws = DZS_Scale.Px(14);
		DZS_W.Place(m_Warn, ix + iw - ws - DZS_Scale.Px(2), iy + DZS_Scale.Px(2), ws, ws);
		if (m_Form == "small")
			return;
		float tx, tw, ty;
		float lineH = DZS_Scale.Px(18);
		if (m_Form == "row")
		{
			tx = ix + iw + DZS_Scale.Px(10);
			float starS = DZS_Scale.Px(28);
			tw = m_W - tx - starS - DZS_Scale.Px(8);
			ty = DZS_Scale.Px(6);
			m_StarHit.Place(m_W - starS - DZS_Scale.Px(6), (m_H - starS) / 2, starS, starS);
			float s = DZS_Scale.Px(16);
			DZS_W.Place(m_Star, (starS - s) / 2, (starS - s) / 2, s, s);
		}
		else
		{
			tx = DZS_Scale.Px(6);
			tw = m_W - DZS_Scale.Px(12);
			ty = ih + DZS_Scale.Px(4);
		}
		DZS_W.Place(m_Name, tx, ty, tw, lineH);
		DZS_W.Place(m_Sub, tx, ty + lineH, tw, lineH);
		float mw = DZS_Scale.Px(80);
		if (m_Form == "row")
			DZS_W.Place(m_Mark, m_W - mw - DZS_Scale.Px(40), ty + lineH, mw, lineH);
		else
			DZS_W.Place(m_Mark, m_W - mw - DZS_Scale.Px(6), DZS_Scale.Px(4), mw, lineH);
		m_HitBg.Show(false);
		m_HitText.Show(false);
	}

	// Name with the place hit by the first search word marked (cards of the full-screen search).
	void SetHitMark(int at, int len)
	{
		if (m_Form != "card" || at < 0 || len <= 0 || !m_Entry)
		{
			if (m_HitBg)
				m_HitBg.Show(false);
			if (m_HitText)
				m_HitText.Show(false);
			return;
		}
		string name = m_Entry.m_Name;
		string folded = DZS_CatText.Fold(name);
		if (folded != name && name.Length() != folded.Length())
			return;
		float x0, y0, nw, nh;
		m_Name.GetPos(x0, y0);
		m_Name.GetSize(nw, nh);
		m_HitText.SetText(DZS_W.Literal(name.Substring(0, at)));
		int before = DZS_W.TextWidth(m_HitText);
		m_HitText.SetText(DZS_W.Literal(name.Substring(at, Math.Min(len, name.Length() - at))));
		int hw = DZS_W.TextWidth(m_HitText);
		if (before + hw > nw)
			return;
		DZS_W.Place(m_HitBg, x0 + before, y0, hw, nh);
		DZS_W.Place(m_HitText, x0 + before, y0, hw + 2, nh);
		m_HitBg.Show(true);
		m_HitText.Show(true);
	}

	void Refresh(string subText, float dt)
	{
		DZS_CatEntry e = m_Entry;
		if (!e)
			return;
		bool hover = m_Hit.m_Hover;
		m_Bg.Show(hover || m_Form == "card");
		if (m_Form == "card" && !hover)
			m_Bg.SetColor(DZS_Color.Get("surfaceRaised"));
		m_Tint.Show(m_InHand || (m_Marked && m_Form == "card"));
		m_Bar.Show(m_InHand && m_Form != "card");
		m_Ring.Show((m_Marked && m_Form == "card") || (m_InHand && m_Form != "row"));
		if (m_Name)
		{
			DZS_W.FitText(m_Name, e.m_Name, m_W - DZS_Scale.Px(80));
			if (m_Form == "card")
				DZS_W.FitText(m_Name, e.m_Name, m_W - DZS_Scale.Px(12));
			m_Name.SetColor(DZS_Color.Get("text"));
			if (m_Off)
				m_Name.SetColor(DZS_Color.Get("textOff"));
			m_Sub.SetText(subText);
			m_Mark.Show(m_InHand);
		}
		if (m_Star)
		{
			if (m_Fav)
			{
				DZS_W.SetIcon(m_Star, "star-filled");
				m_Star.SetColor(DZS_Color.Get("accent"));
			}
			else
			{
				DZS_W.SetIcon(m_Star, "star");
				m_Star.SetColor(DZS_Color.Get("textQuiet"));
				if (m_StarHit.m_Hover)
					m_Star.SetColor(DZS_Color.Get("text"));
			}
		}
		m_Warn.Show(e.m_IsTemplate && e.m_TplMissing > 0);
		bool photo = m_PhotoPath != "";
		m_Photo.Show(photo);
		m_Place.Show(!photo);
		DZS_W.SetIcon(m_Place, DZS_CatalogPlugin.CategoryIcon(e.m_Cat));
		if (m_Down)
		{
			int mx, my;
			GetMousePos(mx, my);
			if (Math.AbsFloat(mx - m_PressX) + Math.AbsFloat(my - m_PressY) > DZS_Scale.Px(DRAG_PX))
			{
				m_Down = false;
				m_Owner.OnTileDrag(this);
			}
		}
	}

	override void OnHitState(DZS_Hit h) {}

	override void OnHitPress(DZS_Hit h, int button)
	{
		if (h == m_Hit && button == MouseState.LEFT)
		{
			GetMousePos(m_PressX, m_PressY);
			m_Down = true;
		}
	}

	override void OnHitRelease(DZS_Hit h)
	{
		if (h == m_Hit)
			m_Down = false;
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (h == m_StarHit)
		{
			if (button == MouseState.LEFT)
				m_Owner.OnTileStar(this);
			return;
		}
		if (button == MouseState.LEFT)
		{
			int mx, my;
			GetMousePos(mx, my);
			if (Math.AbsFloat(mx - m_PressX) + Math.AbsFloat(my - m_PressY) > DZS_Scale.Px(DRAG_PX))
				return;
			float now = GetGame().GetTickTime();
			bool dbl = now - m_LastClick < 0.4;
			m_LastClick = now;
			m_Owner.OnTileClick(this, button);
			if (dbl)
				m_Owner.OnTileDouble(this);
			return;
		}
		m_Owner.OnTileClick(this, button);
	}
}
