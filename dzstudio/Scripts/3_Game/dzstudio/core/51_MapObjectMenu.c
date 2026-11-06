// Context menu of a map object: placing opens it, other plugins add
// their entries below its own.
class DZS_MapObjectMenuSource : Managed
{
	void AddItems(Object o, array<ref DZS_MenuItem> items) {}
	// true when the entry was this source's
	bool OnChoose(Object o, DZS_MenuItem it) { return false; }
}

class DZS_MapObjectMenu
{
	protected static ref array<DZS_MapObjectMenuSource> s_Sources;

	static void AddSource(DZS_MapObjectMenuSource s)
	{
		if (!s_Sources)
			s_Sources = new array<DZS_MapObjectMenuSource>();
		if (s_Sources.Find(s) < 0)
			s_Sources.Insert(s);
	}

	static void Fill(Object o, array<ref DZS_MenuItem> items)
	{
		if (!s_Sources)
			return;
		foreach (DZS_MapObjectMenuSource s : s_Sources)
		{
			if (s)
				s.AddItems(o, items);
		}
	}

	static void Choose(Object o, DZS_MenuItem it)
	{
		if (!s_Sources)
			return;
		foreach (DZS_MapObjectMenuSource s : s_Sources)
		{
			if (s && s.OnChoose(o, it))
				return;
		}
	}
}
