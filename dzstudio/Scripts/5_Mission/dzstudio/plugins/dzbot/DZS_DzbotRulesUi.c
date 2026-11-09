// The dzbot rules of a project mod in dzStudio: an overview of the project's own blocks and changes (as
// DZS_ProjectV2.Build makes them, so ids and refs are the ones dzbot sees) and a dialog per block, change or list
// element. Each dialog holds on to its entity by identity (DZS_ExtOwner, an element's "<list>@<element id>"), never by
// a block id that may be renumbered or a place in a list; it closes when its entity is deleted (also by undo or a
// collab partner) and checks again before it writes. Settings are held by their identifier. Nested conditions (groups
// inside groups, "not") are shown read-only; they are edited in dzbot.

// The dialogs of the rules: lines of cells (DZS_LinesDialog) with the rules they edit.
class DZS_RulesBase : DZS_LinesDialog
{
	protected DZS_DzbotRules m_Rules;
}

// Every own block and change of the open project mod with its rules; a group's block (or the selection's) first.
class DZS_RulesDialog : DZS_RulesBase
{
	static const int MAX_ROWS = 250;

	protected DZS_Instance m_Inst;
	protected ref DZS_Json m_Project;
	protected string m_FocusBlock;
	// block id -> group id of the spawner groups
	protected ref map<string, string> m_GroupOf = new map<string, string>();
	protected ref array<string> m_RowSection = new array<string>();
	protected ref array<string> m_RowRef = new array<string>();
	protected ref array<ref DZS_Json> m_RowJson = new array<ref DZS_Json>();
	protected ref array<TextWidget> m_RowSummary = new array<TextWidget>();
	// where each row's entity keeps its rules, taken when the list was made
	protected ref array<ref DZS_ExtOwner> m_RowOwner = new array<ref DZS_ExtOwner>();
	// the settings' identifiers and places as listed
	protected ref array<string> m_SetIds = new array<string>();
	protected ref array<int> m_SetAt = new array<int>();

	void DZS_RulesDialog(DZS_DzbotRules rules, DZS_Instance inst, string groupId)
	{
		m_Rules = rules;
		m_Inst = inst;
		string name = "";
		string mapKey = "";
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(DZS_Kernel.Get().GetPlugin("projects"));
		if (pp && pp.m_Open)
		{
			name = pp.m_Open.m_Name;
			mapKey = pp.m_Open.m_MapKey;
		}
		m_Project = DZS_ProjectV2.Build(DZS_Kernel.Get(), inst, name, mapKey);
		map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
		for (int i = 0; i < ids.Count(); i++)
			m_GroupOf.Set(ids.GetElement(i), ids.GetKey(i));
		string gid = groupId;
		if (gid == "")
			gid = SelectedGroup(inst);
		if (gid != "")
			m_FocusBlock = ids.Get(gid);
		Setup(LARGE, "dzbot", DZS_Texts.Pick("Regeln", "Rules"), false);
		AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "main");
		Watch(inst);
	}

	// The project changed: the summaries anew (a deleted entity's row says so).
	override void StateChanged()
	{
		Refresh();
	}

	// the group of the lead selected object, "" when none
	static string SelectedGroup(DZS_Instance inst)
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel)
			return "";
		string lead = sel.GetLead();
		if (lead == "")
		{
			array<string> picked = sel.GetSelection();
			if (picked.Count() > 0)
				lead = picked[0];
		}
		if (lead == "")
			return "";
		if (inst.GetKind(lead) == DZS_KIND_GROUP)
			return lead;
		return inst.GetParent(lead);
	}

	string GroupOf(string blockId)
	{
		string gid;
		if (m_GroupOf.Find(blockId, gid))
			return gid;
		return "";
	}

	override void BuildContent(Widget inner)
	{
		m_Parent = inner;
		Note(DZS_Texts.Pick("Wann und wie dzbot die Blöcke und Änderungen auf einem Server anwendet, nach den Einstellungen des Mods. Gespeichert wird mit dem Projekt.", "When and how dzbot applies the blocks and changes on a server, by the mod's settings. Saved with the project."));
		SettingsPart();
		DZS_Json blocks = m_Project.Get("blocks");
		DZS_Json changes = m_Project.Get("changes");
		Heading(DZS_Texts.Pick("Eigene Blöcke", "Own blocks"));
		int shown = 0;
		int more = 0;
		if (m_FocusBlock != "")
		{
			for (int f = 0; f < blocks.Count(); f++)
			{
				DZS_Json fb = blocks.At(f);
				if (fb.GetStr("id") == m_FocusBlock)
				{
					Row("blocks", fb.GetStr("id"), fb, DZS_DzbotRules.BlockLabel(fb), true);
					shown++;
				}
			}
		}
		for (int i = 0; i < blocks.Count(); i++)
		{
			DZS_Json b = blocks.At(i);
			string bid = b.GetStr("id");
			if (bid == m_FocusBlock)
				continue;
			if (shown >= MAX_ROWS)
			{
				more++;
				continue;
			}
			Row("blocks", bid, b, DZS_DzbotRules.BlockLabel(b), false);
			shown++;
		}
		if (blocks.Count() == 0)
			Note(DZS_Texts.Pick("Keine eigenen Blöcke.", "No own blocks."));
		Heading(DZS_Texts.Pick("Änderungen", "Changes"));
		for (int k = 0; k < changes.Count(); k++)
		{
			DZS_Json c = changes.At(k);
			string cref = DZS_DzbotRules.ChangeRef(c);
			if (shown >= MAX_ROWS)
			{
				more++;
				continue;
			}
			Row("changes", cref, c, DZS_DzbotRules.ChangeLabel(c), false);
			shown++;
		}
		if (changes.Count() == 0)
			Note(DZS_Texts.Pick("Keine Änderungen.", "No changes."));
		if (more > 0)
			Note(DZS_Texts.Pick(more.ToString() + " weitere — in dzbot bearbeiten", more.ToString() + " more — edit them in dzbot"));
		AddFootFocus();
	}

	protected void Row(string section, string rid, DZS_Json json, string label, bool focus)
	{
		DZS_RLine l = Line(2, 30);
		string tone = "text";
		if (focus)
			tone = "accentText";
		Label(l, label, 0, tone, 14);
		DZS_ExtOwner o = DZS_EntityExts.Owner(m_Inst, section, rid);
		TextWidget sum = Label(l, Summary(o), -1, "textQuiet", 13);
		int i = m_RowRef.Count();
		Knob(l, "side", DZS_Texts.Pick("Regeln", "Rules"), "edit:" + i);
		m_RowSection.Insert(section);
		m_RowRef.Insert(rid);
		m_RowJson.Insert(json);
		m_RowSummary.Insert(sum);
		m_RowOwner.Insert(o);
	}

	// The mod's settings (config fields): the project's own list, else the one dzbot sent (copied on the first edit).
	protected void SettingsPart()
	{
		Heading(DZS_Texts.Pick("Einstellungen des Mods", "Settings of the mod"));
		DZS_Json list = m_Rules.SettingsList();
		if (!list)
		{
			array<ref DZS_DzbotSetting> sent = m_Rules.Settings();
			if (sent.Count() == 0)
				Note(DZS_Texts.Pick("Noch keine Einstellungen — dzbot fragt sie beim Installieren des Mods ab.", "No settings yet — dzbot asks for them when the mod is installed."));
			else
				Note(DZS_Texts.Pick("Aus dzbot — beim ersten Ändern werden sie ins Projekt übernommen.", "From dzbot — the first change copies them into the project."));
			for (int i = 0; i < sent.Count(); i++)
			{
				DZS_DzbotSetting st = sent[i];
				SettingRow(i, st.m_Id, st.Shown(), st.m_Type);
			}
		}
		else
		{
			for (int k = 0; k < list.Count(); k++)
			{
				DZS_Json entry = list.At(k);
				string id = DZS_DzbotRules.SettingId(entry);
				string shown = id;
				string title = "";
				if (entry.IsObj())
					title = entry.GetStr("title");
				if (title != "" && title != id)
					shown = title + " (" + id + ")";
				if (shown == "")
					shown = DZS_Texts.Pick("(ohne Namen)", "(no name)");
				string type = "";
				if (entry.IsObj())
					type = entry.GetStr("type");
				if (type == DZS_DzbotRules.SECTION)
				{
					string head = title;
					if (head == "")
						head = id;
					SectionRow(k, id, head);
				}
				else
					SettingRow(k, id, shown, type);
			}
		}
		DZS_RLine al = Line(6, 28);
		Knob(al, "side", DZS_Texts.Pick("+ Einstellung", "+ Setting"), "setAdd");
	}

	protected void SettingRow(int at, string id, string shown, string type)
	{
		DZS_RLine l = Line(2, 30);
		Label(l, shown, 0, "text", 14);
		Label(l, DZS_SettingDialog.TypeLabel(type), -1, "textQuiet", 13);
		int i = m_SetIds.Count();
		Knob(l, "side", DZS_Texts.Pick("Bearbeiten", "Edit"), "set:" + i);
		Knob(l, "ghost", "", "setDel:" + i, "x");
		m_SetIds.Insert(id);
		m_SetAt.Insert(at);
	}

	// A section of dzbot's form: a heading over the settings below it.
	protected void SectionRow(int at, string id, string head)
	{
		DZS_RLine l = Line(10, 30);
		Label(l, DZS_Texts.Upper(head), 0, "textMuted", 12);
		int i = m_SetIds.Count();
		Knob(l, "side", DZS_Texts.Pick("Bearbeiten", "Edit"), "set:" + i);
		Knob(l, "ghost", "", "setDel:" + i, "x");
		m_SetIds.Insert(id);
		m_SetAt.Insert(at);
	}

	// The place of a setting in the list as it is now: by its identifier (the listed place decides between equal
	// ones, sections may share one); -1 when it is gone.
	static int FindSetting(DZS_Json list, string id, int listedAt)
	{
		if (!list)
			return -1;
		int found = -1;
		for (int i = 0; i < list.Count(); i++)
		{
			if (DZS_DzbotRules.SettingId(list.At(i)) != id)
				continue;
			if (i == listedAt)
				return i;
			if (found < 0)
				found = i;
		}
		return found;
	}

	static void SettingGone()
	{
		DZS_LinesDialog.GoneNotice("Die Einstellung wurde gelöscht", "This setting was deleted");
	}

	void Reopen()
	{
		if (m_Closed)
			return;
		DZS_RulesDialog again = new DZS_RulesDialog(m_Rules, m_Inst, GroupOf(m_FocusBlock));
		again.KeepScroll(m_Scroll);
		Close();
		again.Open();
	}

	void KeepScroll(float s)
	{
		m_Scroll = s;
	}

	protected string m_AskId;
	protected int m_AskAt = -1;

	protected void SettingKnob(string what, int row)
	{
		if (what == "setAdd")
		{
			DZS_Json fresh = DZS_Json.Obj();
			DZS_Json ty = DZS_Json.Str("number");
			fresh.Set("type", ty);
			DZS_SettingDialog add = new DZS_SettingDialog(m_Rules, this, "", -1, fresh, null, m_Inst);
			add.Open();
			return;
		}
		if (row < 0 || row >= m_SetIds.Count())
			return;
		DZS_Json list = m_Rules.SettingsForEdit();
		int i = FindSetting(list, m_SetIds[row], m_SetAt[row]);
		if (i < 0)
		{
			SettingGone();
			Reopen();
			return;
		}
		DZS_Json entry = list.At(i);
		string id = DZS_DzbotRules.SettingId(entry);
		if (what == "set")
		{
			DZS_Json work = entry.Clone();
			DZS_SettingDialog d = new DZS_SettingDialog(m_Rules, this, id, i, work, null, m_Inst);
			d.Open();
			return;
		}
		m_AskId = id;
		m_AskAt = i;
		string text = DZS_Texts.Pick("Die Einstellung „", "Remove the setting '") + id;
		text += DZS_Texts.Pick("“ entfernen? Regeln, die sie nennen, finden sie danach nicht mehr.", "'? Rules that name it will not find it any more.");
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(DZS_Texts.Pick("dzbot-Regeln", "dzbot rules"), DZS_Texts.Pick("Einstellung entfernen", "Remove setting"), text);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button yes = c.AddFootButton("remove", DZS_Texts.Pick("Entfernen", "Remove"), "danger");
		yes.m_Hit.m_Primary = true;
		c.m_OnResult.Insert(OnRemoveSetting);
		c.Open();
	}

	void OnRemoveSetting(DZS_Dialog d, string result)
	{
		string id = m_AskId;
		int at = m_AskAt;
		m_AskAt = -1;
		if (result != "remove")
			return;
		DZS_Json list = m_Rules.SettingsForEdit();
		int i = FindSetting(list, id, at);
		if (i < 0)
		{
			SettingGone();
			Reopen();
			return;
		}
		list.m_Items.RemoveOrdered(i);
		int renamed;
		m_Rules.CommitSettings(list, "", "", "Einstellung entfernen", "Remove setting", renamed);
		Reopen();
	}

	protected string Summary(DZS_ExtOwner o)
	{
		if (!DZS_EntityExts.Alive(m_Inst, o))
			return DZS_Texts.Pick("gelöscht", "deleted");
		string s = DZS_DzbotRules.Summary(m_Rules.RuleAt(o));
		if (s == "")
			return DZS_Texts.Pick("immer dabei", "always");
		return s;
	}

	// After a rule changed: the summaries anew.
	void Refresh()
	{
		if (m_Closed)
			return;
		for (int i = 0; i < m_RowSummary.Count(); i++)
		{
			TextWidget t = m_RowSummary[i];
			if (t)
				t.SetText(DZS_W.Literal(Summary(m_RowOwner[i])));
		}
		Layout();
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		if (tag == "setAdd")
		{
			SettingKnob(tag, -1);
			return;
		}
		int colon = tag.IndexOf(":");
		if (colon < 0)
			return;
		string what = tag.Substring(0, colon);
		int i = tag.Substring(colon + 1, tag.Length() - colon - 1).ToInt();
		if (what == "set" || what == "setDel")
		{
			SettingKnob(what, i);
			return;
		}
		if (i < 0 || i >= m_RowRef.Count())
			return;
		DZS_Json json = m_RowJson[i];
		DZS_ExtOwner o = m_RowOwner[i];
		if (!json)
			return;
		if (!DZS_EntityExts.Alive(m_Inst, o))
		{
			GoneNotice();
			Refresh();
			return;
		}
		string kind = "change";
		string label = DZS_DzbotRules.ChangeLabel(json);
		if (m_RowSection[i] == "blocks")
		{
			kind = "block";
			label = DZS_DzbotRules.BlockLabel(json);
		}
		DZS_Json rule = m_Rules.RuleAt(o);
		DZS_Json work;
		if (rule)
			work = rule.Clone();
		else
			work = DZS_Json.Obj();
		DZS_Json now = DZS_EntityExts.BlockJson(m_Inst, o);
		if (kind == "block" && now)
			json = now;
		DZS_RuleDialog d = new DZS_RuleDialog(m_Rules, this, null, kind, m_RowRef[i], label, json, work, m_Inst, o);
		if (kind == "block")
			d.SetGroup(GroupOf(m_RowRef[i]));
		d.Open();
	}
}

// One comparison of a condition: the setting, the operator (kept in the condition) and the value.
class DZS_CondRow : Managed
{
	DZS_Json m_Cond;
	DZS_TextField m_Setting;
	DZS_TextField m_Value;
}

// One value of a block or element from a setting or a formula.
class DZS_FieldRow : Managed
{
	ref DZS_Json m_Rule;
	DZS_TextField m_Path;
	DZS_TextField m_Value;
}

// The rules of one block, change or list element (kind "block", "change", "item"); edits a copy ("work") that
// "Apply" stores. An element's dialog stores into the dialog of its block. m_Owner is the block's or change's entity
// (an element's: its block's), m_Ref an element's "<list>@<element id>".
class DZS_RuleDialog : DZS_RulesBase
{
	protected DZS_RulesDialog m_Overview;
	protected DZS_RuleDialog m_Outer;
	protected string m_Kind;
	protected string m_Ref;
	protected string m_LabelText;
	protected ref DZS_Json m_Target;
	protected ref DZS_Json m_Work;
	protected DZS_Instance m_Inst;
	protected string m_GroupId;
	protected ref array<ref DZS_CondRow> m_Conds = new array<ref DZS_CondRow>();
	protected ref array<ref DZS_FieldRow> m_FieldRows = new array<ref DZS_FieldRow>();
	protected DZS_TextField m_Formula;
	protected DZS_TextField m_Count;
	protected DZS_TextField m_Over;
	protected ref array<string> m_ItemPaths = new array<string>();
	protected int m_PickIndex;
	protected ref DZS_ExtOwner m_Owner;

	void DZS_RuleDialog(DZS_DzbotRules rules, DZS_RulesDialog overview, DZS_RuleDialog outer, string kind, string rid, string label, DZS_Json target, DZS_Json work, DZS_Instance inst, DZS_ExtOwner owner)
	{
		m_Owner = owner;
		m_Rules = rules;
		m_Overview = overview;
		m_Outer = outer;
		m_Kind = kind;
		m_Ref = rid;
		m_LabelText = label;
		m_Target = target;
		m_Work = work;
		m_Inst = inst;
		string kicker = DZS_Texts.Pick("dzbot-Regeln", "dzbot rules");
		Setup(LARGE, kicker, DZS_W.Literal(label), true);
		AddFootButtonLeft("drop", DZS_Texts.Pick("Regeln entfernen", "Remove the rules"), "danger");
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button go = AddFootButton("apply", DZS_Texts.Pick("Übernehmen", "Apply"), "main");
		go.m_Hit.m_Primary = true;
		Watch(inst);
	}

	void SetGroup(string groupId)
	{
		m_GroupId = groupId;
	}

	override bool TargetAlive()
	{
		if (m_Kind == "item")
			return DZS_EntityExts.ItemOwner(m_Inst, m_Owner, m_Ref) != null;
		return DZS_EntityExts.Alive(m_Inst, m_Owner);
	}

	// The element an item key names, as it is now; null when it left the block.
	protected DZS_Json ItemJson(string key)
	{
		DZS_ExtOwner block = m_Owner;
		return DZS_EntityExts.ItemJson(m_Inst, block, key);
	}

	// "objects[3] · Land_Tower", the place as it is now; "(gelöscht)" when it left the block.
	protected string ItemLabel(string key)
	{
		string path = DZS_EntityExts.ItemPath(m_Inst, m_Owner, key);
		if (path == "")
			return DZS_Texts.Pick("(gelöscht)", "(deleted)");
		DZS_Json el = ItemJson(key);
		if (el)
			return path + " · " + ElementName(el);
		return path;
	}

	protected string Section()
	{
		if (m_Kind == "block")
			return "blocks";
		return "changes";
	}

	// the data paths count from: the block's data, or the element itself (as they are now when still there)
	protected DZS_Json Data()
	{
		if (m_Kind == "item")
		{
			DZS_Json now = ItemJson(m_Ref);
			if (now)
				return now;
			return m_Target;
		}
		if (m_Kind == "block")
		{
			DZS_Json bj = DZS_EntityExts.BlockJson(m_Inst, m_Owner);
			if (bj)
				return bj.Get("data");
		}
		if (m_Target)
			return m_Target.Get("data");
		return null;
	}

	// whether a repetition around this rule gives "index" / "item"
	protected bool Repeated(bool overList)
	{
		DZS_Json r = m_Work.Get("repeat");
		if (r && r.IsObj() && (!overList || r.Has("over")))
			return true;
		if (m_Outer)
			return m_Outer.Repeated(overList);
		return false;
	}

	override void BuildContent(Widget inner)
	{
		m_Parent = inner;
		BuildCondition();
		if (m_Kind == "change" && DZS_DzbotRules.CanFormula(m_Target))
			BuildFormula();
		if (m_Kind == "change" || (m_Kind == "block" && DZS_DzbotRules.CanRequire(m_Target)))
		{
			DZS_RLine rl = Line(14, 28);
			Switch(rl, m_Work.GetBool("required"), "required");
			Label(rl, DZS_Texts.Pick("Ziel nötig — fehlt es auf einem Server (oder gibt es mehrere), wird das Projekt dort nicht angewendet", "Target required — when a server lacks it (or has several), the project is not applied there"), 0, "text", 13);
		}
		if (m_Kind != "change")
		{
			BuildRepeat();
			BuildFields();
		}
		if (m_Kind == "block")
			BuildItems();
		AddFootFocus();
	}

	// ---- condition ----

	// The comparisons the builder edits and how they join ("all"/"any"); false when the condition is nested.
	protected bool Comparisons(array<DZS_Json> list, out string join)
	{
		join = "all";
		DZS_Json cond = m_Work.Get("if");
		if (!cond || !cond.IsObj() || cond.Count() == 0)
			return true;
		if (cond.Has("field"))
		{
			list.Insert(cond);
			return true;
		}
		DZS_Json group = cond.Get("all");
		if (!group)
		{
			group = cond.Get("any");
			join = "any";
		}
		if (!group || !group.IsArr() || cond.Count() != 1)
			return false;
		for (int i = 0; i < group.Count(); i++)
		{
			DZS_Json one = group.At(i);
			if (!one.IsObj() || !one.Has("field") || one.Has("all") || one.Has("any") || one.Has("not"))
				return false;
			list.Insert(one);
		}
		return true;
	}

	protected void BuildCondition()
	{
		Heading(DZS_Texts.Pick("Nimmt teil, wenn", "Takes part when"), 0);
		array<DZS_Json> list = new array<DZS_Json>();
		string join;
		if (!Comparisons(list, join))
		{
			DZS_Json cond = m_Work.Get("if");
			array<string> chunks = cond.ToChunks(false);
			string text = "";
			foreach (string c : chunks)
				text += c;
			Note(text, "text");
			DZS_RLine nl = Line(4, 28);
			Label(nl, DZS_Texts.Pick("Verschachtelte Bedingung — in dzbot bearbeiten.", "Nested condition — edit it in dzbot."), 0, "textQuiet", 13);
			Knob(nl, "danger", DZS_Texts.Pick("Bedingung entfernen", "Remove the condition"), "condClear");
			return;
		}
		if (list.Count() == 0)
			Note(DZS_Texts.Pick("Immer — keine Bedingung.", "Always — no condition."));
		if (list.Count() > 1)
		{
			DZS_RLine jl = Line(0, 28);
			string jt = DZS_Texts.Pick("alle gelten", "all apply");
			if (join == "any")
				jt = DZS_Texts.Pick("mindestens eine gilt", "at least one applies");
			Knob(jl, "side", jt, "condJoin");
		}
		for (int i = 0; i < list.Count(); i++)
		{
			DZS_Json cmp = list[i];
			DZS_CondRow row = new DZS_CondRow();
			row.m_Cond = cmp;
			DZS_RLine l = Line(4, 28);
			row.m_Setting = Field(l, cmp.GetStr("field"), DZS_Texts.Pick("Einstellung", "Setting"), 180);
			Knob(l, "ghost", "", "condSetting:" + i, "chevron-down");
			string op = cmp.GetStr("op");
			if (op == "")
				op = "eq";
			Knob(l, "side", OpLabel(op), "condOp:" + i);
			if (op != "empty" && op != "not_empty")
			{
				string shown = "";
				DZS_Json v = cmp.Get("value");
				if (v)
					shown = ValueText(v);
				string hint = DZS_Texts.Pick("Wert", "Value");
				if (op == "in" || op == "not_in")
					hint = DZS_Texts.Pick("Werte, mit Komma getrennt", "Values, separated by commas");
				row.m_Value = Field(l, shown, hint, 0);
				DZS_DzbotSetting st = m_Rules.Setting(cmp.GetStr("field"));
				if (st && (st.m_Type == "boolean" || st.m_Options.Count() > 0))
					Knob(l, "ghost", "", "condValue:" + i, "chevron-down");
			}
			Knob(l, "ghost", "", "condDel:" + i, "x");
			m_Conds.Insert(row);
		}
		DZS_RLine al = Line(6, 28);
		Knob(al, "side", DZS_Texts.Pick("+ Bedingung", "+ Condition"), "condAdd");
	}

	static string OpLabel(string op)
	{
		if (op == "eq") return "=";
		if (op == "neq") return "!=";
		if (op == "gt") return ">";
		if (op == "gte") return ">=";
		if (op == "lt") return "<";
		if (op == "lte") return "<=";
		if (op == "in") return DZS_Texts.Pick("eine von", "one of");
		if (op == "not_in") return DZS_Texts.Pick("keine von", "none of");
		if (op == "contains") return DZS_Texts.Pick("enthält", "contains");
		if (op == "empty") return DZS_Texts.Pick("ist leer", "is empty");
		if (op == "not_empty") return DZS_Texts.Pick("ist gesetzt", "is set");
		return op;
	}

	static string ValueText(DZS_Json v)
	{
		if (v.IsBool())
			return DZS_Str.Bool(v.m_Bool);
		if (v.IsStr() || v.IsNum())
			return v.m_Str;
		if (v.IsArr())
		{
			string t = "";
			for (int i = 0; i < v.Count(); i++)
			{
				if (i > 0)
					t += ", ";
				DZS_Json item = v.At(i);
				t += ValueText(item);
			}
			return t;
		}
		return "";
	}

	// Typed as dzbot compares: true/false, a number, else text.
	static DZS_Json ValueOf(string text)
	{
		string t = text;
		t.TrimInPlace();
		if (t == "true" || t == "false")
			return DZS_Json.Boolean(t == "true");
		if (t != "" && DZS_Json.IsNumberText(t) && t != "-" && t != ".")
		{
			string err;
			DZS_Json n = DZS_Json.ParseText(t, err);
			if (n && n.IsNum())
				return n;
		}
		return DZS_Json.Str(t);
	}

	protected DZS_Json NewComparison()
	{
		DZS_Json c = DZS_Json.Obj();
		string first = "";
		foreach (DZS_DzbotSetting st : m_Rules.Settings())
		{
			if (st.m_Type == "boolean")
			{
				first = st.m_Id;
				break;
			}
		}
		DZS_Json fj = DZS_Json.Str(first);
		c.Set("field", fj);
		DZS_Json oj = DZS_Json.Str("eq");
		c.Set("op", oj);
		DZS_Json vj = DZS_Json.Boolean(true);
		c.Set("value", vj);
		return c;
	}

	protected void AddComparison()
	{
		DZS_Json c = NewComparison();
		DZS_Json cond = m_Work.Get("if");
		if (!cond || !cond.IsObj() || cond.Count() == 0)
		{
			m_Work.Set("if", c);
			return;
		}
		if (cond.Has("field"))
		{
			DZS_Json group = DZS_Json.Obj();
			DZS_Json arr = DZS_Json.Arr();
			arr.Add(cond);
			arr.Add(c);
			group.Set("all", arr);
			m_Work.Set("if", group);
			return;
		}
		DZS_Json list = cond.Get("all");
		if (!list)
			list = cond.Get("any");
		if (list && list.IsArr())
			list.Add(c);
	}

	protected void RemoveComparison(int i)
	{
		DZS_Json cond = m_Work.Get("if");
		if (!cond)
			return;
		if (cond.Has("field"))
		{
			m_Work.Remove("if");
			return;
		}
		DZS_Json list = cond.Get("all");
		if (!list)
			list = cond.Get("any");
		if (!list || i < 0 || i >= list.Count())
			return;
		list.m_Items.RemoveOrdered(i);
		if (list.Count() == 1)
		{
			DZS_Json last = list.At(0);
			m_Work.Set("if", last);
		}
		else if (list.Count() == 0)
			m_Work.Remove("if");
	}

	protected void FlipJoin()
	{
		DZS_Json cond = m_Work.Get("if");
		if (!cond)
			return;
		DZS_Json list = cond.Get("all");
		string to = "any";
		if (!list)
		{
			list = cond.Get("any");
			to = "all";
		}
		if (!list)
			return;
		DZS_Json group = DZS_Json.Obj();
		group.Set(to, list);
		m_Work.Set("if", group);
	}

	// ---- formula ----

	protected void BuildFormula()
	{
		Heading(DZS_Texts.Pick("Formel für den Wert", "Formula for the value"));
		DZS_RLine l = Line(0, 28);
		m_Formula = Field(l, m_Work.GetStr("formula"), DZS_Texts.Pick("leer = der Wert des Projekts, z. B. round(vanilla * boost)", "empty = the project's value, e.g. round(vanilla * boost)"), 0);
		Note(DZS_Texts.Pick("Namen: base (Wert auf dem Server), vanilla (Bohemias Wert), ", "Names: base (the server's value), vanilla (Bohemia's value), ") + SettingNames() + DZS_Texts.Pick(" · Funktionen: min max round floor ceil abs clamp", " · functions: min max round floor ceil abs clamp"));
	}

	protected string SettingNames()
	{
		string s = "";
		foreach (DZS_DzbotSetting st : m_Rules.Settings())
		{
			if (s != "")
				s += ", ";
			s += st.m_Id;
		}
		if (s == "")
			return DZS_Texts.Pick("die Einstellungen des Mods", "the mod's settings");
		return s;
	}

	// ---- repetition ----

	protected void BuildRepeat()
	{
		Heading(DZS_Texts.Pick("Wiederholen", "Repeat"));
		DZS_RLine l = Line(0, 28);
		DZS_Json r = m_Work.Get("repeat");
		string mode = DZS_Texts.Pick("nein", "no");
		if (r && r.IsObj() && r.Has("over"))
			mode = DZS_Texts.Pick("je Listeneintrag", "per list entry");
		else if (r && r.IsObj())
			mode = DZS_Texts.Pick("Anzahl", "count");
		Knob(l, "side", mode, "repeatMode");
		if (r && r.IsObj() && r.Has("over"))
		{
			m_Over = Field(l, r.GetStr("over"), DZS_Texts.Pick("Listen-Einstellung", "List setting"), 0);
			Knob(l, "ghost", "", "repeatOver", "chevron-down");
		}
		else if (r && r.IsObj())
			m_Count = Field(l, r.GetStr("count"), DZS_Texts.Pick("Formel, z. B. canisters", "Formula, e.g. canisters"), 0);
		else
			Label(l, DZS_Texts.Pick("einmal", "once"), 0, "textQuiet", 13);
		if (r && r.IsObj())
			Note(DZS_Texts.Pick("In den Kopien lesen Formeln index (0, 1, 2 …) und bei einer Liste item, item.<Eigenschaft>.", "In the copies, formulas read index (0, 1, 2 …) and with a list item, item.<property>."));
	}

	// ---- values from settings and formulas ----

	protected void BuildFields()
	{
		Heading(DZS_Texts.Pick("Werte aus Einstellungen und Formeln", "Values from settings and formulas"));
		DZS_Json fields = m_Work.Get("fields");
		if (fields && fields.IsObj())
		{
			for (int i = 0; i < fields.m_Keys.Count(); i++)
			{
				string path = fields.m_Keys[i];
				DZS_Json rule = fields.Get(path);
				DZS_FieldRow row = new DZS_FieldRow();
				row.m_Rule = rule;
				DZS_RLine l = Line(4, 28);
				row.m_Path = Field(l, path, DZS_Texts.Pick("Pfad, z. B. objects[0].pos", "Path, e.g. objects[0].pos"), 200);
				Knob(l, "ghost", "", "fieldPath:" + i, "chevron-down");
				bool formula = rule && rule.IsObj() && rule.Has("formula");
				string modeText = DZS_Texts.Pick("Einstellung", "Setting");
				string value = "";
				if (rule && rule.IsObj())
					value = rule.GetStr("setting");
				if (formula)
				{
					modeText = DZS_Texts.Pick("Formel", "Formula");
					value = rule.GetStr("formula");
				}
				Knob(l, "side", modeText, "fieldMode:" + i);
				row.m_Value = Field(l, value, modeText, 0);
				if (!formula)
					Knob(l, "ghost", "", "fieldSetting:" + i, "chevron-down");
				Knob(l, "ghost", "", "fieldDel:" + i, "x");
				m_FieldRows.Insert(row);
			}
		}
		DZS_RLine al = Line(6, 28);
		Knob(al, "side", DZS_Texts.Pick("+ Wert", "+ Value"), "fieldAdd");
		string names = "base";
		if (Repeated(false))
			names += ", index";
		if (Repeated(true))
			names += ", item";
		Note(DZS_Texts.Pick("Eine Koordinate gibt eine ganze Position; Formeln lesen ", "A coordinate gives a whole position; formulas read ") + names + DZS_Texts.Pick(" und die Einstellungen (source.x …).", " and the settings (source.x …)."));
	}

	// Paths of the data a value can take: plain values and short number lists (positions), one list level deep.
	protected void Paths(DZS_Json v, string prefix, int depth, array<ref DZS_MenuItem> items)
	{
		if (!v || !v.IsObj() || items.Count() >= 80)
			return;
		for (int i = 0; i < v.m_Keys.Count(); i++)
		{
			string k = v.m_Keys[i];
			DZS_Json child = v.Get(k);
			string p = k;
			if (prefix != "")
				p = prefix + "." + k;
			if (child.IsNum() || child.IsStr() || child.IsBool())
			{
				items.Insert(Item("path:" + p, p + " = " + DZS_DzbotRules.Short(child, 24)));
				continue;
			}
			if (!child.IsArr())
				continue;
			if (child.Count() > 0 && child.Count() <= 4 && child.At(0).IsNum())
			{
				items.Insert(Item("path:" + p, p + " = " + DZS_DzbotRules.Short(child, 32)));
				continue;
			}
			if (depth > 0)
				continue;
			for (int n = 0; n < child.Count() && n < 20; n++)
			{
				DZS_Json el = child.At(n);
				string at = p + "[" + n + "]";
				Paths(el, at, depth + 1, items);
			}
		}
	}

	// ---- single list elements (block only) ----

	// The elements of the block's lists by identity, labelled "objects[3]" with a name; selected objects first.
	protected void Elements(array<ref DZS_MenuItem> items)
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		array<string> keys = DZS_EntityExts.ItemKeys(m_Inst, m_Owner);
		int shown = 0;
		foreach (string key : keys)
		{
			if (shown >= 150)
				break;
			shown++;
			int at = key.IndexOf("@");
			string elem = key.Substring(at + 1, key.Length() - at - 1);
			string text = ItemLabel(key);
			if (sel && sel.IsSelected(elem))
				items.InsertAt(Item("item:" + key, text + DZS_Texts.Pick(" · ausgewählt", " · selected")), 0);
			else
				items.Insert(Item("item:" + key, text));
		}
	}

	static string ElementName(DZS_Json el)
	{
		string n = el.GetStr("name");
		if (n == "")
			n = el.GetStr("type");
		if (n == "")
		{
			DZS_Json pos = el.Get("pos");
			if (pos)
				n = DZS_DzbotRules.Short(pos, 32);
		}
		return n;
	}

	// Indexes into "objects" of the block's group that are selected (the order the project file writes them in).
	protected array<int> SelectedObjects()
	{
		array<int> r = new array<int>();
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel || m_GroupId == "" || !m_Inst)
			return r;
		array<string> kids = m_Inst.GetChildren(m_GroupId);
		int n = 0;
		foreach (string oid : kids)
		{
			if (m_Inst.GetKind(oid) != DZS_KIND_OBJECT)
				continue;
			if (sel.IsSelected(oid))
				r.Insert(n);
			n++;
		}
		return r;
	}

	protected void BuildItems()
	{
		Heading(DZS_Texts.Pick("Einzelne Elemente (Objekte, Fahrzeuge, Positionen)", "Single elements (objects, children, positions)"));
		DZS_Json items = m_Work.Get("items");
		if (items && items.IsObj())
		{
			for (int i = 0; i < items.m_Keys.Count(); i++)
			{
				string path = items.m_Keys[i];
				m_ItemPaths.Insert(path);
				DZS_RLine l = Line(2, 30);
				Label(l, ItemLabel(path), 0, "text", 13);
				string sum = DZS_DzbotRules.Summary(items.Get(path));
				if (sum == "")
					sum = DZS_Texts.Pick("immer dabei", "always");
				Label(l, sum, -1, "textQuiet", 13);
				Knob(l, "side", DZS_Texts.Pick("Regeln", "Rules"), "itemEdit:" + i);
				Knob(l, "ghost", "", "itemDel:" + i, "x");
			}
		}
		DZS_RLine al = Line(6, 28);
		Knob(al, "side", DZS_Texts.Pick("+ Element", "+ Element"), "itemAdd");
	}

	// The element a path like objects[3] names in the block's data as it is now; null when there is none.
	protected DZS_Json ElementAt(string path)
	{
		DZS_Json data = Data();
		int open = path.IndexOf("[");
		if (!data || open <= 0 || path.Get(path.Length() - 1) != "]")
			return null;
		string k = path.Substring(0, open);
		int n = path.Substring(open + 1, path.Length() - open - 2).ToInt();
		DZS_Json list = data.Get(k);
		if (!list || !list.IsArr() || n < 0 || n >= list.Count())
			return null;
		return list.At(n);
	}

	protected void EditItem(string path)
	{
		Collect();
		DZS_Json items = m_Work.Get("items");
		DZS_Json rule = null;
		if (items && items.IsObj())
			rule = items.Get(path);
		DZS_Json work;
		if (rule && rule.IsObj())
			work = rule.Clone();
		else
			work = DZS_Json.Obj();
		DZS_Json el = ItemJson(path);
		if (!el)
		{
			GoneNotice();
			Reopen(false);
			return;
		}
		string label = m_LabelText + " · " + ItemLabel(path);
		DZS_RuleDialog d = new DZS_RuleDialog(m_Rules, m_Overview, this, "item", path, label, el, work, m_Inst, m_Owner);
		d.Open();
	}

	// An element's dialog stored its rules (null = none).
	void ItemDone(string path, DZS_Json rule)
	{
		if (m_Closed)
			return;
		DZS_Json items = m_Work.Get("items");
		if (!items || !items.IsObj())
		{
			items = DZS_Json.Obj();
			m_Work.Set("items", items);
		}
		if (DZS_DzbotRules.IsEmpty(rule))
			items.Remove(path);
		else
			items.Set(path, rule);
		if (items.Count() == 0)
			m_Work.Remove("items");
		Reopen(false);
	}

	// ---- reading the fields, reopening ----

	protected void Collect()
	{
		foreach (DZS_CondRow cr : m_Conds)
		{
			string name = cr.m_Setting.GetText();
			name.TrimInPlace();
			DZS_Json nj = DZS_Json.Str(name);
			cr.m_Cond.Set("field", nj);
			string op = cr.m_Cond.GetStr("op");
			if (op == "")
			{
				DZS_Json oj = DZS_Json.Str("eq");
				cr.m_Cond.Set("op", oj);
			}
			if (!cr.m_Value)
			{
				cr.m_Cond.Remove("value");
				continue;
			}
			string text = cr.m_Value.GetText();
			if (op == "in" || op == "not_in")
			{
				array<string> parts = new array<string>();
				text.Split(",", parts);
				DZS_Json arr = DZS_Json.Arr();
				foreach (string part : parts)
				{
					string one = part;
					one.TrimInPlace();
					if (one == "")
						continue;
					DZS_Json pv = ValueOf(one);
					arr.Add(pv);
				}
				cr.m_Cond.Set("value", arr);
			}
			else
			{
				DZS_Json vv = ValueOf(text);
				cr.m_Cond.Set("value", vv);
			}
		}
		if (m_Formula)
		{
			string f = m_Formula.GetText();
			f.TrimInPlace();
			if (f == "")
				m_Work.Remove("formula");
			else
			{
				DZS_Json fj = DZS_Json.Str(f);
				m_Work.Set("formula", fj);
			}
		}
		DZS_Json r = m_Work.Get("repeat");
		if (r && r.IsObj())
		{
			if (m_Count)
			{
				string count = m_Count.GetText();
				count.TrimInPlace();
				DZS_Json cj = DZS_Json.Str(count);
				r.Set("count", cj);
			}
			if (m_Over)
			{
				string over = m_Over.GetText();
				over.TrimInPlace();
				DZS_Json ovj = DZS_Json.Str(over);
				r.Set("over", ovj);
			}
		}
		if (m_Kind != "change")
		{
			DZS_Json fields = DZS_Json.Obj();
			foreach (DZS_FieldRow fr : m_FieldRows)
			{
				string path = fr.m_Path.GetText();
				path.TrimInPlace();
				if (path == "")
					continue;
				DZS_Json rule = fr.m_Rule;
				if (!rule || !rule.IsObj())
					rule = DZS_Json.Obj();
				string value = fr.m_Value.GetText();
				value.TrimInPlace();
				DZS_Json valueJ = DZS_Json.Str(value);
				if (rule.Has("formula"))
					rule.Set("formula", valueJ);
				else
					rule.Set("setting", valueJ);
				fields.Set(path, rule);
			}
			if (fields.Count() > 0)
				m_Work.Set("fields", fields);
			else
				m_Work.Remove("fields");
		}
	}

	// The same dialog again after a change of its shape; collect=false when the work is already up to date.
	protected void Reopen(bool collect = true)
	{
		if (collect)
			Collect();
		DZS_RuleDialog d = new DZS_RuleDialog(m_Rules, m_Overview, m_Outer, m_Kind, m_Ref, m_LabelText, m_Target, m_Work, m_Inst, m_Owner);
		d.SetGroup(m_GroupId);
		d.KeepScroll(m_Scroll);
		Close();
		d.Open();
	}

	void KeepScroll(float s)
	{
		m_Scroll = s;
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		int colon = tag.IndexOf(":");
		string what = tag;
		int i = -1;
		if (colon >= 0)
		{
			what = tag.Substring(0, colon);
			i = tag.Substring(colon + 1, tag.Length() - colon - 1).ToInt();
		}
		m_PickIndex = i;
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		if (what == "condAdd")
		{
			Collect();
			AddComparison();
			Reopen(false);
		}
		else if (what == "condDel")
		{
			Collect();
			RemoveComparison(i);
			Reopen(false);
		}
		else if (what == "condJoin")
		{
			Collect();
			FlipJoin();
			Reopen(false);
		}
		else if (what == "condClear")
		{
			m_Work.Remove("if");
			Reopen(false);
		}
		else if (what == "condSetting")
		{
			SettingItems(items, false);
			Pick(b, items, what);
		}
		else if (what == "condOp")
		{
			array<string> ops = {"eq", "neq", "gt", "gte", "lt", "lte", "in", "not_in", "contains", "empty", "not_empty"};
			foreach (string op : ops)
				items.Insert(Item(op, OpLabel(op)));
			Pick(b, items, what);
		}
		else if (what == "condValue")
		{
			ValueItems(i, items);
			Pick(b, items, what);
		}
		else if (what == "repeatMode")
		{
			items.Insert(Item("none", DZS_Texts.Pick("nicht wiederholen", "do not repeat")));
			items.Insert(Item("count", DZS_Texts.Pick("eine Anzahl mal (Formel)", "a number of times (formula)")));
			items.Insert(Item("over", DZS_Texts.Pick("je Eintrag einer Listen-Einstellung", "once per entry of a list setting")));
			Pick(b, items, what);
		}
		else if (what == "repeatOver")
		{
			SettingItems(items, true);
			Pick(b, items, what);
		}
		else if (what == "fieldAdd" || what == "fieldPath")
		{
			if (what == "fieldAdd")
				items.Insert(Item("path:", DZS_Texts.Pick("Pfad selbst eingeben", "Type the path")));
			SelectedPaths(items);
			Paths(Data(), "", 0, items);
			Pick(b, items, what);
		}
		else if (what == "fieldMode")
		{
			Collect();
			if (i >= 0 && i < m_FieldRows.Count())
			{
				DZS_Json rule = m_FieldRows[i].m_Rule;
				string path = m_FieldRows[i].m_Path.GetText();
				path.TrimInPlace();
				DZS_Json fields = m_Work.Get("fields");
				if (rule && fields && fields.Has(path))
				{
					DZS_Json flipped = DZS_Json.Obj();
					DZS_Json empty = DZS_Json.Str("");
					if (rule.Has("formula"))
						flipped.Set("setting", empty);
					else
						flipped.Set("formula", empty);
					fields.Set(path, flipped);
				}
			}
			Reopen(false);
		}
		else if (what == "fieldSetting")
		{
			SettingItems(items, false);
			Pick(b, items, what);
		}
		else if (what == "fieldDel")
		{
			Collect();
			if (i >= 0 && i < m_FieldRows.Count())
			{
				string gone = m_FieldRows[i].m_Path.GetText();
				gone.TrimInPlace();
				DZS_Json fl = m_Work.Get("fields");
				if (fl)
					fl.Remove(gone);
			}
			Reopen(false);
		}
		else if (what == "itemAdd")
		{
			Elements(items);
			Pick(b, items, what);
		}
		else if (what == "itemEdit" && i >= 0 && i < m_ItemPaths.Count())
			EditItem(m_ItemPaths[i]);
		else if (what == "itemDel" && i >= 0 && i < m_ItemPaths.Count())
		{
			Collect();
			DZS_Json its = m_Work.Get("items");
			if (its)
				its.Remove(m_ItemPaths[i]);
			Reopen(false);
		}
	}

	// Settings to pick; lists only for a repetition. In a repetition also index and item.
	protected void SettingItems(array<ref DZS_MenuItem> items, bool listsOnly)
	{
		foreach (DZS_DzbotSetting st : m_Rules.Settings())
		{
			if (listsOnly && !st.IsList())
				continue;
			items.Insert(Item(st.m_Id, st.Shown() + " · " + st.m_Type));
			if (listsOnly || st.m_Type != "coordinate")
				continue;
			items.Insert(Item(st.m_Id + ".x", st.m_Id + ".x"));
			items.Insert(Item(st.m_Id + ".z", st.m_Id + ".z"));
		}
		if (listsOnly)
			return;
		if (Repeated(false))
			items.Insert(Item("index", "index"));
		if (Repeated(true))
		{
			items.Insert(Item("item", "item"));
			DZS_Json r = m_Work.Get("repeat");
			string over = "";
			if (r && r.IsObj())
				over = r.GetStr("over");
			DZS_DzbotSetting list = m_Rules.Setting(over);
			if (list)
			{
				foreach (string prop : list.m_Props)
					items.Insert(Item("item." + prop, "item." + prop));
			}
		}
	}

	protected void ValueItems(int i, array<ref DZS_MenuItem> items)
	{
		if (i < 0 || i >= m_Conds.Count())
			return;
		DZS_DzbotSetting st = m_Rules.Setting(m_Conds[i].m_Setting.GetText());
		if (!st)
			return;
		if (st.m_Type == "boolean")
		{
			items.Insert(Item("true", "true"));
			items.Insert(Item("false", "false"));
		}
		foreach (string ov : st.m_Options)
			items.Insert(Item(ov, ov));
	}

	// "objects[n].pos" of the selected objects of the block's group, first.
	protected void SelectedPaths(array<ref DZS_MenuItem> items)
	{
		if (m_Kind != "block")
			return;
		array<int> selected = SelectedObjects();
		foreach (int n : selected)
		{
			string p = "objects[" + n + "].pos";
			string name = "";
			DZS_Json el = ElementAt("objects[" + n + "]");
			if (el)
				name = " · " + ElementName(el);
			items.Insert(Item("path:" + p, p + name + DZS_Texts.Pick(" · ausgewählt", " · selected")));
		}
	}

	override void Picked(string what, string tag)
	{
		int i = m_PickIndex;
		if (what == "condSetting" && i >= 0 && i < m_Conds.Count())
		{
			m_Conds[i].m_Setting.SetText(tag);
			Reopen();
		}
		else if (what == "condValue" && i >= 0 && i < m_Conds.Count() && m_Conds[i].m_Value)
			m_Conds[i].m_Value.SetText(tag);
		else if (what == "condOp" && i >= 0 && i < m_Conds.Count())
		{
			DZS_Json oj = DZS_Json.Str(tag);
			m_Conds[i].m_Cond.Set("op", oj);
			Reopen();
		}
		else if (what == "repeatMode")
		{
			Collect();
			if (tag == "none")
				m_Work.Remove("repeat");
			else
			{
				DZS_Json r = DZS_Json.Obj();
				DZS_Json empty = DZS_Json.Str("");
				r.Set(tag, empty);
				m_Work.Set("repeat", r);
			}
			Reopen(false);
		}
		else if (what == "repeatOver" && m_Over)
			m_Over.SetText(tag);
		else if (what == "fieldSetting" && i >= 0 && i < m_FieldRows.Count())
			m_FieldRows[i].m_Value.SetText(tag);
		else if (what == "fieldPath" && i >= 0 && i < m_FieldRows.Count())
			m_FieldRows[i].m_Path.SetText(tag.Substring(5, tag.Length() - 5));
		else if (what == "fieldAdd")
		{
			Collect();
			string path = tag.Substring(5, tag.Length() - 5);
			DZS_Json fields = m_Work.Get("fields");
			if (!fields || !fields.IsObj())
			{
				fields = DZS_Json.Obj();
				m_Work.Set("fields", fields);
			}
			if (!fields.Has(path))
			{
				DZS_Json fresh = DZS_Json.Obj();
				DZS_Json empty2 = DZS_Json.Str(FirstCoordinate(path));
				fresh.Set("setting", empty2);
				fields.Set(path, fresh);
			}
			Reopen(false);
		}
		else if (what == "itemAdd")
			EditItem(tag.Substring(5, tag.Length() - 5));
	}

	// a position path suggests the first coordinate setting
	protected string FirstCoordinate(string path)
	{
		if (path.IndexOf("pos") < 0)
			return "";
		foreach (DZS_DzbotSetting st : m_Rules.Settings())
		{
			if (st.m_Type == "coordinate")
				return st.m_Id;
		}
		return "";
	}

	override void Switched(string tag, bool on)
	{
		if (tag != "required")
			return;
		if (on)
		{
			DZS_Json yes = DZS_Json.Boolean(true);
			m_Work.Set("required", yes);
		}
		else
			m_Work.Remove("required");
	}

	override void OnFoot(string tag)
	{
		if (tag == "cancel")
		{
			Close();
			return;
		}
		DZS_Json result = null;
		if (tag == "apply")
		{
			Collect();
			result = m_Work;
		}
		else if (tag != "drop")
			return;
		if (!TargetAlive())
		{
			Gone();
			return;
		}
		Close();
		if (m_Kind == "item")
		{
			if (m_Outer)
				m_Outer.ItemDone(m_Ref, result);
			return;
		}
		if (!m_Rules.PutAt(m_Owner, result))
			GoneNotice();
		if (m_Overview)
			m_Overview.Refresh();
	}

	override void Cancel()
	{
		Close();
	}
}

// One setting of the mod (a config field dzbot asks for at install): identifier, title, type, options, default, help.
// Edits a copy of the entry; members it does not show stay in it. A new identifier renames the setting in the rules,
// in the same step of the history. Held by its identifier as it was opened (the place only decides between equal ones).
class DZS_SettingDialog : DZS_RulesBase
{
	protected DZS_RulesDialog m_Overview;
	protected DZS_Instance m_Inst;
	// identifier and place in the project's list when opened; "" / -1 for a new setting
	protected string m_OrigId;
	protected int m_Index;
	protected ref DZS_Json m_Work;
	protected ref array<string> m_Errors = new array<string>();
	protected DZS_TextField m_Id;
	protected DZS_TextField m_Title;
	protected DZS_TextField m_Options;
	protected DZS_TextField m_Default;
	protected DZS_TextField m_Help;
	// dzStudio's types; every other one (object lists, item classes …) keeps its configuration as dzbot wrote it
	static const string TYPES = "number boolean string generic_select coordinate";

	void DZS_SettingDialog(DZS_DzbotRules rules, DZS_RulesDialog overview, string origId, int index, DZS_Json work, array<string> errors, DZS_Instance inst)
	{
		m_Rules = rules;
		m_Overview = overview;
		m_OrigId = origId;
		m_Index = index;
		m_Inst = inst;
		m_Work = work;
		if (errors)
			m_Errors.Copy(errors);
		string title = DZS_Texts.Pick("Neue Einstellung", "New setting");
		string id = DZS_DzbotRules.SettingId(work);
		if (index >= 0 && id != "")
			title = id;
		Setup(MEDIUM, DZS_Texts.Pick("Einstellung des Mods", "Setting of the mod"), DZS_W.Literal(title), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button go = AddFootButton("apply", DZS_Texts.Pick("Übernehmen", "Apply"), "main");
		go.m_Hit.m_Primary = true;
		Watch(inst);
	}

	// Where the setting is in the list now, -1 when it is gone (a new one: always "there").
	protected int Place(DZS_Json list)
	{
		if (m_OrigId == "" && m_Index < 0)
			return -1;
		return DZS_RulesDialog.FindSetting(list, m_OrigId, m_Index);
	}

	override bool TargetAlive()
	{
		if (m_OrigId == "" && m_Index < 0)
			return true;
		DZS_Json list = m_Rules.SettingsForEdit();
		return Place(list) >= 0;
	}

	override void Gone()
	{
		Close();
		DZS_RulesDialog.SettingGone();
		if (m_Overview)
			m_Overview.Reopen();
	}

	static bool StudioType(string type)
	{
		return type == "number" || type == "boolean" || type == "string" || type == "coordinate" || DZS_DzbotRules.IsSelect(type);
	}

	// Whether dzStudio edits the default of a type: as typed JSON or as text; not a section's or an object list's.
	static bool EditsDefault(string type)
	{
		return type != DZS_DzbotRules.SECTION && type != "object_list";
	}

	static string TypeLabel(string type)
	{
		if (type == "number")
			return DZS_Texts.Pick("Zahl", "Number");
		if (type == "boolean")
			return DZS_Texts.Pick("Ja/Nein", "Yes/no");
		if (type == "string")
			return DZS_Texts.Pick("Text", "Text");
		if (DZS_DzbotRules.IsSelect(type))
			return DZS_Texts.Pick("Auswahl", "Choice");
		if (type == "coordinate")
			return DZS_Texts.Pick("Position", "Position");
		if (type == DZS_DzbotRules.SECTION)
			return DZS_Texts.Pick("Abschnitt", "Section");
		if (type == "object_list")
			return DZS_Texts.Pick("Objektliste", "Object list");
		return type;
	}

	override void BuildContent(Widget inner)
	{
		m_Parent = inner;
		foreach (string e : m_Errors)
			Note(e, "danger");
		string type = m_Work.GetStr("type");
		bool section = type == DZS_DzbotRules.SECTION;
		DZS_RLine il = Line(0, 30);
		Label(il, DZS_Texts.Pick("Name (in Regeln)", "Identifier (in rules)"), 170, "text", 13);
		m_Id = Field(il, DZS_DzbotRules.SettingId(m_Work), "boost", 0);
		DZS_RLine tl = Line(6, 30);
		Label(tl, DZS_Texts.Pick("Titel", "Title"), 170, "text", 13);
		m_Title = Field(tl, m_Work.GetStr("title"), DZS_Texts.Pick("Wie Server-Admins sie sehen", "As server admins see it"), 0);
		if (section)
		{
			Note(DZS_Texts.Pick("Ein Abschnitt ist eine Überschrift über den Einstellungen darunter; seine Sichtbarkeit bearbeitest du in dzbot.", "A section is a heading over the settings below it; its visibility is edited in dzbot."));
			AddFootFocus();
			return;
		}
		DZS_RLine yl = Line(6, 30);
		Label(yl, DZS_Texts.Pick("Typ", "Type"), 170, "text", 13);
		Knob(yl, "side", TypeLabel(type), "type", "chevron-down");
		if (!StudioType(type))
			Note(DZS_Texts.Pick("Die Konfiguration dieses Typs bearbeitest du in dzbot; dzStudio behält sie, wie sie ist.", "This type's configuration is edited in dzbot; dzStudio keeps it as it is."));
		if (DZS_DzbotRules.IsSelect(type))
		{
			DZS_RLine ol = Line(6, 30);
			Label(ol, DZS_Texts.Pick("Optionen", "Options"), 170, "text", 13);
			m_Options = Field(ol, OptionsText(), DZS_Texts.Pick("wert=Bezeichnung, wert2=Bezeichnung 2", "value=Label, value2=Label 2"), 0);
		}
		DZS_RLine rl = Line(6, 30);
		Label(rl, DZS_Texts.Pick("Pflichtfeld", "Required"), 170, "text", 13);
		Switch(rl, m_Work.GetBool("required"), "required");
		Label(rl, DZS_Texts.Pick("Eine Installation braucht einen Wert", "An installation needs a value"), 0, "textQuiet", 13);
		if (EditsDefault(type))
		{
			DZS_RLine dl = Line(6, 30);
			Label(dl, DZS_Texts.Pick("Standardwert", "Default"), 170, "text", 13);
			m_Default = Field(dl, DefaultText(), DefaultHint(type), 0);
		}
		DZS_RLine hl = Line(6, 30);
		Label(hl, DZS_Texts.Pick("Hilfe", "Help"), 170, "text", 13);
		m_Help = Field(hl, m_Work.GetStr("help"), DZS_Texts.Pick("Erklärung für Server-Admins", "Explanation for server admins"), 0);
		if (m_Work.Has("visibleIf"))
			Note(DZS_Texts.Pick("Sichtbar nur unter einer Bedingung — die bearbeitest du in dzbot.", "Shown only under a condition — edited in dzbot."));
		AddFootFocus();
	}

	override void Switched(string tag, bool on)
	{
		if (tag != "required")
			return;
		if (on)
		{
			DZS_Json yes = DZS_Json.Boolean(true);
			m_Work.Set("required", yes);
		}
		else
			m_Work.Remove("required");
	}

	protected string OptionsText()
	{
		DZS_Json opts = DZS_DzbotRules.OptionsOf(m_Work);
		if (!opts)
			return "";
		string t = "";
		for (int i = 0; i < opts.Count(); i++)
		{
			DZS_Json o = opts.At(i);
			string v = "";
			string label = "";
			if (o.IsObj())
			{
				DZS_Json vj = o.Get("value");
				if (vj)
					v = DZS_RuleDialog.ValueText(vj);
				label = o.GetStr("label");
			}
			else
				v = DZS_RuleDialog.ValueText(o);
			if (t != "")
				t += ", ";
			t += v;
			if (label != "" && label != v)
				t += "=" + label;
		}
		return t;
	}

	protected string DefaultText()
	{
		DZS_Json d = m_Work.Get("default");
		if (!d)
			return "";
		return DZS_RuleDialog.ValueText(d);
	}

	protected static string DefaultHint(string type)
	{
		if (type == "boolean")
			return "true / false";
		if (type == "coordinate")
			return "x, y, z";
		if (type == "number")
			return DZS_Texts.Pick("Zahl", "Number");
		return DZS_Texts.Pick("leer = keiner", "empty = none");
	}

	// The texts into the work copy; problems as lines.
	protected array<string> Collect()
	{
		array<string> errors = new array<string>();
		string type = m_Work.GetStr("type");
		string id = m_Id.GetText();
		id.TrimInPlace();
		DZS_Json idj = DZS_Json.Str(id);
		m_Work.Set("identifier", idj);
		if (m_Work.Has("id"))
		{
			DZS_Json legacy = DZS_Json.Str(id);
			m_Work.Set("id", legacy);
		}
		// dzStudio's older name for a choice becomes dzbot's
		if (type == "select")
		{
			type = DZS_DzbotRules.SELECT;
			DZS_Json sel = DZS_Json.Str(type);
			m_Work.Set("type", sel);
		}
		SetText("title", m_Title.GetText());
		if (m_Help)
			SetText("help", m_Help.GetText());
		if (m_Options)
			CollectOptions(m_Options.GetText());
		if (!m_Default)
			return errors;
		string dt = m_Default.GetText();
		dt.TrimInPlace();
		if (dt == "")
			m_Work.Remove("default");
		else
		{
			string err = "";
			DZS_Json dv = DefaultOf(type, dt, err);
			if (dv)
				m_Work.Set("default", dv);
			else
				errors.Insert(DZS_Texts.Pick("Standardwert: ", "Default: ") + err);
		}
		return errors;
	}

	protected void SetText(string key, string text)
	{
		string t = text;
		t.TrimInPlace();
		if (t == "")
		{
			m_Work.Remove(key);
			return;
		}
		DZS_Json tj = DZS_Json.Str(t);
		m_Work.Set(key, tj);
	}

	protected static DZS_Json DefaultOf(string type, string text, out string err)
	{
		err = "";
		if (type == "number")
		{
			if (DZS_Json.IsNumberText(text))
				return DZS_Json.Num(text);
			err = DZS_Texts.Pick("keine Zahl", "not a number");
			return null;
		}
		if (type == "boolean")
		{
			string low = text;
			low.ToLower();
			if (low == "true" || low == "1" || low == "ja" || low == "yes")
				return DZS_Json.Boolean(true);
			if (low == "false" || low == "0" || low == "nein" || low == "no")
				return DZS_Json.Boolean(false);
			err = "true / false";
			return null;
		}
		if (type == "coordinate")
		{
			array<string> parts = new array<string>();
			text.Split(",", parts);
			DZS_Json arr = DZS_Json.Arr();
			foreach (string p : parts)
			{
				string n = p;
				n.TrimInPlace();
				if (!DZS_Json.IsNumberText(n))
				{
					err = "x, y, z";
					return null;
				}
				DZS_Json nj = DZS_Json.Num(n);
				arr.Add(nj);
			}
			if (arr.Count() != 3)
			{
				err = "x, y, z";
				return null;
			}
			return arr;
		}
		return DZS_Json.Str(text);
	}

	// "a=Label, b" into the options where the entry keeps them; an option that stays keeps its other members.
	protected void CollectOptions(string text)
	{
		DZS_Json was = DZS_DzbotRules.OptionsOf(m_Work);
		DZS_Json fresh = DZS_Json.Arr();
		array<string> parts = new array<string>();
		text.Split(",", parts);
		foreach (string part : parts)
		{
			string p = part;
			p.TrimInPlace();
			if (p == "")
				continue;
			string v = p;
			string label = "";
			int eq = p.IndexOf("=");
			if (eq > 0)
			{
				v = p.Substring(0, eq);
				label = p.Substring(eq + 1, p.Length() - eq - 1);
				v.TrimInPlace();
				label.TrimInPlace();
			}
			DZS_Json o = null;
			if (was)
			{
				for (int i = 0; i < was.Count(); i++)
				{
					DZS_Json old = was.At(i);
					if (old.IsObj() && old.Has("value") && DZS_RuleDialog.ValueText(old.Get("value")) == v)
						o = old.Clone();
				}
			}
			if (!o)
				o = DZS_Json.Obj();
			DZS_Json vj = DZS_Json.Str(v);
			o.Set("value", vj);
			if (label != "")
			{
				DZS_Json lj = DZS_Json.Str(label);
				o.Set("label", lj);
			}
			fresh.Add(o);
		}
		// in dzbot's place; "options" and "multiple" beside the entry (dzStudio's older shape) move into it
		DZS_Json tc = DZS_DzbotRules.TypeConfigForEdit(m_Work);
		tc.Set("options", fresh);
		m_Work.Remove("options");
		DZS_Json mul = m_Work.Get("multiple");
		if (mul && mul.IsBool() && !tc.Has("multiple"))
		{
			DZS_Json mulCopy = mul.Clone();
			tc.Set("multiple", mulCopy);
		}
		m_Work.Remove("multiple");
	}

	protected void Rebuild(array<string> errors)
	{
		DZS_SettingDialog d = new DZS_SettingDialog(m_Rules, m_Overview, m_OrigId, m_Index, m_Work, errors, m_Inst);
		Close();
		d.Open();
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		if (tag != "type")
			return;
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		array<string> types = new array<string>();
		string all = TYPES;
		all.Split(" ", types);
		foreach (string t : types)
		{
			DZS_MenuItem it = Item(t, TypeLabel(t));
			items.Insert(it);
		}
		Pick(b, items, "type");
	}

	override void Picked(string what, string tag)
	{
		if (what != "type")
			return;
		array<string> errors = Collect();
		string was = m_Work.GetStr("type");
		DZS_Json ty = DZS_Json.Str(tag);
		m_Work.Set("type", ty);
		DZS_Json tc = m_Work.Get("typeConfig");
		if (!DZS_DzbotRules.IsSelect(tag))
		{
			m_Work.Remove("options");
			m_Work.Remove("multiple");
			if (tc && tc.IsObj())
			{
				tc.Remove("options");
				tc.Remove("multiple");
			}
		}
		// an object list's configuration means nothing to the type picked instead
		if (was == "object_list" && tag != was)
		{
			m_Work.Remove("props");
			if (tc && tc.IsObj())
			{
				tc.Remove("properties");
				tc.Remove("props");
			}
		}
		if (tc && tc.IsObj() && tc.Count() == 0)
			m_Work.Remove("typeConfig");
		// a default that no longer fits the type is dropped rather than written as the wrong JSON
		DZS_Json d = m_Work.Get("default");
		if (d && was != tag)
		{
			string dErr = "";
			string dText = DZS_RuleDialog.ValueText(d);
			DZS_Json dv = DefaultOf(tag, dText, dErr);
			if (dv)
				m_Work.Set("default", dv);
			else
				m_Work.Remove("default");
		}
		Rebuild(errors);
	}

	protected static bool ValidId(string id)
	{
		if (id == "")
			return false;
		string first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
		string rest = first + "0123456789";
		if (first.IndexOf(id.Get(0)) < 0)
			return false;
		for (int i = 1; i < id.Length(); i++)
		{
			if (rest.IndexOf(id.Get(i)) < 0)
				return false;
		}
		return true;
	}

	override void OnFoot(string tag)
	{
		if (tag != "apply")
		{
			Close();
			return;
		}
		array<string> errors = Collect();
		string id = DZS_DzbotRules.SettingId(m_Work);
		if (!ValidId(id))
			errors.Insert(DZS_Texts.Pick("Name: Buchstaben, Ziffern und _, nicht mit einer Ziffer vorne", "Identifier: letters, digits and _, not starting with a digit"));
		bool section = m_Work.GetStr("type") == DZS_DzbotRules.SECTION;
		DZS_Json list = m_Rules.SettingsForEdit();
		bool isNew = m_OrigId == "" && m_Index < 0;
		int at = Place(list);
		if (!isNew && at < 0)
		{
			Gone();
			return;
		}
		for (int i = 0; i < list.Count() && !section; i++)
		{
			DZS_Json other = list.At(i);
			if (i == at || !other.IsObj() || other.GetStr("type") == DZS_DzbotRules.SECTION)
				continue;
			if (DZS_DzbotRules.SettingId(other) == id)
				errors.Insert(DZS_Texts.Pick("Name schon vergeben", "Identifier already used"));
		}
		if (errors.Count() > 0)
		{
			Rebuild(errors);
			return;
		}
		string old = "";
		if (at >= 0)
		{
			old = DZS_DzbotRules.SettingId(list.At(at));
			list.m_Items.RemoveOrdered(at);
			list.m_Items.InsertAt(m_Work, at);
		}
		else
			list.Add(m_Work);
		if (section)
			old = "";
		string stepDe = "Einstellung ändern";
		string stepEn = "Edit setting";
		if (isNew)
		{
			stepDe = "Einstellung hinzufügen";
			stepEn = "Add setting";
		}
		else if (old != "" && old != id)
		{
			stepDe = "Einstellung umbenennen";
			stepEn = "Rename setting";
		}
		int renamed;
		m_Rules.CommitSettings(list, old, id, stepDe, stepEn, renamed);
		DZS_Log.Info("dzbot: setting " + id + " saved, " + renamed + " reference(s) renamed");
		Close();
		if (m_Overview)
			m_Overview.Reopen();
	}

	override void Cancel()
	{
		Close();
	}
}
