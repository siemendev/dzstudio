// Drawing over the scene: the game cannot
// outline a model, so boxes, handles and the selection line are lines of a canvas over the scene, drawn anew
// every frame from the corners projected onto the screen.
class DZS_PlaceCanvas : Managed
{
	CanvasWidget m_Canvas;

	void DZS_PlaceCanvas(Widget parent)
	{
		m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", parent));
	}

	void Begin()
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Canvas, 0, 0, sw, sh);
		m_Canvas.Clear();
	}

	void Show(bool s)
	{
		m_Canvas.Show(s);
	}

	void Line(float x1, float y1, float x2, float y2, float width, int color)
	{
		DZS_Clip.Line(m_Canvas, x1, y1, x2, y2, width, color);
	}

	// Dashes and gaps of 'dash' pixels each.
	void Dashed(float x1, float y1, float x2, float y2, float width, int color, float dash)
	{
		float dx = x2 - x1;
		float dy = y2 - y1;
		float len = Math.Sqrt(dx * dx + dy * dy);
		if (len < 0.5)
			return;
		float t = 0;
		while (t < len)
		{
			float e = Math.Min(t + dash, len);
			DZS_Clip.Line(m_Canvas, x1 + dx * t / len, y1 + dy * t / len, x1 + dx * e / len, y1 + dy * e / len, width, color);
			t += 2 * dash;
		}
	}

	void Seg3(vector a, vector b, float width, int color, bool dashed = false)
	{
		float x1, y1, x2, y2;
		if (!DZS_PlaceMath.Segment(a, b, x1, y1, x2, y2))
			return;
		if (dashed)
			Dashed(x1, y1, x2, y2, width, color, DZS_Scale.Px(6));
		else
			Line(x1, y1, x2, y2, width, color);
	}

	// The twelve edges of a box given by its eight corners (bit 0 = x, 1 = y, 2 = z).
	void Box(array<vector> c, float width, int color, bool dashed)
	{
		for (int i = 0; i < 8; i++)
		{
			for (int bit = 1; bit <= 4; bit *= 2)
			{
				if ((i & bit) == 0)
					Seg3(c[i], c[i | bit], width, color, dashed);
			}
		}
	}

	// A circle in 3D (centre, two unit axes of its plane, radius) as 48 segments.
	void Circle(vector centre, vector a, vector b, float radius, float width, int color)
	{
		vector prev = centre + a * radius;
		for (int i = 1; i <= 48; i++)
		{
			float ang = i * Math.PI2 / 48;
			vector p = centre + a * (radius * Math.Cos(ang)) + b * (radius * Math.Sin(ang));
			Seg3(prev, p, width, color);
			prev = p;
		}
	}

	void Square(float cx, float cy, float size, int color)
	{
		float h = size / 2;
		for (float y = cy - h; y < cy + h; y += 1)
			Line(cx - h, y + 0.5, cx + h, y + 0.5, 1, color);
	}
}

// The toolbar over the selection.
class DZS_ObjectBar : DZS_HitListener
{
	ref DZS_Hit m_Surface;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected TextWidget m_Label;
	protected ref array<ref DZS_Button> m_Buttons;
	protected ref array<string> m_Actions;
	protected ref array<int> m_Orders;
	protected ref array<bool> m_SingleOnly;
	protected ref map<string, string> m_LabelDe;
	protected ref map<string, string> m_LabelEn;
	protected ref array<string> m_Overflow;
	protected ref DZS_Button m_More;
	DZS_PlacementPlugin m_P;
	float m_W;
	float m_H;

	void DZS_ObjectBar(Widget parent, DZS_PlacementPlugin p)
	{
		m_P = p;
		m_Surface = new DZS_Hit(parent, this, "placement.bar");
		m_Surface.m_Surface = true;
		m_Surface.m_FocusKind = "";
		m_Bg = DZS_W.Panel(m_Surface.m_W, DZS_Color.Get("surface"));
		m_Border = new DZS_Border(m_Surface.m_W, DZS_Color.Get("lineStrong"));
		m_Label = DZS_W.Text(m_Surface.m_W, DZS_Font.READBOLD, 13, DZS_Color.Get("text"));
		m_Buttons = new array<ref DZS_Button>();
		m_Actions = new array<string>();
		m_Orders = new array<int>();
		m_SingleOnly = new array<bool>();
		m_LabelDe = new map<string, string>();
		m_LabelEn = new map<string, string>();
		m_Overflow = new array<string>();
		m_More = new DZS_Button(m_Surface.m_W, "ghost", "", "ellipsis", 28);
		m_More.m_Hit.m_FocusKind = "";
		m_More.SetTip(DZS_Texts.Pick("Weitere", "More"));
		m_More.m_OnClick.Insert(OnMore);
		m_Surface.m_W.Show(false);
	}

	void AddButton(string actionId, string icon, string label, int order = 1000, bool singleOnly = false, string labelEn = "")
	{
		DZS_Button b = new DZS_Button(m_Surface.m_W, "ghost", label, icon, 28);
		b.m_Tag = actionId;
		b.m_Hit.m_FocusKind = "";
		b.m_OnClick.Insert(OnButton);
		int at = 0;
		while (at < m_Orders.Count() && m_Orders[at] <= order)
			at++;
		m_Buttons.InsertAt(b, at);
		m_Actions.InsertAt(actionId, at);
		m_Orders.InsertAt(order, at);
		m_SingleOnly.InsertAt(singleOnly, at);
		if (labelEn != "")
		{
			m_LabelDe.Set(actionId, label);
			m_LabelEn.Set(actionId, labelEn);
		}
	}

	string ForeignLabel(string actionId)
	{
		if (!m_LabelDe.Contains(actionId))
			return "";
		string de = m_LabelDe.Get(actionId);
		string en = m_LabelEn.Get(actionId);
		return DZS_Texts.Pick(de, en);
	}

	void OnButton(DZS_Button b)
	{
		m_P.OnBarButton(b.m_Tag);
	}

	// Knobs that did not fit into 560 px, as a menu.
	void OnMore(DZS_Button b)
	{
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		foreach (string id : m_Overflow)
		{
			DZS_MenuItem it = new DZS_MenuItem();
			it.m_ActionId = id;
			it.m_Text = m_P.BarButtonLabel(id);
			it.m_Binding = ui.FirstBinding(id);
			string rd, re;
			it.m_Enabled = ui.IsActionEnabled(id, rd, re);
			it.m_Reason = DZS_Texts.Pick(rd, re);
			items.Insert(it);
		}
		float bx, by, bw, bh;
		b.GetWidget().GetScreenPos(bx, by);
		b.GetWidget().GetScreenSize(bw, bh);
		DZS_Popup pop = DZS_Kernel.Get().GetScreen().OpenContextMenu(items, bx, by + bh + DZS_Scale.Px(2));
		pop.m_OnChoose.Insert(OnMoreChoose);
	}

	void OnMoreChoose(DZS_Popup p, DZS_MenuItem it)
	{
		DZS_Kernel.Get().GetScreen().CloseMenu();
		m_P.OnBarButton(it.m_ActionId);
	}

	void Hide()
	{
		m_Surface.m_W.Show(false);
	}

	float ShownHeight()
	{
		if (!IsShown())
			return 0;
		return m_H;
	}

	bool IsShown()
	{
		return m_Surface.m_W.IsVisible();
	}

	// Fixed at the top in the middle, 12 px below the top bar.
	protected string m_StateKey;
	protected float m_StateAt;
	protected ref array<bool> m_Visible = new array<bool>();

	// Button states are asked again only when the selection or the state changed (selection, state version, tool, lead).
	void Show(string label, string stateKey = "")
	{
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		float now = GetGame().GetTickTime();
		bool fresh = stateKey == "" || stateKey != m_StateKey || m_Visible.Count() != m_Buttons.Count();
		if (fresh)
		{
			m_StateKey = stateKey;
			m_StateAt = now;
		}
		m_Label.SetText(DZS_W.Literal(label));
		float pad = DZS_Scale.Px(6);
		float gap = DZS_Scale.Px(4);
		float x = pad + DZS_W.TextWidth(m_Label) + DZS_Scale.Px(10);
		float bh = DZS_Scale.Px(28);
		float maxW = DZS_Scale.Px(560);
		float moreW = m_More.GetWidth() + gap;
		m_Overflow.Clear();
		array<DZS_Button> shown = new array<DZS_Button>();
		if (fresh)
		{
			m_Visible.Clear();
			for (int i = 0; i < m_Buttons.Count(); i++)
			{
				DZS_Button b = m_Buttons[i];
				string id = m_Actions[i];
				bool visible = m_P.BarButtonVisible(id) && (!m_SingleOnly[i] || m_P.SelectionCount() == 1) && ui.GetAction(id) != null;
				m_Visible.Insert(visible);
				if (visible)
				{
					string rd, re;
					bool on = ui.IsActionEnabled(id, rd, re);
					string reason = DZS_Texts.Pick(rd, re);
					b.SetEnabled(on, reason);
					b.SetOn(ui.IsActionChecked(id));
					string blabel = m_P.BarButtonLabel(id);
					b.SetLabel(blabel);
					b.SetTipAction(blabel, id);
				}
			}
		}
		for (int vi = 0; vi < m_Buttons.Count(); vi++)
		{
			DZS_Button vb = m_Buttons[vi];
			vb.GetWidget().Show(false);
			if (m_Visible[vi])
				shown.Insert(vb);
		}
		for (int k = 0; k < shown.Count(); k++)
		{
			DZS_Button sb = shown[k];
			float room = maxW;
			if (k < shown.Count() - 1)
				room -= moreW;
			if (m_Overflow.Count() > 0 || x + sb.GetWidth() > room)
			{
				m_Overflow.Insert(sb.m_Tag);
				continue;
			}
			sb.GetWidget().Show(true);
			sb.Place(x, pad);
			x += sb.GetWidth() + gap;
		}
		m_More.Show(m_Overflow.Count() > 0);
		if (m_Overflow.Count() > 0)
		{
			m_More.Place(x, pad);
			x += m_More.GetWidth() + gap;
		}
		m_W = x + pad - gap;
		m_H = bh + 2 * pad;
		DZS_W.Place(m_Bg, 0, 0, m_W, m_H);
		m_Border.Place(0, 0, m_W, m_H);
		DZS_W.Place(m_Label, pad, pad, DZS_W.TextWidth(m_Label) + 2, bh);
		int scrW, scrH;
		GetScreenSize(scrW, scrH);
		float px = (scrW - m_W) / 2;
		float py = DZS_Scale.Px(DZS_Screen.TOP_H) + DZS_Scale.Px(12);
		m_Surface.Place(px, py, m_W, m_H);
		m_Surface.m_W.Show(true);
	}
}
