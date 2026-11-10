// Blocks and changes of a project file that no part of the editor takes over when it opens — kinds it has no editor
// for (events without positions, items, globals, presets, restricted areas …, written by dzbot's workbench, its AI or
// another program), loot entries while the loot editor is off for the project, layer entries a layer would not write
// back as they are (DZS_LayerTake). They are carried over unchanged, extensions included, and written back at every
// save, so a save never drops what the editor cannot show.
//
// Opening puts them into the project's plugin entries under NAME (the working folder's project part, then the
// projects:plugins companion); every build of the project file appends them after the editor's own blocks and changes.
// They are taken anew from the file at every open, and never written into the editor extension's data.
class DZS_KeptBlocks
{
	static const string NAME = "kept";

	// {blocks, changes, slug?} of the root that the open leaves to nobody, null when there are none. part: the editor
	// part the open goes on with (its "loot" and "plugins").
	static DZS_Json Of(DZS_Json root, DZS_Json part)
	{
		bool lootOn = false;
		DZS_Json loot = part.Get("loot");
		if (loot && loot.IsObj())
			lootOn = loot.GetBool("enabled");
		map<string, bool> layerBlocks = new map<string, bool>();
		map<string, bool> layerChanges = new map<string, bool>();
		DZS_Json plugins = part.Get("plugins");
		if (plugins && plugins.IsObj())
		{
			for (int p = 0; p < plugins.m_Keys.Count(); p++)
			{
				if (plugins.m_Keys[p] == NAME)
					continue;
				DZS_Json frag = plugins.Get(plugins.m_Keys[p]);
				if (!frag || !frag.IsObj())
					continue;
				DZS_Json fb = frag.Get("blocks");
				if (fb && fb.IsArr())
				{
					for (int i = 0; i < fb.Count(); i++)
					{
						DZS_Json one = fb.At(i);
						if (one.IsObj())
							layerBlocks.Set(one.GetStr("id"), true);
					}
				}
				DZS_Json fc = frag.Get("changes");
				if (fc && fc.IsArr())
				{
					for (int k = 0; k < fc.Count(); k++)
					{
						DZS_Json two = fc.At(k);
						if (two.IsObj())
							layerChanges.Set(DZS_ProjectRefs.ChangeRef(two), true);
					}
				}
			}
		}
		DZS_Json blocks = DZS_Json.Arr();
		DZS_Json changes = DZS_Json.Arr();
		DZS_Json rb = root.Get("blocks");
		if (rb && rb.IsArr())
		{
			for (int b = 0; b < rb.Count(); b++)
			{
				DZS_Json block = rb.At(b);
				if (!block.IsObj())
					continue;
				string kind = block.GetStr("kind");
				if (kind == "spawnerGroup" || (lootOn && kind.IndexOf("loot") == 0) || layerBlocks.Contains(block.GetStr("id")))
					continue;
				blocks.Add(block);
			}
		}
		DZS_Json rc = root.Get("changes");
		if (rc && rc.IsArr())
		{
			for (int c = 0; c < rc.Count(); c++)
			{
				DZS_Json change = rc.At(c);
				if (!change.IsObj())
					continue;
				if ((lootOn && change.GetStr("block").IndexOf("loot") == 0) || layerChanges.Contains(DZS_ProjectRefs.ChangeRef(change)))
					continue;
				changes.Add(change);
			}
		}
		string slug = root.GetStr("slug");
		if (blocks.Count() == 0 && changes.Count() == 0 && slug == "")
			return null;
		DZS_Json kept = DZS_Json.Obj();
		kept.Set("blocks", blocks);
		kept.Set("changes", changes);
		if (slug != "")
		{
			DZS_Json sj = DZS_Json.Str(slug);
			kept.Set("slug", sj);
		}
		return kept;
	}

	// The part the open goes on with, with the kept entries of the root under plugins.kept (an older one replaced).
	static void Put(DZS_Json root, DZS_Json part)
	{
		DZS_Json plugins = part.Get("plugins");
		if (plugins && plugins.IsObj())
			plugins.Remove(NAME);
		DZS_Json kept = Of(root, part);
		if (!kept)
			return;
		if (!plugins || !plugins.IsObj())
		{
			plugins = DZS_Json.Obj();
			part.Set("plugins", plugins);
		}
		plugins.Set(NAME, kept);
		int nb = kept.Get("blocks").Count();
		int nc = kept.Get("changes").Count();
		if (nb + nc > 0)
			DZS_Log.Info("project open: " + nb + " block(s) and " + nc + " change(s) carried over unchanged");
	}

	// The kept entries of the open project, null when there are none.
	static DZS_Json From(DZS_Instance inst)
	{
		if (!inst)
			return null;
		int v;
		string entries;
		if (!inst.GetCompanion(DZS_PROJECT_ID, DZS_LocalStore.GROUP_PLUGINS, v, entries))
			return null;
		string text = DZS_ProjectSections.Stored(entries, NAME);
		if (text == "")
			return null;
		string err;
		DZS_Json kept = DZS_Json.ParseText(text, err);
		if (!kept || !kept.IsObj())
			return null;
		return kept;
	}

	// The kept blocks ('list' "blocks") or changes ("changes") of a kind; the layers show them read-only.
	static array<ref DZS_Json> Entries(DZS_Instance inst, string list, string kind)
	{
		array<ref DZS_Json> r = new array<ref DZS_Json>();
		DZS_Json kept = From(inst);
		if (!kept)
			return r;
		DZS_Json part = kept.Get(list);
		if (!part || !part.IsArr())
			return r;
		string member = "kind";
		if (list == "changes")
			member = "block";
		for (int i = 0; i < part.Count(); i++)
		{
			DZS_Json e = part.At(i);
			if (e.IsObj() && e.GetStr(member) == kind)
				r.Insert(e);
		}
		return r;
	}

	// A number of a kept entry ("" when it is none).
	static string Num(DZS_Json o, string key)
	{
		if (!o || !o.IsObj())
			return "";
		DZS_Json v = o.Get(key);
		if (!v || !v.IsNum())
			return "";
		return v.m_Str;
	}

	// The panels' two lines for a kept entry.
	static string ReadOnlyTitle()
	{
		return DZS_Texts.Pick("Außerhalb von dzStudio angelegt", "Made outside dzStudio");
	}

	static string ReadOnlyHint()
	{
		return DZS_Texts.Pick("Nur sichtbar · dort bearbeiten (dzbot)", "Shown only · edit it there (dzbot)");
	}

	// Whether a rule of a kept block or change names the setting (as a whole word: a condition's field, a formula, a
	// setting or list it reads). Renaming such a setting here would leave that rule on the old name.
	static bool Uses(DZS_Instance inst, string name)
	{
		DZS_Json kept = From(inst);
		if (!kept || name == "")
			return false;
		array<string> lists = {"blocks", "changes"};
		foreach (string ln : lists)
		{
			DZS_Json part = kept.Get(ln);
			if (!part)
				continue;
			string text = DZS_Files.Join(part.ToChunks());
			int from = 0;
			while (from <= text.Length() - name.Length())
			{
				int at = text.IndexOfFrom(from, name);
				if (at < 0)
					break;
				int end = at + name.Length();
				if ((at == 0 || !Word(text.Get(at - 1))) && (end >= text.Length() || !Word(text.Get(end))))
					return true;
				from = at + 1;
			}
		}
		return false;
	}

	protected static bool Word(string c)
	{
		int a = c.ToAscii();
		return (a >= 97 && a <= 122) || (a >= 65 && a <= 90) || (a >= 48 && a <= 57) || a == 95;
	}

	// The kept entries after the editor's own in the project file. keepSlug: the file's slug stays (a project saved to
	// a provider — dzbot, a hosted session: the files of its own blocks stay in dzstudio/<slug>/ on the server); a
	// local project's slug follows its name.
	static void AddTo(DZS_Json kept, DZS_Json root, bool keepSlug)
	{
		if (!kept)
			return;
		DZS_ProjectV2.AddFragment(kept, root.Get("blocks"), root.Get("changes"));
		string slug = kept.GetStr("slug");
		if (slug != "" && keepSlug)
		{
			DZS_Json sj = DZS_Json.Str(slug);
			root.Set("slug", sj);
		}
	}
}
