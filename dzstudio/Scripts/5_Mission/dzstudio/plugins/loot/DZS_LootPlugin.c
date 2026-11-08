// Loot: switch per project, setting per object derived from
// mapgrouppos.xml, own entries as elements "loot:entry" of the project state (one per type, its content as text),
// rows and entries written back on every save, the files followed after writing instead of read again, preview of the
// points of one selected object. The base state is the vanilla state fetched into the profile folder, else the
// working state as read when loot was switched on or the project opened.
class DZS_LootActions : DZS_ActionHandler
{
	DZS_LootPlugin m_P;

	override bool IsActionChecked(string actionId) { return actionId == "loot.toggle" && m_P.IsOn(); }

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (!m_P.HasProject())
		{
			reasonDe = "Kein Projekt offen";
			reasonEn = "No project open";
			return false;
		}
		if ((actionId == "loot.toggle" || actionId == "loot.edit") && m_P.m_WaitDe != "")
		{
			reasonDe = m_P.m_WaitDe;
			reasonEn = m_P.m_WaitEn;
			return false;
		}
		if (actionId == "loot.vanilla" && m_P.m_Fetch)
		{
			reasonDe = "Wird gerade geladen";
			reasonEn = "Loading";
			return false;
		}
		if (actionId == "loot.edit")
			return m_P.CanEditSelected(reasonDe, reasonEn);
		return true;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "loot.toggle")
			m_P.Toggle();
		else if (actionId == "loot.vanilla")
			m_P.AskVanilla();
		else if (actionId == "loot.edit")
			m_P.EditSelected();
	}
}

// Question or notice with a text over several lines (the kernel's confirmation shows one line).
class DZS_LootTextDialog : DZS_Dialog
{
	protected ref array<string> m_Lines = new array<string>();
	protected ref array<TextWidget> m_Widgets = new array<TextWidget>();

	void DZS_LootTextDialog(string kicker, string title, string text)
	{
		Wrap(text, 74, m_Lines);
		Setup(MEDIUM, kicker, title, true);
	}

	static void Wrap(string text, int width, array<string> into)
	{
		array<string> words = new array<string>();
		text.Split(" ", words);
		string line = "";
		foreach (string w : words)
		{
			if (line == "")
				line = w;
			else if (DZS_Names.Chars(line) + 1 + DZS_Names.Chars(w) > width)
			{
				into.Insert(line);
				line = w;
			}
			else
				line += " " + w;
		}
		if (line != "")
			into.Insert(line);
	}

	override void BuildContent(Widget inner)
	{
		foreach (string l : m_Lines)
		{
			TextWidget t = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
			t.SetText(l);
			m_Widgets.Insert(t);
		}
		AddFootFocus();
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		float lh = DZS_Scale.Px(22);
		foreach (TextWidget t : m_Widgets)
		{
			DZS_W.Place(t, 0, y, width, lh);
			y += lh;
		}
		return y;
	}
}

class DZS_LootSave : DZS_SaveContributor
{
	DZS_LootPlugin m_P;

	override void Contribute(DZS_Instance inst, string folder, array<ref DZS_SaveFileOut> files, DZS_Issues issues, array<string> writtenTypes, array<string> originals)
	{
		m_P.Contribute(inst, folder, files, issues, writtenTypes, originals);
	}

	override void Saved(DZS_Instance inst, string folder)
	{
		m_P.Saved(inst, folder);
	}

	override void Dropped(DZS_Instance inst)
	{
		m_P.Dropped(inst);
	}
}

// One entry of the change list for storage providers.
class DZS_LootChange : Managed
{
	string m_File;
	string m_Kind;
	string m_Key;
	string m_Text;
}

// The loot files' texts by checksum for a shared session.
class DZS_LootTexts : DZS_FileTexts
{
	DZS_LootPlugin m_P;

	override void Named(DZS_Instance inst, array<string> names, array<string> sums) { m_P.TextsNamed(inst, names, sums); }
	override array<string> TextOf(string sum) { return m_P.m_Texts.Get(sum); }
	override bool Take(DZS_Instance inst, string sum, array<string> chunks) { return m_P.TakeText(inst, sum, chunks); }
	override void SetWaiting(string de, string en)
	{
		m_P.m_WaitDe = de;
		m_P.m_WaitEn = en;
	}
}

class DZS_LootMapMenu : DZS_MapObjectMenuSource
{
	DZS_LootPlugin m_P;

	override void AddItems(Object o, array<ref DZS_MenuItem> items)
	{
		m_P.AddMapMenuItems(o, items);
	}

	override bool OnChoose(Object o, DZS_MenuItem it)
	{
		return m_P.OnMapMenuChoose(o, it);
	}
}

[DZS_RegisterPlugin(DZS_LootPlugin)]
class DZS_LootPlugin : DZS_Plugin
{
	static const string SETTING = "loot:setting";
	static const string KIND = "loot:entry";
	static const string NONE = "none";
	// "as before": the points of the base state
	static const string VANILLA = "base";
	static const string OWN = "own";

	protected ref DZS_LootActions m_Actions;
	protected ref DZS_LootSave m_Save;
	protected ref DZS_LootRowUi m_Row;
	ref DZS_LootFiles m_Files;
	// the vanilla state, only for comparing and "Set to vanilla"
	ref DZS_LootFiles m_Base;
	string m_BaseFrom;
	protected DZS_Issues m_BaseIssuesFor;
	ref DZS_LootVanilla m_Fetch;
	ref DZS_LootEditor m_Editor;
	protected ref DZS_LootBarGroup m_Bar;
	protected CanvasWidget m_Canvas;
	protected bool m_World;
	protected string m_PendingValue;
	protected ref array<string> m_PendingIds;
	protected int m_StateVer;
	protected int m_CacheVer = -1;
	protected DZS_Instance m_CacheInst;
	protected ref array<string> m_Objects = new array<string>();
	protected ref map<string, int> m_WithLoot = new map<string, int>();
	protected ref map<string, string> m_ElemOf = new map<string, string>();
	protected ref map<string, string> m_SigCache = new map<string, string>();
	protected ref map<string, string> m_VanSig = new map<string, string>();
	// written types of the state (lower-case key): name and original text ("" = none before the project)
	protected ref map<string, string> m_WName = new map<string, string>();
	protected ref map<string, string> m_WOrig = new map<string, string>();
	protected int m_WVer = -1;
	protected DZS_Instance m_WInst;
	// written types after the save in progress
	protected ref array<string> m_SaveNames;
	protected ref array<string> m_SaveOrigs;
	protected int m_SigGen = -1;
	// patches of the save in progress, followed once it is written
	protected ref array<ref DZS_XmlPatch> m_PosPatches;
	protected ref array<ref DZS_XmlPatch> m_ProtoPatches;
	protected ref array<string> m_PosOut;
	protected ref array<string> m_ProtoOut;
	ref array<ref DZS_LootChange> m_Changes = new array<ref DZS_LootChange>();

	override string GetName() { return "loot"; }

	ref map<string, ref array<string>> m_Texts = new map<string, ref array<string>>();
	string m_WaitDe;
	string m_WaitEn;
	protected ref DZS_LootTexts m_TextHolder;
	protected string m_FilesField;
	protected ref DZS_LootMapMenu m_MapMenu;

	override void OnRegister()
	{
		m_TextHolder = new DZS_LootTexts();
		m_TextHolder.m_P = this;
		DZS_FileTexts.Set(m_TextHolder);
		DZS_TypeRegistry types = m_Kernel.GetTypes();
		types.RegisterOperationType("loot", "loot:edit", 1);
		types.RegisterCompanionType("loot", SETTING, 1);
		array<string> parents = {DZS_KIND_PROJECT};
		types.RegisterElementKind("loot", KIND, 1, parents);
		m_Actions = new DZS_LootActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("loot", "Loot", "Loot");
		ui.RegisterAction("loot", "loot.toggle", "Loot", "Loot", null, m_Actions);
		ui.SetGroup("loot.toggle", "loot");
		ui.AddMenuEntry("project", "loot.toggle", 450, true);
		ui.RegisterAction("loot", "loot.vanilla", "Vanilla-Stand holen …", "Fetch vanilla state …", null, m_Actions);
		ui.SetGroup("loot.vanilla", "loot");
		ui.AddMenuEntry("project", "loot.vanilla", 451);
		ui.RegisterAction("loot", "loot.edit", "Loot bearbeiten", "Edit loot", null, m_Actions);
		ui.SetGroup("loot.edit", "loot");
		m_Save = new DZS_LootSave();
		m_Save.m_P = this;
		m_MapMenu = new DZS_LootMapMenu();
		m_MapMenu.m_P = this;
		DZS_MapObjectMenu.AddSource(m_MapMenu);
		m_Kernel.SetService("save.loot", m_Save);
		RegisterTexts();
		m_Editor = new DZS_LootEditor(this, m_Kernel);
		m_Editor.Register();
		m_Bar = new DZS_LootBarGroup();
		m_Bar.m_P = this;
		DZS_BarGroups.Add(m_Bar);
	}

	protected void RegisterTexts()
	{
		DZS_Texts.Add("loot.row", "Loot", "Loot");
		DZS_Texts.Add("loot.none", "keiner", "none");
		DZS_Texts.Add("loot.vanilla", "wie vorher", "As before");
		DZS_Texts.Add("loot.o.project", "von diesem Projekt", "From this project");
		DZS_Texts.Add("loot.o.new", "neu", "new");
		DZS_Texts.Add("loot.o.before", "vorher angepasst", "Customised before");
		DZS_Texts.Add("loot.o.vanilla", "wie Vanilla", "As vanilla");
		DZS_Texts.Add("loot.o.there", "vorher da", "Already there");
		DZS_Texts.Add("loot.o.missing", "fehlt", "missing");
		DZS_Texts.Add("loot.o.foreign", "Markierung eines anderen Projekts", "Mark of another project");
		DZS_Texts.Add("loot.own", "eigene Punkte", "own points");
		DZS_Texts.Add("loot.edit", "Bearbeiten", "Edit");
		DZS_Texts.Add("loot.noVanilla", "Vor dem Projekt hatte dieser Typ keinen Loot-Eintrag", "This type had no loot entry before the project");
		DZS_Texts.Add("loot.noEntry", "Typ hat keinen Loot-Eintrag — bekommt keinen Loot", "Type has no loot entry — gets no loot");
		DZS_Texts.Add("loot.cannotTitle", "Loot lässt sich nicht einschalten", "Loot cannot be switched on");
		DZS_Texts.Add("loot.cannotText", "Die Basis des Projekts hat keine mapgroupproto.xml und mapgrouppos.xml. Wähle unter Projekt › Auf Missionsordner anwenden … Vanilla oder einen Missionsordner mit beiden Dateien und öffne das Projekt neu.", "The project's base has no mapgroupproto.xml and mapgrouppos.xml. Choose vanilla or a mission folder with both files under Project › Apply to mission folder … and open the project again.");
		DZS_Texts.Add("loot.unreadable", "%1 ist nicht lesbar: %2", "%1 is not readable: %2");
		DZS_Texts.Add("loot.noBase", "Kein Vanilla-Stand — „wie Vanilla“ und „vorher angepasst“ lassen sich nicht unterscheiden. Leg mapgroupproto.xml selbst in den Unterordner vanilla\\ des Projektordners oder hol ihn mit Projekt › Vanilla-Stand holen …", "No vanilla state — \"As vanilla\" and \"Customised before\" cannot be told apart. Put mapgroupproto.xml into the subfolder vanilla\\ of the project folder yourself or fetch it with Project › Fetch vanilla state …");
		DZS_Texts.Add("loot.on", "Loot einschalten", "Turn on loot");
		DZS_Texts.Add("loot.off", "Loot ausschalten", "Turn loot off");
		DZS_Texts.Add("loot.offQ", "Beim nächsten Speichern entfernt der Editor %1 und setzt %2 zurück bzw. entfernt sie; eigene Punkte von %3 gehen verloren.", "At the next save the editor removes %1 and resets or removes %2; own points of %3 are lost.");
		DZS_Texts.Add("loot.change", "%1: Loot ändern", "%1: Change loot");
		DZS_Texts.Add("loot.changeN", "%1 Objekte: Loot ändern", "%1 objects: Change loot");
		DZS_Texts.Add("loot.typeQTitle", "Loot für den ganzen Typ", "Loot for the whole type");
		DZS_Texts.Add("loot.typeQ", "Das gilt für %1: %2 im Projekt und %3 auf der Karte. Alle Objekte dieses Typs mit Loot stellen um.", "This applies to %1: %2 in the project and %3 on the map. All objects of this type with loot switch.");
		DZS_Texts.Add("loot.switch", "Umstellen", "Switch");
		DZS_Texts.Add("loot.facts", "%1 Punkte · betrifft %2 auf der Karte und %3 im Projekt", "%1 points · affects %2 on the map and %3 in the project");
		DZS_Texts.Add("loot.skipped", "%1 ohne Vanilla-Loot übersprungen", "%1 without vanilla loot skipped");
		DZS_Texts.Add("loot.vanillaTitle", "Vanilla-Stand holen", "Fetch vanilla state");
		DZS_Texts.Add("loot.vanillaQ", "Der Editor lädt mapgroupproto.xml und cfglimitsdefinition.xml der Karte %1 aus Bohemias offiziellem Repository DayZ-Central-Economy und legt sie unter dzstudio\\vanilla\\%2 im Profilordner ab (nicht im Projekt).", "The editor downloads mapgroupproto.xml and cfglimitsdefinition.xml of the map %1 from Bohemia’s official repository DayZ-Central-Economy into dzstudio\\vanilla\\%2 in the profile folder (not into the project).");
		DZS_Texts.Add("loot.vanillaGo", "Laden", "Download");
		DZS_Texts.Add("loot.vanillaLoading", "Vanilla-Stand für %1 wird geladen …", "Vanilla state for %1 is loading …");
		DZS_Texts.Add("loot.vanillaOk", "Vanilla-Stand für %1 geholt — gilt nach dem nächsten Öffnen des Projekts", "Vanilla state for %1 fetched — applies after the project is opened again");
		DZS_Texts.Add("loot.vanillaFail", "Kein Vanilla-Stand (%1)", "No vanilla state (%1)");
		DZS_Texts.Add("loot.vanillaUsed", "Vanilla-Stand: %1", "Vanilla state: %1");
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		DZS_PropertiesApi props = DZS_PropertiesApi.Get();
		if (props && !m_Row)
		{
			m_Row = new DZS_LootRowUi();
			m_Row.m_P = this;
			props.AddRow(m_Row);
		}
		if (m_World)
		{
			Widget layer = m_Kernel.GetScreen().GetSceneLayer();
			m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", layer));
		}
		m_Editor.OnSessionStart(m_World);
	}

	override void OnSessionEnd()
	{
		m_Editor.Stop("");
		DropFiles();
	}

	protected void DropFiles()
	{
		m_Files = null;
		m_Base = null;
		m_SigCache.Clear();
		m_VanSig.Clear();
		m_WVer = -1;
		m_PosPatches = null;
		m_ProtoPatches = null;
	}

	// ---- project ----

	bool HasProject()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		return pp && pp.m_Open && m_Kernel.GetShown();
	}

	// ---- texts by checksum (shared sessions) ----

	void TextsNamed(DZS_Instance inst, array<string> names, array<string> sums)
	{
		if (!inst)
			return;
		string le;
		inst.GetField(DZS_PROJECT_ID, "lootEnabled", le);
		if (le != "true")
			return;
		string lf;
		inst.GetField(DZS_PROJECT_ID, "lootFiles", lf);
		array<string> items = new array<string>();
		lf.Split(",", items);
		foreach (string it : items)
		{
			string one = it;
			one.TrimInPlace();
			int sp = one.IndexOf(" ");
			if (sp <= 0)
				continue;
			names.Insert(one.Substring(0, sp));
			sums.Insert(one.Substring(sp + 1, one.Length() - sp - 1));
		}
	}

	bool TakeText(DZS_Instance inst, string sum, array<string> chunks)
	{
		string have = DZS_Sha256.Of(chunks);
		if (have != sum)
		{
			DZS_Log.Warn("loot: fetched text has checksum " + have + ", expected " + sum);
			return false;
		}
		m_Texts.Set(sum, chunks);
		DZS_Log.Info("loot: text " + sum + " taken, " + DZS_Files.Length(chunks) + " bytes");
		TryTexts(inst);
		return true;
	}

	protected void Remember(DZS_LootFiles f)
	{
		if (f.m_ProtoChunks && f.m_ProtoSha != "")
			m_Texts.Set(f.m_ProtoSha, f.m_ProtoChunks);
		if (f.m_PosChunks && f.m_PosSha != "")
			m_Texts.Set(f.m_PosSha, f.m_PosChunks);
	}

	// The state names every read loot file with its checksum; part of the load.
	protected void NameFiles(DZS_Instance inst)
	{
		string now = DZS_LootFiles.PROTO + " " + m_Files.m_ProtoSha + ", " + DZS_LootFiles.POS + " " + m_Files.m_PosSha;
		m_FilesField = now;
		string have;
		inst.GetField(DZS_PROJECT_ID, "lootFiles", have);
		if (have == now)
			return;
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "lootFiles", now);
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		DZS_Log.Info("loot: files named in the state: " + now);
	}

	// Loot is on, but the files are not (or no longer) the ones read from disk: build them from the held texts.
	void TryTexts(DZS_Instance inst)
	{
		if (!inst || !IsOn())
			return;
		array<string> names = new array<string>();
		array<string> sums = new array<string>();
		TextsNamed(inst, names, sums);
		string psum = "";
		string qsum = "";
		for (int i = 0; i < names.Count(); i++)
		{
			if (names[i] == DZS_LootFiles.PROTO)
				psum = sums[i];
			else if (names[i] == DZS_LootFiles.POS)
				qsum = sums[i];
		}
		if (psum == "" || qsum == "")
			return;
		if (m_Files && m_Files.m_ProtoSha == psum && m_Files.m_PosSha == qsum)
			return;
		array<string> pc = m_Texts.Get(psum);
		array<string> qc = m_Texts.Get(qsum);
		if (!pc || !qc)
			return;
		DZS_LootFiles f = DZS_LootFiles.FromChunks(pc, qc, DZS_LootVanilla.PathOf(MapKey(), DZS_LootFiles.LIMITS));
		if (f.m_Error != "")
		{
			DZS_Log.Warn("loot: texts of the session unreadable: " + f.m_Error);
			return;
		}
		m_Files = f;
		m_SigCache.Clear();
		m_WVer = -1;
		LoadBase(Folder());
		m_Files.Match(inst, ClassObjects(inst), Mark(inst));
		m_StateVer++;
		DZS_Log.Info("loot: files from the session's texts, " + m_Files.m_Index.Count() + " entries, " + m_Files.m_Rows.Count() + " rows, no file read from disk");
	}

	// Where the loot files are read: the project's working folder, or the folder of an older project.
	string Folder()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open || !m_Kernel.GetShown())
			return "";
		return pp.m_Open.ReadFolder();
	}

	string MapKey()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open)
			return "";
		return pp.m_Open.m_MapKey;
	}

	bool IsOn()
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return false;
		string le;
		inst.GetField(DZS_PROJECT_ID, "lootEnabled", le);
		return le == "true";
	}

	// -dzsLootSkip=MASK (search for crashes): 1 no reading when a project opens, 2 no preview, 4 no derived settings
	static int Skip()
	{
		string v;
		if (!GetCLIParam("dzsLootSkip", v))
			return 0;
		return v.ToInt();
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		m_Editor.Stop("");
		DropFiles();
		if (!inst || (Skip() & 1) != 0)
			return;
		string folder = Folder();
		m_FilesField = "";
		if (folder == "" || !IsOn() || !DZS_LootFiles.Present(folder))
		{
			TryTexts(inst);
			return;
		}
		if (!ReadFiles(inst, folder, OpenIssues()))
			return;
		NameFiles(inst);
		CheckWritten(inst);
		WrittenElements(inst);
		if ((Skip() & 4) == 0)
			DeriveSettings(inst);
	}

	// The entries of the written types become elements at opening (part of the
	// load, origin system, no step), with their content as read.
	protected void WrittenElements(DZS_Instance inst)
	{
		array<string> types = WrittenTypes(inst);
		DZS_OpBuilder b = null;
		int n = 0;
		foreach (string t : types)
		{
			if (ElementOf(inst, t) != "")
				continue;
			DZS_LootEEntry e = OpenEntry(t);
			if (!e)
				continue;
			if (!b)
				b = inst.BeginOperation("loot:edit", 1);
			SetEntry(b, inst, t, e);
			n++;
		}
		if (!b)
			return;
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		DZS_Log.Info("loot: " + n + " written type(s) as elements at opening");
	}

	// Written types whose entry no longer carries the project's mark fall out of the list (part of the load);
	// marked entries the list does not name are reported.
	protected void CheckWritten(DZS_Instance inst)
	{
		DZS_LootOrigin lo = Origin();
		if (lo || Provider())
		{
			ProviderWritten(inst, lo);
			return;
		}
		string mark = Mark(inst);
		if (mark == "")
			return;
		Written(inst);
		array<string> names = new array<string>();
		array<string> origs = new array<string>();
		int dropped = 0;
		for (int i = 0; i < m_WName.Count(); i++)
		{
			string key = m_WName.GetKey(i);
			string nm = m_WName.GetElement(i);
			string have;
			bool marked = m_Files.MarkOf(nm, have);
			if (!marked || have != mark)
			{
				dropped++;
				continue;
			}
			string og;
			m_WOrig.Find(key, og);
			names.Insert(nm);
			origs.Insert(og);
		}
		array<string> unknown = new array<string>();
		for (int k = 0; k < m_Files.m_Index.Count(); k++)
		{
			string tk = m_Files.m_Index.GetKey(k);
			string m2;
			if (m_Files.MarkOf(tk, m2) && m2 == mark && !m_WName.Contains(tk))
			{
				DZS_XmlNode ug = m_Files.Group(tk);
				string shown = tk;
				if (ug)
					shown = ug.Get("name");
				unknown.Insert(shown);
			}
		}
		if (unknown.Count() > 0)
		{
			string ude = "";
			string uen = "";
			if (unknown.Count() == 1)
			{
				ude = "Ursprünglicher Text von " + unknown[0] + " fehlt — Zurücksetzen nimmt nur die Markierung weg";
				uen = "Original text of " + unknown[0] + " is missing — resetting only removes the mark";
			}
			else
			{
				string more = (unknown.Count() - 1).ToString();
				ude = "Ursprünglicher Text von " + unknown[0] + " und " + more + " weiteren fehlt — Zurücksetzen nimmt nur die Markierung weg";
				uen = "Original text of " + unknown[0] + " and " + more + " more is missing — resetting only removes the mark";
			}
			m_Kernel.GetScreen().ShowMessage("loot.marks", "warning", DZS_Texts.Pick(ude, uen), 10.0);
		}
		if (dropped == 0)
			return;
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "writtenTypes", DZS_ProjectsPlugin.WrittenJson(names, origs));
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		m_WVer = -1;
		string en = Plural(dropped, "Eintrag", "Einträge", "entry", "entries");
		m_Kernel.GetScreen().ShowMessage("loot.marks", "info", en + DZS_Texts.Pick(" dieses Projekts stehen nicht mehr in mapgroupproto.xml — aus der Liste genommen", " of this project are no longer in mapgroupproto.xml — taken out of the list"), 10.0);
		DZS_Log.Info("loot: " + dropped + " written type(s) without the mark dropped, " + unknown.Count() + " marked without original text");
	}

	// With a storage provider the project's entries are the ones its delta adds, their base state is the
	// provider's base; part of the load, not a step.
	protected void ProviderWritten(DZS_Instance inst, DZS_LootOrigin lo)
	{
		if (!lo)
			return;
		Written(inst);
		bool same = m_WName.Count() == lo.m_OwnTypes.Count();
		for (int i = 0; same && i < lo.m_OwnTypes.Count(); i++)
		{
			string k = lo.m_OwnTypes[i];
			k.ToLower();
			string og;
			same = m_WName.Contains(k) && m_WOrig.Find(k, og) && og == lo.m_Origs[i];
		}
		if (same)
			return;
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "writtenTypes", DZS_ProjectsPlugin.WrittenJson(lo.m_OwnTypes, lo.m_Origs));
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		m_WVer = -1;
		DZS_Log.Info("loot: written types from the provider: " + lo.m_OwnTypes.Count());
	}

	// Reads the working state and the vanilla state (project folder vanilla\, else the fetched one).
	protected bool ReadFiles(DZS_Instance inst, string folder, DZS_Issues issues = null)
	{
		DZS_LootFiles f = DZS_LootFiles.Read(folder);
		if (f.m_Error != "")
		{
			DZS_Log.Warn("loot: " + f.m_ErrorFile + " unreadable: " + f.m_Error);
			return false;
		}
		DZS_LootOrigin lo = Origin();
		if (lo || Provider())
		{
			if (!lo || lo.m_NoDe != "")
			{
				DZS_Log.Warn("loot: no loot for " + folder);
				return false;
			}
			f.m_OwnRows = lo.m_OwnRows;
		}
		m_Files = f;
		Remember(f);
		m_SigCache.Clear();
		m_WVer = -1;
		LoadBase(folder, issues);
		array<string> objs = ClassObjects(inst);
		string mark = Mark(inst);
		m_Files.Match(inst, objs, mark);
		int matched = 0;
		foreach (DZS_LootRow r : m_Files.m_Rows)
		{
			if (r.m_Obj != "")
				matched++;
		}
		DZS_Log.Info("loot: read " + m_Files.m_Index.Count() + " entries, " + m_Files.m_Rows.Count() + " rows (" + matched + " of the project, mark '" + mark + "'), vanilla " + BaseName());
		return true;
	}

	// The open project's read list (check list for reading), while it is being opened.
	protected DZS_Issues OpenIssues()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open || pp.m_Open.m_OpenIssues == m_BaseIssuesFor)
			return null;
		m_BaseIssuesFor = pp.m_Open.m_OpenIssues;
		return m_BaseIssuesFor;
	}

	protected void LoadBase(string folder, DZS_Issues issues = null)
	{
		m_Base = null;
		m_BaseFrom = "";
		m_VanSig.Clear();
		string own = DZS_LocalStore.Path(folder, "vanilla/" + DZS_LootFiles.PROTO);
		if (folder != "" && FileExist(own))
		{
			string ownLim = DZS_LocalStore.Path(folder, "vanilla/" + DZS_LootFiles.LIMITS);
			if (!FileExist(ownLim))
				ownLim = DZS_LootVanilla.PathOf(MapKey(), DZS_LootFiles.LIMITS);
			DZS_LootFiles hand = DZS_LootFiles.ReadBase(own, ownLim);
			if (hand.m_Error == "")
			{
				m_Base = hand;
				m_BaseFrom = "vanilla\\";
				string srcName = DZS_LootVanilla.SOURCE_FILE;
				if (!FileExist(DZS_LocalStore.Path(folder, "vanilla/" + srcName)) && FileExist(DZS_LocalStore.Path(folder, "vanilla/" + DZS_LegacyNames.VANILLA_SOURCE)))
					srcName = DZS_LegacyNames.VANILLA_SOURCE;
				string src = DZS_LocalStore.Path(folder, "vanilla/" + srcName);
				if (issues && FileExist(src) && !DZS_LootVanilla.ValidSource(src))
				{
					DZS_Issue bad = issues.Add(DZS_Issue.HINT, "vanilla\\" + srcName + " ist nicht gültig und wird übergangen — die Vanilla-Dateien daneben gelten", "vanilla\\" + srcName + " is not valid and is skipped — the vanilla files next to it apply");
					bad.m_File = "vanilla\\" + srcName;
				}
				DZS_Log.Info("loot: vanilla state from the project's vanilla\\, companion file " + DZS_Str.Bool(FileExist(src)));
				return;
			}
			DZS_Log.Warn("loot: vanilla\\" + DZS_LootFiles.PROTO + " of the project unreadable: " + hand.m_Error);
			if (issues)
			{
				DZS_Issue unr = issues.Add(DZS_Issue.WARNING, "vanilla\\" + DZS_LootFiles.PROTO + " ist nicht lesbar (" + hand.m_Error + ") — sie gilt nicht als Vanilla-Stand", "vanilla\\" + DZS_LootFiles.PROTO + " is unreadable (" + hand.m_Error + ") — it does not count as the vanilla state");
				unr.m_File = "vanilla\\" + DZS_LootFiles.PROTO;
			}
		}
		else if (folder != "" && issues && (FileExist(DZS_LocalStore.Path(folder, "vanilla/" + DZS_LootFiles.POS)) || FileExist(DZS_LocalStore.Path(folder, "vanilla/" + DZS_LootFiles.LIMITS)) || FileExist(DZS_LocalStore.Path(folder, "vanilla/" + DZS_LootVanilla.SOURCE_FILE)) || FileExist(DZS_LocalStore.Path(folder, "vanilla/" + DZS_LegacyNames.VANILLA_SOURCE))))
		{
			DZS_Issue miss = issues.Add(DZS_Issue.WARNING, "vanilla\\" + DZS_LootFiles.PROTO + " fehlt — Dateien in vanilla\\ gelten nicht als Vanilla-Stand", "vanilla\\" + DZS_LootFiles.PROTO + " is missing — the files in vanilla\\ do not count as the vanilla state");
			miss.m_File = "vanilla\\" + DZS_LootFiles.PROTO;
		}
		string mk = MapKey();
		if (mk == "" || !DZS_LootVanilla.Have(mk))
			return;
		string bp = DZS_LootVanilla.PathOf(mk, DZS_LootFiles.PROTO);
		string bl = DZS_LootVanilla.PathOf(mk, DZS_LootFiles.LIMITS);
		DZS_LootFiles b = DZS_LootFiles.ReadBase(bp, bl);
		if (b.m_Error != "")
		{
			DZS_Log.Warn("loot: vanilla state unreadable: " + b.m_Error);
			return;
		}
		m_Base = b;
		m_BaseFrom = DZS_LootVanilla.FetchedDate(mk);
	}

	// The project's mark in the loot files (an older local project only; a project of version 2 and a provider's
	// project know their entries and rows from DZS_LootOrigin).
	string Mark(DZS_Instance inst)
	{
		if (!inst || IsProvider() || Origin())
			return "";
		string id;
		inst.GetField(DZS_PROJECT_ID, "lootProjectId", id);
		return id;
	}

	DZS_StoreProvider Provider()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open)
			return null;
		return pp.ProviderOf(pp.m_Open.m_Folder);
	}

	// The project's loot entries and rows: the provider's word when it keeps no loot for the project, else the working
	// folder's (version 2), else the provider's; null for an older local project (its mark decides).
	DZS_LootOrigin Origin()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open)
			return null;
		DZS_StoreProvider sp = pp.ProviderOf(pp.m_Open.m_Folder);
		DZS_LootOrigin lo = null;
		if (sp)
			lo = sp.LootOrigin(pp.m_Open.m_Folder);
		if (lo && lo.m_NoDe != "")
			return lo;
		if (pp.m_Open.m_Origin)
			return pp.m_Open.m_Origin;
		return lo;
	}

	bool IsProvider()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open)
			return false;
		return pp.ProviderOf(pp.m_Open.m_Folder) != null;
	}

	// Written types of the state, cached per state version.
	protected void Written(DZS_Instance inst)
	{
		if (m_WVer == m_StateVer && m_WInst == inst)
			return;
		m_WVer = m_StateVer;
		m_WInst = inst;
		m_WName.Clear();
		m_WOrig.Clear();
		if (!inst)
			return;
		string wts;
		inst.GetField(DZS_PROJECT_ID, "writtenTypes", wts);
		array<string> names = new array<string>();
		array<string> origs = new array<string>();
		DZS_ProjectsPlugin.ReadWritten(wts, names, origs);
		for (int i = 0; i < names.Count(); i++)
		{
			string nm = names[i];
			string key = nm;
			key.ToLower();
			string og = origs[i];
			m_WName.Set(key, nm);
			m_WOrig.Set(key, og);
		}
	}

	bool IsWritten(DZS_Instance inst, string type)
	{
		Written(inst);
		string key = type;
		key.ToLower();
		return m_WName.Contains(key);
	}

	// The base state of a type: the original text of a written type, else the
	// working state as read. Returns false when the type had no entry before the project.
	bool StartText(string type, out string text)
	{
		text = "";
		DZS_Instance inst = m_Kernel.GetShown();
		Written(inst);
		string key = type;
		key.ToLower();
		if (m_WName.Contains(key))
		{
			m_WOrig.Find(key, text);
			return text != "";
		}
		if (!m_Files || !m_Files.HasEntry(type))
			return false;
		text = m_Files.SourceOf(type);
		return true;
	}

	// Value of dzstudio at the base state's group element.
	bool StartMark(string type, out string mark)
	{
		mark = "";
		DZS_Instance inst = m_Kernel.GetShown();
		if (IsWritten(inst, type))
		{
			string text;
			if (!StartText(type, text))
				return false;
			return MarkOfText(text, mark);
		}
		if (!m_Files)
			return false;
		return m_Files.MarkOf(type, mark);
	}

	// dzstudio="…" in the start tag of a group text.
	static bool MarkOfText(string text, out string mark)
	{
		mark = "";
		int close = text.IndexOf(">");
		if (close < 0)
			return false;
		string head = text.Substring(0, close);
		int at = head.IndexOf(" dzstudio=\"");
		if (at < 0)
			return false;
		int from = at + 11;
		string rest = head.Substring(from, head.Length() - from);
		int q = rest.IndexOf("\"");
		if (q < 0)
			return false;
		mark = rest.Substring(0, q);
		return true;
	}

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		m_StateVer++;
		string ff;
		if (inst && inst == m_Kernel.GetShown() && inst.GetField(DZS_PROJECT_ID, "lootFiles", ff) && ff != m_FilesField)
		{
			m_FilesField = ff;
			TryTexts(inst);
		}
		DZS_WorldLines.Moved();
		m_Editor.OnStateChanged(inst);
	}

	// Objects with a class name, loot elements and objects with loot per type; rebuilt after each change.
	protected void Cache(DZS_Instance inst)
	{
		if (m_CacheVer == m_StateVer && m_CacheInst == inst)
			return;
		m_CacheVer = m_StateVer;
		m_CacheInst = inst;
		m_Objects = ScanObjects(inst);
		m_WithLoot.Clear();
		m_ElemOf.Clear();
		array<string> top = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string t : top)
		{
			if (inst.GetKind(t) != KIND)
				continue;
			string type;
			inst.GetField(t, "type", type);
			type.ToLower();
			m_ElemOf.Set(type, t);
		}
		foreach (string oid : m_Objects)
		{
			string s = SettingOf(inst, oid);
			if (s != NONE)
			{
				string on;
				inst.GetField(oid, DZS_Obj.NAME, on);
				on.ToLower();
				int n = 0;
				m_WithLoot.Find(on, n);
				m_WithLoot.Set(on, n + 1);
			}
		}
	}

	// Every type the project edits or wrote before: its loot editor elements and the written types.
	array<string> EditedTypes(DZS_Instance inst)
	{
		Cache(inst);
		Written(inst);
		array<string> r = new array<string>();
		array<string> seen = new array<string>();
		for (int i = 0; i < m_ElemOf.Count(); i++)
		{
			string type;
			inst.GetField(m_ElemOf.GetElement(i), "type", type);
			string low = type;
			low.ToLower();
			if (seen.Find(low) >= 0)
				continue;
			seen.Insert(low);
			r.Insert(type);
		}
		for (int w = 0; w < m_WName.Count(); w++)
		{
			string wk = m_WName.GetKey(w);
			if (seen.Find(wk) >= 0)
				continue;
			seen.Insert(wk);
			r.Insert(m_WName.GetElement(w));
		}
		return r;
	}

	array<string> ClassObjects(DZS_Instance inst)
	{
		Cache(inst);
		array<string> r = new array<string>();
		r.Copy(m_Objects);
		return r;
	}

	protected array<string> ScanObjects(DZS_Instance inst)
	{
		array<string> r = new array<string>();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			if (inst.GetKind(g) != DZS_KIND_GROUP)
				continue;
			array<string> ids = inst.GetChildren(g);
			foreach (string id : ids)
			{
				string name;
				inst.GetField(id, DZS_Obj.NAME, name);
				if (!DZS_Obj.IsModelPath(name))
					r.Insert(id);
			}
		}
		return r;
	}

	// ---- entries ----

	string ElementOf(DZS_Instance inst, string type)
	{
		Cache(inst);
		string key = type;
		key.ToLower();
		string id;
		m_ElemOf.Find(key, id);
		return id;
	}

	// The entry as the project has it now: the loot editor's element, else the working state as read, or null.
	DZS_LootEEntry Effective(DZS_Instance inst, string type)
	{
		string id = ElementOf(inst, type);
		if (id != "")
		{
			string gone;
			inst.GetField(id, "gone", gone);
			if (gone == "1")
				return null;
			string data;
			inst.GetField(id, "data", data);
			return DZS_LootEEntry.FromText(data);
		}
		return OpenEntry(type);
	}

	// The working state (as read, followed after each save).
	DZS_LootEEntry OpenEntry(string type)
	{
		if (!m_Files)
			return null;
		return m_Files.Entry(type);
	}

	static DZS_LootEEntry FromSource(string text)
	{
		if (text == "")
			return null;
		array<string> chunks = DZS_Files.Chunk(text);
		DZS_XmlDoc d = DZS_Xml.Parse(chunks);
		if (d.m_Error != "" || !d.m_Root)
			return null;
		return DZS_LootEEntry.FromNode(d, d.m_Root);
	}

	// Whether the base state has an entry for the type.
	bool HasBase(string type)
	{
		string text;
		return StartText(type, text);
	}

	// The base state's entry (a copy), or null.
	DZS_LootEEntry BaseEntry(string type)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (IsWritten(inst, type))
		{
			string text;
			if (!StartText(type, text))
				return null;
			return FromSource(text);
		}
		return OpenEntry(type);
	}

	string BaseSource(string type)
	{
		string text;
		StartText(type, text);
		return text;
	}

	protected string BaseSig(string type)
	{
		if (m_SigGen != m_StateVer)
		{
			m_SigCache.Clear();
			m_SigGen = m_StateVer;
		}
		string key = type;
		key.ToLower();
		string s;
		if (m_SigCache.Find(key, s))
			return s;
		DZS_LootEEntry b = BaseEntry(type);
		s = "";
		if (b)
			s = b.Sig();
		m_SigCache.Set(key, s);
		return s;
	}

	bool HasVanilla(string type)
	{
		return m_Base && m_Base.HasEntry(type);
	}

	DZS_LootEEntry VanillaEntry(string type)
	{
		if (!m_Base)
			return null;
		return m_Base.Entry(type);
	}

	string VanillaSig(string type)
	{
		string key = type;
		key.ToLower();
		string s;
		if (m_VanSig.Find(key, s))
			return s;
		s = "";
		DZS_LootEEntry v = VanillaEntry(type);
		if (v)
			s = v.Sig();
		m_VanSig.Set(key, s);
		return s;
	}

	// "base" (equal to the base state), "own" (from this project), "new" (from this project, not in the
	// base state), "missing"
	string Status(DZS_Instance inst, string type)
	{
		DZS_LootEEntry e = Effective(inst, type);
		bool hasBase = HasBase(type);
		if (!e)
			return "missing";
		if (!hasBase)
			return "new";
		string es = e.Sig();
		string bs = BaseSig(type);
		if (es == bs)
			return "base";
		return "own";
	}

	// The shown origin: "project", "new", "before", "vanilla", "there", "missing";
	// foreign is set when the base state carries the mark of another project.
	string Origin(DZS_Instance inst, string type, out bool foreign)
	{
		foreign = false;
		string st = Status(inst, type);
		if (st == "own")
			return "project";
		if (st == "new" || st == "missing")
			return st;
		string mark;
		if (StartMark(type, mark))
		{
			string mine = Mark(inst);
			if (mark != mine || mine == "")
			{
				foreign = true;
				return "before";
			}
		}
		if (!m_Base)
			return "there";
		if (!HasVanilla(type))
			return "before";
		string vs = VanillaSig(type);
		string bs = BaseSig(type);
		if (vs == bs)
			return "vanilla";
		return "before";
	}

	string OriginText(DZS_Instance inst, string type)
	{
		bool foreign;
		string o = Origin(inst, type, foreign);
		if (o == "new")
			return DZS_Texts.Get("loot.o.project") + " · " + DZS_Texts.Get("loot.o.new");
		return DZS_Texts.Get("loot.o." + o);
	}

	// A new content for a type's entry as one operation (a builder may be handed in to join a larger one); src
	// "vanilla" writes the vanilla element's text (Set to vanilla).
	void SetEntry(DZS_OpBuilder b, DZS_Instance inst, string type, DZS_LootEEntry e, bool reset = false, string src = "")
	{
		string id = ElementOf(inst, type);
		string data = "";
		string gone = "";
		if (e)
			data = e.ToText();
		else
			gone = "1";
		string rs = "";
		if (reset)
			rs = "1";
		if (id == "")
		{
			map<string, string> f = new map<string, string>();
			f.Set("type", type);
			f.Set("data", data);
			f.Set("gone", gone);
			f.Set("reset", rs);
			f.Set("src", src);
			b.Create(KIND, DZS_PROJECT_ID, DZS_END, f);
			return;
		}
		b.SetField(id, "data", data);
		b.SetField(id, "gone", gone);
		b.SetField(id, "reset", rs);
		b.SetField(id, "src", src);
	}

	// Own points start with a copy of the base state (without one: an entry with an empty lootFloor).
	DZS_LootEEntry StartOwn(string type)
	{
		DZS_LootEEntry b = BaseEntry(type);
		if (b)
		{
			DZS_Instance inst = m_Kernel.GetShown();
			if (!IsWritten(inst, type))
				return b;
			foreach (DZS_LootEContainer c : b.m_Containers)
			{
				c.m_Src = -1;
				foreach (DZS_LootEPoint p : c.m_Points)
					p.m_Src = -1;
			}
			return b;
		}
		DZS_LootEEntry e = new DZS_LootEEntry();
		e.m_Name = type;
		DZS_LootEContainer lc = new DZS_LootEContainer();
		lc.m_Name = "lootFloor";
		e.m_Containers.Insert(lc);
		return e;
	}

	string BaseName()
	{
		if (m_Base)
			return m_BaseFrom;
		return "none";
	}

	string ShownName(string type)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat)
			return cat.ShownName(type);
		return type;
	}

	// ---- settings ----

	protected string Derived(DZS_Instance inst, string id)
	{
		if (!m_Files)
			return NONE;
		DZS_LootRow row = m_Files.RowOfObject(id);
		if (!row)
			return NONE;
		string st = Status(inst, row.m_Name);
		if (st == "own" || st == "new")
			return OWN;
		return VANILLA;
	}

	// Default for newly placed objects: own if the type has own points, vanilla if the base state has an entry.
	string DefaultFor(DZS_Instance inst, string type)
	{
		if (!m_Files)
			return NONE;
		string st = Status(inst, type);
		if (st == "own" || st == "new")
			return OWN;
		if (HasBase(type))
			return VANILLA;
		return NONE;
	}

	string SettingOf(DZS_Instance inst, string id)
	{
		int v;
		string data;
		if (inst.GetCompanion(id, SETTING, v, data))
			return data;
		string name;
		inst.GetField(id, DZS_Obj.NAME, name);
		return DefaultFor(inst, name);
	}

	// After opening: the settings from the files, as a system change (no step).
	protected void DeriveSettings(DZS_Instance inst)
	{
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		array<string> objs = ClassObjects(inst);
		foreach (string id : objs)
		{
			string want = Derived(inst, id);
			int v;
			string have;
			bool has = inst.GetCompanion(id, SETTING, v, have);
			if (!has || have != want)
				b.SetCompanion(id, SETTING, 1, want);
		}
		if (b.IsEmpty())
			return;
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		DZS_Log.Info("loot: settings derived for " + op.CountChanges() + " object(s)");
	}

	// ---- switch ----

	void Toggle()
	{
		DZS_Instance inst = m_Kernel.GetShown();
		string folder = Folder();
		if (inst && IsOn())
		{
			AskOff(inst);
			return;
		}
		if (!inst || folder == "")
			return;
		if (IsOn())
		{
			AskOff(inst);
			return;
		}
		DZS_StoreProvider sp = Provider();
		DZS_LootOrigin lo = Origin();
		if (sp || lo)
		{
			if (!lo || lo.m_NoDe != "")
			{
				string nd = "Loot gibt es für dieses Projekt nicht";
				string ne = "There is no loot for this project";
				if (lo)
				{
					nd = lo.m_NoDe;
					ne = lo.m_NoEn;
				}
				m_Kernel.GetScreen().ShowMessage("loot.base", "warning", DZS_W.Literal(DZS_Texts.Pick(nd, ne)), 8.0);
				DZS_Log.Info("loot: cannot switch on in a provider project: " + nd);
				return;
			}
		}
		if (!DZS_LootFiles.Present(folder) && lo && FetchBase(folder))
			return;
		if (!DZS_LootFiles.Present(folder))
		{
			string dKick = DZS_Texts.Get("loot.row");
			string dTitle = DZS_Texts.Get("loot.cannotTitle");
			DZS_LootTextDialog d = new DZS_LootTextDialog(dKick, dTitle, DZS_Texts.Get("loot.cannotText"));
			d.AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "main");
			d.Open();
			DZS_Log.Info("loot: cannot switch on, files missing in " + folder);
			return;
		}
		DZS_LootFiles probe = DZS_LootFiles.Read(folder);
		if (probe.m_Error != "")
		{
			string txt = DZS_Texts.Fmt("loot.unreadable", probe.m_ErrorFile, probe.m_Error);
			string eKick = DZS_Texts.Get("loot.row");
			string eTitle = DZS_Texts.Get("loot.cannotTitle");
			DZS_LootTextDialog e = new DZS_LootTextDialog(eKick, eTitle, txt);
			e.AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "main");
			e.Open();
			DZS_Log.Info("loot: cannot switch on: " + txt);
			return;
		}
		probe = null;
		ReadFiles(inst, folder);
		array<string> objs = ClassObjects(inst);
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "lootEnabled", "true");
		string pid = Mark(inst);
		if (pid == "" && !sp && !lo)
		{
			pid = DZS_ProjectsPlugin.NewProjectId();
			b.SetField(DZS_PROJECT_ID, "lootProjectId", pid);
			DZS_Log.Info("loot: new project id " + pid);
		}
		b.SetField(DZS_PROJECT_ID, "lootFiles", DZS_LootFiles.PROTO + " " + m_Files.m_ProtoSha + ", " + DZS_LootFiles.POS + " " + m_Files.m_PosSha);
		int rows = 0;
		foreach (string id : objs)
		{
			string want = Derived(inst, id);
			if (want == NONE)
			{
				string name;
				inst.GetField(id, DZS_Obj.NAME, name);
				want = DefaultFor(inst, name);
			}
			else
				rows++;
			b.SetCompanion(id, SETTING, 1, want);
		}
		string de = DZS_Texts.Get("loot.on");
		DZS_SubmitResult r = b.Commit(de, "Turn on loot");
		DZS_Log.Info("loot: on " + r.Ok() + ", " + m_Files.m_Index.Count() + " entries, " + m_Files.m_Rows.Count() + " rows, " + rows + " of the project");
		if (m_Base)
			m_Kernel.GetScreen().ShowMessage("loot.base", "info", DZS_Texts.Fmt("loot.vanillaUsed", m_BaseFrom), 4.0);
		else
			m_Kernel.GetScreen().ShowMessage("loot.base", "info", DZS_Texts.Get("loot.noBase"), 10.0);
	}

	// A project of version 2 without loot files in its working folder: they come from its base, fetched first when
	// it is the vanilla mission; Toggle runs again once they are there. False when they cannot come (the dialog).
	protected bool FetchBase(string work)
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open || pp.m_Open.m_Work == "" || m_BaseFetch)
			return m_BaseFetch != null;
		string mk = MapKey();
		string dir = DZS_WorkFolder.UsableBase(pp.m_Open.m_Base, mk, null);
		if (dir != "")
		{
			if (!DZS_WorkFolder.CopyLoot(dir, mk, work) || !DZS_LootFiles.Present(work))
				return false;
			DZS_Log.Info("loot: base files copied into the working folder from " + dir);
			Toggle();
			return true;
		}
		m_BaseFetch = new DZS_LootVanilla();
		m_BaseFetch.m_OnDone.Insert(OnBaseFetched);
		if (!m_BaseFetch.StartMission(mk))
		{
			m_BaseFetch = null;
			return false;
		}
		m_Kernel.GetScreen().ShowBanner("loot.base", 20, "info", DZS_Texts.Fmt("loot.vanillaLoading", MapName(mk)));
		return true;
	}

	protected ref DZS_LootVanilla m_BaseFetch;

	void OnBaseFetched(DZS_LootVanilla v)
	{
		m_Kernel.GetScreen().HideBanner("loot.base");
		bool ok = v.m_Ok;
		string why = v.m_Why;
		m_BaseFetch = null;
		if (!ok)
		{
			VanillaFailed(why);
			return;
		}
		string work = Folder();
		if (work == "" || IsOn())
			return;
		string dir = DZS_WorkFolder.UsableBase("vanilla", MapKey(), null);
		if (dir == "" || !DZS_WorkFolder.CopyLoot(dir, MapKey(), work))
		{
			VanillaFailed("mapgroupproto.xml");
			return;
		}
		Toggle();
	}

	protected void AskOff(DZS_Instance inst)
	{
		int rows = 0;
		map<string, bool> ownTypes = new map<string, bool>();
		array<string> objs = ClassObjects(inst);
		foreach (string id : objs)
		{
			string s = SettingOf(inst, id);
			if (s == NONE)
				continue;
			rows++;
			if (s == OWN)
			{
				string name;
				inst.GetField(id, DZS_Obj.NAME, name);
				name.ToLower();
				ownTypes.Set(name, true);
			}
		}
		int written = WrittenTypes(inst).Count();
		string rowsText = Plural(rows, "Zeile", "Zeilen", "row", "rows");
		string entriesText = Plural(written, "Eintrag", "Einträge", "entry", "entries");
		string typesText = Plural(ownTypes.Count(), "Typ", "Typen", "type", "types");
		string q = DZS_Texts.Fmt("loot.offQ", rowsText, entriesText, typesText);
		string cKick = DZS_Texts.Get("loot.row");
		string cTitle = DZS_Texts.Get("loot.off");
		DZS_LootTextDialog c = new DZS_LootTextDialog(cKick, cTitle, q);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("off", DZS_Texts.Get("loot.off"), "danger");
		c.m_OnResult.Insert(OnOffAnswer);
		c.Open();
		DZS_Log.Info("loot: off asked: " + q);
	}

	static string Plural(int n, string de1, string deN, string en1, string enN)
	{
		if (n == 1)
			return "1 " + DZS_Texts.Pick(de1, en1);
		return n.ToString() + " " + DZS_Texts.Pick(deN, enN);
	}

	void OnOffAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("loot: off answer " + result);
		if (result != "off")
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return;
		m_Editor.Stop("");
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "lootEnabled", "false");
		array<string> objs = ClassObjects(inst);
		foreach (string id : objs)
			b.ClearCompanion(id, SETTING);
		Cache(inst);
		for (int e = 0; e < m_ElemOf.Count(); e++)
		{
			string eid = m_ElemOf.GetElement(e);
			b.Delete(eid);
		}
		string de = DZS_Texts.Get("loot.off");
		DZS_SubmitResult r = b.Commit(de, "Turn loot off");
		DZS_Log.Info("loot: off " + r.Ok());
	}

	void Dropped(DZS_Instance inst)
	{
		m_Editor.Stop("");
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		b.SetField(DZS_PROJECT_ID, "lootEnabled", "false");
		b.SetField(DZS_PROJECT_ID, "lootFiles", "");
		b.SetField(DZS_PROJECT_ID, "writtenTypes", DZS_ProjectsPlugin.WrittenJson(new array<string>(), new array<string>()));
		array<string> objs = ClassObjects(inst);
		foreach (string id : objs)
			b.ClearCompanion(id, SETTING);
		Cache(inst);
		for (int e = 0; e < m_ElemOf.Count(); e++)
		{
			string eid = m_ElemOf.GetElement(e);
			b.Delete(eid);
		}
		DZS_SubmitResult r = inst.SubmitSystem(b.GetOperation(), true);
		DZS_Log.Info("loot: dropped from the copy " + r.Ok());
	}

	array<string> WrittenTypes(DZS_Instance inst)
	{
		Written(inst);
		array<string> r = new array<string>();
		for (int i = 0; i < m_WName.Count(); i++)
		{
			string nm = m_WName.GetElement(i);
			r.Insert(nm);
		}
		return r;
	}

	// ---- vanilla state (answers to section 9 (1)) ----

	void AskVanilla()
	{
		string mk = MapKey();
		if (mk == "")
			return;
		string vw = DZS_Maps.WorldOf(mk);
		if (vw == "")
			vw = mk;
		string q = DZS_Texts.Fmt("loot.vanillaQ", MapName(mk), vw);
		string cKick = DZS_Texts.Get("loot.row");
		string cTitle = DZS_Texts.Get("loot.vanillaTitle");
		DZS_LootTextDialog c = new DZS_LootTextDialog(cKick, cTitle, q);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("go", DZS_Texts.Get("loot.vanillaGo"), "main");
		c.m_OnResult.Insert(OnVanillaAnswer);
		c.Open();
	}

	static string MapName(string mk)
	{
		if (mk == "chernarus")
			return "Chernarus";
		if (mk == "livonia")
			return "Livonia";
		if (mk == "sakhal")
			return "Sakhal";
		if (mk == "badlands")
			return "Nasdara";
		return mk;
	}

	void OnVanillaAnswer(DZS_Dialog d, string result)
	{
		if (result != "go")
			return;
		string mk = MapKey();
		m_Fetch = new DZS_LootVanilla();
		m_Fetch.m_OnDone.Insert(OnVanillaDone);
		if (!m_Fetch.Start(mk))
		{
			m_Fetch = null;
			VanillaFailed("no network");
			return;
		}
		m_Kernel.GetScreen().ShowBanner("loot.vanilla", 20, "info", DZS_Texts.Fmt("loot.vanillaLoading", MapName(mk)));
	}

	void OnVanillaDone(DZS_LootVanilla v)
	{
		m_Kernel.GetScreen().HideBanner("loot.vanilla");
		bool ok = v.m_Ok;
		string why = v.m_Why;
		m_Fetch = null;
		if (!ok)
		{
			VanillaFailed(why);
			return;
		}
		m_Kernel.GetScreen().ShowMessage("loot.base", "success", DZS_Texts.Fmt("loot.vanillaOk", MapName(MapKey())), 5.0);
		DZS_Log.Info("loot: vanilla state fetched for " + MapKey() + ", applies at the next opening");
	}

	protected void VanillaFailed(string why)
	{
		m_Kernel.GetScreen().ShowMessage("loot.base", "warning", DZS_Texts.Fmt("loot.vanillaFail", why), 10.0);
		DZS_Log.Info("loot: vanilla state failed: " + why);
	}

	// ---- setting per object ----

	bool OffersVanilla(DZS_Instance inst, string id)
	{
		string name;
		inst.GetField(id, DZS_Obj.NAME, name);
		if (HasBase(name))
			return true;
		int v;
		string data;
		return inst.GetCompanion(id, SETTING, v, data) && data == VANILLA;
	}

	void Choose(array<string> ids, string value)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return;
		array<string> change = new array<string>();
		map<string, bool> typesToSwitch = new map<string, bool>();
		int skipped = 0;
		foreach (string id : ids)
		{
			if (inst.IsLocked(id) || inst.IsLocked(inst.GetParent(id)))
				continue;
			if (value == VANILLA && !OffersVanilla(inst, id))
			{
				skipped++;
				continue;
			}
			if (SettingOf(inst, id) == value)
				continue;
			change.Insert(id);
			if (value != NONE)
			{
				string name;
				inst.GetField(id, DZS_Obj.NAME, name);
				typesToSwitch.Set(name, true);
			}
		}
		if (change.Count() == 0)
		{
			if (skipped > 0)
				SkippedMessage(skipped);
			return;
		}
		// type-wide: every object of these types with loot switches too
		array<string> also = new array<string>();
		array<string> objs = ClassObjects(inst);
		map<string, bool> lowTypes = new map<string, bool>();
		for (int lt = 0; lt < typesToSwitch.Count(); lt++)
		{
			string lk = typesToSwitch.GetKey(lt);
			lk.ToLower();
			lowTypes.Set(lk, true);
		}
		int inProject = 0;
		foreach (string oid : objs)
		{
			string on;
			inst.GetField(oid, DZS_Obj.NAME, on);
			on.ToLower();
			if (!lowTypes.Contains(on))
				continue;
			string cur = SettingOf(inst, oid);
			if (cur == NONE && change.Find(oid) < 0)
				continue;
			inProject++;
			if (change.Find(oid) < 0 && cur != value)
				also.Insert(oid);
		}
		int mapCount = 0;
		if (m_Files)
		{
			for (int t = 0; t < typesToSwitch.Count(); t++)
				mapCount += m_Files.ForeignRows(typesToSwitch.GetKey(t));
		}
		bool needsAsk = value != NONE && (also.Count() > 0 || mapCount > 0);
		foreach (string a : also)
			change.Insert(a);
		m_PendingIds = change;
		m_PendingValue = value;
		m_PendingTypes = new array<string>();
		for (int pt = 0; pt < typesToSwitch.Count(); pt++)
		{
			string tv3 = typesToSwitch.GetKey(pt);
			m_PendingTypes.Insert(tv3);
		}
		if (!needsAsk)
		{
			Apply(skipped);
			return;
		}
		string typesText = Plural(typesToSwitch.Count(), "Typ", "Typen", "type", "types");
		string projText = Plural(inProject, "Objekt", "Objekte", "object", "objects");
		string mapText = Plural(mapCount, "Gebäude", "Gebäude", "building", "buildings");
		string q = DZS_Texts.Fmt("loot.typeQ", typesText, projText, mapText);
		string cKick = DZS_Texts.Get("loot.row");
		string cTitle = DZS_Texts.Get("loot.typeQTitle");
		DZS_LootTextDialog c = new DZS_LootTextDialog(cKick, cTitle, q);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("switch", DZS_Texts.Get("loot.switch"), "main");
		c.m_OnResult.Insert(OnTypeAnswer);
		c.m_Tag = skipped.ToString();
		c.Open();
		DZS_Log.Info("loot: type-wide question: " + q);
	}

	protected ref array<string> m_PendingTypes;

	void OnTypeAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("loot: type-wide answer " + result);
		if (result != "switch")
			return;
		Apply(d.m_Tag.ToInt());
	}

	protected void SkippedMessage(int skipped)
	{
		string t = Plural(skipped, "Objekt", "Objekte", "object", "objects");
		m_Kernel.GetScreen().ShowMessage("loot.skipped", "info", DZS_Texts.Fmt("loot.skipped", t), 3.0);
	}

	protected void Apply(int skipped)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !m_PendingIds)
			return;
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		foreach (string id : m_PendingIds)
			b.SetCompanion(id, SETTING, 1, m_PendingValue);
		// the entries follow the setting: own points start from a copy, "as vanilla" resets the entry
		if (m_PendingTypes)
		{
			foreach (string type : m_PendingTypes)
			{
				string st = Status(inst, type);
				if (m_PendingValue == OWN && st == "missing")
				{
					DZS_LootEEntry e = StartOwn(type);
					SetEntry(b, inst, type, e);
				}
				else if (m_PendingValue == VANILLA && (st == "own" || st == "new") && HasBase(type))
				{
					DZS_LootEEntry be = BaseEntry(type);
					SetEntry(b, inst, type, be, true);
				}
			}
		}
		string de;
		string en;
		if (m_PendingIds.Count() == 1)
		{
			string name;
			inst.GetField(m_PendingIds[0], DZS_Obj.NAME, name);
			string shown = ShownName(name);
			de = DZS_Texts.FmtIn("loot.change", true, shown);
			en = DZS_Texts.FmtIn("loot.change", false, shown);
		}
		else
		{
			string cnt = m_PendingIds.Count().ToString();
			de = DZS_Texts.FmtIn("loot.changeN", true, cnt);
			en = DZS_Texts.FmtIn("loot.changeN", false, cnt);
		}
		DZS_SubmitResult r = b.Commit(de, en);
		DZS_Log.Info("loot: " + m_PendingIds.Count() + " object(s) -> " + m_PendingValue + " ok " + r.Ok() + " step '" + de + "'");
		m_PendingIds = null;
		m_PendingTypes = null;
		if (skipped > 0)
			SkippedMessage(skipped);
	}

	// ---- editing ----

	bool CanEditSelected(out string reasonDe, out string reasonEn)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!inst || !sel || !IsOn() || !m_Files)
		{
			reasonDe = "Loot ist aus";
			reasonEn = "Loot is off";
			return false;
		}
		array<string> ids = sel.GetSelection();
		if (ids.Count() != 1)
		{
			reasonDe = "Genau ein Objekt auswählen";
			reasonEn = "Select exactly one object";
			return false;
		}
		string id = ids[0];
		if (inst.IsLocked(id) || inst.IsLocked(inst.GetParent(id)))
		{
			reasonDe = "Gesperrt";
			reasonEn = "Locked";
			return false;
		}
		string name;
		inst.GetField(id, DZS_Obj.NAME, name);
		if (DZS_Obj.IsModelPath(name))
		{
			reasonDe = "Pflanzen und Felsen haben keinen Loot";
			reasonEn = "Plants and rocks have no loot";
			return false;
		}
		if (SettingOf(inst, id) == NONE)
		{
			reasonDe = "Loot ist für dieses Objekt aus — erst eine Einstellung wählen";
			reasonEn = "Loot is off for this object — choose a setting first";
			return false;
		}
		return m_Editor.CanStart(reasonDe, reasonEn);
	}

	void EditSelected()
	{
		string rd, re;
		if (!CanEditSelected(rd, re))
			return;
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		array<string> ids = sel.GetSelection();
		m_Editor.StartAtObject(ids[0]);
	}

	// ---- saving ----

	void Contribute(DZS_Instance inst, string folder, array<ref DZS_SaveFileOut> files, DZS_Issues issues, array<string> writtenTypes, array<string> originals)
	{
		m_PosPatches = null;
		m_ProtoPatches = null;
		m_SaveNames = null;
		m_SaveOrigs = null;
		m_Changes.Clear();
		bool on = IsOn();
		if (!m_Files && on && DZS_LootFiles.Present(folder))
			ReadFiles(inst, folder);
		if (!m_Files)
			return;
		string mark = Mark(inst);
		array<ref DZS_XmlPatch> posPatches = new array<ref DZS_XmlPatch>();
		array<ref DZS_XmlPatch> protoPatches = new array<ref DZS_XmlPatch>();
		DZS_XmlDoc posDoc = m_Files.m_PosDoc;
		string eol = posDoc.m_Eol;
		array<string> objs = ClassObjects(inst);
		map<string, bool> withLoot = new map<string, bool>();
		string added = "";
		string addedIds = "";
		foreach (string id : objs)
		{
			string s = NONE;
			if (on)
				s = SettingOf(inst, id);
			if (s == NONE)
				continue;
			withLoot.Set(id, true);
			string name, pos, ypr;
			inst.GetField(id, DZS_Obj.NAME, name);
			inst.GetField(id, DZS_Obj.POS, pos);
			inst.GetField(id, DZS_Obj.YPR, ypr);
			vector p = DZS_Obj.QuickVec(pos);
			vector a = DZS_Obj.QuickVec(ypr);
			string text = DZS_LootFiles.RowText(name, p, a, mark);
			DZS_LootRow row = m_Files.RowOfObject(id);
			if (row && row.m_Node)
			{
				if (!Same(row, p, a))
				{
					DZS_XmlWriter.Add(posPatches, row.m_Node.m_Start, row.m_Node.m_End, text);
					DZS_XmlPatch rp = posPatches[posPatches.Count() - 1];
					rp.m_Parent = posDoc.m_Root;
					rp.m_Tag = id;
					string oldRow = posDoc.Raw(row.m_Node);
					AddChange(DZS_LootFiles.POS, "removed", "", oldRow);
					AddChange(DZS_LootFiles.POS, "added", "", text);
				}
			}
			else
			{
				added += m_Files.m_PosIndent + text + eol;
				if (addedIds != "")
					addedIds += ",";
				addedIds += id;
				AddChange(DZS_LootFiles.POS, "added", "", text);
			}
			if (!on)
				continue;
			DZS_LootEEntry eff = Effective(inst, name);
			if (!eff)
			{
				DZS_Issue miss = issues.Add(DZS_Issue.WARNING, name + ": " + DZS_Texts.Pick("Typ hat keinen Loot-Eintrag — bekommt keinen Loot", "type has no loot entry — gets no loot"), name + ": type has no loot entry — gets no loot");
				miss.m_File = DZS_LootFiles.POS;
				miss.m_ObjectId = id;
			}
		}
		int stray = 0;
		foreach (DZS_LootRow r : m_Files.m_Rows)
		{
			if (!r.m_Node)
				continue;
			bool mine = r.m_Obj != "" && !withLoot.Contains(r.m_Obj);
			if (!mine && r.m_Obj == "" && ((mark != "" && r.m_Marked && r.m_Mark == mark) || r.m_Own))
			{
				mine = true;
				stray++;
			}
			if (mine)
			{
				DZS_XmlWriter.Add(posPatches, r.m_Node.m_LeadStart, r.m_Node.m_End, "");
				string gone = posDoc.Raw(r.m_Node);
				AddChange(DZS_LootFiles.POS, "removed", "", gone);
			}
		}
		if (stray > 0)
		{
			string st1 = Plural(stray, "Zeile", "Zeilen", "row", "rows");
			Hint(issues, DZS_Issue.HINT, st1 + " dieses Projekts gehören zu keinem Objekt mehr — das Speichern entfernt sie", st1 + " of this project belong to no object any more — saving removes them");
		}
		if (added != "")
		{
			string closeTag = "</" + posDoc.m_Root.m_Name + ">";
			int at = posDoc.LineStart(posDoc.m_Root.m_End - closeTag.Length());
			DZS_XmlWriter.Add(posPatches, at, at, added);
			DZS_XmlPatch ap = posPatches[posPatches.Count() - 1];
			ap.m_Parent = posDoc.m_Root;
			ap.m_Tag = addedIds;
		}
		m_SaveNames = new array<string>();
		m_SaveOrigs = new array<string>();
		Written(inst);
		for (int w = 0; w < m_WName.Count(); w++)
		{
			string wn = m_WName.GetElement(w);
			string wk = m_WName.GetKey(w);
			string wo;
			m_WOrig.Find(wk, wo);
			m_SaveNames.Insert(wn);
			m_SaveOrigs.Insert(wo);
		}
		EntryPatches(inst, on, mark, protoPatches, issues);
		writtenTypes.Clear();
		originals.Clear();
		for (int k = 0; k < m_SaveNames.Count(); k++)
		{
			string sn = m_SaveNames[k];
			string so = m_SaveOrigs[k];
			writtenTypes.Insert(sn);
			originals.Insert(so);
		}
		if (posPatches.Count() > 0)
		{
			DZS_SaveFileOut fo = new DZS_SaveFileOut();
			fo.m_Rel = DZS_LootFiles.POS;
			fo.m_Chunks = DZS_XmlWriter.Apply(posDoc, posPatches);
			fo.m_ReadSum = m_Files.m_PosSum;
			files.Insert(fo);
			m_PosOut = fo.m_Chunks;
			m_PosPatches = posPatches;
		}
		if (protoPatches.Count() > 0)
		{
			DZS_SaveFileOut fp = new DZS_SaveFileOut();
			fp.m_Rel = DZS_LootFiles.PROTO;
			fp.m_Chunks = DZS_XmlWriter.Apply(m_Files.m_Proto, protoPatches);
			fp.m_ReadSum = m_Files.m_ProtoSum;
			files.Insert(fp);
			m_ProtoOut = fp.m_Chunks;
			m_ProtoPatches = protoPatches;
		}
		DZS_Log.Info("loot: save " + posPatches.Count() + " row patch(es), " + protoPatches.Count() + " entry patch(es), on " + on + ", written types " + m_SaveNames.Count() + ", mark '" + mark + "'");
	}

	// The written types after the save in progress (for a storage provider building deltas).
	void SaveWritten(array<string> names, array<string> origs)
	{
		names.Clear();
		origs.Clear();
		if (!m_SaveNames)
			return;
		for (int i = 0; i < m_SaveNames.Count(); i++)
		{
			string n = m_SaveNames[i];
			string o = m_SaveOrigs[i];
			names.Insert(n);
			origs.Insert(o);
		}
	}

	protected void AddChange(string file, string kind, string key, string text)
	{
		DZS_LootChange c = new DZS_LootChange();
		c.m_File = file;
		c.m_Kind = kind;
		c.m_Key = key;
		c.m_Text = text;
		m_Changes.Insert(c);
	}

	// Text of another element adapted to the file: its line ends.
	protected string ToEol(string text, string eol)
	{
		string t = DZS_Xml.ToLf(text);
		if (eol == "\r\n")
			t = DZS_Xml.ToCrlf(t);
		return t;
	}

	// The vanilla element's text with the project's mark as the last attribute of its start tag.
	protected string VanillaText(string type, string mark, string eol)
	{
		if (!m_Base)
			return "";
		string src = m_Base.SourceOf(type);
		if (src == "")
			return "";
		src = ToEol(src, eol);
		if (mark == "")
			return src;
		int close = src.IndexOf(">");
		if (close < 0)
			return src;
		int cut = close;
		string before = src.Substring(close - 1, 1);
		if (before == "/")
			cut = close - 1;
		string head = src.Substring(0, cut);
		while (head.Length() > 0)
		{
			string last = head.Substring(head.Length() - 1, 1);
			if (last != " " && last != "\t")
				break;
			head = head.Substring(0, head.Length() - 1);
		}
		string tail = src.Substring(head.Length(), src.Length() - head.Length());
		return head + " dzstudio=\"" + mark + "\"" + tail;
	}

	// Entries the project touched: an entry that differs from its
	// base state is written with the mark and remembered with its original text; one equal to it (reset, undone,
	// loot off) gets the original text back, or is removed when there was none.
	protected void EntryPatches(DZS_Instance inst, bool on, string mark, array<ref DZS_XmlPatch> out2, DZS_Issues issues)
	{
		DZS_XmlDoc doc = m_Files.m_Proto;
		map<string, string> keys = new map<string, string>();
		if (on)
		{
			Cache(inst);
			for (int i = 0; i < m_ElemOf.Count(); i++)
			{
				string eid = m_ElemOf.GetElement(i);
				string type;
				inst.GetField(eid, "type", type);
				string tk4 = m_ElemOf.GetKey(i);
				keys.Set(tk4, type);
			}
		}
		foreach (string w : m_SaveNames)
		{
			string wk = w;
			wk.ToLower();
			if (!keys.Contains(wk))
				keys.Set(wk, w);
		}
		string closeTag = "</" + doc.m_Root.m_Name + ">";
		int endAt = doc.LineStart(doc.m_Root.m_End - closeTag.Length());
		string ind = m_Files.m_ProtoIndent;
		if (ind == "")
			ind = "\t";
		for (int k = 0; k < keys.Count(); k++)
		{
			string tname = keys.GetElement(k);
			string startText;
			bool startHas = StartText(tname, startText);
			DZS_LootEEntry eff = null;
			string src = "";
			if (on)
			{
				eff = Effective(inst, tname);
				string eid2 = ElementOf(inst, tname);
				if (eid2 != "")
					inst.GetField(eid2, "src", src);
			}
			DZS_XmlNode g = m_Files.Group(tname);
			string effSig = "";
			if (eff)
				effSig = eff.Sig();
			string startSig = BaseSig(tname);
			bool differs = eff != null && (!startHas || effSig != startSig);
			if (differs)
			{
				EntryIssues(eff, tname, issues);
				string orig = "";
				if (startHas)
					orig = startText;
				string vsig = VanillaSig(tname);
				if (src == "vanilla" && vsig != "" && vsig == effSig)
				{
					string vt = VanillaText(tname, mark, doc.m_Eol);
					if (g)
						DZS_XmlWriter.Add(out2, g.m_Start, g.m_End, vt);
					else
						DZS_XmlWriter.Add(out2, endAt, endAt, ind + vt + doc.m_Eol);
					DZS_XmlPatch vp = out2[out2.Count() - 1];
					vp.m_Parent = doc.m_Root;
					vp.m_Tag = tname;
					AddChange(DZS_LootFiles.PROTO, "replaced", tname, vt);
					AddWritten(tname, orig);
					continue;
				}
				if (!g)
				{
					string text = eff.GroupText(ind, m_Files.m_ProtoUnit, doc.m_Eol, mark);
					DZS_XmlWriter.Add(out2, endAt, endAt, ind + text + doc.m_Eol);
					DZS_XmlPatch np = out2[out2.Count() - 1];
					np.m_Parent = doc.m_Root;
					np.m_Tag = tname;
					string kind = "new";
					if (startHas)
						kind = "replaced";
					AddChange(DZS_LootFiles.PROTO, kind, tname, text);
					AddWritten(tname, orig);
					continue;
				}
				int first = out2.Count();
				DZS_LootDiff diff = new DZS_LootDiff(doc, out2, m_Files.m_ProtoUnit);
				DZS_LootEEntry cur = DZS_LootEEntry.FromNode(doc, g);
				string curSig = cur.Sig();
				if (curSig != effSig)
					diff.OneGroup(g, eff);
				string have;
				bool marked = g.Has("dzstudio");
				have = g.Get("dzstudio");
				if (mark != "" && (!marked || have != mark))
					diff.Mark(g, mark);
				else if (mark == "" && marked)
					diff.Mark(g, "");
				for (int pi = first; pi < out2.Count(); pi++)
					out2[pi].m_Tag = tname;
				if (out2.Count() > first)
					AddChange(DZS_LootFiles.PROTO, "replaced", tname, "");
				AddWritten(tname, orig);
				continue;
			}
			// equal to the base state, removed or loot off: back to the text before the project
			if (startHas)
			{
				string want = ToEol(startText, doc.m_Eol);
				if (g)
				{
					string now = doc.Raw(g);
					if (now != want)
					{
						DZS_XmlWriter.Add(out2, g.m_Start, g.m_End, want);
						DZS_XmlPatch rp = out2[out2.Count() - 1];
						rp.m_Parent = doc.m_Root;
						rp.m_Tag = tname;
						AddChange(DZS_LootFiles.PROTO, "replaced", tname, want);
					}
				}
				else
				{
					DZS_XmlWriter.Add(out2, endAt, endAt, ind + want + doc.m_Eol);
					DZS_XmlPatch ip = out2[out2.Count() - 1];
					ip.m_Parent = doc.m_Root;
					ip.m_Tag = tname;
					AddChange(DZS_LootFiles.PROTO, "replaced", tname, want);
				}
			}
			else if (g)
			{
				DZS_XmlWriter.Add(out2, g.m_LeadStart, g.m_End, "");
				out2[out2.Count() - 1].m_Tag = tname;
				AddChange(DZS_LootFiles.PROTO, "removed", tname, "");
			}
			RemoveWritten(tname);
		}
	}

	protected void AddWritten(string type, string orig)
	{
		string b = type;
		b.ToLower();
		foreach (string w : m_SaveNames)
		{
			string a = w;
			a.ToLower();
			if (a == b)
				return;
		}
		m_SaveNames.Insert(type);
		m_SaveOrigs.Insert(orig);
	}

	protected void RemoveWritten(string type)
	{
		string b = type;
		b.ToLower();
		for (int i = m_SaveNames.Count() - 1; i >= 0; i--)
		{
			string a = m_SaveNames[i];
			a.ToLower();
			if (a == b)
			{
				m_SaveNames.Remove(i);
				m_SaveOrigs.Remove(i);
			}
		}
	}

	// Check list entries of an own entry.
	protected void EntryIssues(DZS_LootEEntry e, string type, DZS_Issues issues)
	{
		if (e.m_Usages.Count() == 0)
			Hint(issues, DZS_Issue.HINT, type + ": Eintrag ohne Usage", type + ": entry without usage");
		foreach (DZS_LootEContainer c : e.m_Containers)
		{
			if (c.m_Cats.Count() == 0)
				Hint(issues, DZS_Issue.HINT, type + " · " + c.m_Name + ": Container ohne Kategorie", type + " · " + c.m_Name + ": container without category");
			if (c.m_Points.Count() == 0)
				Hint(issues, DZS_Issue.HINT, type + " · " + c.m_Name + ": Container ohne Punkte", type + " · " + c.m_Name + ": container without points");
			foreach (string k : c.m_Cats)
				NameIssue(issues, type + " · " + c.m_Name, "category", k, m_Files.m_Categories);
			foreach (string t : c.m_Tags)
				NameIssue(issues, type + " · " + c.m_Name, "tag", t, m_Files.m_Tags);
		}
		foreach (string u : e.m_Usages)
			NameIssue(issues, type, "usage", u, m_Files.m_Usages);
		foreach (string v : e.m_Values)
			NameIssue(issues, type, "value", v, m_Files.m_Values);
	}

	protected void Hint(DZS_Issues issues, string severity, string de, string en)
	{
		DZS_Issue i = issues.Add(severity, de, en);
		i.m_File = DZS_LootFiles.PROTO;
	}

	protected void NameIssue(DZS_Issues issues, string where, string kind, string name, array<string> list)
	{
		if (list.Count() == 0 || list.Find(name) >= 0)
			return;
		string low = name;
		low.ToLower();
		foreach (string l : list)
		{
			string ll = l;
			ll.ToLower();
			if (ll == low)
			{
				Hint(issues, DZS_Issue.HINT, where + ": " + kind + " „" + name + "“ heißt in cfglimitsdefinition.xml „" + l + "“", where + ": " + kind + " \"" + name + "\" is \"" + l + "\" in cfglimitsdefinition.xml");
				return;
			}
		}
		Hint(issues, DZS_Issue.WARNING, where + ": " + kind + " „" + name + "“ steht nicht in cfglimitsdefinition.xml", where + ": " + kind + " \"" + name + "\" is not in cfglimitsdefinition.xml");
	}

	// After writing: the editor follows its parsed files with the patches it wrote (no second reading);
	// the written types go into the state as the result of the save.
	void Saved(DZS_Instance inst, string folder)
	{
		if (!m_Files)
			return;
		int t0 = TickCount(0);
		if (m_PosPatches)
		{
			m_Files.m_PosDoc.Follow(m_PosPatches, m_PosOut);
			m_Files.FollowRows(m_PosPatches);
			m_Files.m_PosSum = DZS_Files.Checksum(m_PosOut);
			m_Files.m_PosSha = DZS_Sha256.Of(m_PosOut);
			m_Files.m_PosChunks = m_PosOut;
			m_Texts.Set(m_Files.m_PosSha, m_PosOut);
		}
		if (m_ProtoPatches)
		{
			m_Files.m_Proto.Follow(m_ProtoPatches, m_ProtoOut);
			m_Files.Reindex();
			m_Files.m_ProtoSum = DZS_Files.Checksum(m_ProtoOut);
			m_Files.m_ProtoSha = DZS_Sha256.Of(m_ProtoOut);
			m_Files.m_ProtoChunks = m_ProtoOut;
			m_Texts.Set(m_Files.m_ProtoSha, m_ProtoOut);
		}
		string filesNow = DZS_LootFiles.PROTO + " " + m_Files.m_ProtoSha + ", " + DZS_LootFiles.POS + " " + m_Files.m_PosSha;
		foreach (DZS_LootChange c : m_Changes)
			DZS_Log.Info("loot: change " + c.m_File + " " + c.m_Kind + " " + c.m_Key + " " + c.m_Text);
		DZS_Log.Info("loot: followed the written files in " + ((TickCount(0) - t0) / 10000) + " ms, " + m_Files.m_Index.Count() + " entries, " + m_Files.m_Rows.Count() + " rows");
		if (inst)
		{
			DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
			string filesHave;
			inst.GetField(DZS_PROJECT_ID, "lootFiles", filesHave);
			if (IsOn() && filesHave != filesNow)
				b.SetField(DZS_PROJECT_ID, "lootFiles", filesNow);
			if (m_SaveNames)
			{
				string json = DZS_ProjectsPlugin.WrittenJson(m_SaveNames, m_SaveOrigs);
				string have;
				inst.GetField(DZS_PROJECT_ID, "writtenTypes", have);
				if (json != have)
				{
					b.SetField(DZS_PROJECT_ID, "writtenTypes", json);
					DZS_Log.Info("loot: written types now " + m_SaveNames.Count());
				}
			}
			if (!b.IsEmpty())
			{
				DZS_Operation op = b.GetOperation();
				inst.SubmitSystem(op, true);
				DZS_Log.Info("loot: result of the save: " + filesNow);
			}
		}
		m_PosPatches = null;
		m_ProtoPatches = null;
		m_PosOut = null;
		m_ProtoOut = null;
		m_SaveNames = null;
		m_SaveOrigs = null;
		m_SigCache.Clear();
		m_StateVer++;
	}

	// Unchanged object: position within 0.5 mm and the same yaw, pitch and roll as its row.
	protected bool Same(DZS_LootRow row, vector p, vector ypr)
	{
		for (int i = 0; i < 3; i++)
		{
			if (Math.AbsFloat(row.m_Pos[i] - p[i]) > 0.0005)
				return false;
		}
		float yaw = DZS_PlaceMath.Norm180(ypr[0]);
		float dy = DZS_PlaceMath.Norm180(row.m_Rpy[2] - yaw);
		if (Math.AbsFloat(dy) >= 0.0005)
			return false;
		if (Math.AbsFloat(row.m_Rpy[1] - ypr[1]) >= 0.0005)
			return false;
		return Math.AbsFloat(row.m_Rpy[0] - ypr[2]) < 0.0005;
	}

	// ---- preview of the points ----

	override void OnUpdate(float dt)
	{
		m_Editor.Update(dt);
		if (!m_Canvas)
			return;
		m_Canvas.Clear();
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Canvas, 0, 0, sw, sh);
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !m_Files || !IsOn() || m_Kernel.GetScreen().IsUiHidden() || m_Kernel.GetScreen().IsHome())
			return;
		if (m_Editor.IsOn())
		{
			m_Editor.Draw(m_Canvas);
			return;
		}
		if ((Skip() & 2) != 0)
			return;
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!sel || (cat && cat.GetHand() != ""))
			return;
		string lh;
		inst.GetField(DZS_PROJECT_ID, "lootHidden", lh);
		if (lh == "true")
			return;
		array<string> ids = sel.GetSelection();
		if (ids.Count() != 1)
			return;
		string id = ids[0];
		if (SettingOf(inst, id) == NONE)
			return;
		string name, pos, ypr;
		inst.GetField(id, DZS_Obj.NAME, name);
		DZS_LootEEntry e = Effective(inst, name);
		if (!e)
			return;
		vector at, angles;
		float scale;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (!scene || !scene.GetView(id, at, angles, scale))
		{
			inst.GetField(id, DZS_Obj.POS, pos);
			inst.GetField(id, DZS_Obj.YPR, ypr);
			at = DZS_Obj.QuickVec(pos);
			angles = DZS_Obj.QuickVec(ypr);
		}
		DrawEntry(m_Canvas, "pv:" + name, e, at, angles, 0.30, null, "");
	}

	// ---- loot group, types and map buildings ----

	// Types of the loot group: of project objects with loot, and types whose entry is own,
	// in the order of their shown names.
	array<string> ListedTypes(DZS_Instance inst)
	{
		map<string, string> byKey = new map<string, string>();
		array<string> objs = ClassObjects(inst);
		foreach (string id : objs)
		{
			if (SettingOf(inst, id) == NONE)
				continue;
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			string k = name;
			k.ToLower();
			if (!byKey.Contains(k))
				byKey.Set(k, name);
		}
		array<string> top = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string t : top)
		{
			if (inst.GetKind(t) != KIND)
				continue;
			string type;
			inst.GetField(t, "type", type);
			string tk = type;
			tk.ToLower();
			if (byKey.Contains(tk))
				continue;
			string st = Status(inst, type);
			if (st == "own" || st == "new")
				byKey.Set(tk, type);
		}
		array<string> shown = new array<string>();
		map<string, string> typeOf = new map<string, string>();
		for (int i = 0; i < byKey.Count(); i++)
		{
			string ty = byKey.GetElement(i);
			string sn = ShownName(ty);
			string sk = sn + "|" + ty;
			sk.ToLower();
			shown.Insert(sk);
			typeOf.Set(sk, ty);
		}
		shown.Sort();
		array<string> r = new array<string>();
		foreach (string s2 : shown)
			r.Insert(typeOf.Get(s2));
		return r;
	}

	bool UsedByObjects(DZS_Instance inst, string type)
	{
		array<string> objs = ClassObjects(inst);
		string low = type;
		low.ToLower();
		foreach (string id : objs)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			name.ToLower();
			if (name == low && SettingOf(inst, id) != NONE)
				return true;
		}
		return false;
	}

	// Click on a type of the loot group: the nearest building of the type to the camera, first among the project's
	// objects with loot, else among the map buildings with a row; the camera turns to it like Focus.
	bool EditType(DZS_Instance inst, string type)
	{
		string rd, re;
		if (!m_Editor.CanStart(rd, re))
		{
			m_Kernel.GetScreen().ShowMessage("loot.type", "info", DZS_Texts.Pick(rd, re), 4.0);
			return false;
		}
		vector cam = GetGame().GetCurrentCameraPosition();
		string low = type;
		low.ToLower();
		string best = "";
		float bestD = 1e30;
		array<string> objs = ClassObjects(inst);
		foreach (string id : objs)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			name.ToLower();
			if (name != low || SettingOf(inst, id) == NONE || inst.IsLocked(id))
				continue;
			string pos;
			inst.GetField(id, DZS_Obj.POS, pos);
			vector p = DZS_Obj.QuickVec(pos);
			float d = vector.Distance(p, cam);
			if (d < bestD)
			{
				bestD = d;
				best = id;
			}
		}
		if (best != "")
		{
			string bp;
			inst.GetField(best, DZS_Obj.POS, bp);
			vector bv = DZS_Obj.QuickVec(bp);
			Focus(bv);
			m_Editor.StartAtObject(best);
			return true;
		}
		Object mo = NearestMapBuilding(type, cam);
		if (!mo)
		{
			m_Kernel.GetScreen().ShowMessage("loot.type", "info", DZS_Texts.Pick("Kein Gebäude dieses Typs auf der Karte oder im Projekt", "No building of this type on the map or in the project"), 4.0);
			return false;
		}
		Focus(mo.GetPosition());
		m_Editor.StartAtMap(mo);
		return true;
	}

	protected void Focus(vector p)
	{
		DZS_CameraPlugin cam = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
		if (!cam)
			return;
		vector half = Vector(8, 5, 8);
		cam.FocusOn(p - half, p + half, true);
	}

	// The map object of a type nearest to a point among the rows of mapgrouppos.xml that belong to no project object.
	Object NearestMapBuilding(string type, vector near)
	{
		if (!m_Files)
			return null;
		string low = type;
		low.ToLower();
		DZS_LootRow best = null;
		float bestD = 1e30;
		foreach (DZS_LootRow row : m_Files.m_Rows)
		{
			if (!row.m_Valid || row.m_Obj != "" || row.m_Key != low)
				continue;
			float d = vector.Distance(row.m_Pos, near);
			if (d < bestD)
			{
				bestD = d;
				best = row;
			}
		}
		if (!best)
			return null;
		return MapObjectAt(type, best.m_Pos);
	}

	static Object MapObjectAt(string type, vector pos)
	{
		string low = type;
		low.ToLower();
		array<Object> objs = new array<Object>();
		GetGame().GetObjectsAtPosition3D(pos, 2.0, objs, null);
		DZS_SceneApi scene = DZS_SceneApi.Get();
		Object best = null;
		float bestD = 1e30;
		foreach (Object o : objs)
		{
			string t = o.GetType();
			t.ToLower();
			if (t != low)
				continue;
			if (scene && scene.IdOf(o) != "")
				continue;
			float d = vector.Distance(o.GetPosition(), pos);
			if (d < bestD)
			{
				bestD = d;
				best = o;
			}
		}
		return best;
	}

	void RemoveEntry(DZS_Instance inst, string type)
	{
		DZS_OpBuilder ob = inst.BeginOperation("loot:edit", 1);
		SetEntry(ob, inst, type, null);
		string shown = ShownName(type);
		string de = DZS_Texts.FmtIn("loot.stepDelete", true, shown);
		string en = DZS_Texts.FmtIn("loot.stepDelete", false, shown);
		DZS_SubmitResult r = ob.Commit(de, en);
		DZS_Log.Info("loot: step '" + de + "' ok " + r.Ok());
	}

	// Entry in the context menu of a map object, below placing's own.
	void AddMapMenuItems(Object o, array<ref DZS_MenuItem> items)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!o || !inst || !IsOn() || m_Editor.IsOn())
			return;
		string type = o.GetType();
		if (type == "" || !GetGame().ConfigIsExisting("CfgVehicles " + type))
			return;
		DZS_MenuItem it = new DZS_MenuItem();
		it.m_Tag = "loot.editMap";
		string shown = ShownName(type);
		it.m_Text = DZS_Texts.Pick("Loot bearbeiten (", "Edit loot (") + shown + ")";
		string rd, re;
		it.m_Enabled = m_Editor.CanStart(rd, re);
		if (it.m_Enabled && (!m_Files || !Effective(inst, type)))
		{
			it.m_Enabled = false;
			rd = "Kein Loot-Eintrag — eigene Punkte gibt es nur an Objekten des Projekts";
			re = "No loot entry — own points only exist at objects of the project";
		}
		it.m_Reason = DZS_Texts.Pick(rd, re);
		it.m_SepBefore = items.Count() > 0;
		items.Insert(it);
		DZS_Log.Info("loot: map object menu " + type + " at " + o.GetPosition() + " enabled " + it.m_Enabled);
	}

	bool OnMapMenuChoose(Object o, DZS_MenuItem it)
	{
		if (it.m_Tag != "loot.editMap")
			return false;
		if (it.m_Enabled && o)
			m_Editor.StartAtMap(o);
		return true;
	}

	// The points of an entry at a building as cylinders: container
	// colours in order, covered pieces with half the opacity, selected points in the accent colour and wider.
	void DrawEntry(CanvasWidget cv, string keyBase, DZS_LootEEntry e, vector origin, vector ypr, float alpha, map<string, bool> selected, string hover)
	{
		vector r, u, f;
		DZS_PlaceMath.Axes(ypr, r, u, f);
		float w1 = Math.Max(1, DZS_Scale.Px(1));
		float w2 = Math.Max(2, DZS_Scale.Px(2));
		int accent = DZS_Color.Get("accent");
		for (int c = 0; c < e.m_Containers.Count(); c++)
		{
			DZS_LootEContainer lc = e.m_Containers[c];
			int base = ContainerColor(c);
			for (int i = 0; i < lc.m_Points.Count(); i++)
			{
				DZS_LootEPoint pt = lc.m_Points[i];
				if (!pt.Valid())
					continue;
				string pk = DZS_LootEditor.Key(c, i);
				int col = base;
				float w = w1;
				if (selected && selected.Contains(pk))
				{
					col = accent;
					w = w2;
				}
				if (pk == hover)
					col = Lighter(col);
				int full = WithAlpha(col, alpha);
				int half = WithAlpha(col, alpha * 0.5);
				vector bpos = WorldOf(origin, r, u, f, pt.P());
				Cylinder(keyBase + ":" + pk, bpos, pt.Range(), pt.Height(), w, full, half);
			}
		}
		DZS_WorldLines.Flush(cv);
	}

	// Colour of the c-th container: the colour row of the groups, from the ninth on again from the start.
	static int ContainerColor(int c)
	{
		if (!s_Colors)
		{
			s_Colors = new array<int>();
			array<string> row = DZS_Palette.Colors();
			foreach (string hex : row)
			{
				int v = DZS_Palette.ToARGB(hex);
				s_Colors.Insert(v);
			}
		}
		return s_Colors[c % s_Colors.Count()];
	}

	protected static ref array<int> s_Colors;

	static int WithAlpha(int argb, float alpha)
	{
		int a = Math.Round(alpha * 255);
		return (argb & 0x00FFFFFF) | (a << 24);
	}

	static int Lighter(int argb)
	{
		int rr = (argb >> 16) & 255;
		int gg = (argb >> 8) & 255;
		int bb = argb & 255;
		rr = rr + (255 - rr) / 2;
		gg = gg + (255 - gg) / 2;
		bb = bb + (255 - bb) / 2;
		return (argb & 0xFF000000) | (rr << 16) | (gg << 8) | bb;
	}

	// A loot point (px, py, pz) lies at the model point (-pz, py, px) of its building.
	static vector WorldOf(vector origin, vector r, vector u, vector f, vector p)
	{
		return origin + r * (-p[2]) + u * p[1] + f * p[0];
	}

	// Model point of a world position (inverse of WorldOf), as loot point coordinates.
	static vector LootOf(vector origin, vector r, vector u, vector f, vector w)
	{
		vector d = w - origin;
		float mx = vector.Dot(d, r);
		float my = vector.Dot(d, u);
		float mz = vector.Dot(d, f);
		return Vector(mz, my, -mx);
	}

	protected static ref array<int> s_CylPairs;

	// Ground circle and top circle through 24 points each and the two outlines left and right as seen from the camera.
	protected void Cylinder(string key, vector base, float radius, float height, float width, int full, int half)
	{
		if (!s_CylPairs)
		{
			s_CylPairs = new array<int>();
			for (int k = 0; k < 24; k++)
			{
				s_CylPairs.Insert(k);
				s_CylPairs.Insert((k + 1) % 24);
				s_CylPairs.Insert(24 + k);
				s_CylPairs.Insert(24 + (k + 1) % 24);
			}
			s_CylPairs.Insert(48);
			s_CylPairs.Insert(49);
			s_CylPairs.Insert(50);
			s_CylPairs.Insert(51);
		}
		array<vector> pts = new array<vector>();
		vector top = base + Vector(0, height, 0);
		for (int i = 0; i < 24; i++)
		{
			float a = Math.PI2 * i / 24;
			vector d = Vector(Math.Cos(a) * radius, 0, Math.Sin(a) * radius);
			pts.Insert(base + d);
		}
		for (int j = 0; j < 24; j++)
		{
			float a2 = Math.PI2 * j / 24;
			vector d2 = Vector(Math.Cos(a2) * radius, 0, Math.Sin(a2) * radius);
			pts.Insert(top + d2);
		}
		vector cam = GetGame().GetCurrentCameraPosition();
		vector side = base - cam;
		side[1] = 0;
		vector right = Vector(side[2], 0, -side[0]);
		if (right.Length() > 0.0001)
			right = right.Normalized() * radius;
		pts.Insert(base + right);
		pts.Insert(top + right);
		pts.Insert(base - right);
		pts.Insert(top - right);
		DZS_WorldLines.Lines(key, pts, s_CylPairs, width, full, half, DZS_WorldLines.PRIO_NEW);
	}

	// Counts for the row: points of the entry, buildings of the map, objects of the project with loot of this type.
	void Facts(DZS_Instance inst, string type, out int points, out int mapRows, out int inProject)
	{
		points = 0;
		mapRows = 0;
		inProject = 0;
		if (!m_Files)
			return;
		DZS_LootEEntry e = Effective(inst, type);
		if (e)
			points = e.CountPoints();
		mapRows = m_Files.ForeignRows(type);
		Cache(inst);
		string key = type;
		key.ToLower();
		m_WithLoot.Find(key, inProject);
	}
}

// Row "Loot" in the properties part: none · as vanilla · own points, "Edit", warnings and counts.
class DZS_LootRowUi : DZS_PropertyRow
{
	DZS_LootPlugin m_P;
	protected Widget m_Frame;
	protected TextWidget m_Label;
	protected ref DZS_Chip m_None;
	protected ref DZS_Chip m_Vanilla;
	protected ref DZS_Chip m_Own;
	protected ref DZS_Chip m_Edit;
	protected TextWidget m_Warn;
	protected TextWidget m_Facts;
	protected ref array<string> m_Ids;

	override void Build(Widget parent)
	{
		m_Frame = DZS_W.Frame(parent);
		m_Label = DZS_PropertiesApi.RowLabel(m_Frame, DZS_Texts.Get("loot.row"));
		m_None = new DZS_Chip(m_Frame, DZS_LootPlugin.NONE, DZS_Texts.Get("loot.none"));
		m_None.m_OnClick.Insert(OnChip);
		m_Vanilla = new DZS_Chip(m_Frame, DZS_LootPlugin.VANILLA, DZS_Texts.Get("loot.vanilla"));
		m_Vanilla.m_OnClick.Insert(OnChip);
		m_Own = new DZS_Chip(m_Frame, DZS_LootPlugin.OWN, DZS_Texts.Get("loot.own"));
		m_Own.m_OnClick.Insert(OnChip);
		m_Edit = new DZS_Chip(m_Frame, "edit", DZS_Texts.Get("loot.edit"));
		m_Edit.m_OnClick.Insert(OnEdit);
		m_Warn = DZS_W.Text(m_Frame, DZS_Font.READ, 12, DZS_Color.Get("warning"));
		m_Facts = DZS_W.Text(m_Frame, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
	}

	override bool AppliesTo(DZS_Instance inst, array<string> ids)
	{
		if (!m_P.IsOn() || m_P.m_Editor.IsOn())
			return false;
		foreach (string id : ids)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			if (!DZS_Obj.IsModelPath(name))
				return true;
		}
		return false;
	}

	protected float m_LastY;
	protected float m_LastW;

	override float Layout(DZS_Instance inst, array<string> ids, float y, float width)
	{
		m_LastY = y;
		m_LastW = width;
		m_Ids = new array<string>();
		string common = "";
		bool mixed = false;
		bool vanillaOk = false;
		bool anyUnlocked = false;
		foreach (string id : ids)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			if (DZS_Obj.IsModelPath(name))
				continue;
			m_Ids.Insert(id);
			string s = m_P.SettingOf(inst, id);
			if (common == "")
				common = s;
			else if (common != s)
				mixed = true;
			if (m_P.OffersVanilla(inst, id))
				vanillaOk = true;
			if (!inst.IsLocked(id) && !inst.IsLocked(inst.GetParent(id)))
				anyUnlocked = true;
		}
		if (mixed)
			common = "";
		m_None.SetOn(common == DZS_LootPlugin.NONE);
		m_Vanilla.SetOn(common == DZS_LootPlugin.VANILLA);
		m_Own.SetOn(common == DZS_LootPlugin.OWN);
		string lockedWhy = "";
		if (!anyUnlocked)
			lockedWhy = DZS_Texts.Pick("Gesperrt", "Locked");
		m_None.SetEnabled(anyUnlocked, lockedWhy);
		m_Own.SetEnabled(anyUnlocked, lockedWhy);
		string vWhy = lockedWhy;
		if (anyUnlocked && !vanillaOk)
			vWhy = DZS_Texts.Get("loot.noVanilla");
		m_Vanilla.SetEnabled(anyUnlocked && vanillaOk, vWhy);
		m_WaitShown = "";
		if (m_P.m_WaitDe != "")
			ApplyWait();
		string ed, ee;
		bool canEdit = m_P.CanEditSelected(ed, ee);
		m_Edit.SetEnabled(canEdit, DZS_Texts.Pick(ed, ee));
		m_Edit.Show(m_Ids.Count() == 1);
		float h = DZS_Scale.Px(16);
		DZS_W.Place(m_Frame, 0, y, width, DZS_Scale.Px(120));
		DZS_W.Place(m_Label, 0, 0, width, DZS_Scale.Px(14));
		float gap = DZS_Scale.Px(4);
		float w1 = m_None.Width();
		float w2 = m_Vanilla.Width();
		float w3 = m_Own.Width();
		float extra = (width - 2 * gap - w1 - w2 - w3) / 3;
		if (extra < 0)
		{
			w1 = (width - 2 * gap) / 3;
			w2 = w1;
			w3 = w1;
			extra = 0;
		}
		w1 += extra;
		w2 += extra;
		w3 += extra;
		m_None.Place(0, h, w1);
		m_Vanilla.Place(w1 + gap, h, w2);
		m_Own.Place(w1 + w2 + 2 * gap, h, w3);
		float ty = h + DZS_Chip.Height() + DZS_Scale.Px(4);
		if (m_Ids.Count() == 1)
		{
			float ew = m_Edit.Width();
			m_Edit.Place(width - ew, ty, ew);
			ty += DZS_Chip.Height() + DZS_Scale.Px(4);
		}
		string warn = "";
		string facts = "";
		if (m_Ids.Count() == 1)
		{
			string one = m_Ids[0];
			string type;
			inst.GetField(one, DZS_Obj.NAME, type);
			DZS_LootEEntry eff = m_P.Effective(inst, type);
			if (common == DZS_LootPlugin.VANILLA && !eff)
				warn = DZS_Texts.Get("loot.noEntry");
			if (eff)
			{
				int pts, mapRows, inProj;
				m_P.Facts(inst, type, pts, mapRows, inProj);
				string ptsText = pts.ToString();
				string mapText = DZS_LootPlugin.Plural(mapRows, "Gebäude", "Gebäude", "building", "buildings");
				string projText = DZS_LootPlugin.Plural(inProj, "Objekt", "Objekte", "object", "objects");
				facts = DZS_Texts.Fmt("loot.facts", ptsText, mapText, projText);
			}
		}
		m_Warn.SetText(warn);
		m_Facts.SetText(facts);
		m_Warn.Show(warn != "");
		if (warn != "")
		{
			DZS_W.Place(m_Warn, 0, ty, width, DZS_Scale.Px(18));
			ty += DZS_Scale.Px(18);
		}
		DZS_W.Place(m_Facts, 0, ty, width, DZS_Scale.Px(18));
		ty += DZS_Scale.Px(22);
		return ty;
	}

	void OnChip(DZS_Chip c)
	{
		if (!m_Ids)
			return;
		m_P.Choose(m_Ids, c.m_Key);
	}

	void OnEdit(DZS_Chip c)
	{
		m_P.EditSelected();
	}

	override void Show(bool show) { m_Frame.Show(show); }

	// While new loot texts are fetched in a session the setting is view-only.
	protected string m_WaitShown;

	protected void ApplyWait()
	{
		m_WaitShown = m_P.m_WaitDe;
		string why = DZS_Texts.Pick(m_P.m_WaitDe, m_P.m_WaitEn);
		m_None.SetEnabled(false, why);
		m_Vanilla.SetEnabled(false, why);
		m_Own.SetEnabled(false, why);
	}

	override void Update(float dt)
	{
		if (m_Ids && m_P.m_WaitDe != m_WaitShown)
		{
			if (m_P.m_WaitDe != "")
				ApplyWait();
			else
			{
				m_WaitShown = "";
				DZS_Instance inst = DZS_Kernel.Get().GetShown();
				array<string> keep = new array<string>();
				keep.Copy(m_Ids);
				if (inst)
					Layout(inst, keep, m_LastY, m_LastW);
			}
		}
	}
}
