// Developer plugin: runs the kernel acceptance checks in the game when started with -dzsSelfTest,
// and leaves a small demo scene whose operations can then be undone and redone with the keys.
class DZS_SelfTestRelay : DZS_SyncSlot
{
	DZS_SyncSlot m_Peer;
	bool m_Relay;
	ref array<ref array<string>> m_Sent;
	ref array<ref DZS_SubmitResult> m_Rejected;

	void DZS_SelfTestRelay()
	{
		m_Sent = new array<ref array<string>>();
		m_Rejected = new array<ref DZS_SubmitResult>();
	}

	override void OnOperation(array<string> text, DZS_Origin origin, string opId)
	{
		array<string> copy = new array<string>();
		copy.Copy(text);
		m_Sent.Insert(copy);
		if (m_Relay && m_Peer)
			m_Peer.SubmitRemote(text);
	}

	override void OnRejected(DZS_SubmitResult result)
	{
		m_Rejected.Insert(result);
	}

	array<string> LastSent()
	{
		if (m_Sent.Count() == 0)
			return null;
		return m_Sent[m_Sent.Count() - 1];
	}
}

[DZS_RegisterPlugin(DZS_SelfTestPlugin)]
class DZS_SelfTestPlugin : DZS_Plugin
{
	protected bool m_Active;
	protected string m_Mode;
	protected bool m_Ran;
	protected float m_Wait;
	protected int m_Pass;
	protected int m_Fail;
	protected int m_SaveCount;
	protected ref array<ref DZS_Notice> m_Notices;
	protected ref map<string, Object> m_Spawned;
	protected Camera m_DemoCamera;

	void DZS_SelfTestPlugin()
	{
		m_Notices = new array<ref DZS_Notice>();
		m_Spawned = new map<string, Object>();
	}

	override string GetName() { return "selftest"; }

	override void OnRegister()
	{
		m_Active = GetCLIParam("dzsSelfTest", m_Mode);
		m_Kernel.GetTypes().RegisterOperationType("selftest", "selftest:edit", 1);
		m_Kernel.GetTypes().RegisterCompanionType("selftest", "selftest:note", 1);
	}

	override void OnUpdate(float dt)
	{
		if (!m_Active || m_Ran)
			return;
		m_Wait += dt;
		if (m_Wait < 2.0)
			return;
		m_Ran = true;
		RunAll();
	}

	override void OnNotice(DZS_Notice notice)
	{
		m_Notices.Insert(notice);
	}

	override void OnSessionEnd()
	{
		for (int i = 0; i < m_Spawned.Count(); i++)
		{
			if (m_Spawned.GetElement(i))
				GetGame().ObjectDelete(m_Spawned.GetElement(i));
		}
		m_Spawned.Clear();
		if (m_DemoCamera)
			GetGame().ObjectDelete(m_DemoCamera);
	}

	// Mirrors objects of the shown instance into the world, driven only by events.
	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		if (!m_Active || inst != m_Kernel.GetShown())
			return;
		for (int i = 0; i < op.CountChanges(); i++)
		{
			DZS_Change c = op.GetChange(i);
			string id = c.GetId();
			if (c.GetKind() == DZS_ChangeKind.CREATE && c.GetSnapshotKind() == DZS_KIND_OBJECT)
				SpawnFor(inst, id);
			else if (c.GetKind() == DZS_ChangeKind.DELETE)
				DespawnFor(id);
			else if (c.GetKind() == DZS_ChangeKind.SET_FIELD && c.GetKey() == "pos")
			{
				Object o = m_Spawned.Get(id);
				if (o)
					o.SetPosition(c.GetNew().ToVector());
			}
		}
	}

	protected void SpawnFor(DZS_Instance inst, string id)
	{
		string cls, pos;
		if (!inst.GetField(id, "name", cls) || !inst.GetField(id, "pos", pos))
			return;
		Object o = GetGame().CreateObjectEx(cls, pos.ToVector(), ECE_LOCAL | ECE_CREATEPHYSICS);
		if (o)
			m_Spawned.Set(id, o);
	}

	protected void DespawnFor(string id)
	{
		Object o = m_Spawned.Get(id);
		if (o)
			GetGame().ObjectDelete(o);
		m_Spawned.Remove(id);
	}

	// ---- helpers ----

	protected void Log(string s)
	{
		DZS_Log.Info("selftest " + s);
	}

	protected void Check(string name, bool ok, string detail = "")
	{
		if (ok)
		{
			m_Pass++;
			if (detail != "" && m_Mode == "big")
				Log("PASS " + name + " (" + detail + ")");
			else
				Log("PASS " + name);
		}
		else
		{
			m_Fail++;
			Log("FAIL " + name + " " + detail);
		}
	}

	protected string NewGroup(DZS_Instance inst, string name)
	{
		DZS_OpBuilder b = inst.BeginOperation("selftest:edit");
		map<string, string> f = new map<string, string>();
		f.Set("name", name);
		f.Set("color", "#4a90d9");
		string id = b.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, f);
		b.Commit("Gruppe " + name + " anlegen", "Create group " + name);
		return id;
	}

	protected string Place(DZS_Instance inst, string group, string cls, string pos)
	{
		DZS_OpBuilder b = inst.BeginOperation("selftest:edit");
		map<string, string> f = new map<string, string>();
		f.Set("name", cls);
		f.Set("pos", pos);
		f.Set("ypr", "0 0 0");
		f.Set("scale", "1");
		string id = b.Create(DZS_KIND_OBJECT, group, DZS_END, f);
		DZS_SubmitResult r = b.Commit(cls + " platzieren", "Place " + cls);
		if (!r.Ok())
			return "";
		return id;
	}

	protected bool SetPos(DZS_Instance inst, string id, string pos, string mergeKey = "")
	{
		DZS_OpBuilder b = inst.BeginOperation("selftest:edit");
		b.SetField(id, "pos", pos);
		return b.Commit("Objekt verschieben", "Move object", mergeKey).Ok();
	}

	protected void Save(DZS_Instance inst)
	{
		m_SaveCount++;
		DZS_OpBuilder b = inst.BeginOperation("selftest:edit");
		b.SetCompanion(DZS_PROJECT_ID, "selftest:note", 1, "rev " + m_SaveCount);
		inst.SubmitSystem(b.GetOperation(), true);
	}

	protected string Dump(DZS_Instance inst, string id)
	{
		DZS_ElementData e = inst.GetElementCopy(id);
		if (!e)
			return "<none>";
		array<string> chunks = e.ToJson().ToChunks();
		string s = "";
		foreach (string c : chunks)
			s += c;
		return s;
	}

	protected bool SameChunks(array<string> a, array<string> b)
	{
		if (a.Count() != b.Count())
			return false;
		for (int i = 0; i < a.Count(); i++)
		{
			if (a[i] != b[i])
				return false;
		}
		return true;
	}

	protected bool SameState(DZS_Instance a, DZS_Instance b)
	{
		array<string> ta = a.ExportState();
		array<string> tb = b.ExportState();
		return SameChunks(ta, tb);
	}

	protected string LastNotice()
	{
		if (m_Notices.Count() == 0)
			return "";
		return m_Notices[m_Notices.Count() - 1].m_De;
	}

	protected string CsvChildren(DZS_Instance inst, string id)
	{
		array<string> ch = inst.GetChildren(id);
		string s = "";
		foreach (string c : ch)
			s += c + ",";
		return s;
	}

	// ---- the run ----

	protected void RunAll()
	{
		Log("start (mode '" + m_Mode + "')");
		DZS_Instance a = m_Kernel.GetShown();
		if (!a)
		{
			Check("shown instance", false);
			return;
		}
		if (m_Mode == "big")
		{
			TestBig();
			Log("done: " + m_Pass + " passed, " + m_Fail + " failed");
			return;
		}
		TestNames();
		TestPlaceUndoRedo(a);
		TestDeleteRestoresAll(a);
		TestUnsavedCount(a);
		TestNoEffect(a);
		TestMergeAndGesture(a);
		TestClean(a);
		TestTextForm(a);
		TestForeign(a);
		TestTwoInstances(a);
		TestConflicts(a);
		TestReloadDropsStep(a);
		TestGroupDeletedStep(a);
		TestAuthor(a);
		Log("done: " + m_Pass + " passed, " + m_Fail + " failed");
		BuildDemo(a);
	}

	protected void TestNames()
	{
		string longName = "";
		for (int i = 0; i < 7; i++)
			longName += "Ä123456789";
		string cut = DZS_Str.StepName(longName);
		int chars = 0;
		for (int k = 0; k < cut.Length(); k++)
		{
			if (!DZS_Utf8.IsContinuation(cut, k))
				chars++;
		}
		Check("step name cut to 60 chars with ellipsis", chars == 60 && cut.IndexOf("…") == cut.Length() - 3, cut);
		Check("short step name unchanged", DZS_Str.StepName("Holzkiste löschen") == "Holzkiste löschen");
	}

	protected void TestPlaceUndoRedo(DZS_Instance a)
	{
		string g = NewGroup(a, "Basis");
		Check("group created", a.GetKind(g) == DZS_KIND_GROUP && a.GetGroupIds().Count() == 1);
		string k1 = Place(a, g, "WoodenCrate", "100 5 100");
		string k2 = Place(a, g, "WoodenCrate", "101 5 100");
		string k3 = Place(a, g, "WoodenCrate", "102 5 100");
		Check("three crates placed in order", CsvChildren(a, g) == k1 + "," + k2 + "," + k3 + ",");
		Check("undo name", a.GetUndoName() == "WoodenCrate platzieren" && a.GetUndoName(true) == "Place WoodenCrate");
		a.Undo();
		Check("undo notice", LastNotice() == "Rückgängig: WoodenCrate platzieren", LastNotice());
		a.Undo();
		a.Undo();
		Check("three undos remove the crates", !a.Has(k1) && !a.Has(k2) && !a.Has(k3));
		a.Redo();
		Check("redo notice", LastNotice() == "Wiederholt: WoodenCrate platzieren", LastNotice());
		a.Redo();
		Check("two redos bring back the first two with the same ids", a.Has(k1) && a.Has(k2) && !a.Has(k3) && CsvChildren(a, g) == k1 + "," + k2 + ",");
		Check("redo still possible", a.CanRedo());
		SetPos(a, k1, "100 5 105");
		Check("new step clears redo", !a.CanRedo());
		// delete, undo, move, undo twice, redo twice: the move acts on the same object
		DZS_OpBuilder del = a.BeginOperation("selftest:edit");
		del.Delete(k2);
		del.Commit("WoodenCrate löschen", "Delete WoodenCrate");
		a.Undo();
		SetPos(a, k2, "120 5 120");
		a.Undo();
		a.Undo();
		a.Redo();
		a.Redo();
		string p2;
		a.GetField(k2, "pos", p2);
		Check("move after undone delete acts on the same object", a.Has(k2) && p2 == "120 5 120", p2);
		int len = a.GetHistoryLength();
		a.Undo();
		a.Undo();
		a.Undo();
		a.Undo();
		a.Undo();
		a.Undo();
		a.Undo();
		Check("undo back to empty project", !a.CanUndo() && a.GetGroupIds().Count() == 0, "len " + len);
		while (a.CanRedo())
			a.Redo();
		Check("redo everything", a.GetGroupIds().Count() == 1 && a.Has(k1) && a.Has(k2));
	}

	protected void TestDeleteRestoresAll(DZS_Instance a)
	{
		string g = NewGroup(a, "Sperren");
		string x1 = Place(a, g, "Barrel_Blue", "200 5 200");
		string x2 = Place(a, g, "Barrel_Blue", "201 5 200");
		string x3 = Place(a, g, "Barrel_Blue", "202 5 200");
		string x4 = Place(a, g, "Barrel_Blue", "203 5 200");
		DZS_OpBuilder b = a.BeginOperation("selftest:edit");
		b.SetField(x2, "ypr", "46.2 0 0");
		b.SetField(x2, "scale", "1.25");
		b.SetHidden(x2, true);
		b.SetLocked(x1, true);
		b.SetLocked(x2, true);
		b.SetLocked(x3, true);
		b.SetCompanion(x2, "selftest:note", 1, "gelesen: {\"x\":1}");
		b.Commit("3 Objekte sperren", "Lock 3 objects");
		string before = Dump(a, x1);
		before += Dump(a, x2);
		before += Dump(a, x3);
		before += CsvChildren(a, g);
		DZS_OpBuilder d = a.BeginOperation("selftest:edit");
		d.Delete(x1);
		d.Delete(x2);
		d.Delete(x3);
		d.Commit("3 Objekte löschen", "Delete 3 objects");
		Check("three locked objects deleted", !a.Has(x1) && !a.Has(x2) && !a.Has(x3) && CsvChildren(a, g) == x4 + ",");
		a.Undo();
		string after = Dump(a, x1);
		after += Dump(a, x2);
		after += Dump(a, x3);
		after += CsvChildren(a, g);
		Check("undo restores full state (id, fields, hidden, locked, companion, order)", before == after, after);
		a.Undo();
		Check("next undo unlocks/unhides again", !a.IsLocked(x1) && !a.IsHidden(x2));
		a.Redo();
	}

	protected void TestUnsavedCount(DZS_Instance a)
	{
		string g = NewGroup(a, "Zaehler");
		Save(a);
		Check("saved: 0 unsaved", a.GetUnsavedCount() == 0);
		Place(a, g, "Barrel_Blue", "300 5 300");
		Place(a, g, "Barrel_Blue", "301 5 300");
		Place(a, g, "Barrel_Blue", "302 5 300");
		Check("three steps: 3 unsaved", a.GetUnsavedCount() == 3);
		a.Undo();
		a.Undo();
		Check("two undos: 1 unsaved", a.GetUnsavedCount() == 1);
		a.Undo();
		Check("back at save point: 0", a.GetUnsavedCount() == 0);
		a.Undo();
		a.Undo();
		Check("two undos past save point: 2", a.GetUnsavedCount() == 2);
		a.Redo();
		a.Redo();
		Check("redo back to save point: 0", a.GetUnsavedCount() == 0);
		a.Redo();
		a.Redo();
		a.Redo();
		Save(a);
		a.Undo();
		a.Undo();
		Place(a, g, "Barrel_Blue", "310 5 300");
		Check("save point lost after new step: 3", a.GetUnsavedCount() == 3, "got " + a.GetUnsavedCount());
		a.Undo();
		Check("then undo: 2", a.GetUnsavedCount() == 2, "got " + a.GetUnsavedCount());
		a.Redo();
		Save(a);
		Check("save consequences are no step", a.GetUnsavedCount() == 0 && a.GetUndoName() == "Barrel_Blue platzieren");
	}

	protected void TestNoEffect(DZS_Instance a)
	{
		string g = a.GetGroupIds()[0];
		string k = a.GetChildren(g)[0];
		int len = a.GetHistoryLength();
		DZS_OpBuilder b = a.BeginOperation("selftest:edit");
		b.SetLocked(k, a.IsLocked(k));
		string pos;
		a.GetField(k, "pos", pos);
		b.SetField(k, "pos", pos);
		DZS_SubmitResult r = b.Commit("nichts", "nothing");
		Check("action without effect makes no step", r.m_Status == DZS_SubmitStatus.EMPTY && a.GetHistoryLength() == len);
	}

	protected void TestMergeAndGesture(DZS_Instance a)
	{
		string g = a.GetGroupIds()[0];
		string k = a.GetChildren(g)[0];
		string start;
		a.GetField(k, "pos", start);
		int len = a.GetHistoryPosition();
		SetPos(a, k, "150 5 150", "wheel:" + k);
		SetPos(a, k, "151 5 150", "wheel:" + k);
		SetPos(a, k, "152 5 150", "wheel:" + k);
		Check("wheel notches within 1 s are one step", a.GetHistoryPosition() == len + 1 && a.GetHistoryLength() == len + 1);
		a.Undo();
		string now;
		a.GetField(k, "pos", now);
		Check("one undo takes back all merged operations", now == start, now);
		a.Redo();
		m_Kernel.SetGesture("selftest", true);
		string rd, re;
		bool enabledDuring = m_Kernel.GetUi().IsActionEnabled("core.undo", rd, re);
		bool triggered = m_Kernel.GetUi().Trigger("core.undo");
		m_Kernel.SetGesture("selftest", false);
		Check("undo does nothing while a gesture runs", !enabledDuring && !triggered && a.GetHistoryPosition() == len + 1 && !a.CanRedo());
		Check("undo enabled after the gesture", m_Kernel.GetUi().IsActionEnabled("core.undo", rd, re));
	}

	protected void TestClean(DZS_Instance a)
	{
		string ga = NewGroup(a, "A");
		string gb = NewGroup(a, "B");
		string oa = Place(a, ga, "Barrel_Blue", "400 5 400");
		Place(a, gb, "Barrel_Blue", "401 5 400");
		Save(a);
		SetPos(a, oa, "402 5 400");
		Check("changed group A is not clean, B is", !a.IsGroupClean(ga) && a.IsGroupClean(gb));
		a.Undo();
		Check("after undo to save point both clean", a.IsGroupClean(ga) && a.IsGroupClean(gb));
		DZS_OpBuilder b = a.BeginOperation("selftest:edit");
		b.SetHidden(oa, true);
		b.Commit("Barrel_Blue ausblenden", "Hide Barrel_Blue");
		Check("hiding marks group and manifest not clean", !a.IsGroupClean(ga) && !a.IsManifestClean());
		a.Undo();
		Check("empty project text says nothing new", a.IsManifestClean());
	}

	protected void TestTextForm(DZS_Instance a)
	{
		string g = a.GetGroupIds()[0];
		string big = "";
		while (big.Length() < 10240)
			big += "Quelltext mit Umlauten äöüß und € \"zitiert\" \\ ";
		DZS_OpBuilder b = a.BeginOperation("selftest:edit");
		map<string, string> f = new map<string, string>();
		f.Set("name", "Land_Test");
		f.Set("pos", "500 5 500");
		f.Set("unknown", big);
		string id = b.Create(DZS_KIND_OBJECT, g, DZS_END, f, true, true);
		b.SetCompanion(id, "fremd:daten", 3, "{\"keep\":\"ä\"}");
		b.Commit("Land_Test platzieren", "Place Land_Test");
		array<string> text = a.ExportState();
		bool sizes = true;
		bool boundaries = true;
		for (int i = 0; i < text.Count(); i++)
		{
			if (text[i].Length() > DZS_CHUNK_BYTES)
				sizes = false;
			if (text[i].Length() > 0 && DZS_Utf8.IsContinuation(text[i], 0))
				boundaries = false;
		}
		Check("text form chunks <= 1000 bytes", sizes, "chunks " + text.Count());
		Check("no chunk splits a UTF-8 character", boundaries);
		string error;
		DZS_Instance c = m_Kernel.OpenFromState(text, error);
		Check("open from text form", c != null, error);
		if (!c)
			return;
		Check("same state after text round trip (ids, hidden, locked, companions)", SameState(a, c));
		string back;
		c.GetField(id, "unknown", back);
		Check("10 KB field survives byte for byte", back == big && back.Length() == big.Length(), "len " + back.Length());
		int ver;
		string data;
		Check("unknown companion kept", c.GetCompanion(id, "fremd:daten", ver, data) && ver == 3 && data == "{\"keep\":\"ä\"}");
		m_Kernel.CloseInstance(c);
		DZS_OpBuilder d = a.BeginOperation("selftest:edit");
		d.Delete(id);
		d.Commit("Land_Test löschen", "Delete Land_Test");
	}

	protected void TestForeign(DZS_Instance a)
	{
		array<string> text = a.ExportState();
		array<string> newer = new array<string>();
		newer.Copy(text);
		string first = newer[0];
		first.Replace("\"textform\":1", "\"textform\":99");
		newer[0] = first;
		string error;
		DZS_Instance n = m_Kernel.OpenFromState(newer, error);
		Check("newer text form does not open and names the version", !n && error.Contains("99"), error);
		string foreign = "{\"textform\":1,\"state\":{\"kinds\":{\"core:project\":1,\"fremd:ding\":2},\"elements\":[{\"id\":\"project\",\"kind\":\"core:project\",\"parent\":\"\",\"after\":\"\",\"hidden\":false,\"locked\":false,\"fields\":{},\"comp\":{}},{\"id\":\"x-1\",\"kind\":\"fremd:ding\",\"parent\":\"project\",\"after\":\"\",\"hidden\":false,\"locked\":false,\"fields\":{},\"comp\":{}}]}}";
		array<string> ft = {foreign};
		string error2;
		DZS_Instance m = m_Kernel.OpenFromState(ft, error2);
		Check("state with a missing plugin's kind does not open and names it", !m && error2.Contains("fremd"), error2);
	}

	protected void TestTwoInstances(DZS_Instance a)
	{
		string error;
		DZS_Instance b = m_Kernel.OpenFromState(a.ExportState(), error);
		Check("second instance from text form of the first", b != null && SameState(a, b) && b.GetSessionCode() != a.GetSessionCode(), error);
		if (!b)
			return;
		DZS_SelfTestRelay sa = new DZS_SelfTestRelay();
		DZS_SelfTestRelay sb = new DZS_SelfTestRelay();
		Check("only one plugin per sync slot", a.AttachSync(sa) && !a.AttachSync(sb));
		b.AttachSync(sb);
		sa.m_Peer = sb;
		sb.m_Peer = sa;
		sa.m_Relay = true;
		sb.m_Relay = true;
		string g = NewGroup(a, "Wechsel");
		for (int i = 0; i < 200; i++)
		{
			DZS_Instance cur = a;
			if (i % 2 == 1)
				cur = b;
			array<string> kids = cur.GetChildren(g);
			int action = Math.RandomInt(0, 7);
			if (kids.Count() < 3)
				action = 0;
			string target = "";
			if (kids.Count() > 0)
				target = kids[Math.RandomInt(0, kids.Count())];
			if (action == 0)
				Place(cur, g, "Barrel_Blue", (600 + i).ToString() + " 5 600");
			else if (action == 1)
				SetPos(cur, target, (600 + i).ToString() + " 6 601");
			else if (action == 2)
			{
				DZS_OpBuilder del = cur.BeginOperation("selftest:edit");
				del.Delete(target);
				del.Commit("Barrel_Blue löschen", "Delete Barrel_Blue");
			}
			else if (action == 3)
				cur.Undo();
			else if (action == 4)
				cur.Redo();
			else if (action == 5)
			{
				DZS_OpBuilder hid = cur.BeginOperation("selftest:edit");
				hid.SetHidden(target, !cur.IsHidden(target));
				hid.Commit("Barrel_Blue ausblenden", "Hide Barrel_Blue");
			}
			else
			{
				DZS_OpBuilder mv = cur.BeginOperation("selftest:edit");
				mv.Move(target, g, "");
				mv.Commit("umsortieren", "Reorder");
			}
		}
		Check("200 alternating changes: both states equal", SameState(a, b), "a " + a.CountElements() + " b " + b.CountElements());
		Save(a);
		int va, vb;
		string da, db;
		a.GetCompanion(DZS_PROJECT_ID, "selftest:note", va, da);
		b.GetCompanion(DZS_PROJECT_ID, "selftest:note", vb, db);
		Check("save consequences reach the other instance", da == db && da != "", da + " / " + db);
		// silent reload: B accepts no outside system operations, A reloads and B gets A's ids
		sb.SetNoExternalSystemOps(true);
		DZS_OpBuilder rb = b.BeginOperation("selftest:edit");
		rb.Create(DZS_KIND_OBJECT, g, DZS_END);
		DZS_SubmitResult blocked = b.SubmitSystem(rb.GetOperation(), false, true);
		Check("blocked outside system operation in B", !blocked.Ok());
		DZS_OpBuilder rl = a.BeginOperation("selftest:edit");
		array<string> old = a.GetChildren(g);
		foreach (string o : old)
			rl.Delete(o);
		map<string, string> f = new map<string, string>();
		f.Set("name", "Barrel_Blue");
		f.Set("pos", "700 5 700");
		rl.Create(DZS_KIND_OBJECT, g, DZS_END, f);
		rl.Create(DZS_KIND_OBJECT, g, DZS_END, f);
		DZS_SubmitResult reload = a.SubmitSystem(rl.GetOperation(), false, true);
		Check("silent reload in A reaches B with the same ids", reload.Ok() && SameState(a, b));
		// duplicates and foreign types
		array<string> last = sa.LastSent();
		DZS_SubmitResult dup = sb.SubmitRemote(last);
		Check("same remote operation twice changes state once", dup.m_Status == DZS_SubmitStatus.DUPLICATE && SameState(a, b));
		DZS_Operation fake = DZS_Operation.FromText(last, error);
		DZS_Json fj = fake.ToJson();
		fj.Set("type", DZS_Json.Str("fremd:op"));
		fj.Set("tv", DZS_Json.Int(4));
		fj.Set("id", DZS_Json.Str("zzzz-1"));
		DZS_Json wrap = DZS_Json.Obj().Set("textform", DZS_Json.Int(1)).Set("op", fj);
		int rejectedBefore = sb.m_Rejected.Count();
		DZS_SubmitResult unk = sb.SubmitRemote(wrap.ToChunks());
		Check("remote op of a missing plugin is rejected with plugin and version", unk.m_Status == DZS_SubmitStatus.UNKNOWN_TYPE && unk.m_MissingPlugin == "fremd" && unk.m_MissingVersion == 4 && sb.m_Rejected.Count() == rejectedBefore + 1);
		// undo belongs to each instance
		string oa = Place(a, g, "Barrel_Blue", "710 5 710");
		string ob = Place(b, g, "Barrel_Blue", "711 5 711");
		a.Undo();
		Check("undo in A takes back only A's step", !a.Has(oa) && a.Has(ob) && !b.Has(oa) && b.Has(ob));
		sa.Detach();
		sb.Detach();
		m_Kernel.CloseInstance(b);
	}

	protected void TestConflicts(DZS_Instance a)
	{
		string error;
		string g = NewGroup(a, "Konflikt");
		array<string> fifty = new array<string>();
		DZS_OpBuilder mk = a.BeginOperation("selftest:edit");
		for (int i = 0; i < 50; i++)
		{
			map<string, string> f = new map<string, string>();
			f.Set("name", "Barrel_Blue");
			f.Set("pos", (800 + i).ToString() + " 5 800");
			fifty.Insert(mk.Create(DZS_KIND_OBJECT, g, DZS_END, f));
		}
		mk.Commit("50 Objekte einfügen", "Paste 50 objects");
		string x = fifty[0];
		string y = fifty[10];
		DZS_Instance c = m_Kernel.OpenFromState(a.ExportState(), error);
		DZS_Instance d = m_Kernel.OpenFromState(a.ExportState(), error);
		DZS_SelfTestRelay sc = new DZS_SelfTestRelay();
		DZS_SelfTestRelay sd = new DZS_SelfTestRelay();
		c.AttachSync(sc);
		d.AttachSync(sd);
		// C deletes X, D moves X (unconfirmed); D replays C: the move is dropped
		DZS_OpBuilder del = c.BeginOperation("selftest:edit");
		del.Delete(x);
		del.Commit("Barrel_Blue löschen", "Delete Barrel_Blue");
		array<string> delText = sc.LastSent();
		SetPos(d, x, "900 5 900");
		array<ref array<string>> remotes = new array<ref array<string>>();
		remotes.Insert(delText);
		int notices = m_Notices.Count();
		array<string> dropped = sd.Rebase(remotes);
		Check("conflict: own move of a deleted object is dropped with a notice", dropped.Count() == 1 && m_Notices.Count() > notices && LastNotice().Contains("Schritt nicht mehr möglich"), LastNotice());
		Check("conflict: both states equal", SameState(c, d));
		// a composite move of 50 where one was deleted elsewhere moves none
		DZS_OpBuilder del2 = c.BeginOperation("selftest:edit");
		del2.Delete(y);
		del2.Commit("Barrel_Blue löschen", "Delete Barrel_Blue");
		array<string> del2Text = sc.LastSent();
		DZS_OpBuilder mv = d.BeginOperation("selftest:edit");
		array<string> kids = d.GetChildren(g);
		foreach (string k : kids)
			mv.SetField(k, "pos", "1 2 3");
		mv.Commit(kids.Count().ToString() + " Objekte verschieben", "Move objects");
		array<ref array<string>> remotes2 = new array<ref array<string>>();
		remotes2.Insert(del2Text);
		array<string> dropped2 = sd.Rebase(remotes2);
		string p;
		d.GetField(fifty[20], "pos", p);
		Check("composite of 49 moves with one deleted object: none moved", dropped2.Count() == 1 && p == "820 5 800" && SameState(c, d), p);
		// insert behind an object that was deleted elsewhere lands at the end of the group
		string z = fifty[30];
		DZS_OpBuilder del3 = c.BeginOperation("selftest:edit");
		del3.Delete(z);
		del3.Commit("Barrel_Blue löschen", "Delete Barrel_Blue");
		array<string> del3Text = sc.LastSent();
		DZS_OpBuilder ins = d.BeginOperation("selftest:edit");
		string neu = ins.Create(DZS_KIND_OBJECT, g, z);
		ins.Commit("einfügen", "Paste");
		array<ref array<string>> remotes3 = new array<ref array<string>>();
		remotes3.Insert(del3Text);
		array<string> dropped3 = sd.Rebase(remotes3);
		array<string> after = d.GetChildren(g);
		Check("insert behind deleted object goes to the end", dropped3.Count() == 0 && after[after.Count() - 1] == neu);
		sc.Detach();
		sd.Detach();
		m_Kernel.CloseInstance(c);
		m_Kernel.CloseInstance(d);
		a.Undo();
	}

	protected void TestGroupDeletedStep(DZS_Instance a)
	{
		string g = NewGroup(a, "Weg");
		string p = Place(a, g, "Barrel_Blue", "952 5 950");
		a.Undo();
		DZS_OpBuilder rl = a.BeginOperation("selftest:edit");
		rl.Delete(g);
		a.SubmitSystem(rl.GetOperation(), false, true);
		bool redone = a.Redo();
		string en = "";
		if (m_Notices.Count() > 0)
			en = m_Notices[m_Notices.Count() - 1].m_En;
		Check("redo into a deleted group names the group", !redone && !a.Has(p) && LastNotice() == "Schritt nicht mehr möglich: Gruppe wurde gelöscht" && en == "Step no longer possible: Group was deleted", LastNotice() + " / " + en);
	}

	protected void TestReloadDropsStep(DZS_Instance a)
	{
		string g = NewGroup(a, "Neuladen");
		string before = Place(a, g, "Barrel_Blue", "950 5 950");
		string p = Place(a, g, "Barrel_Blue", "951 5 950");
		int len = a.GetHistoryLength();
		DZS_OpBuilder rl = a.BeginOperation("selftest:edit");
		rl.Delete(p);
		a.SubmitSystem(rl.GetOperation(), false, true);
		int notices = m_Notices.Count();
		bool undone = a.Undo();
		Check("undo of a step whose object was reloaded away fails and is dropped", !undone && a.GetHistoryLength() == len - 1 && LastNotice() == "Schritt nicht mehr möglich: Objekt wurde gelöscht", LastNotice());
		a.Undo();
		Check("next undo acts on the step before", !a.Has(before));
		a.Redo();
	}

	protected void TestAuthor(DZS_Instance a)
	{
		DZS_SelfTestRelay s = new DZS_SelfTestRelay();
		a.AttachSync(s);
		Place(a, a.GetGroupIds()[0], "Barrel_Blue", "990 5 990");
		string error;
		DZS_Operation op = DZS_Operation.FromText(s.LastSent(), error);
		Check("operation carries installation id and profile name", op && op.GetAuthor() == m_Kernel.GetAuthorId() && op.GetAuthorName() == m_Kernel.GetAuthorName() && op.GetAuthor().Length() == 16, error);
		s.Detach();
		a.Undo();
		Log("installation id " + m_Kernel.GetAuthorId());
	}

	protected void TestBig()
	{
		DZS_Instance a = m_Kernel.GetShown();
		string g = NewGroup(a, "Import");
		float t0 = GetGame().GetTickTime();
		DZS_OpBuilder b = a.BeginOperation("selftest:edit");
		map<string, bool> ids = new map<string, bool>();
		for (int i = 0; i < 10000; i++)
		{
			map<string, string> f = new map<string, string>();
			f.Set("name", "Barrel_Blue");
			f.Set("pos", (i % 100).ToString() + " 5 " + (i / 100).ToString());
			ids.Set(b.Create(DZS_KIND_OBJECT, g, DZS_END, f), true);
		}
		float t1 = GetGame().GetTickTime();
		DZS_SubmitResult r = b.Commit("10000 Objekte importieren", "Import 10000 objects");
		float t2 = GetGame().GetTickTime();
		Check("import of 10000 objects is one step", r.Ok() && a.GetChildren(g).Count() == 10000, "build " + (t1 - t0) + " s, apply " + (t2 - t1) + " s");
		array<string> text = a.ExportState();
		float t3 = GetGame().GetTickTime();
		string error;
		DZS_Instance c = m_Kernel.OpenFromState(text, error);
		float t4 = GetGame().GetTickTime();
		Check("10000 objects survive the text form", c && SameState(a, c), "export " + (t3 - t2) + " s, open " + (t4 - t3) + " s, chunks " + text.Count());
		if (c)
		{
			DZS_OpBuilder b2 = c.BeginOperation("selftest:edit");
			for (int k = 0; k < 10000; k++)
				ids.Set(b2.Create(DZS_KIND_OBJECT, g, DZS_END), true);
			b2.Commit("weitere", "more");
			Check("ids across two instances are all distinct", ids.Count() == 20000, "distinct " + ids.Count());
			m_Kernel.CloseInstance(c);
		}
		int c0 = TickCount(0);
		a.Undo();
		int c1 = TickCount(0);
		Check("undo of the import", a.GetChildren(g).Count() == 0, "ticks " + (c1 - c0));
		string o = Place(a, g, "Barrel_Blue", "5 5 5");
		for (int s = 0; s < 10000; s++)
			SetPos(a, o, s.ToString() + " 5 5");
		int c2 = TickCount(0);
		a.Undo();
		int c3 = TickCount(0);
		a.Redo();
		int c4 = TickCount(0);
		Log("timing: history " + a.GetHistoryLength() + " steps; undo " + (c3 - c2) + " ticks, redo " + (c4 - c3) + " ticks; tick time now " + GetGame().GetTickTime() + " s, ticks " + TickCount(0));
	}

	// Three objects in front of a close camera; their placements are the steps left for the key test.
	protected void BuildDemo(DZS_Instance a)
	{
		float size = GetGame().GetWorld().GetWorldSize();
		float cx = size * 0.5;
		float cz = size * 0.5;
		if (m_Kernel.GetWorld() == "ChernarusPlus")
		{
			cx = 7545;
			cz = 7440;
		}
		string g = NewGroup(a, "Demo");
		for (int i = 0; i < 3; i++)
		{
			float x = cx + (i - 1) * 16;
			float y = GetGame().SurfaceY(x, cz);
			Place(a, g, "Wreck_UH1Y", x.ToString() + " " + y.ToString() + " " + cz.ToString());
		}
		vector camPos = Vector(cx, GetGame().SurfaceY(cx, cz - 40) + 30, cz - 40);
		m_DemoCamera = Camera.Cast(GetGame().CreateObject("staticcamera", camPos, true));
		if (m_DemoCamera)
		{
			m_DemoCamera.SetActive(true);
			m_DemoCamera.LookAt(Vector(cx, GetGame().SurfaceY(cx, cz) + 2, cz));
		}
		Log("demo ready: " + m_Spawned.Count() + " objects in the world, undo '" + a.GetUndoName() + "', history " + a.GetHistoryPosition() + "/" + a.GetHistoryLength());
	}
}
