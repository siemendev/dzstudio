// Files and folders that versions up to 0.5 created under German names. Run() moves them to their
// current names once per game start: copy, then delete the original; a target that already exists is never
// overwritten (the original stays where it is).
class DZS_LegacyNames
{
	static const string PROJECT_FILE = "projekt.dzstudio.json";
	static const string BACKUP_HEAD = "sicherung.dzstudio";
	static const string VANILLA_SOURCE = "quelle.json";

	protected static bool s_Done;
	protected static int s_Moved;
	protected static int s_Kept;

	static void Run()
	{
		if (s_Done)
			return;
		s_Done = true;
		s_Moved = 0;
		s_Kept = 0;
		string p = "$profile:dzstudio\\";
		MoveFile(p + "einstellungen.json", p + "settings.json");
		MoveFile(p + "einstellungen.neu.json", p + "settings.new.json");
		MoveFile(p + "einstellungen.defekt.json", p + "settings.broken.json");
		MoveFile(p + "konsolenliste.txt", p + "console-list.txt");
		MoveFile(p + "pinsel-sets.txt", p + "brush-sets.txt");
		MoveFolder(p + "vorlagen", p + "templates");
		MoveFolder(p + "ausruestung", p + "loadouts");
		MoveFolder(p + "pinsel", p + "brushes");
		MoveFolder(p + "sicherung", p + "backups");
		MoveFolder("$saves:dzstudio\\Projekte", "$saves:dzstudio\\Projects");

		RenameInFolders(p + "backups", PROJECT_FILE, "project.dzstudio.json");
		RenameInFolders(p + "backups", BACKUP_HEAD, "backup.dzstudio");
		RenameInFolders(p + "vanilla", VANILLA_SOURCE, "source.json");

		if (s_Moved > 0 || s_Kept > 0)
			DZS_Log.Info("legacy names: " + s_Moved + " moved or dropped, " + s_Kept + " kept because the new name exists or copying failed");
	}

	// Moves one file; false if it stays where it is.
	static bool MoveFile(string from, string to)
	{
		if (!FileExist(from) || DZS_Files.IsFolder(from))
			return false;
		if (FileExist(to))
		{
			DZS_Log.Warn("legacy names: " + to + " exists, " + from + " kept");
			s_Kept++;
			return false;
		}
		if (!CopyFile(from, to) || !FileExist(to))
		{
			DZS_Log.Error("legacy names: copying " + from + " to " + to + " failed, kept");
			s_Kept++;
			return false;
		}
		DeleteFile(from);
		DZS_Log.Info("legacy names: " + from + " -> " + to);
		s_Moved++;
		return true;
	}

	// Moves a folder file by file into the new one (merging if that exists); the old folder goes once it is empty.
	static void MoveFolder(string from, string to)
	{
		if (!DZS_Files.IsFolder(from))
			return;
		DZS_Files.MakeDirs(to);
		array<string> names;
		array<bool> folders;
		DZS_Files.List(from, names, folders);
		for (int i = 0; i < names.Count(); i++)
		{
			if (folders[i])
				MoveFolder(from + "\\" + names[i], to + "\\" + names[i]);
			else
				MoveFile(from + "\\" + names[i], to + "\\" + names[i]);
		}
		array<string> left;
		array<bool> leftFolders;
		DZS_Files.List(from, left, leftFolders);
		if (left.Count() == 0)
		{
			DeleteFile(from);
			DZS_Log.Info("legacy names: folder " + from + " -> " + to);
		}
	}

	// In every direct subfolder of root: rel 'from' -> rel 'to'.
	static void RenameInFolders(string root, string from, string to)
	{
		if (!DZS_Files.IsFolder(root))
			return;
		array<string> names;
		array<bool> folders;
		DZS_Files.List(root, names, folders);
		for (int i = 0; i < names.Count(); i++)
		{
			if (folders[i])
				MoveFile(root + "\\" + names[i] + "\\" + from, root + "\\" + names[i] + "\\" + to);
		}
	}

	// Also for plugins: an old file or folder that is only a cache and is made again.
	static void Drop(string path)
	{
		if (FileExist(path) && DeleteFile(path))
		{
			DZS_Log.Info("legacy names: " + path + " deleted");
			s_Moved++;
		}
	}

	static void DropFolder(string path)
	{
		if (!DZS_Files.IsFolder(path))
			return;
		DZS_Files.DeleteTree(path);
		DZS_Log.Info("legacy names: " + path + " deleted");
		s_Moved++;
	}
}
