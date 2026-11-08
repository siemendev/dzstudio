// The editor's layers (Objects, Spawns, Effects, Territories): tabs in the top bar, one on at a time. Objects is the
// normal editor; every other layer takes over the scene while it is on.
class DZS_Layer : Managed
{
	string Id() { return ""; }
	bool IsOn() { return false; }
	void TurnOff() {}
}

class DZS_Layers
{
	protected static ref array<DZS_Layer> s_All;

	static void Add(DZS_Layer l)
	{
		if (!s_All)
			s_All = new array<DZS_Layer>();
		s_All.Insert(l);
	}

	// Turns every layer off except 'keep' ("" = back to Objects).
	static void Only(string keep)
	{
		if (!s_All)
			return;
		foreach (DZS_Layer l : s_All)
		{
			if (l.Id() != keep && l.IsOn())
				l.TurnOff();
		}
	}

	static string Active()
	{
		if (!s_All)
			return "";
		foreach (DZS_Layer l : s_All)
		{
			if (l.IsOn())
				return l.Id();
		}
		return "";
	}
}
