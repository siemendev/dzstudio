// The dzbot rules of a project mod in dzStudio: an overview of the project's own blocks and changes (as
// DZS_ProjectV2.Build makes them, so ids and refs are the ones dzbot sees) and a dialog per block, change or list
// element. Nested conditions (groups inside groups, "not") are shown read-only; they are edited in dzbot.

// One cell of a dialog line; width in reference pixels, 0 = a share of the rest, -1 = its own width.
class DZS_RCell : Managed
{
	TextWidget m_Text;
	string m_Full;
	DZS_TextField m_Field;
	DZS_Button m_Button;
	DZS_Toggle m_Toggle;
	float m_W;
}

class DZS_RLine : Managed
{
	ref array<ref DZS_RCell> m_Cells = new array<ref DZS_RCell>();
	float m_H = 28;
	float m_Before;
}

// Dialog of lines of cells, placed left to right; knobs, switches and pickers report by tag.
class DZS_RulesBase : DZS_Dialog
{
	protected DZS_DzbotRules m_Rules;
	protected Widget m_Parent;
	protected ref array<ref DZS_RLine> m_Lines = new array<ref DZS_RLine>();
	protected ref array<ref DZS_TextField> m_Fields = new array<ref DZS_TextField>();
	protected ref array<ref DZS_Button> m_Knobs = new array<ref DZS_Button>();
	protected ref array<ref DZS_Toggle> m_Toggles = new array<ref DZS_Toggle>();
	protected ref array<string> m_ToggleTags = new array<string>();
	protected string m_PickWhat;

	protected DZS_RLine Line(float before = 0, float height = 28)
	{
		DZS_RLine l = new DZS_RLine();
		l.m_Before = before;
		l.m_H = height;
		m_Lines.Insert(l);
		return l;
	}

	protected TextWidget Label(DZS_RLine l, string text, float w = -1, string tone = "text", float size = 13)
	{
		DZS_RCell c = new DZS_RCell();
		c.m_Text = DZS_W.Text(m_Parent, DZS_Font.READ, size, DZS_Color.Get(tone));
		c.m_Full = text;
		c.m_Text.SetText(DZS_W.Literal(text));
		c.m_W = w;
		l.m_Cells.Insert(c);
		return c.m_Text;
	}

	protected void Heading(string text, float before = 14)
	{
		DZS_RLine l = Line(before, 20);
		Label(l, DZS_Texts.Upper(text), 0, "textMuted", 12);
	}

	protected void Note(string text, string tone = "textQuiet")
	{
		DZS_RLine l = Line(0, 22);
		Label(l, text, 0, tone, 13);
	}

	protected DZS_TextField Field(DZS_RLine l, string text, string placeholder, float w = 0)
	{
		DZS_TextField f = new DZS_TextField("", placeholder, "textMuted", 120);
		f.Build(m_Parent);
		f.SetText(text);
		m_Fields.Insert(f);
		AddFocusField(f);
		DZS_RCell c = new DZS_RCell();
		c.m_Field = f;
		c.m_W = w;
		l.m_Cells.Insert(c);
		return f;
	}

	protected DZS_Button Knob(DZS_RLine l, string kind, string label, string tag, string icon = "")
	{
		DZS_Button b = new DZS_Button(m_Parent, kind, label, icon, 28);
		b.m_Tag = tag;
		b.m_OnClick.Insert(OnKnob);
		m_Knobs.Insert(b);
		AddFocusHit(b.m_Hit);
		DZS_RCell c = new DZS_RCell();
		c.m_Button = b;
		c.m_W = -1;
		l.m_Cells.Insert(c);
		return b;
	}

	protected DZS_Toggle Switch(DZS_RLine l, bool on, string tag)
	{
		DZS_Toggle t = new DZS_Toggle(m_Parent);
		t.SetOn(on);
		t.m_OnChange.Insert(OnSwitch);
		m_Toggles.Insert(t);
		m_ToggleTags.Insert(tag);
		AddFocusHit(t.m_Hit);
		DZS_RCell c = new DZS_RCell();
		c.m_Toggle = t;
		c.m_W = -1;
		l.m_Cells.Insert(c);
		return t;
	}

	protected float CellWidth(DZS_RCell c)
	{
		if (c.m_W > 0)
			return DZS_Scale.Px(c.m_W);
		if (c.m_Text)
			return DZS_W.TextWidth(c.m_Text) + DZS_Scale.Px(2);
		if (c.m_Button)
			return c.m_Button.GetWidth();
		if (c.m_Toggle)
			return DZS_Toggle.Width() + DZS_Scale.Px(4);
		return DZS_Scale.Px(120);
	}

	override float LayoutContent(float width)
	{
		float gap = DZS_Scale.Px(8);
		float y = 0;
		foreach (DZS_RLine l : m_Lines)
		{
			y += DZS_Scale.Px(l.m_Before);
			float h = DZS_Scale.Px(l.m_H);
			float fixedW = 0;
			int flex = 0;
			foreach (DZS_RCell c : l.m_Cells)
			{
				if (c.m_W == 0)
					flex++;
				else
					fixedW += CellWidth(c);
			}
			if (l.m_Cells.Count() > 1)
				fixedW += gap * (l.m_Cells.Count() - 1);
			float share = 0;
			if (flex > 0)
				share = Math.Max(DZS_Scale.Px(40), (width - fixedW) / flex);
			float x = 0;
			foreach (DZS_RCell pc : l.m_Cells)
			{
				float cw = share;
				if (pc.m_W != 0)
					cw = CellWidth(pc);
				if (pc.m_Text)
				{
					if (pc.m_W >= 0)
						DZS_W.FitText(pc.m_Text, pc.m_Full, cw);
					float th = DZS_W.TextHeight(pc.m_Text);
					DZS_W.Place(pc.m_Text, x, y + (h - th) / 2, cw, th);
				}
				else if (pc.m_Field)
				{
					int refW = cw / DZS_Scale.Factor();
					pc.m_Field.SetWidthRef(refW);
					pc.m_Field.Place(x, y + (h - pc.m_Field.Height()) / 2, pc.m_Field.Height());
				}
				else if (pc.m_Button)
					pc.m_Button.Place(x, y + (h - pc.m_Button.GetHeight()) / 2);
				else if (pc.m_Toggle)
					pc.m_Toggle.Place(x + DZS_Scale.Px(2), y + (h - DZS_Toggle.Height()) / 2);
				x += cw + gap;
			}
			y += h;
		}
		return y + DZS_Scale.Px(4);
	}

	void OnKnob(DZS_Button b)
	{
		Knobbed(b, b.m_Tag);
	}

	void OnSwitch(DZS_Toggle t, bool on)
	{
		int i = m_Toggles.Find(t);
		if (i >= 0)
			Switched(m_ToggleTags[i], on);
	}

	void Knobbed(DZS_Button b, string tag) {}
	void Switched(string tag, bool on) {}
	void Picked(string what, string tag) {}

	// A choice under a knob; the tag of the chosen entry comes back in Picked(what, tag).
	protected void Pick(DZS_Button at, array<ref DZS_MenuItem> items, string what)
	{
		if (items.Count() == 0)
			return;
		m_PickWhat = what;
		float px, py;
		at.GetWidget().GetScreenPos(px, py);
		DZS_Popup pop = DZS_Kernel.Get().GetScreen().OpenContextMenu(items, px, py + at.GetHeight() + DZS_Scale.Px(2));
		pop.m_OnChoose.Insert(OnPicked);
	}

	void OnPicked(DZS_Popup p, DZS_MenuItem it)
	{
		DZS_Kernel.Get().GetScreen().CloseMenu();
		if (!it.m_Enabled)
			return;
		Picked(m_PickWhat, it.m_Tag);
	}

	protected static DZS_MenuItem Item(string tag, string text)
	{
		DZS_MenuItem it = new DZS_MenuItem();
		it.m_Tag = tag;
		it.m_Text = text;
		return it;
	}
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
		if (m_Rules.Settings().Count() == 0)
			Note(DZS_Texts.Pick("Keine Einstellungen bekannt — Namen frei eingeben oder in dzbot Einstellungen anlegen.", "No settings known — type the names or add settings in dzbot."), "warning");
		DZS_Json blocks = m_Project.Get("blocks");
		DZS_Json changes = m_Project.Get("changes");
		array<string> seenBlocks = new array<string>();
		array<string> seenChanges = new array<string>();
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
			seenBlocks.Insert(bid);
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
			seenChanges.Insert(cref);
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
		Orphans("blocks", seenBlocks);
		Orphans("changes", seenChanges);
		AddFootFocus();
	}

	protected void Row(string section, string rid, DZS_Json json, string label, bool focus)
	{
		DZS_RLine l = Line(2, 30);
		string tone = "text";
		if (focus)
			tone = "accentText";
		Label(l, label, 0, tone, 14);
		TextWidget sum = Label(l, Summary(section, rid), -1, "textQuiet", 13);
		int i = m_RowRef.Count();
		Knob(l, "side", DZS_Texts.Pick("Regeln", "Rules"), "edit:" + i);
		m_RowSection.Insert(section);
		m_RowRef.Insert(rid);
		m_RowJson.Insert(json);
		m_RowSummary.Insert(sum);
	}

	// Rules whose block or change the project no longer has: dzbot reports them as problems.
	protected void Orphans(string section, array<string> seen)
	{
		array<string> refs = m_Rules.Refs(section);
		bool head = false;
		foreach (string r : refs)
		{
			if (seen.Find(r) >= 0)
				continue;
			if (!head)
			{
				Heading(DZS_Texts.Pick("Regeln ohne Ziel", "Rules without a target"));
				head = true;
			}
			DZS_RLine l = Line(2, 30);
			Label(l, r, 0, "textMuted", 13);
			int i = m_RowRef.Count();
			Knob(l, "danger", DZS_Texts.Pick("Entfernen", "Remove"), "drop:" + i);
			m_RowSection.Insert(section);
			m_RowRef.Insert(r);
			m_RowJson.Insert(null);
			m_RowSummary.Insert(null);
		}
	}

	protected string Summary(string section, string rid)
	{
		string s = DZS_DzbotRules.Summary(m_Rules.Rule(section, rid));
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
				t.SetText(DZS_W.Literal(Summary(m_RowSection[i], m_RowRef[i])));
		}
		Layout();
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		int colon = tag.IndexOf(":");
		if (colon < 0)
			return;
		string what = tag.Substring(0, colon);
		int i = tag.Substring(colon + 1, tag.Length() - colon - 1).ToInt();
		if (i < 0 || i >= m_RowRef.Count())
			return;
		if (what == "drop")
		{
			m_Rules.Put(m_RowSection[i], m_RowRef[i], null);
			DZS_RulesDialog again = new DZS_RulesDialog(m_Rules, m_Inst, GroupOf(m_FocusBlock));
			Close();
			again.Open();
			return;
		}
		DZS_Json json = m_RowJson[i];
		if (!json)
			return;
		string kind = "change";
		string label = DZS_DzbotRules.ChangeLabel(json);
		if (m_RowSection[i] == "blocks")
		{
			kind = "block";
			label = DZS_DzbotRules.BlockLabel(json);
		}
		DZS_Json rule = m_Rules.Rule(m_RowSection[i], m_RowRef[i]);
		DZS_Json work;
		if (rule)
			work = rule.Clone();
		else
			work = DZS_Json.Obj();
		DZS_RuleDialog d = new DZS_RuleDialog(m_Rules, this, null, kind, m_RowRef[i], label, json, work, m_Inst);
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
// "Apply" stores. An element's dialog stores into the dialog of its block.
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

	void DZS_RuleDialog(DZS_DzbotRules rules, DZS_RulesDialog overview, DZS_RuleDialog outer, string kind, string rid, string label, DZS_Json target, DZS_Json work, DZS_Instance inst)
	{
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
	}

	void SetGroup(string groupId)
	{
		m_GroupId = groupId;
	}

	protected string Section()
	{
		if (m_Kind == "block")
			return "blocks";
		return "changes";
	}

	// the data paths count from: the block's data, or the element itself
	protected DZS_Json Data()
	{
		if (m_Kind == "item")
			return m_Target;
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

	// The elements of the block's lists: "objects[3]" with a name; selected objects of its group first.
	protected void Elements(array<ref DZS_MenuItem> items)
	{
		DZS_Json data = Data();
		if (!data || !data.IsObj())
			return;
		array<int> selected = SelectedObjects();
		for (int i = 0; i < data.m_Keys.Count(); i++)
		{
			string k = data.m_Keys[i];
			DZS_Json list = data.Get(k);
			if (!list.IsArr())
				continue;
			for (int n = 0; n < list.Count() && n < 150; n++)
			{
				DZS_Json el = list.At(n);
				if (!el.IsObj())
					continue;
				string path = k + "[" + n + "]";
				string text = path + " · " + ElementName(el);
				if (k == "objects" && selected.Find(n) >= 0)
					items.InsertAt(Item("item:" + path, text + DZS_Texts.Pick(" · ausgewählt", " · selected")), 0);
				else
					items.Insert(Item("item:" + path, text));
			}
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
				string name = path;
				DZS_Json el = ElementAt(path);
				if (el)
					name = path + " · " + ElementName(el);
				Label(l, name, 0, "text", 13);
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

	// The element a path like objects[3] names in the block's data; null when there is none.
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
		DZS_Json el = ElementAt(path);
		string label = m_LabelText + " · " + path;
		if (el)
			label += " · " + ElementName(el);
		DZS_RuleDialog d = new DZS_RuleDialog(m_Rules, m_Overview, this, "item", path, label, el, work, m_Inst);
		d.Open();
	}

	// An element's dialog stored its rules (null = none).
	void ItemDone(string path, DZS_Json rule)
	{
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
		DZS_RuleDialog d = new DZS_RuleDialog(m_Rules, m_Overview, m_Outer, m_Kind, m_Ref, m_LabelText, m_Target, m_Work, m_Inst);
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
		Close();
		if (m_Kind == "item")
		{
			if (m_Outer)
				m_Outer.ItemDone(m_Ref, result);
			return;
		}
		m_Rules.Put(Section(), m_Ref, result);
		if (m_Overview)
			m_Overview.Refresh();
	}

	override void Cancel()
	{
		Close();
	}
}
