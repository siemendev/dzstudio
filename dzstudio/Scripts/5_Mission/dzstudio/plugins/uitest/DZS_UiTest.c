// Test plugin for the surface (start with -dzsUiTest). Uses only the public interface: a section in each
// side bar, two tools and a switch in the toolbar, a menu entry, a status segment, its own settings tab,
// a slider in the camera tab and an action with default F6. Commands come from
// $profile:dzstudio\uitest.txt (see editor/mod-tools/README.md).
class DZS_UiTestContent : DZS_PanelContent
{
	protected ref array<TextWidget> m_Lines;
	protected int m_Count;

	void DZS_UiTestContent(int count)
	{
		m_Count = count;
	}

	override void Build(Widget parent)
	{
		m_Lines = new array<TextWidget>();
		for (int i = 0; i < m_Count; i++)
		{
			TextWidget t = DZS_W.Text(parent, DZS_Font.READ, 13, DZS_Color.Get("text"));
			t.SetText(DZS_Texts.Fmt("uitest.line", (i + 1).ToString()));
			m_Lines.Insert(t);
		}
	}

	override float Layout(float width)
	{
		float y = DZS_Scale.Px(8);
		foreach (TextWidget t : m_Lines)
		{
			DZS_W.Place(t, DZS_Scale.Px(12), y, width - DZS_Scale.Px(24), DZS_Scale.Px(32));
			y += DZS_Scale.Px(32);
		}
		return y + DZS_Scale.Px(8);
	}
}

class DZS_UiTestActions : DZS_ActionHandler
{
	DZS_UiTest m_P;

	override bool IsActionChecked(string actionId) { return actionId == "uitest.switch" && m_P.m_SwitchOn; }

	override void OnAction(string actionId)
	{
		m_P.OnAction(actionId);
	}
}

[DZS_RegisterPlugin(DZS_UiTest)]
class DZS_UiTest : DZS_Plugin
{
	static const string CMD_FILE = "$profile:dzstudio\\uitest.txt";

	protected bool m_On;
	protected float m_Poll;
	protected ref DZS_UiTestActions m_Actions;
	bool m_SwitchOn;
	protected ref DZS_StatusText m_Seg;
	protected int m_F6;

	override string GetName() { return "uitest"; }

	override void OnRegister()
	{
		string v;
		m_On = GetCLIParam("dzsUiTest", v);
		if (!m_On)
			return;
		DZS_Texts.Add("uitest.line", "Testzeile %1", "Test line %1");
		m_Kernel.GetTypes().RegisterOperationType("uitest", "uitest:edit", 1);
		m_Actions = new DZS_UiTestActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterPanel("uitest", "uitest.left", "Katalog (Test)", "Catalogue (test)", "left", "library", new DZS_UiTestContent(30));
		ui.RegisterPanel("uitest", "uitest.groups", "Gruppen (Test)", "Groups (test)", "right", "layers", new DZS_UiTestContent(4));
		ui.RegisterPanel("uitest", "uitest.section", "Test-Abschnitt", "Test section", "right", "puzzle", new DZS_UiTestContent(12));
		ui.RegisterPanel("uitest", "uitest.props", "Eigenschaften (Test)", "Properties (test)", "right", "sliders-horizontal", new DZS_UiTestContent(3));
		ui.RegisterAction("uitest", "uitest.toolA", "Testwerkzeug A", "Test tool A", null, m_Actions);
		ui.RegisterAction("uitest", "uitest.toolB", "Testwerkzeug B", "Test tool B", null, m_Actions);
		ui.SetListed("uitest.toolA", false);
		ui.SetListed("uitest.toolB", false);
		ui.RegisterTool("uitest", "uitest.toolA", "Testwerkzeug A", "Test tool A", "tools", "mouse-pointer-2", 10);
		ui.RegisterTool("uitest", "uitest.toolB", "Testwerkzeug B", "Test tool B", "tools", "box", 20);
		ui.RegisterAction("uitest", "uitest.switch", "Testschalter", "Test switch", null, m_Actions);
		ui.SetListed("uitest.switch", false);
		ui.RegisterTool("uitest", "uitest.switch", "Testschalter", "Test switch", "switches", "mountain", 30);
		ui.AddMenuEntry("view", "uitest.switch", 330, true);
		array<string> f6 = {"F6"};
		ui.RegisterKeyGroup("uitest", "UI-Test", "UI test");
		ui.RegisterAction("uitest", "uitest.f6", "Testaktion", "Test action", f6, m_Actions);
		ui.SetGroup("uitest.f6", "uitest");
		DZS_Settings st = m_Kernel.GetSettings();
		st.RegisterTab("uitest", "UI-Test", "UI test");
		st.DefineToggle("uitest.flag", "uitest", "Testschalter", "Test switch", "Schickt beim Umlegen ein Ereignis", "Sends an event when flipped", false);
		st.DefineSlider("uitest.amount", "camera", "Testregler", "Test slider", "Regler eines fremden Plugins im Reiter Kamera", "Slider of another plugin in the camera tab", 0, 10, 0.5, 1, "", 5);
		DZS_Log.Info("uitest on, commands from " + CMD_FILE);
	}

	protected ref DZS_UiTestRow m_Row;

	override void OnSessionStart(string world)
	{
		if (!m_On)
			return;
		bool second = m_Kernel.GetUi().SetPanelMain("uitest.groups");
		DZS_Log.Info("uitest: second main section in the right bar accepted " + second);
		DZS_PropertiesApi props = DZS_PropertiesApi.Get();
		if (props && !m_Row)
		{
			m_Row = new DZS_UiTestRow();
			props.AddRow(m_Row);
		}
		m_Seg = new DZS_StatusText("uitest.seg", DZS_Font.MONO, 160);
		m_Kernel.GetScreen().AddStatusLeft(m_Seg);
		m_Seg.SetText("Test-Segment");
	}

	void OnAction(string id)
	{
		if (id == "uitest.switch")
			m_SwitchOn = !m_SwitchOn;
		if (id == "uitest.f6")
		{
			m_F6++;
			m_Kernel.GetScreen().ShowMessage("uitest.f6", "info", "Testaktion " + m_F6, 2.0);
		}
		DZS_Log.Info("uitest action " + id);
	}

	override void OnSettingChanged(string key)
	{
		if (m_On && key.IndexOf("uitest.") == 0)
			DZS_Log.Info("uitest setting event " + key);
	}

	override void OnUpdate(float dt)
	{
		if (!m_On)
			return;
		m_Poll -= dt;
		if (m_Poll > 0)
			return;
		m_Poll = 0.25;
		if (!FileExist(CMD_FILE))
			return;
		array<string> lines = DZS_Settings.ReadLines(CMD_FILE);
		DeleteFile(CMD_FILE);
		if (!lines)
			return;
		foreach (string l : lines)
		{
			string c = l;
			c.Replace((13).AsciiToString(), "");
			c.Replace((10).AsciiToString(), "");
			c.TrimInPlace();
			if (c != "")
				Run(c);
		}
	}

	protected Widget m_Sheet;

	protected void FontSheet()
	{
		if (m_Sheet)
			m_Sheet.Unlink();
		m_Sheet = DZS_FontSheet.Show(m_Kernel.GetScreen().GetDialogLayer());
	}

	protected void Run(string c)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		array<string> p = new array<string>();
		c.Split(" ", p);
		for (int i = p.Count() - 1; i >= 0; i--)
		{
			if (p[i] == "")
				p.Remove(i);
		}
		if (p[0] == "scene")
		{
			float x, y, w, h;
			scr.GetSceneRect(x, y, w, h);
			int sw, sh;
			GetScreenSize(sw, sh);
			DZS_Log.Info("uitest scene " + x + " " + y + " " + w + " " + h + " screen " + sw + "x" + sh + " scale " + DZS_Scale.Factor());
		}
		else if (p[0] == "keys")
		{
			DZS_Log.Info("uitest keys " + p[1] + " = [" + DZS_UiRegistry.Joined(m_Kernel.GetUi().GetBindings(p[1])) + "] target " + m_Kernel.GetUi().GetKeyboardTarget());
		}
		else if (p[0] == "file")
		{
			array<string> f = DZS_Settings.ReadLines(DZS_Settings.FILE);
			string all = "";
			if (f)
			{
				foreach (string fl : f)
				{
					string t = fl;
					t.TrimInPlace();
					all += t;
				}
			}
			DZS_Log.Info("uitest file " + all);
		}
		else if (p[0] == "setting")
		{
			DZS_Log.Info("uitest setting " + p[1] + " = " + m_Kernel.GetSettings().GetSettingNumber(p[1]) + " / bool " + DZS_Str.Bool(m_Kernel.GetSettings().GetBool(p[1])));
		}
		else if (p[0] == "change")
		{
			DZS_Instance inst = m_Kernel.GetShown();
			DZS_OpBuilder b = inst.BeginOperation("uitest:edit");
			map<string, string> gf = new map<string, string>();
			gf.Set("name", "Testgruppe");
			b.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, gf);
			b.Commit("Testgruppe anlegen", "Create test group");
			DZS_Log.Info("uitest change, unsaved " + inst.GetUnsavedCount());
		}
		else if (p[0] == "fonts")
			FontSheet();
		else if (p[0] == "fontsoff")
		{
			if (m_Sheet)
				m_Sheet.Unlink();
		}
		else if (p[0] == "flush")
			m_Kernel.GetSettings().Flush();
		else
			DZS_Log.Info("uitest: unknown command " + c);
	}
}

// A row in the properties part from a plugin.
class DZS_UiTestRow : DZS_PropertyRow
{
	protected Widget m_Frame;
	protected TextWidget m_Label;
	protected TextWidget m_Value;

	override void Build(Widget parent)
	{
		m_Frame = DZS_W.Frame(parent);
		m_Label = DZS_PropertiesApi.RowLabel(m_Frame, "Test-Zeile");
		m_Value = DZS_W.Text(m_Frame, DZS_Font.READ, 13, DZS_Color.Get("text"));
	}

	override bool AppliesTo(DZS_Instance inst, array<string> ids) { return ids.Count() > 0; }

	override float Layout(DZS_Instance inst, array<string> ids, float y, float width)
	{
		m_Value.SetText(DZS_Texts.Pick("Test-Plugin · " + ids.Count() + " ausgewählt", "Test plugin · " + ids.Count() + " selected"));
		DZS_W.Place(m_Frame, 0, y, width, DZS_Scale.Px(40));
		DZS_W.Place(m_Label, 0, 0, width, DZS_Scale.Px(14));
		DZS_W.Place(m_Value, 0, DZS_Scale.Px(15), width, DZS_Scale.Px(20));
		return DZS_Scale.Px(40);
	}

	override void Show(bool show) { m_Frame.Show(show); }
}
