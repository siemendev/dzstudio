// Public interface of the catalog plugin. The catalog offers it as the service "catalog";
// other plugins ask DZS_CatalogApi.Get() and never touch the plugin's classes.

// A plugin that has photos and descriptions of objects. The photo is a file in the
// profile folder in a format the game shows (DDS or TGA); "" means none.
class DZS_ImageSource : Managed
{
	string GetPhoto(string name) { return ""; }
	string GetDescription(string name) { return ""; }
	// Name in the game's language; "" if the source has none in that language.
	string GetName(string name, bool german) { return ""; }
}

// One object of a template: values as the editor keeps them.
class DZS_TemplateObject : Managed
{
	string m_Name;
	vector m_Pos;
	vector m_Ypr;
	float m_Scale = 1;
	bool m_Persist;
	string m_Custom;
}

// A plugin that adds entries to the context menu of a catalog object (not a template), e.g. the brush.
// AddItems gets the entry's class or model path; OnChoose returns true if the action was its own.
class DZS_CatalogMenuSource : Managed
{
	void AddItems(string entry, array<ref DZS_MenuItem> items) {}
	bool OnChoose(string actionId, string entry) { return false; }
}

class DZS_CatalogApi : Managed
{
	static DZS_CatalogApi Get()
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return null;
		return DZS_CatalogApi.Cast(k.GetService("catalog"));
	}

	// ---- class directory: every class of the running game, case-insensitive ----
	// Spelling of the game, or "" if the game does not know the class.
	string Spelling(string name) { return ""; }
	// Whether the game can load a model path.
	bool ModelLoadable(string path) { return false; }
	// Whether the catalog offers this class or model path (case-insensitive).
	bool InCatalog(string name) { return false; }
	// Name shown in the catalog (game name, list name or readable class name).
	string ShownName(string name) { return name; }
	// Whether another object of the catalog carries the same Spielname.
	bool NameShared(string name) { return false; }
	// Symbol of the catalog category of a name (object tile without a picture).
	string IconOf(string name) { return "package"; }
	bool IsReady() { return false; }

	// ---- hand: the next click into the scene places this entry (a class, a model path or "tpl:<id>") ----
	string GetHand() { return ""; }
	void TakeInHand(string entry) {}
	void DropHand() {}
	// Placing counts for "Zuletzt benutzt" (from the hand, not duplicating, pasting or brushes).
	void NoteUsed(string entry) {}

	// ---- templates ----
	// Saves objects as a template; returns its id or "" (name taken, nothing in the catalog).
	string SaveTemplate(string name, array<ref DZS_TemplateObject> objects) { return ""; }
	array<ref DZS_TemplateObject> TemplateObjects(string id) { return null; }

	void AddImageSource(DZS_ImageSource source) {}
	// A source has new photos, names or descriptions: the catalog asks again.
	void SourcesChanged() {}
	// Only photos changed (cheaper than SourcesChanged).
	void PhotosChanged() {}
	void AddMenuSource(DZS_CatalogMenuSource source) {}
}
