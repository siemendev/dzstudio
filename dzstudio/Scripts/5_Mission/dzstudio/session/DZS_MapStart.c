// Starts a map as an editor session. The game needs a mission folder to load a world; the editor
// creates an empty one in its profile folder (the folder suffix names the world) and recognises it
// again when the mission starts.
class DZS_MapStart
{
	static const string MISSION_DIR = "$profile:dzstudio\\mission";
	static const string MISSION_PREFIX = "dzstudio.";

	static string MissionPath(string world)
	{
		return MISSION_DIR + "\\" + MISSION_PREFIX + world;
	}

	static bool IsEditorMission(string missionPath)
	{
		string p = missionPath;
		p.ToLower();
		return p.Contains("dzstudio\\mission\\" + MISSION_PREFIX) || p.Contains("dzstudio/mission/" + MISSION_PREFIX);
	}

	static void StartMap(string world)
	{
		MakeDirectory("$profile:dzstudio");
		MakeDirectory(MISSION_DIR);
		string path = MissionPath(world);
		MakeDirectory(path);
		DZS_Log.Info("starting map " + world + " via " + path);
		GetGame().PlayMission(path);
	}
}
