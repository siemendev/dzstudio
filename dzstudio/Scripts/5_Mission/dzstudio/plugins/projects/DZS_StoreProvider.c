// Loot in a provider project: the provider keeps the Ausgangsstand
// and knows which entries and rows are the project's; the working state lies in the mirror folder as usual.
class DZS_LootOrigin : Managed
{
	// rows of the project: "name" + "\n" + "pos", both as raw attribute text
	ref map<string, bool> m_OwnRows = new map<string, bool>();
	// entries of the project (type as in the file) and their text in the Ausgangsstand ("" = none there)
	ref array<string> m_OwnTypes = new array<string>();
	ref array<string> m_Origs = new array<string>();
	// why loot cannot be switched on ("" = it can)
	string m_NoDe;
	string m_NoEn;
}

// A storage provider besides the local folder. Its projects are mirrored into a folder
// under the local root whose name starts with "@" (not listed as local projects); opening fetches them first, saving
// hands the planned files to the provider, which answers with DZS_ProjectsPlugin.ProviderSaved.
class DZS_StoreProvider : Managed
{
	// Providers register in their plugin's OnRegister, before any project opens (also after a world change).
	protected static ref array<ref DZS_StoreProvider> s_All;

	static array<ref DZS_StoreProvider> All()
	{
		if (!s_All)
			s_All = new array<ref DZS_StoreProvider>();
		return s_All;
	}

	static void Register(DZS_StoreProvider p)
	{
		All();
		for (int i = s_All.Count() - 1; i >= 0; i--)
		{
			if (s_All[i].Id() == p.Id())
				s_All.RemoveOrdered(i);
		}
		s_All.Insert(p);
	}

	string Id() { return ""; }
	bool Owns(string folder) { return false; }
	// Meta text before map and groups, e.g. "dzbot-Platzierung".
	string Label(string folder) { return ""; }
	// Fetch the current state into the folder, then call DZS_ProjectsPlugin.Fetched.
	void Fetch(string folder) {}
	void Save(string folder, array<ref DZS_SaveFile> files) {}
	// A save of this folder is still undecided (e.g. shown as failed, repeated in the background): a new one waits.
	bool SaveRunning(string folder) { return false; }
	bool AutoSaveOff(string folder, out string de, out string en) { return false; }
	void Opened(string folder) {}
	// What the provider needs to go on with a restored project (e.g. the base revisions), kept in the backup head;
	// Restored hands it back before Opened when the project is opened from the backup.
	DZS_Json BackupState(string folder) { return null; }
	void Restored(string folder, DZS_Json state) {}
	// The fetched project part was changed by the provider (e.g. a choice to keep in the manifest): the project opens
	// with 1 unsaved change; asked once per opening.
	bool PartChanged(string folder) { return false; }
	void Closed(string folder) {}
	// Tab in the project browser ("" = none) and its rows (m_Folder = the provider's cache folder).
	string TabLabel() { return ""; }
	void RefreshRows() {}
	bool RowsLoading() { return false; }
	string RowsEmptyText() { return ""; }
	void Rows(array<ref DZS_ProjectInfo> into) {}
	string RowMeta(DZS_ProjectInfo p) { return ""; }
	// Mark at the end of a row ("Sitzung"), and rows that cannot be opened with the reason.
	string RowMark(DZS_ProjectInfo p) { return ""; }
	bool RowOff(DZS_ProjectInfo p, out string de, out string en) { return false; }
	// Renaming and deleting are not offered for this tab: the reason.
	void NoRename(out string de, out string en)
	{
		de = "Umbenennen und Löschen bleiben im Web";
		en = "Renaming and deleting stay on the web";
	}
	string Address() { return ""; }
	// null = the provider keeps no loot (loot only in the local folder).
	DZS_LootOrigin LootOrigin(string folder) { return null; }
	// Groups cannot be deleted in this project (e.g. a dzbot placement has exactly one group); de/en = the reason.
	bool GroupsFixed(string folder, out string de, out string en) { return false; }
	// Hints for the check list when the fetched project opens.
	void OpenIssues(string folder, DZS_Issues into) {}
}
