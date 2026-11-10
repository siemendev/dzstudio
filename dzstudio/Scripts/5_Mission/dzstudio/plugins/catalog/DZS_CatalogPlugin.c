// The object catalog: a bundled plugin that uses only the public interface. It offers the
// console list checked against the running game, search, favourites, recently used entries, templates, the
// class directory for other plugins and the hand. Placing itself belongs to the placement plugin (section 6).
class DZS_CatalogService : DZS_CatalogApi
{
	DZS_CatalogPlugin m_P;

	override string Spelling(string name) { return DZS_ClassDir.Find(name); }
	override bool ModelLoadable(string path) { return FileExist(path); }
	override bool InCatalog(string name) { return m_P.m_Store.IsReady() && m_P.m_Store.Find(name) != null && !m_P.m_Store.Find(name).m_IsTemplate; }
	override string ShownName(string name)
	{
		DZS_CatEntry e = m_P.m_Store.Find(name);
		if (e)
			return e.m_Name;
		return DZS_CatText.Readable(name);
	}
	override bool NameShared(string name)
	{
		DZS_CatEntry e = m_P.m_Store.Find(name);
		if (!e || e.m_IsTemplate)
			return false;
		return m_P.m_Store.NameCount(e.m_Name) > 1;
	}
	override string IconOf(string name)
	{
		DZS_CatEntry e = m_P.m_Store.Find(name);
		if (e)
			return DZS_CatalogPlugin.CategoryIcon(e.m_Cat);
		return "package";
	}
	override bool IsReady() { return m_P.m_Store.IsReady(); }
	override string GetHand()
	{
		if (m_P.m_Hand)
			return m_P.m_Hand.m_Key;
		return "";
	}
	override void TakeInHand(string entry) { m_P.TakeInHand(m_P.m_Store.Find(entry)); }
	override void DropHand() { m_P.DropHand(); }
	override void NoteUsed(string entry) { m_P.NoteUsed(entry); }
	override string SaveTemplate(string name, array<ref DZS_TemplateObject> objects) { return m_P.m_Templates.Save(name, objects); }
	override array<ref DZS_TemplateObject> TemplateObjects(string id) { return m_P.m_Templates.Objects(id); }
	override void AddImageSource(DZS_ImageSource source)
	{
		if (m_P.m_Sources.Find(source) < 0)
			m_P.m_Sources.Insert(source);
		m_P.SourcesChanged();
	}
	override void SourcesChanged() { m_P.SourcesChanged(); }
	override void PhotosChanged() { m_P.ForgetPhotos(); m_P.MarkDirty(); if (m_P.m_Full) m_P.m_Full.MarkDirty(); }
	override void AddMenuSource(DZS_CatalogMenuSource source) { m_P.m_MenuSources.Insert(source); }
}

class DZS_CatalogActions : DZS_ActionHandler
{
	DZS_CatalogPlugin m_P;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn) { return m_P.ActionEnabled(actionId, reasonDe, reasonEn); }
	override void OnAction(string actionId) { m_P.OnAction(actionId); }
}

class DZS_CatalogEsc : DZS_EscHandler
{
	DZS_CatalogPlugin m_P;
	string m_What;

	override bool IsEscActive()
	{
		if (m_What == "full")
			return m_P.m_Full && m_P.m_Full.IsOpen();
		return m_P.m_Hand != null;
	}

	override void OnEsc()
	{
		if (m_What == "full")
			m_P.CloseFull(false);
		else
			m_P.DropHand();
	}
}

[DZS_RegisterPlugin(DZS_CatalogPlugin)]
class DZS_CatalogPlugin : DZS_Plugin
{
	static const int RECENT_MAX = 20;

	ref DZS_CatalogStore m_Store;
	ref DZS_CatalogTemplates m_Templates;
	ref DZS_CatPreviews m_Previews;
	ref DZS_CatalogPanel m_Panel;
	ref DZS_CatalogFull m_Full;
	ref array<ref DZS_ImageSource> m_Sources;
	ref array<ref DZS_CatalogMenuSource> m_MenuSources;
	protected ref DZS_CatalogService m_Service;
	protected ref DZS_CatalogActions m_Actions;
	protected ref DZS_CatalogEsc m_EscHand;
	protected ref DZS_CatalogEsc m_EscFull;
	DZS_CatEntry m_Hand;
	string m_Cat = "all";
	protected string m_Query = "";
	protected ref DZS_CatResult m_List;
	protected bool m_Dirty = true;
	protected ref map<string, bool> m_Favs;
	protected ref array<string> m_Recent;
	protected DZS_CatEntry m_CtxEntry;
	protected bool m_Session;
	protected bool m_WasReady;
	float m_LastSearchMs;
	protected ref map<string, string> m_PhotoCache;
	protected ref map<string, string> m_DescCache;

	override string GetName() { return "catalog"; }

	DZS_Kernel Kernel() { return m_Kernel; }

	override void OnRegister()
	{
		RegisterTexts();
		m_Store = new DZS_CatalogStore();
		m_Templates = new DZS_CatalogTemplates(this);
		m_Previews = new DZS_CatPreviews();
		m_Sources = new array<ref DZS_ImageSource>();
		m_MenuSources = new array<ref DZS_CatalogMenuSource>();
		m_PhotoCache = new map<string, string>();
		m_DescCache = new map<string, string>();
		m_Service = new DZS_CatalogService();
		m_Service.m_P = this;
		m_Kernel.SetService("catalog", m_Service);
		m_Actions = new DZS_CatalogActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("catalog", "Katalog", "Catalogue");
		array<string> kf = {"Ctrl+F"};
		ui.RegisterAction("catalog", "catalog.search", "Suchfeld des Katalogs", "Catalogue search field", kf, m_Actions);
		ui.SetGroup("catalog.search", "catalog");
		array<string> kff = {"Ctrl+Shift+F"};
		ui.RegisterAction("catalog", "catalog.full", "Vollbild-Suche", "Full-screen search", kff, m_Actions);
		ui.SetGroup("catalog.full", "catalog");
		ui.SetWhen("catalog.full", "scene,fullsearch");
		ui.AddMenuEntry("view", "catalog.full", 150, true);
		array<string> kn = {"DOWN", "NEXT"};
		ui.RegisterAction("catalog", "catalog.next", "Nächster Eintrag in der Hand", "Next entry in the hand", kn, m_Actions);
		ui.SetGroup("catalog.next", "catalog");
		ui.SetWhen("catalog.next", "hand");
		array<string> kp = {"UP", "PRIOR"};
		ui.RegisterAction("catalog", "catalog.prev", "Voriger Eintrag in der Hand", "Previous entry in the hand", kp, m_Actions);
		ui.SetGroup("catalog.prev", "catalog");
		ui.SetWhen("catalog.prev", "hand");
		array<string> kk = {"Ctrl+K"};
		ui.RegisterAction("catalog", "catalog.saveTemplate", "Als Vorlage speichern …", "Save as template …", kk, m_Actions);
		ui.SetGroup("catalog.saveTemplate", "catalog");
		ui.SetWhen("catalog.saveTemplate", "selection");
		ui.AddMenuEntry("edit", "catalog.saveTemplate", 902);
		array<string> ctx = {"catalog.ctx.hand", "catalog.ctx.fav", "catalog.ctx.copy", "catalog.ctx.rename", "catalog.ctx.delete"};
		foreach (string c : ctx)
		{
			ui.RegisterAction("catalog", c, c, c, null, m_Actions);
			ui.SetListed(c, false);
		}
		m_Panel = new DZS_CatalogPanel(this);
		ui.RegisterPanel("catalog", "catalog.bar", "Katalog", "Catalogue", "left", "library", m_Panel, 0);
		ui.SetPanelMain("catalog.bar");
		m_Kernel.GetSettings().Define("catalog.tileSize", DZS_Json.Int(128));
		DZS_SheetParts.Add(DZS_CatSheetPart);
	}

	override void OnSessionStart(string world)
	{
		if (world == "")
			return;
		m_Session = true;
		m_Favs = new map<string, bool>();
		m_Recent = new array<string>();
		DZS_Json fav = m_Kernel.GetSettings().Get("catalog.favourites");
		if (fav && fav.IsArr())
		{
			for (int i = 0; i < fav.Count(); i++)
			{
				DZS_Json fe = fav.At(i);
				string fk = fe.m_Str;
				m_Favs.Set(fk, true);
			}
		}
		DZS_Json rec = m_Kernel.GetSettings().Get("catalog.recent");
		if (rec && rec.IsArr())
		{
			for (int k = 0; k < rec.Count(); k++)
			{
				DZS_Json re = rec.At(k);
				string rk = re.m_Str;
				m_Recent.Insert(rk);
			}
		}
		m_EscFull = new DZS_CatalogEsc();
		m_EscFull.m_P = this;
		m_EscFull.m_What = "full";
		m_Kernel.GetScreen().AddEscHandler(m_EscFull, 5);
		m_EscHand = new DZS_CatalogEsc();
		m_EscHand.m_P = this;
		m_EscHand.m_What = "hand";
		m_Kernel.GetScreen().AddEscHandler(m_EscHand, 60);
		m_Store.Start();
		m_Templates.LoadAll();
		DZS_Log.Info("catalog: session start, " + m_Favs.Count() + " favourite(s), " + m_Recent.Count() + " recent, " + m_Store.m_Templates.Count() + " template(s)");
	}

	override void OnSessionEnd()
	{
		if (m_Previews)
			m_Previews.Clear();
		if (m_Full)
			m_Full.Destroy();
	}

	override void OnUpdate(float dt)
	{
		if (!m_Session)
			return;
		m_Previews.NewFrame();
		if (!m_Store.IsReady())
			m_Store.Step();
		ApplyNames();
		if (m_Store.IsReady() && !m_WasReady)
		{
			m_WasReady = true;
			ForgetMissing();
			m_Templates.CountMissing();
			m_Dirty = true;
		}
		if (!m_Panel.m_Search)
			return;
		string q = m_Panel.m_Search.Query();
		if (m_Full && m_Full.IsOpen())
			q = m_Full.Query();
		if (q != m_Query)
		{
			m_Query = q;
			m_Dirty = true;
			m_Panel.ResetScroll();
		}
		if (m_Dirty && m_Store.IsReady())
			Recompute();
		DZS_PanelInfo info = m_Kernel.GetUi().GetPanel("catalog.bar");
		if (m_Store.IsReady())
			info.m_Meta = DZS_Texts.Fmt("catalog.count", m_Store.CountObjects().ToString());
		else
			info.m_Meta = "…";
		UpdateHandHint();
		if (m_Full)
			m_Full.Update(dt);
	}

	// ---- list of the bar ----

	array<DZS_CatEntry> Scope(string cat)
	{
		array<DZS_CatEntry> r = new array<DZS_CatEntry>();
		if (cat == "recent")
		{
			foreach (string k : m_Recent)
			{
				DZS_CatEntry re = m_Store.Find(k);
				if (re)
					r.Insert(re);
			}
			return r;
		}
		foreach (DZS_CatEntry e : m_Store.m_All)
		{
			if (cat == "all" || (cat == "favorites" && m_Favs.Contains(e.m_Low)) || e.m_Cat == cat)
				r.Insert(e);
		}
		return r;
	}

	protected void Recompute()
	{
		int t0 = TickCount(0);
		m_List = m_Store.Search(m_Query, Scope(m_Cat), m_Cat == "recent");
		m_LastSearchMs = (TickCount(0) - t0) / 10000.0;
		m_Dirty = false;
		DZS_Log.Info("catalog: search '" + m_Query + "' in " + m_Cat + ": " + m_List.m_Items.Count() + " hit(s) in " + m_LastSearchMs + " ms");
	}

	int ListCount()
	{
		if (!m_Store.IsReady() || !m_List)
			return 0;
		return m_List.m_Items.Count();
	}

	DZS_CatEntry ListAt(int i) { return m_List.m_Items[i]; }

	array<DZS_CatEntry> ListItems()
	{
		if (!m_List)
			return new array<DZS_CatEntry>();
		return m_List.m_Items;
	}

	void EmptyState(out string icon, out string title, out string text, out string b1, out string b2, out bool loading)
	{
		if (!m_Store.IsReady())
		{
			icon = "search";
			title = DZS_Texts.Get("catalog.loading.title");
			text = DZS_Texts.Get("catalog.loading.text");
			loading = true;
			return;
		}
		if (m_Cat == "favorites" && m_Query == "")
		{
			icon = "star";
			title = DZS_Texts.Get("catalog.nofav.title");
			text = DZS_Texts.Get("catalog.nofav.text");
			return;
		}
		icon = "search";
		title = DZS_Texts.Get("catalog.none.title");
		text = DZS_Texts.Fmt("catalog.none.text", m_Query);
		b1 = DZS_Texts.Get("catalog.none.clear");
		if (m_Cat != "all")
			b2 = DZS_Texts.Get("catalog.none.all");
	}

	array<string> ChipKeys()
	{
		array<string> r = {"all", "favorites", "buildings", "military", "walls", "furniture", "wrecks", "items", "lights", "plants", "rocks", "spawns", "other"};
		if (m_Store.m_Templates.Count() > 0)
			r.Insert("templates");
		return r;
	}

	static array<string> Categories()
	{
		array<string> r = {"buildings", "military", "walls", "furniture", "wrecks", "items", "lights", "plants", "rocks", "spawns", "other", "templates"};
		return r;
	}

	string CategoryName(string key)
	{
		return DZS_Texts.Get("catalog.cat." + key);
	}

	static string CategoryIcon(string key)
	{
		if (key == "buildings") return "house";
		if (key == "military") return "shield";
		if (key == "walls") return "brick-wall";
		if (key == "furniture") return "armchair";
		if (key == "wrecks") return "car";
		if (key == "items") return "package";
		if (key == "lights") return "lamp";
		if (key == "plants") return "tree-pine";
		if (key == "rocks") return "mountain";
		if (key == "spawns") return "car";
		if (key == "templates") return "layout-template";
		return "shapes";
	}

	// Second line of a tile: the category, for templates the number of objects.
	string SubText(DZS_CatEntry e)
	{
		if (e.m_IsTemplate)
			return ObjectsText(e.m_TplCount);
		if (m_Store.NameCount(e.m_Name) > 1)
			return e.m_Key;
		return CategoryName(e.m_Cat);
	}

	static string ObjectsText(int n)
	{
		if (n == 1)
			return DZS_Texts.Get("catalog.objects.one");
		return DZS_Texts.Fmt("catalog.objects", n.ToString());
	}

	void SetCategory(string key)
	{
		if (key == m_Cat)
			return;
		m_Cat = key;
		m_Dirty = true;
		m_Panel.ResetScroll();
		DZS_Log.Info("catalog: category " + key);
	}

	void ClearSearch()
	{
		m_Panel.m_Search.OnClear(null);
	}

	// ---- favourites: kept at once, for every project ----

	bool IsFavourite(DZS_CatEntry e) { return e && m_Favs.Contains(e.m_Low); }

	void ToggleFavourite(DZS_CatEntry e)
	{
		if (!e)
			return;
		if (m_Favs.Contains(e.m_Low))
			m_Favs.Remove(e.m_Low);
		else
			m_Favs.Set(e.m_Low, true);
		SaveFavourites();
		DZS_Log.Info("catalog: favourite " + e.m_Key + " " + DZS_Str.Bool(m_Favs.Contains(e.m_Low)));
	}

	protected void SaveFavourites()
	{
		DZS_Json a = DZS_Json.Arr();
		foreach (string k, bool on : m_Favs)
			a.Add(DZS_Json.Str(k));
		m_Kernel.GetSettings().Set("catalog.favourites", a);
	}

	// A favourite or recent entry whose entry is gone is forgotten silently.
	protected void ForgetMissing()
	{
		if (!m_Store.IsReady())
			return;
		array<string> gone = new array<string>();
		foreach (string k, bool on : m_Favs)
		{
			if (!m_Store.Find(k))
				gone.Insert(k);
		}
		foreach (string g : gone)
			m_Favs.Remove(g);
		if (gone.Count() > 0)
			SaveFavourites();
		for (int i = m_Recent.Count() - 1; i >= 0; i--)
		{
			if (!m_Store.Find(m_Recent[i]))
				m_Recent.Remove(i);
		}
		SaveRecent();
	}

	// ---- recently used ----

	void NoteUsed(string key)
	{
		DZS_CatEntry e = m_Store.Find(key);
		if (!e)
			return;
		int at = m_Recent.Find(e.m_Low);
		if (at >= 0)
			m_Recent.RemoveOrdered(at);
		m_Recent.InsertAt(e.m_Low, 0);
		while (m_Recent.Count() > RECENT_MAX)
			m_Recent.Remove(m_Recent.Count() - 1);
		SaveRecent();
		if (m_Cat == "recent")
			m_Dirty = true;
	}

	protected void SaveRecent()
	{
		DZS_Json a = DZS_Json.Arr();
		foreach (string k : m_Recent)
			a.Add(DZS_Json.Str(k));
		m_Kernel.GetSettings().Set("catalog.recent", a);
	}

	array<DZS_CatEntry> RecentEntries(int max)
	{
		array<DZS_CatEntry> r = new array<DZS_CatEntry>();
		if (!m_Store.IsReady())
			return r;
		foreach (string k : m_Recent)
		{
			DZS_CatEntry e = m_Store.Find(k);
			if (e)
				r.Insert(e);
			if (r.Count() >= max)
				break;
		}
		return r;
	}

	int RecentCount() { return RecentEntries(RECENT_MAX).Count(); }

	// ---- hand ----

	bool IsInHand(DZS_CatEntry e) { return e && e == m_Hand; }

	// While swapping templates cannot be chosen.
	bool IsOffNow(DZS_CatEntry e)
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		return e && e.m_IsTemplate && sel && sel.IsSwapping();
	}

	void TakeInHand(DZS_CatEntry e)
	{
		if (!e)
			return;
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel && sel.IsSwapping() && !e.m_IsTemplate)
		{
			sel.SwapTo(e.m_Key);
			return;
		}
		m_Hand = e;
		m_Kernel.GetUi().SetSceneState("hand");
		DZS_Log.Info("catalog: in hand " + e.m_Key);
		m_Kernel.GetEvents().FirePluginEvent("catalog.hand", null);
		int idx = ListItems().Find(e);
		if (idx >= 0)
			m_Panel.ScrollToIndex(idx);
	}

	void DropHand()
	{
		if (!m_Hand)
			return;
		DZS_Log.Info("catalog: hand dropped (" + m_Hand.m_Key + ")");
		m_Hand = null;
		m_Kernel.GetUi().SetSceneState("none");
		m_Kernel.GetEvents().FirePluginEvent("catalog.hand", null);
	}

	protected void UpdateHandHint()
	{
		if (m_Hand)
			m_Kernel.GetScreen().SetHint("catalog", 10, DZS_Texts.Fmt("catalog.hint.hand", DZS_W.Literal(m_Hand.m_Name)));
		else
			m_Kernel.GetScreen().SetHint("catalog", 10, "");
	}

	// ↓/PgDn and ↑/PgUp with an entry in the hand.
	void StepHand(int dir)
	{
		array<DZS_CatEntry> list = ListItems();
		if (!m_Hand || list.Count() == 0)
			return;
		int i = list.Find(m_Hand);
		int next;
		if (i < 0)
		{
			if (dir > 0)
				next = 0;
			else
				next = list.Count() - 1;
		}
		else
			next = i + dir;
		if (next < 0 || next >= list.Count())
			return;
		TakeInHand(list[next]);
	}

	// ---- tiles ----

	void OnTileClick(DZS_CatTile t, int button, string where)
	{
		if (!t.m_Entry)
			return;
		if (button == MouseState.RIGHT)
		{
			OpenContext(t.m_Entry);
			return;
		}
		if (where == "full")
		{
			m_Full.Mark(t.m_Entry);
			return;
		}
		TakeInHand(t.m_Entry);
	}

	void OnTileDrag(DZS_CatTile t)
	{
		if (t.m_Entry)
			TakeInHand(t.m_Entry);
	}

	// ---- photos of image sources: DDS or TGA only ----

	string PhotoOf(DZS_CatEntry e)
	{
		if (!e || e.m_IsTemplate || m_Sources.Count() == 0)
			return "";
		string p;
		if (m_PhotoCache.Find(e.m_Low, p))
			return p;
		p = "";
		foreach (DZS_ImageSource s : m_Sources)
		{
			string got = s.GetPhoto(e.m_Key);
			string low = got;
			low.ToLower();
			int dot = low.LastIndexOf(".");
			string ext = "";
			if (dot >= 0)
				ext = low.Substring(dot, low.Length() - dot);
			if (got != "" && (ext == ".dds" || ext == ".tga" || ext == ".edds") && FileExist(got))
			{
				p = got;
				break;
			}
		}
		m_PhotoCache.Set(e.m_Low, p);
		return p;
	}

	bool PhotoPending(DZS_CatEntry e)
	{
		if (!e || e.m_IsTemplate)
			return false;
		foreach (DZS_ImageSource s : m_Sources)
		{
			if (s.PhotoPending(e.m_Key))
				return true;
		}
		return false;
	}

	bool PhotoProgress(out int done, out int total)
	{
		done = 0;
		total = 0;
		bool busy = false;
		foreach (DZS_ImageSource s : m_Sources)
		{
			int d, t;
			if (!s.PhotoProgress(d, t))
				continue;
			busy = true;
			done += d;
			total += t;
		}
		return busy;
	}

	string DescriptionOf(DZS_CatEntry e)
	{
		if (!e || e.m_IsTemplate)
			return "";
		foreach (DZS_ImageSource s : m_Sources)
		{
			string d = s.GetDescription(e.m_Key);
			if (d != "")
				return d;
		}
		return "";
	}

	void ForgetPhotos()
	{
		m_PhotoCache.Clear();
	}

	// ---- context menu ----

	void OpenContext(DZS_CatEntry e)
	{
		m_CtxEntry = e;
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		DZS_MenuItem t614 = Item("catalog.ctx.hand", DZS_Texts.Get("catalog.ctx.hand"), "hand");
		items.Insert(t614);
		if (IsFavourite(e))
		{
			DZS_MenuItem t616 = Item("catalog.ctx.fav", DZS_Texts.Get("catalog.ctx.unfav"), "star");
			items.Insert(t616);
		}
		else
		{
			DZS_MenuItem t618 = Item("catalog.ctx.fav", DZS_Texts.Get("catalog.ctx.fav"), "star");
			items.Insert(t618);
		}
		if (e.m_IsTemplate)
		{
			DZS_MenuItem t621 = Item("catalog.ctx.rename", DZS_Texts.Get("catalog.ctx.rename"), "");
			items.Insert(t621);
			DZS_MenuItem t622 = Item("catalog.ctx.delete", DZS_Texts.Get("catalog.ctx.delete"), "trash");
			items.Insert(t622);
		}
		else
		{
			DZS_MenuItem t626 = Item("catalog.ctx.copy", DZS_Texts.Get("catalog.ctx.copy"), "copy");
			items.Insert(t626);
			foreach (DZS_CatalogMenuSource src : m_MenuSources)
				src.AddItems(e.m_Key, items);
		}
		DZS_Popup pop = m_Kernel.GetScreen().OpenContextMenu(items);
		pop.m_OnChoose.Insert(OnContextChoose);
	}

	void OnContextChoose(DZS_Popup p, DZS_MenuItem it)
	{
		m_Kernel.GetScreen().CloseMenu();
		if (m_CtxEntry && !m_CtxEntry.m_IsTemplate)
		{
			foreach (DZS_CatalogMenuSource src : m_MenuSources)
			{
				if (src.OnChoose(it.m_ActionId, m_CtxEntry.m_Key))
					return;
			}
		}
		OnAction(it.m_ActionId);
	}

	protected DZS_MenuItem Item(string action, string text, string icon)
	{
		DZS_MenuItem it = new DZS_MenuItem();
		it.m_ActionId = action;
		it.m_Text = text;
		it.m_Icon = icon;
		return it;
	}

	// Save as template: the selected objects as the project holds them.
	protected void SaveSelectionAsTemplate()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		DZS_Instance inst = m_Kernel.GetShown();
		if (!sel || !inst)
			return;
		array<string> ids = sel.GetSelection();
		array<ref DZS_TemplateObject> objs = new array<ref DZS_TemplateObject>();
		foreach (string id : ids)
		{
			DZS_TemplateObject o = new DZS_TemplateObject();
			string name, pos, ypr, scale, persist, custom;
			inst.GetField(id, DZS_Obj.NAME, name);
			inst.GetField(id, DZS_Obj.POS, pos);
			inst.GetField(id, DZS_Obj.YPR, ypr);
			inst.GetField(id, DZS_Obj.SCALE, scale);
			inst.GetField(id, DZS_Obj.PERSIST, persist);
			inst.GetField(id, DZS_Obj.CUSTOM, custom);
			o.m_Name = name;
			o.m_Pos = DZS_Obj.QuickVec(pos);
			o.m_Ypr = DZS_Obj.QuickVec(ypr);
			o.m_Scale = 1;
			if (scale != "")
				o.m_Scale = scale.ToFloat();
			o.m_Persist = persist == "1";
			o.m_Custom = custom;
			objs.Insert(o);
		}
		if (objs.Count() == 0)
			return;
		string lead = sel.GetLead();
		string leadName;
		inst.GetField(lead, DZS_Obj.NAME, leadName);
		DZS_CatEntry le = m_Store.Find(leadName);
		string suggestion = leadName;
		if (le)
			suggestion = le.m_Name;
		m_Templates.AskSave(objs, suggestion);
	}

	// ---- actions ----

	bool ActionEnabled(string id, out string de, out string en)
	{
		if (id == "catalog.saveTemplate")
		{
			DZS_SelectionApi sel = DZS_SelectionApi.Get();
			if (sel && sel.GetSelection().Count() > 0)
				return true;
			de = "Erst Objekte auswählen";
			en = "Select objects first";
			return false;
		}
		return true;
	}

	void OnAction(string id)
	{
		if (id == "catalog.saveTemplate")
		{
			SaveSelectionAsTemplate();
			return;
		}
		if (id == "catalog.search")
		{
			if (m_Kernel.GetScreen().IsBarCollapsed("left"))
				m_Kernel.GetScreen().SetBarCollapsed("left", false);
			m_Panel.m_Search.m_KeepOnFocus = true;
			m_Kernel.GetScreen().FocusField(m_Panel.m_Search);
			return;
		}
		if (id == "catalog.full")
		{
			if (m_Full && m_Full.IsOpen())
				CloseFull(false);
			else
				OpenFull();
			return;
		}
		if (id == "catalog.next")
		{
			StepHand(1);
			return;
		}
		if (id == "catalog.prev")
		{
			StepHand(-1);
			return;
		}
		DZS_CatEntry e = m_CtxEntry;
		if (!e)
			return;
		if (id == "catalog.ctx.hand")
		{
			if (FullOpen())
				m_Full.PlaceEntry(e);
			else
				TakeInHand(e);
		}
		else if (id == "catalog.ctx.fav")
			ToggleFavourite(e);
		else if (id == "catalog.ctx.copy")
		{
			GetGame().CopyToClipboard(e.m_Key);
			m_Kernel.GetScreen().ShowMessage("catalog.copy", "success", DZS_Texts.Get("catalog.copied"), 2.0);
			DZS_Log.Info("catalog: class name copied " + e.m_Key);
		}
		else if (id == "catalog.ctx.rename")
			m_Templates.AskRename(e);
		else if (id == "catalog.ctx.delete")
			m_Templates.AskDelete(e);
	}

	// ---- full-screen search ----

	void OpenFull()
	{
		if (!m_Full)
			m_Full = new DZS_CatalogFull(this);
		if (m_Full.IsOpen())
			return;
		if (!m_Panel.m_Search)
			return;
		string q = m_Panel.m_Search.Query();
		if (m_Kernel.GetScreen().IsFieldFocused())
			m_Kernel.GetScreen().Unfocus(false);
		m_Full.Open(q);
	}

	void CloseFull(bool placed)
	{
		if (!m_Full || !m_Full.IsOpen())
			return;
		string q = m_Full.Query();
		m_Full.Close();
		m_Panel.m_Search.SetText(q);
		m_Dirty = true;
	}

	void TemplatesChanged()
	{
		m_Store.Reorder();
		m_Dirty = true;
		ForgetMissing();
	}

	void MarkDirty() { m_Dirty = true; }

	protected bool m_NamesPending;

	void SourcesChanged()
	{
		ForgetPhotos();
		m_NamesPending = true;
		ApplyNames();
	}

	protected void ApplyNames()
	{
		if (!m_NamesPending || !m_Store.IsReady())
			return;
		m_NamesPending = false;
		int changed = m_Store.ApplySourceNames(m_Sources);
		m_Store.Reorder();
		m_Dirty = true;
		if (m_Full)
			m_Full.MarkDirty();
		DZS_Log.Info("catalog: names from image sources applied, " + changed + " changed");
	}

	bool FullOpen() { return m_Full && m_Full.IsOpen(); }

	protected int m_ProjectStamp;

	int ProjectStamp() { return m_ProjectStamp; }

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op) { m_ProjectStamp++; }

	override void OnShownInstanceChanged(DZS_Instance inst) { m_ProjectStamp++; }

	// ---- language table ----

	protected void RegisterTexts()
	{
		DZS_Texts.Add("catalog.count", "%1 spawnbar", "%1 spawnable");
		DZS_Texts.Add("catalog.search.placeholder", "Name oder Klassenname …", "Name or class name …");
		DZS_Texts.Add("catalog.recent.empty", "Hier erscheint, was du zuletzt platziert hast", "What you placed last shows here");
		DZS_Texts.Add("catalog.full.open", "Alle Objekte im Vollbild", "All objects full screen");
		DZS_Texts.Add("catalog.inHand", "in der Hand", "in hand");
		DZS_Texts.Add("catalog.photos.loading", "Bilder laden", "Loading pictures");
		DZS_Texts.Add("catalog.photos.list", "Bildkatalog laden", "Loading picture catalogue");
		DZS_Texts.Add("catalog.photos.done", "Bilder geladen", "Pictures loaded");
		DZS_Texts.Add("catalog.photos.count", "%1 / %2", "%1 / %2");
		DZS_Texts.Add("catalog.fav.tip", "Favorit", "Favourite");
		DZS_Texts.Add("catalog.swap.noTemplate", "Austauschen geht nur mit einzelnen Objekten", "Swapping works with single objects only");
		DZS_Texts.Add("catalog.cat.all", "Alle", "All");
		DZS_Texts.Add("catalog.cat.favorites", "Favoriten", "Favourites");
		DZS_Texts.Add("catalog.cat.recent", "Zuletzt benutzt", "Recently used");
		DZS_Texts.Add("catalog.cat.buildings", "Gebäude", "Buildings");
		DZS_Texts.Add("catalog.cat.military", "Militär", "Military");
		DZS_Texts.Add("catalog.cat.walls", "Mauern und Zäune", "Walls and fences");
		DZS_Texts.Add("catalog.cat.furniture", "Einrichtung", "Furnishing");
		DZS_Texts.Add("catalog.cat.wrecks", "Wracks und Trümmer", "Wrecks and debris");
		DZS_Texts.Add("catalog.cat.items", "Gegenstände", "Items");
		DZS_Texts.Add("catalog.cat.lights", "Licht", "Light");
		DZS_Texts.Add("catalog.cat.plants", "Pflanzen", "Plants");
		DZS_Texts.Add("catalog.cat.rocks", "Felsen", "Rocks");
		DZS_Texts.Add("catalog.cat.spawns", "Fahrzeuge und KI", "Vehicles and AI");
		DZS_Texts.Add("catalog.cat.other", "Sonstiges", "Other");
		DZS_Texts.Add("catalog.cat.templates", "Vorlagen", "Templates");
		DZS_Texts.Add("catalog.objects", "%1 Objekte", "%1 objects");
		DZS_Texts.Add("catalog.objects.one", "1 Objekt", "1 object");
		DZS_Texts.Add("catalog.loading.title", "Katalog wird geladen …", "Loading catalogue …");
		DZS_Texts.Add("catalog.loading.text", "Gleich da — der Editor prüft, was dieses Spiel setzen kann.", "Almost there — the editor checks what this game can place.");
		DZS_Texts.Add("catalog.none.title", "Nichts gefunden", "Nothing found");
		DZS_Texts.Add("catalog.none.text", "Für „%1“ gibt es kein Objekt, das auf Konsole spawnt. Tippfehler verzeihen wir — vielleicht heißt es im Spiel anders.", "There is no object for “%1” that spawns on console. We forgive typos — maybe the game calls it differently.");
		DZS_Texts.Add("catalog.none.clear", "Suche leeren", "Clear search");
		DZS_Texts.Add("catalog.none.all", "In allen Kategorien suchen", "Search all categories");
		DZS_Texts.Add("catalog.nofav.title", "Noch keine Favoriten", "No favourites yet");
		DZS_Texts.Add("catalog.nofav.text", "Setz den Stern an einem Objekt, dann steht es hier.", "Set the star on an object and it shows here.");
		DZS_Texts.Add("catalog.hint.hand", "In die Szene zeigen, um %1 zu platzieren · [Esc] abbrechen", "Point into the scene to place %1 · [Esc] cancel");
		DZS_Texts.Add("catalog.ctx.hand", "In die Hand nehmen", "Take in hand");
		DZS_Texts.Add("catalog.ctx.fav", "Zu Favoriten", "Add to favourites");
		DZS_Texts.Add("catalog.ctx.unfav", "Aus Favoriten entfernen", "Remove from favourites");
		DZS_Texts.Add("catalog.ctx.copy", "Klassenname kopieren", "Copy class name");
		DZS_Texts.Add("catalog.ctx.rename", "Umbenennen …", "Rename …");
		DZS_Texts.Add("catalog.ctx.delete", "Löschen …", "Delete …");
		DZS_Texts.Add("catalog.copied", "Klassenname kopiert", "Class name copied");
		DZS_Texts.Add("catalog.full.hits", "%1 Treffer", "%1 hits");
		DZS_Texts.Add("catalog.full.esc", "[Esc] schließen", "[Esc] close");
		DZS_Texts.Add("catalog.full.range", "// Bereich", "// Scope");
		DZS_Texts.Add("catalog.full.kind", "// Art", "// Kind");
		DZS_Texts.Add("catalog.full.place", "Platzieren", "Place");
		DZS_Texts.Add("catalog.full.hint", "Pfeile wählen · [Enter] platzieren · [Esc] zurück", "Arrows choose · [Enter] place · [Esc] back");
		DZS_Texts.Add("catalog.fact.size", "Maße", "Size");
		DZS_Texts.Add("catalog.fact.console", "Konsole", "Console");
		DZS_Texts.Add("catalog.fact.project", "Im Projekt", "In the project");
		DZS_Texts.Add("catalog.fact.content", "Inhalt", "Content");
		DZS_Texts.Add("catalog.fact.spawns", "spawnt ✓", "spawns ✓");
		DZS_Texts.Add("catalog.fact.light", "spawnt ✓ · leuchtet: ungeprüft", "spawns ✓ · lights: unchecked");
		DZS_Texts.Add("catalog.fact.unused", "nicht verwendet", "not used");
		DZS_Texts.Add("catalog.fact.inGroup", "%1 × in %2", "%1 × in %2");
		DZS_Texts.Add("catalog.fact.moreGroups", "+ %1 Gruppen", "+ %1 groups");
		DZS_Texts.Add("catalog.fact.allSpawn", "alle %1 Objekte spawnen ✓", "all %1 objects spawn ✓");
		DZS_Texts.Add("catalog.fact.notInCatalog", "%1 von %2 nicht mehr im Katalog", "%1 of %2 no longer in the catalogue");
		DZS_Texts.Add("catalog.fact.kinds", "%1 Objekte, %2 Arten", "%1 objects, %2 kinds");
		DZS_Texts.Add("catalog.tpl.kicker", "Vorlage", "Template");
		DZS_Texts.Add("catalog.tpl.save", "Als Vorlage speichern", "Save as template");
		DZS_Texts.Add("catalog.tpl.saveBtn", "Speichern", "Save");
		DZS_Texts.Add("catalog.tpl.rename", "Vorlage umbenennen", "Rename template");
		DZS_Texts.Add("catalog.tpl.renameBtn", "Umbenennen", "Rename");
		DZS_Texts.Add("catalog.tpl.name", "Name", "Name");
		DZS_Texts.Add("catalog.tpl.noName", "Name fehlt", "Name missing");
		DZS_Texts.Add("catalog.tpl.taken", "Name schon vergeben", "Name already taken");
		DZS_Texts.Add("catalog.tpl.notIn", "%1 Objekte stehen nicht im Katalog und kommen nicht in die Vorlage.", "%1 objects are not in the catalogue and do not go into the template.");
		DZS_Texts.Add("catalog.tpl.none", "Keines der Objekte steht im Katalog", "None of the objects is in the catalogue");
		DZS_Texts.Add("catalog.tpl.tooMany", "Eine Vorlage fasst höchstens 2000 Objekte.", "A template holds at most 2000 objects.");
		DZS_Texts.Add("catalog.tpl.saved", "Vorlage %1 gespeichert", "Template %1 saved");
		DZS_Texts.Add("catalog.tpl.deleteTitle", "Löschen", "Delete");
		DZS_Texts.Add("catalog.tpl.deleteAsk", "Vorlage %1 löschen? Das lässt sich nicht rückgängig machen.", "Delete template %1? This cannot be undone.");
		DZS_Texts.Add("catalog.tpl.missing", "%1 von %2 Objekten stehen nicht mehr im Katalog und werden ausgelassen", "%1 of %2 objects are no longer in the catalogue and are left out");
	}
}
