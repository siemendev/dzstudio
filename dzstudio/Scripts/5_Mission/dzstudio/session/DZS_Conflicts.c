// Mods that take over the same main menu, missions and keys as DZSTUDIO. With one of them loaded
// DZSTUDIO stays off and tells the player once per game start.
class DZS_Conflicts
{
	protected static bool s_Checked;
	protected static string s_Name;
	protected static bool s_Shown;
	protected static float s_Wait;

	static string Found()
	{
		if (!s_Checked)
		{
			s_Checked = true;
			if (GetGame().ConfigIsExisting("CfgMods DayZEditor"))
				s_Name = "DayZ Editor";
			if (s_Name != "")
				DZS_Log.Warn(s_Name + " is loaded as well, DZSTUDIO stays off");
		}
		return s_Name;
	}

	static void Update(float timeslice)
	{
		if (s_Shown || Found() == "")
			return;
		s_Wait += timeslice;
		if (s_Wait < 1.5)
			return;
		s_Shown = true;
		string text;
		if (DZS_Texts.IsGerman())
			text = string.Format("%1 ist ebenfalls aktiv. DZSTUDIO und %1 belegen dasselbe Hauptmenü und dieselben Tasten und können nicht zusammen laufen, deshalb bleibt DZSTUDIO aus.\n\nDeaktiviere einen der beiden im DayZ Launcher und starte das Spiel neu.", s_Name);
		else
			text = string.Format("%1 is enabled as well. DZSTUDIO and %1 both take over the main menu and the same keys and cannot run together, so DZSTUDIO stays off.\n\nDisable one of them in the DayZ Launcher and restart the game.", s_Name);
		GetGame().GetUIManager().ShowDialog("DZSTUDIO", text, 0, DBT_OK, DBB_OK, DMT_WARNING, null);
	}
}

modded class MissionBase
{
	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		DZS_Conflicts.Update(timeslice);
	}
}
