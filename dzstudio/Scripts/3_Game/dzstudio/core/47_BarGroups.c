// Groups of plugins in the project bar after the object groups (e.g. the loot group):
// a head row with hide and lock knobs, no colour and no "select all"; unfolded, rows of the plugin.
class DZS_BarGroupRow : Managed
{
	string m_Key;
	string m_Name;
	string m_Sub;
	string m_Mark;
	bool m_Lead;
}

class DZS_BarGroup : Managed
{
	string GetKey() { return ""; }
	bool IsShown(DZS_Instance inst) { return false; }
	string GetName() { return ""; }
	string GetSub(DZS_Instance inst) { return ""; }
	bool IsHidden(DZS_Instance inst) { return false; }
	bool IsLocked(DZS_Instance inst) { return false; }
	// Hide and lock knobs; off with a reason (tooltip).
	bool KnobsEnabled(DZS_Instance inst, out string reasonDe, out string reasonEn) { return true; }
	void SetHidden(DZS_Instance inst, bool hidden) {}
	void SetLocked(DZS_Instance inst, bool locked) {}
	void GetRows(DZS_Instance inst, array<ref DZS_BarGroupRow> into) {}
	void OnRowClick(DZS_Instance inst, string key, int button) {}
}

class DZS_BarGroups
{
	protected static ref array<ref DZS_BarGroup> s_List;

	static void Add(DZS_BarGroup g)
	{
		if (!s_List)
			s_List = new array<ref DZS_BarGroup>();
		s_List.Insert(g);
	}

	static array<DZS_BarGroup> All()
	{
		array<DZS_BarGroup> r = new array<DZS_BarGroup>();
		if (!s_List)
			return r;
		foreach (DZS_BarGroup g : s_List)
			r.Insert(g);
		return r;
	}

	static void Clear()
	{
		if (s_List)
			s_List.Clear();
	}
}
