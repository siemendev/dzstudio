// The settings dialog: a large tool dialog with tabs; changes work
// at once, Esc or "Schließen" keep everything.
class DZS_TabButton : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	protected TextWidget m_Text;
	protected Widget m_Bar;
	protected ref DZS_Border m_FocusRing;
	string m_Key;
	bool m_Selected;
	DZS_SettingsDialog m_Dialog;

	void DZS_TabButton(Widget parent, DZS_SettingsDialog dlg, string key, string name)
	{
		m_Dialog = dlg;
		m_Key = key;
		m_Hit = new DZS_Hit(parent, this, "tab");
		m_Hit.m_FocusKind = "button";
		m_Text = DZS_W.Text(m_Hit.m_W, DZS_Font.DISPLAY, 22, DZS_Color.Get("textMuted"), true);
		m_Text.SetText(name);
		m_Bar = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accent"));
		m_FocusRing = new DZS_Border(m_Hit.m_W, DZS_Color.Get("accent"));
	}

	float Width() { return DZS_W.TextWidth(m_Text) + DZS_Scale.Px(24); }

	void Place(float x, float y, float h)
	{
		float w = Width();
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Text, 0, 0, w, h);
		DZS_W.Place(m_Bar, 0, h - DZS_Scale.Px(2), w, DZS_Scale.Px(2));
		m_FocusRing.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		Refresh();
	}

	void Refresh()
	{
		m_Bar.Show(m_Selected);
		m_FocusRing.Show(m_Hit.m_Focus);
		if (m_Selected)
			m_Text.SetColor(DZS_Color.Get("accentText"));
		else if (m_Hit.m_Hover)
			m_Text.SetColor(DZS_Color.Get("text"));
		else
			m_Text.SetColor(DZS_Color.Get("textMuted"));
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_Dialog.SelectTab(m_Key);
	}
}

// One row: name and explanation left, the control directly after them, "Auf Standard" at the end.
class DZS_SettingRow : DZS_HitListener
{
	DZS_SettingDef m_Def;
	protected DZS_Settings m_St;
	protected Widget m_Frame;
	protected ref DZS_Hit m_NameHit;
	protected TextWidget m_Name;
	protected TextWidget m_Expl;
	ref DZS_Toggle m_Toggle;
	ref array<ref DZS_Toggle> m_RowToggles;
	protected ref array<TextWidget> m_RowNames;
	ref DZS_Slider m_Slider;
	protected TextWidget m_Value;
	ref DZS_NumberField m_Field;
	ref DZS_Button m_Reset;

	void DZS_SettingRow(Widget parent, DZS_SettingDef def)
	{
		m_Def = def;
		m_St = DZS_Kernel.Get().GetSettings();
		m_Frame = DZS_W.Frame(parent);
		m_NameHit = new DZS_Hit(m_Frame, this, "rowName");
		m_Name = DZS_W.Text(m_NameHit.m_W, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_Name.SetText(def.Name());
		m_Expl = DZS_W.Text(m_NameHit.m_W, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Expl.SetText(def.Expl());
		if (def.m_Kind == "toggle")
		{
			m_Toggle = new DZS_Toggle(m_Frame);
			m_Toggle.m_OnChange.Insert(OnToggle);
			if (def.m_RowKeys)
			{
				m_RowToggles = new array<ref DZS_Toggle>();
				m_RowNames = new array<TextWidget>();
				foreach (int i, string rk : def.m_RowKeys)
				{
					DZS_Toggle t = new DZS_Toggle(m_Frame);
					t.m_Hit.m_Tag = rk;
					t.m_OnChange.Insert(OnRowToggle);
					m_RowToggles.Insert(t);
					TextWidget rn = DZS_W.Text(m_Frame, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
					rn.SetText(DZS_Texts.Pick(def.m_RowNamesDe[i], def.m_RowNamesEn[i]));
					m_RowNames.Insert(rn);
				}
			}
		}
		else
		{
			m_Slider = new DZS_Slider(m_Frame, def.m_Min, def.m_Max);
			m_Slider.m_OnChange.Insert(OnSlide);
			m_Slider.m_OnRelease.Insert(OnSlideRelease);
			m_Value = DZS_W.Text(m_Frame, DZS_Font.MONO, 12, DZS_Color.Get("text"), false, true);
			m_Field = new DZS_NumberField("setting." + def.m_Key, "", "textMuted", 80);
			m_Field.Build(m_Frame);
			m_Field.m_OnCommit.Insert(OnFieldCommit);
			m_Field.m_OnWheel.Insert(OnFieldWheel);
		}
		m_Reset = new DZS_Button(m_Frame, "ghost", "", "undo-2", 28);
		m_Reset.m_OnClick.Insert(OnReset);
		Refresh();
	}

	string Format(float v)
	{
		return DZS_Num.Fixed(v, m_Def.m_Decimals) + m_Def.m_Unit;
	}

	string DefaultText()
	{
		DZS_Json d = m_St.GetDefault(m_Def.m_Key);
		if (!d)
			return "";
		if (d.IsBool())
		{
			if (d.m_Bool)
				return DZS_Texts.Get("core.on");
			return DZS_Texts.Get("core.off");
		}
		return Format(d.m_Str.ToFloat());
	}

	bool IsDefault()
	{
		if (!m_St.IsDefault(m_Def.m_Key))
			return false;
		if (m_Def.m_RowKeys)
		{
			foreach (string rk : m_Def.m_RowKeys)
			{
				if (!m_St.IsDefault(rk))
					return false;
			}
		}
		return true;
	}

	void Refresh()
	{
		if (m_Toggle)
		{
			bool on = m_St.GetBool(m_Def.m_Key);
			if (m_Toggle.IsOn() != on)
				m_Toggle.SetOn(on);
			if (m_RowToggles)
			{
				foreach (DZS_Toggle t : m_RowToggles)
				{
					bool ron = m_St.GetBool(t.m_Hit.m_Tag);
					if (t.IsOn() != ron)
						t.SetOn(ron);
				}
			}
		}
		if (m_Slider)
		{
			float v = m_St.GetSettingNumber(m_Def.m_Key);
			if (!m_Slider.IsDragging())
				m_Slider.SetValue(v);
			m_Value.SetText(Format(v));
			m_Field.ShowValue(DZS_Num.Fixed(v, m_Def.m_Decimals));
		}
		bool def = IsDefault();
		m_Reset.Show(!def);
		m_Reset.SetTip(DZS_Texts.Fmt("core.settings.default", DefaultText()));
	}

	// Height of the row at this width.
	float Layout(float y, float width)
	{
		float h = DZS_Scale.Px(52);
		float nw = Math.Max(DZS_W.TextWidth(m_Name), DZS_W.TextWidth(m_Expl));
		DZS_W.Place(m_Frame, 0, y, width, h);
		m_NameHit.Place(0, 0, nw + DZS_Scale.Px(4), h);
		DZS_W.Place(m_Name, 0, DZS_Scale.Px(6), nw + 2, DZS_Scale.Px(20));
		DZS_W.Place(m_Expl, 0, DZS_Scale.Px(26), nw + 2, DZS_Scale.Px(20));
		float x = nw + DZS_Scale.Px(20);
		if (m_Toggle)
		{
			m_Toggle.Place(x, (h - DZS_Toggle.Height()) / 2);
			x += DZS_Toggle.Width() + DZS_Scale.Px(16);
			if (m_RowToggles)
			{
				foreach (int i, DZS_Toggle t : m_RowToggles)
				{
					float rw = DZS_W.TextWidth(m_RowNames[i]);
					DZS_W.Place(m_RowNames[i], x, 0, rw + 2, h);
					x += rw + DZS_Scale.Px(8);
					t.Place(x, (h - DZS_Toggle.Height()) / 2);
					x += DZS_Toggle.Width() + DZS_Scale.Px(16);
				}
			}
		}
		else
		{
			float sx = Math.Max(x, DZS_Scale.Px(300));
			m_Slider.Place(sx, (h - DZS_Slider.Height()) / 2);
			x = sx + DZS_Slider.Width() + DZS_Scale.Px(12);
			DZS_W.Place(m_Value, x, 0, DZS_Scale.Px(64), h);
			x += DZS_Scale.Px(64) + DZS_Scale.Px(12);
			m_Field.Place(x, 0, h);
			x += m_Field.GetWidth() + DZS_Scale.Px(16);
		}
		m_Reset.Place(x, (h - m_Reset.GetHeight()) / 2);
		return h;
	}

	void AddFocus(DZS_Dialog dlg)
	{
		if (m_Toggle)
		{
			dlg.AddFocusHit(m_Toggle.m_Hit);
			if (m_RowToggles)
			{
				foreach (DZS_Toggle t : m_RowToggles)
					dlg.AddFocusHit(t.m_Hit);
			}
		}
		if (m_Field)
			dlg.AddFocusField(m_Field);
		dlg.AddFocusHit(m_Reset.m_Hit);
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (h == m_NameHit && button == MouseState.LEFT && m_Toggle)
			m_Toggle.Flip();
	}

	void OnToggle(DZS_Toggle t, bool on)
	{
		m_St.Set(m_Def.m_Key, DZS_Json.Boolean(on));
		Refresh();
	}

	void OnRowToggle(DZS_Toggle t, bool on)
	{
		m_St.Set(t.m_Hit.m_Tag, DZS_Json.Boolean(on));
		Refresh();
	}

	void OnSlide(DZS_Slider s, float v)
	{
		m_St.SetHoldFlush(true);
		float n = DZS_Settings.Normalize(m_Def, v);
		s.SetValue(n);
		if (n != m_St.GetSettingNumber(m_Def.m_Key))
			m_St.SetSettingNumber(m_Def.m_Key, n);
		Refresh();
	}

	void OnSlideRelease(DZS_Slider s)
	{
		m_St.SetHoldFlush(false);
	}

	void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		if (!DZS_Num.Parse(typed, v))
		{
			r.m_Ok = false;
			return;
		}
		m_St.SetSettingNumber(m_Def.m_Key, v);
		r.m_Ok = true;
		Refresh();
	}

	// Wheel with focus: one step of the setting (with Shift a tenth, but never below the smallest step).
	void OnFieldWheel(DZS_NumberField f, int steps, bool fine)
	{
		float step = m_Def.m_Step;
		if (fine)
			step = Math.Max(step / 10, m_Def.m_Step);
		m_St.SetSettingNumber(m_Def.m_Key, m_St.GetSettingNumber(m_Def.m_Key) + steps * step);
		Refresh();
		f.ForceValue(DZS_Num.Fixed(m_St.GetSettingNumber(m_Def.m_Key), m_Def.m_Decimals));
	}

	void OnReset(DZS_Button b)
	{
		m_St.Reset(m_Def.m_Key);
		if (m_Def.m_RowKeys)
		{
			foreach (string rk : m_Def.m_RowKeys)
				m_St.Reset(rk);
		}
		Refresh();
	}

	void Update()
	{
		if (m_Slider)
			m_Slider.Update();
		Refresh();
	}

	void Destroy()
	{
		m_NameHit.Destroy();
		if (m_Toggle)
			m_Toggle.m_Hit.Destroy();
		if (m_RowToggles)
		{
			foreach (DZS_Toggle t : m_RowToggles)
				t.m_Hit.Destroy();
		}
		if (m_Slider)
			m_Slider.m_Hit.Destroy();
		m_Reset.Destroy();
		m_Frame.Unlink();
	}
}

class DZS_SettingsDialog : DZS_Dialog
{
	protected static string s_LastTab = "camera";

	protected ref array<ref DZS_TabButton> m_Tabs;
	protected string m_Tab;
	protected Widget m_TabFrame;
	protected ref array<ref DZS_SettingRow> m_Rows;
	protected ref DZS_KeysTab m_Keys;
	protected DZS_SettingsPage m_Custom;
	protected Widget m_Page;
	protected bool m_Rebuild;

	void DZS_SettingsDialog()
	{
		string kicker = DZS_Texts.Get("core.settings.kicker");
		string title = DZS_Texts.Get("core.settings.title");
		Setup(LARGE, kicker, title, false);
		m_Tabs = new array<ref DZS_TabButton>();
		m_Rows = new array<ref DZS_SettingRow>();
		m_ExtraHeadH = DZS_Scale.Px(40);
		array<DZS_SettingTab> tabs = DZS_Kernel.Get().GetSettings().GetTabs();
		m_TabFrame = DZS_W.Frame(m_Box.m_W);
		foreach (DZS_SettingTab t : tabs)
			m_Tabs.Insert(new DZS_TabButton(m_TabFrame, this, t.m_Key, t.Name()));
		m_Tab = s_LastTab;
		if (!DZS_Kernel.Get().GetSettings().FindTab(m_Tab) || !HasTab(m_Tab))
			m_Tab = "camera";
		AddFootButton("resetAll", DZS_Texts.Get("core.settings.resetAll"), "side");
		AddFootButton("resetTab", "", "side");
		AddFootButton("close", DZS_Texts.Get("core.dialog.closeButton"), "main");
		SetFootNote(DZS_Version.Label());
	}

	protected bool HasTab(string key)
	{
		foreach (DZS_TabButton t : m_Tabs)
		{
			if (t.m_Key == key)
				return true;
		}
		return false;
	}

	string CurrentTab() { return m_Tab; }

	override void BuildContent(Widget inner)
	{
		BuildPage();
	}

	void SelectTab(string key)
	{
		if (key == m_Tab)
			return;
		m_Tab = key;
		s_LastTab = key;
		DZS_Log.Info("settings tab " + key);
		BuildPage();
		m_Scroll = 0;
		InitialFocus();
	}

	protected void BuildPage()
	{
		DZS_Screen scr = DZS_Kernel.Get().GetScreen();
		if (scr.IsFieldFocused())
			scr.Unfocus(false);
		ClearFocusables();
		foreach (DZS_SettingRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		if (m_Keys)
			m_Keys.Destroy();
		m_Keys = null;
		if (m_Custom)
			m_Custom.Destroy();
		m_Custom = null;
		if (m_Page)
			m_Page.Unlink();
		m_Page = DZS_W.Frame(m_Inner);
		foreach (DZS_TabButton tb : m_Tabs)
		{
			tb.m_Selected = tb.m_Key == m_Tab;
			AddFocusHit(tb.m_Hit);
		}
		if (m_Tab == "keys")
		{
			m_Keys = new DZS_KeysTab(m_Page, this);
			m_Keys.AddFocus();
		}
		else if (DZS_Kernel.Get().GetSettings().GetPage(m_Tab))
		{
			m_Custom = DZS_Kernel.Get().GetSettings().GetPage(m_Tab);
			m_Custom.Build(m_Page, this);
		}
		else
		{
			array<DZS_SettingDef> defs = DZS_Kernel.Get().GetSettings().GetDefs(m_Tab);
			foreach (DZS_SettingDef d : defs)
			{
				DZS_SettingRow row = new DZS_SettingRow(m_Page, d);
				row.AddFocus(this);
				m_Rows.Insert(row);
			}
		}
		AddFootFocus();
	}

	// After the key list was rebuilt, its elements replace the old ones in the Tab order.
	void RefocusKeys()
	{
		DZS_Hit keep = FocusedHit();
		ClearFocusables();
		foreach (DZS_TabButton tb : m_Tabs)
			AddFocusHit(tb.m_Hit);
		if (m_Keys)
			m_Keys.AddFocus();
		AddFootFocus();
		if (keep && !keep.m_Destroyed)
			SetFocusToHit(keep);
	}

	override void LayoutHeadExtra(float x, float y, float w)
	{
		DZS_W.Place(m_TabFrame, x, y, w, DZS_Scale.Px(40));
		float tx = 0;
		foreach (DZS_TabButton t : m_Tabs)
		{
			t.Place(tx, 0, DZS_Scale.Px(40));
			tx += t.Width() + DZS_Scale.Px(4);
		}
	}

	override float LayoutContent(float width)
	{
		if (!m_Page)
			return 0;
		float y = 0;
		if (m_Keys)
			y = m_Keys.Layout(width);
		else if (m_Custom)
			y = m_Custom.Layout(width);
		else
		{
			foreach (DZS_SettingRow r : m_Rows)
			{
				y += r.Layout(y, width);
				y += DZS_Scale.Px(4);
			}
		}
		DZS_W.Place(m_Page, 0, 0, width, Math.Max(y, 1));
		return y;
	}

	protected bool TabIsDefault(string tab)
	{
		DZS_Settings st = DZS_Kernel.Get().GetSettings();
		if (tab == "keys")
			return !st.HasCustomKeys();
		array<string> keys = st.TabKeys(tab);
		foreach (string k : keys)
		{
			if (!st.IsDefault(k))
				return false;
		}
		return true;
	}

	protected bool AllDefault()
	{
		foreach (DZS_TabButton t : m_Tabs)
		{
			if (!TabIsDefault(t.m_Key))
				return false;
		}
		return true;
	}

	override void UpdateContent(float dt)
	{
		foreach (DZS_SettingRow r : m_Rows)
			r.Update();
		if (m_Keys)
			m_Keys.Update(dt);
		if (m_Custom)
			m_Custom.Update(dt);
		string tabName = m_Tab;
		foreach (DZS_TabButton t : m_Tabs)
		{
			if (t.m_Key == m_Tab)
			{
				DZS_SettingTab st = DZS_Kernel.Get().GetSettings().FindTab(m_Tab);
				tabName = st.Name();
			}
		}
		DZS_Button rt = FootButton("resetTab");
		rt.SetLabel(DZS_Texts.Fmt("core.settings.resetTab", tabName));
		string why = DZS_Texts.Get("core.settings.allDefault");
		bool tabDef = TabIsDefault(m_Tab);
		rt.SetEnabled(!tabDef, why);
		FootButton("resetAll").SetEnabled(!AllDefault(), why);
	}

	override void OnFoot(string tag)
	{
		if (tag == "close")
		{
			Close();
			return;
		}
		DZS_ConfirmDialog c;
		string kicker = DZS_Texts.Get("core.settings.kicker");
		string title;
		string ask;
		if (tag == "resetTab")
		{
			string tabName = DZS_Kernel.Get().GetSettings().FindTab(m_Tab).Name();
			title = DZS_Texts.Fmt("core.settings.resetTab", tabName);
			ask = DZS_Texts.Fmt("core.settings.resetTabAsk", tabName);
			c = new DZS_ConfirmDialog(kicker, title, ask);
			c.m_Tag = "resetTab";
		}
		else
		{
			title = DZS_Texts.Get("core.settings.resetAll");
			ask = DZS_Texts.Get("core.settings.resetAllAsk");
			c = new DZS_ConfirmDialog(kicker, title, ask);
			c.m_Tag = "resetAll";
		}
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("reset", DZS_Texts.Get("core.settings.reset"), "danger");
		c.m_OnResult.Insert(OnConfirm);
		c.Open();
	}

	void OnConfirm(DZS_Dialog d, string result)
	{
		if (result != "reset")
			return;
		DZS_ConfirmDialog c = DZS_ConfirmDialog.Cast(d);
		DZS_Settings st = DZS_Kernel.Get().GetSettings();
		if (c.m_Tag == "resetTab")
			ResetTab(m_Tab);
		else
		{
			foreach (DZS_TabButton t : m_Tabs)
				ResetTab(t.m_Key);
		}
		DZS_Log.Info("settings reset " + c.m_Tag + " (" + m_Tab + ")");
	}

	protected void ResetTab(string tab)
	{
		DZS_Settings st = DZS_Kernel.Get().GetSettings();
		if (tab == "keys")
		{
			DZS_Kernel.Get().GetUi().ResetAllBindings();
			return;
		}
		array<string> keys = st.TabKeys(tab);
		foreach (string k : keys)
			st.Reset(k);
	}

	override void Cancel()
	{
		Close();
	}

	override void OnClosed()
	{
		foreach (DZS_SettingRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		if (m_Keys)
			m_Keys.Destroy();
		if (m_Custom)
			m_Custom.Destroy();
		m_Custom = null;
		DZS_Kernel.Get().GetSettings().SetHoldFlush(false);
		s_LastTab = m_Tab;
	}
}
