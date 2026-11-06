// Marks of plugins: a plugin hands over its whole list of marks at once; the map draws them over
// the object points and under the camera mark, a plugin's later marks above its earlier ones.
class DZS_MapMark : Managed
{
	string m_Id;
	float m_X;
	float m_Z;
	int m_Color;
	bool m_Ring;
	// diameter in reference pixels
	float m_Size = 10;
	string m_Tip;
}

class DZS_MapMarkOwner : Managed
{
	// Double click on the mark (instead of the jump over the map).
	void OnMapMarkAction(string markId) {}
}

class DZS_MapMarks
{
	protected static ref array<string> s_Plugins;
	protected static ref map<string, ref array<ref DZS_MapMark>> s_Marks;
	protected static ref map<string, DZS_MapMarkOwner> s_Owners;
	protected static int s_Stamp;

	protected static void Init()
	{
		if (s_Plugins)
			return;
		s_Plugins = new array<string>();
		s_Marks = new map<string, ref array<ref DZS_MapMark>>();
		s_Owners = new map<string, DZS_MapMarkOwner>();
	}

	static void Set(string plugin, array<ref DZS_MapMark> marks, DZS_MapMarkOwner owner)
	{
		Init();
		if (s_Plugins.Find(plugin) < 0)
			s_Plugins.Insert(plugin);
		array<ref DZS_MapMark> copy = new array<ref DZS_MapMark>();
		foreach (DZS_MapMark m : marks)
			copy.Insert(m);
		s_Marks.Set(plugin, copy);
		s_Owners.Set(plugin, owner);
		s_Stamp++;
	}

	static void Clear(string plugin)
	{
		Init();
		if (!s_Marks.Contains(plugin))
			return;
		s_Marks.Remove(plugin);
		s_Owners.Remove(plugin);
		s_Stamp++;
	}

	static int Stamp() { return s_Stamp; }

	// Bottom to top.
	static void All(array<DZS_MapMark> marks, array<DZS_MapMarkOwner> owners)
	{
		Init();
		foreach (string p : s_Plugins)
		{
			array<ref DZS_MapMark> list = s_Marks.Get(p);
			if (!list)
				continue;
			DZS_MapMarkOwner o = s_Owners.Get(p);
			foreach (DZS_MapMark m : list)
			{
				marks.Insert(m);
				owners.Insert(o);
			}
		}
	}
}
