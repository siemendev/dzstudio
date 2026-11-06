// Displayed pose: a pose a plugin sets for an object that only shows it elsewhere (live moves of
// others, views during a gesture). View only: no state, no operation. Whatever draws an object for display (model,
// frames, dots, the object toolbar, map points) asks here first; operations and values use the project state.
class DZS_ViewPose
{
	protected static ref map<string, vector> s_Pos;
	protected static ref map<string, vector> s_Ypr;
	protected static ref map<string, float> s_Scale;
	protected static int s_Stamp;

	protected static void Init()
	{
		if (s_Pos)
			return;
		s_Pos = new map<string, vector>();
		s_Ypr = new map<string, vector>();
		s_Scale = new map<string, float>();
	}

	static void Set(string id, vector pos, vector ypr, float scale)
	{
		Init();
		s_Pos.Set(id, pos);
		s_Ypr.Set(id, ypr);
		s_Scale.Set(id, scale);
		s_Stamp++;
	}

	static void Clear(string id)
	{
		Init();
		if (!s_Pos.Contains(id))
			return;
		s_Pos.Remove(id);
		s_Ypr.Remove(id);
		s_Scale.Remove(id);
		s_Stamp++;
	}

	static void ClearAll()
	{
		Init();
		s_Pos.Clear();
		s_Ypr.Clear();
		s_Scale.Clear();
		s_Stamp++;
	}

	static bool Get(string id, out vector pos, out vector ypr, out float scale)
	{
		Init();
		if (!s_Pos.Contains(id))
			return false;
		pos = s_Pos.Get(id);
		ypr = s_Ypr.Get(id);
		scale = s_Scale.Get(id);
		return true;
	}

	static bool Has(string id)
	{
		Init();
		return s_Pos.Contains(id);
	}

	// Changes with every Set or Clear; caches of drawn poses compare it.
	static int Stamp() { return s_Stamp; }
}
