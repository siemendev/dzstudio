modded class MissionGameplay
{
	protected ref DZS_Session m_DZS_Session;

	override void OnMissionStart()
	{
		super.OnMissionStart();
		if (DZS_MapStart.IsEditorMission(GetGame().GetMissionPath()))
		{
			m_DZS_Session = new DZS_Session();
			m_DZS_Session.Start();
		}
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		if (m_DZS_Session)
			m_DZS_Session.Update(timeslice);
	}

	override void OnMissionFinish()
	{
		if (m_DZS_Session)
			m_DZS_Session.End();
		super.OnMissionFinish();
	}
}

// The editor's main menu replaces the game's: no game menu, no intro scene;
// the kernel runs without a world and the projects plugin shows its menu.
modded class MissionMainMenu
{
	protected bool m_DZS_Kernel;

	override void OnInit()
	{
		m_NoCutscene = true;
		GetOnInputDeviceChanged().Insert(OnInputDeviceChanged);
		DZS_Log.Info("main menu, api " + DZS_API_VERSION);
		string sheet;
		if (GetCLIParam("dzsFontSheet", sheet))
			DZS_FontSheet.Show(null);
		string world;
		if (DZS_Handover.s_Kind == "" && GetCLIParam("dzsMap", world) && world != "" && !DZS_Handover.s_AutoDone)
		{
			DZS_Handover.s_AutoDone = true;
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(DZS_AutoStart, 1500, false, world);
			return;
		}
		DZS_Kernel.Start("");
		m_DZS_Kernel = true;
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		if (m_DZS_Kernel && DZS_Kernel.Get())
			DZS_Kernel.Get().Update(timeslice);
	}

	override void OnMissionFinish()
	{
		if (m_DZS_Kernel && DZS_Kernel.Get())
			DZS_Kernel.Get().Shutdown();
		m_DZS_Kernel = false;
		super.OnMissionFinish();
	}

	void DZS_AutoStart(string world)
	{
		DZS_MapStart.StartMap(world);
	}
}

// Every font role at a range of widget heights, with the size the game reports (test aid).
class DZS_FontSheet
{
	static Widget Show(Widget parent)
	{
		Widget sheet = DZS_W.Panel(parent, ARGB(255, 10, 10, 12));
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(sheet, 0, 0, sw, sh);
		array<string> roles = {"display", "label", "labelbold", "mono", "menu", "read", "readbold"};
		float x = 20;
		foreach (string r : roles)
		{
			float y = 20;
			for (int h = 14; h <= 40; h += 2)
			{
				TextWidget t = TextWidget.Cast(DZS_W.FromTexts(r, sheet));
				t.SetPos(x, y);
				t.SetSize(540, h);
				t.SetText(r + " " + h + " Wg Äö 1111,11");
				int tw, th;
				t.GetTextSize(tw, th);
				DZS_Log.Info("font " + r + " h" + h + " -> " + tw + "x" + th);
				y += h + 6;
			}
			x += 540;
		}
		return sheet;
	}
}
