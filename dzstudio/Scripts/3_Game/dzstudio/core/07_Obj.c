// Fields of an object in the project state. Every float32 value
// stands as its shortest exact text; vectors as three texts separated by one space.
class DZS_Obj
{
	static const string NAME = "name";
	static const string POS = "pos";
	static const string YPR = "ypr";
	static const string SCALE = "scale";
	// sizes the editor sets; read values outside stay as they are
	static const float SCALE_MIN = 0.05;
	static const float SCALE_MAX = 20;
	static const string PERSIST = "persist";
	static const string CUSTOM = "customString";

	static const string GROUP_NAME = "name";
	static const string GROUP_COLOR = "color";
	static const string GROUP_FILE = "file";
	static const string GROUP_FORMAT = "format";

	static string Vec(float a, float b, float c)
	{
		string ta = DZS_F32.ToText(a);
		string tb = DZS_F32.ToText(b);
		string tc = DZS_F32.ToText(c);
		return ta + " " + tb + " " + tc;
	}

	// Exact read of a vector field.
	static bool ParseVec(string s, out float a, out float b, out float c)
	{
		array<string> parts = new array<string>();
		s.Split(" ", parts);
		if (parts.Count() != 3)
			return false;
		return DZS_F32.FromText(parts[0], a) && DZS_F32.FromText(parts[1], b) && DZS_F32.FromText(parts[2], c);
	}

	// Quick read for display (not exact in the last bit).
	static vector QuickVec(string s)
	{
		array<string> parts = new array<string>();
		s.Split(" ", parts);
		if (parts.Count() != 3)
			return vector.Zero;
		return Vector(parts[0].ToFloat(), parts[1].ToFloat(), parts[2].ToFloat());
	}

	static bool IsModelPath(string name)
	{
		return name.Contains("\\") || name.Contains("/");
	}

	// Shortest text of a number for a field.
	static string Num(float f)
	{
		return DZS_F32.ToText(f);
	}
}

// Classes the running game knows: found without regard to case,
// with the spelling of the game.
class DZS_ClassDir
{
	protected static ref map<string, string> s_Names;

	protected static void Build()
	{
		s_Names = new map<string, string>();
		array<string> roots = {"CfgVehicles", "CfgWeapons", "CfgMagazines", "CfgNonAIVehicles", "CfgAmmo"};
		foreach (string root : roots)
		{
			int n = GetGame().ConfigGetChildrenCount(root);
			for (int i = 0; i < n; i++)
			{
				string name;
				GetGame().ConfigGetChildName(root, i, name);
				string low = name;
				low.ToLower();
				if (!s_Names.Contains(low))
					s_Names.Set(low, name);
			}
		}
		DZS_Log.Info("class directory: " + s_Names.Count() + " classes");
	}

	static void All(array<string> into)
	{
		if (!s_Names)
			Build();
		for (int i = 0; i < s_Names.Count(); i++)
			into.Insert(s_Names.GetElement(i));
	}

	// Spelling of the game, or "" if the class is unknown.
	static string Find(string name)
	{
		if (!s_Names)
			Build();
		string low = name;
		low.ToLower();
		string r;
		if (s_Names.Find(low, r))
			return r;
		return "";
	}
}
