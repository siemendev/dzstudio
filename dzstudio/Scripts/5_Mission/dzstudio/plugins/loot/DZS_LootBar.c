// The loot group in the project bar: the types the project
// uses or changed, a click on a type starts the editing at the nearest building of the type; hide and lock.
class DZS_LootBarGroup : DZS_BarGroup
{
	DZS_LootPlugin m_P;
	protected string m_MenuType;

	override string GetKey() { return "loot"; }
	override bool IsShown(DZS_Instance inst) { return m_P.IsOn(); }
	override string GetName() { return DZS_Texts.Get("loot.row"); }

	override string GetSub(DZS_Instance inst)
	{
		array<string> types = m_P.ListedTypes(inst);
		int n = types.Count();
		string word = DZS_LootPlugin.Plural(n, "Gebäudetyp", "Gebäudetypen", "building type", "building types");
		return DZS_LootFiles.PROTO + " · " + word;
	}

	override bool IsHidden(DZS_Instance inst)
	{
		string v;
		inst.GetField(DZS_PROJECT_ID, "lootHidden", v);
		return v == "true";
	}

	override bool IsLocked(DZS_Instance inst)
	{
		string v;
		inst.GetField(DZS_PROJECT_ID, "lootLocked", v);
		return v == "true";
	}

	override bool KnobsEnabled(DZS_Instance inst, out string reasonDe, out string reasonEn)
	{
		if (m_P.m_Editor.IsOn())
		{
			reasonDe = "Während der Loot-Bearbeitung nicht verfügbar";
			reasonEn = "Not available while editing loot";
			return false;
		}
		return true;
	}

	override void SetHidden(DZS_Instance inst, bool hidden)
	{
		string rd, re;
		if (!KnobsEnabled(inst, rd, re))
			return;
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "lootHidden", DZS_Str.Bool(hidden));
		DZS_SubmitResult r;
		if (hidden)
			r = b.Commit("Ausblenden", "Hide");
		else
			r = b.Commit("Einblenden", "Show");
		DZS_Log.Info("loot: group hidden " + hidden + " ok " + r.Ok());
	}

	override void SetLocked(DZS_Instance inst, bool locked)
	{
		string rd, re;
		if (!KnobsEnabled(inst, rd, re))
			return;
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "lootLocked", DZS_Str.Bool(locked));
		DZS_SubmitResult r;
		if (locked)
			r = b.Commit("Sperren", "Lock");
		else
			r = b.Commit("Entsperren", "Unlock");
		DZS_Log.Info("loot: group locked " + locked + " ok " + r.Ok());
	}

	override void GetRows(DZS_Instance inst, array<ref DZS_BarGroupRow> into)
	{
		array<string> types = m_P.ListedTypes(inst);
		foreach (string t : types)
		{
			DZS_BarGroupRow row = new DZS_BarGroupRow();
			row.m_Key = t;
			row.m_Name = m_P.ShownName(t);
			DZS_LootEEntry e = m_P.Effective(inst, t);
			int pts = 0;
			if (e)
				pts = e.CountPoints();
			row.m_Sub = DZS_LootPlugin.Plural(pts, "Punkt", "Punkte", "point", "points");
			row.m_Mark = m_P.OriginText(inst, t);
			string a = t;
			a.ToLower();
			string b = m_P.m_Editor.m_Type;
			b.ToLower();
			row.m_Lead = m_P.m_Editor.IsOn() && a == b;
			into.Insert(row);
		}
	}

	override void OnRowClick(DZS_Instance inst, string key, int button)
	{
		if (button == MouseState.RIGHT)
		{
			OpenRowMenu(inst, key);
			return;
		}
		m_P.EditType(inst, key);
	}

	protected void OpenRowMenu(DZS_Instance inst, string type)
	{
		m_MenuType = type;
		string rd, re;
		bool can = m_P.m_Editor.CanStart(rd, re);
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		DZS_MenuItem edit = new DZS_MenuItem();
		edit.m_Tag = "edit";
		edit.m_Text = DZS_Texts.Pick("Loot bearbeiten", "Edit loot");
		edit.m_Enabled = can;
		edit.m_Reason = DZS_Texts.Pick(rd, re);
		items.Insert(edit);
		string st = m_P.Status(inst, type);
		if (st == "own" && m_P.HasBase(type))
		{
			DZS_MenuItem reset = new DZS_MenuItem();
			reset.m_Tag = "reset";
			reset.m_Text = DZS_Texts.Pick("Zurücksetzen …", "Reset …");
			reset.m_Enabled = can;
			reset.m_Reason = edit.m_Reason;
			items.Insert(reset);
		}
		if (st == "new" && !m_P.UsedByObjects(inst, type))
		{
			DZS_MenuItem del = new DZS_MenuItem();
			del.m_Tag = "remove";
			del.m_Text = DZS_Texts.Pick("Eintrag entfernen", "Remove entry");
			items.Insert(del);
		}
		DZS_Popup pop = DZS_Kernel.Get().GetScreen().OpenContextMenu(items);
		pop.m_OnChoose.Insert(OnMenu);
		DZS_Log.Info("loot: type menu " + type + ", " + items.Count() + " entries");
	}

	void OnMenu(DZS_Popup p, DZS_MenuItem it)
	{
		DZS_Kernel.Get().GetScreen().CloseMenu();
		DZS_Instance inst = DZS_Kernel.Get().GetShown();
		if (!inst || !it.m_Enabled)
			return;
		if (it.m_Tag == "edit")
			m_P.EditType(inst, m_MenuType);
		else if (it.m_Tag == "reset")
		{
			if (m_P.EditType(inst, m_MenuType))
				m_P.m_Editor.AskReset();
		}
		else if (it.m_Tag == "remove")
			m_P.RemoveEntry(inst, m_MenuType);
	}
}
