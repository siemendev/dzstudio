class DZS_SubmitResult : Managed
{
	DZS_SubmitStatus m_Status;
	string m_OpId;
	string m_ReasonDe;
	string m_ReasonEn;
	string m_MissingPlugin;
	int m_MissingVersion;

	bool Ok() { return m_Status == DZS_SubmitStatus.APPLIED; }

	static DZS_SubmitResult Make(DZS_SubmitStatus status, string opId = "")
	{
		DZS_SubmitResult r = new DZS_SubmitResult();
		r.m_Status = status;
		r.m_OpId = opId;
		return r;
	}
}

class DZS_Notice : Managed
{
	string m_Kind;
	string m_De;
	string m_En;
	string m_Tone;
	int m_DurationMs;

	void DZS_Notice(string kind, string de, string en, string tone = "info", int durationMs = 2000)
	{
		m_Kind = kind;
		m_De = de;
		m_En = en;
		m_Tone = tone;
		m_DurationMs = durationMs;
	}
}

// Builds one operation against the current state plus the changes already added, so that several
// changes in one operation (delete three neighbours, create then move) get the right preconditions.
class DZS_OpBuilder : Managed
{
	protected DZS_Instance m_Inst;
	protected ref DZS_Operation m_Op;
	protected ref map<string, ref DZS_ElementData> m_Overlay;
	protected ref map<string, bool> m_Gone;

	void DZS_OpBuilder(DZS_Instance inst, string type, int typeVersion)
	{
		m_Inst = inst;
		m_Op = new DZS_Operation(type, typeVersion);
		m_Overlay = new map<string, ref DZS_ElementData>();
		m_Gone = new map<string, bool>();
	}

	DZS_Operation GetOperation() { return m_Op; }
	bool IsEmpty() { return m_Op.CountChanges() == 0; }

	DZS_ElementData Look(string id)
	{
		if (m_Gone.Contains(id))
			return null;
		DZS_ElementData e = m_Overlay.Get(id);
		if (e)
			return e;
		e = m_Inst.GetElementCopy(id);
		if (e)
			m_Overlay.Set(id, e);
		return e;
	}

	protected string PrevSibling(DZS_ElementData parent, string id)
	{
		int idx = parent.m_Children.Find(id);
		if (idx <= 0)
			return "";
		return parent.m_Children[idx - 1];
	}

	protected string ResolveAfter(DZS_ElementData parent, string after)
	{
		if (after == "")
			return "";
		int count = parent.m_Children.Count();
		if (count == 0)
			return "";
		string lastId = parent.m_Children[count - 1];
		if (after == DZS_END || after == lastId || parent.m_Children.Find(after) < 0)
			return lastId;
		return after;
	}

	protected void PlaceChild(DZS_ElementData parent, string id, string after)
	{
		if (after == "")
		{
			if (parent.m_Children.Count() == 0)
				parent.m_Children.Insert(id);
			else
				parent.m_Children.InsertAt(id, 0);
			return;
		}
		int last = parent.m_Children.Count() - 1;
		if (last >= 0 && parent.m_Children[last] == after)
		{
			parent.m_Children.Insert(id);
			return;
		}
		int idx = parent.m_Children.Find(after);
		if (idx < 0 || idx + 1 >= parent.m_Children.Count())
			parent.m_Children.Insert(id);
		else
			parent.m_Children.InsertAt(id, idx + 1);
	}

	// Creates an element; after = "" places it first, DZS_END last. Returns its new id or "".
	string Create(string kind, string parentId, string after, map<string, string> fields = null, bool hidden = false, bool locked = false, array<ref DZS_Companion> companions = null, string fixedId = "")
	{
		DZS_ElementData snap = new DZS_ElementData();
		snap.m_Kind = kind;
		snap.m_Hidden = hidden;
		snap.m_Locked = locked;
		if (fields)
		{
			for (int i = 0; i < fields.Count(); i++)
				snap.m_Fields.Set(fields.GetKey(i), fields.GetElement(i));
		}
		if (companions)
		{
			foreach (DZS_Companion c : companions)
			{
				DZS_Companion t131 = c.Copy();
				snap.m_Companions.Set(c.m_Type, t131);
			}
		}
		if (fixedId != "")
			snap.m_Id = fixedId;
		else
			snap.m_Id = m_Inst.NewElementId();
		if (Look(snap.m_Id))
			return "";
		snap.m_Parent = parentId;
		if (parentId != "")
		{
			DZS_ElementData parent = Look(parentId);
			if (!parent)
				return "";
			snap.m_After = ResolveAfter(parent, after);
			PlaceChild(parent, snap.m_Id, snap.m_After);
		}
		DZS_Change t148 = DZS_Change.Create(snap);
		m_Op.AddChange(t148);
		DZS_ElementData mine = snap.Copy(false);
		m_Overlay.Set(snap.m_Id, mine);
		m_Gone.Remove(snap.m_Id);
		return snap.m_Id;
	}

	// Deletes an element with everything below it, carrying their full state for undo.
	bool Delete(string id)
	{
		DZS_ElementData e = Look(id);
		if (!e)
			return false;
		array<string> children = new array<string>();
		children.Copy(e.m_Children);
		for (int i = children.Count() - 1; i >= 0; i--)
			Delete(children[i]);
		DZS_ElementData snap = e.Copy(false);
		if (e.m_Parent != "")
		{
			DZS_ElementData parent = Look(e.m_Parent);
			snap.m_After = PrevSibling(parent, id);
			parent.m_Children.RemoveOrdered(parent.m_Children.Find(id));
		}
		DZS_Change t172 = DZS_Change.Delete(snap);
		m_Op.AddChange(t172);
		m_Overlay.Remove(id);
		m_Gone.Set(id, true);
		return true;
	}

	bool SetField(string id, string key, string value)
	{
		DZS_ElementData e = Look(id);
		if (!e)
			return false;
		string old;
		bool has = e.m_Fields.Find(key, old);
		if (has && old == value)
			return false;
		DZS_Change t187 = DZS_Change.SetField(id, key, has, old, true, value);
		m_Op.AddChange(t187);
		e.m_Fields.Set(key, value);
		return true;
	}

	bool ClearField(string id, string key)
	{
		DZS_ElementData e = Look(id);
		string old;
		if (!e || !e.m_Fields.Find(key, old))
			return false;
		DZS_Change t198 = DZS_Change.SetField(id, key, true, old, false, "");
		m_Op.AddChange(t198);
		e.m_Fields.Remove(key);
		return true;
	}

	bool SetHidden(string id, bool hidden)
	{
		DZS_ElementData e = Look(id);
		if (!e || e.m_Hidden == hidden)
			return false;
		DZS_Change t208 = DZS_Change.SetFlag(DZS_ChangeKind.SET_HIDDEN, id, e.m_Hidden, hidden);
		m_Op.AddChange(t208);
		e.m_Hidden = hidden;
		return true;
	}

	bool SetLocked(string id, bool locked)
	{
		DZS_ElementData e = Look(id);
		if (!e || e.m_Locked == locked)
			return false;
		DZS_Change t218 = DZS_Change.SetFlag(DZS_ChangeKind.SET_LOCKED, id, e.m_Locked, locked);
		m_Op.AddChange(t218);
		e.m_Locked = locked;
		return true;
	}

	// Moves an element behind 'after' in 'toParent' ("" = first, DZS_END = last); also reorders.
	bool Move(string id, string toParent, string after)
	{
		DZS_ElementData e = Look(id);
		DZS_ElementData target = Look(toParent);
		if (!e || !target || e.m_Parent == "")
			return false;
		DZS_ElementData from = Look(e.m_Parent);
		string fromAfter = PrevSibling(from, id);
		from.m_Children.RemoveOrdered(from.m_Children.Find(id));
		string toAfter = ResolveAfter(target, after);
		if (e.m_Parent == toParent && fromAfter == toAfter)
		{
			PlaceChild(from, id, fromAfter);
			return false;
		}
		PlaceChild(target, id, toAfter);
		DZS_Change t240 = DZS_Change.Move(id, e.m_Parent, fromAfter, toParent, toAfter);
		m_Op.AddChange(t240);
		e.m_Parent = toParent;
		return true;
	}

	bool SetCompanion(string id, string type, int version, string data)
	{
		DZS_ElementData e = Look(id);
		if (!e)
			return false;
		DZS_Companion old = e.m_Companions.Get(type);
		DZS_Companion fresh = new DZS_Companion(type, version, data);
		if (old && old.Equals(fresh))
			return false;
		DZS_Change t254 = DZS_Change.SetCompanion(id, type, old, fresh);
		m_Op.AddChange(t254);
		e.m_Companions.Set(type, fresh);
		return true;
	}

	bool ClearCompanion(string id, string type)
	{
		DZS_ElementData e = Look(id);
		if (!e)
			return false;
		DZS_Companion old = e.m_Companions.Get(type);
		if (!old)
			return false;
		DZS_Change t267 = DZS_Change.SetCompanion(id, type, old, null);
		m_Op.AddChange(t267);
		e.m_Companions.Remove(type);
		return true;
	}

	DZS_SubmitResult Commit(string nameDe, string nameEn, string mergeKey = "")
	{
		return m_Inst.Submit(m_Op, nameDe, nameEn, mergeKey);
	}
}

// One open project in the kernel: its state, history, id source, sync slot and protocol.
// Payload of "core.stepObjects": the objects an undo or redo touched (gone ones included).
class DZS_StepObjects : Managed
{
	DZS_Instance m_Inst;
	bool m_Redo;
	ref array<string> m_Ids = new array<string>();
}

class DZS_SaveMark : Managed
{
	DZS_Step m_Step;
	bool m_StepSet;
	int m_Foreign;
	string m_Mode;
}

class DZS_Instance : DZS_ProjectState
{
	protected DZS_Kernel m_Kernel;
	protected string m_Code;
	protected string m_SavedClock;
	protected int m_Counter;
	protected string m_Label;
	protected ref DZS_History m_History;
	protected ref map<string, bool> m_KnownOps;
	protected ref array<ref DZS_Operation> m_Unconfirmed;
	protected ref map<string, DZS_Step> m_UndoRedoOf;
	protected ref DZS_SyncSlot m_Sync;
	protected bool m_NoExternalSystemOps;
	protected bool m_Protocol;
	protected int m_ProtocolSeq;
	protected ref DZS_Instance m_Shadow;
	protected ref DZS_SyncSlot m_ShadowSlot;
	protected bool m_IsShadow;
	protected int m_LastUndoRedoTick;
	protected string m_SaveMode;
	protected bool m_SaveFailed;
	protected bool m_Saving;
	protected int m_Foreign;
	protected ref DZS_Step m_ForeignTouched;
	// View state, never an operation: the group new objects go into; see DZS_Kernel.GetActiveGroup.
	string m_ActiveGroup;

	void DZS_Instance(DZS_Kernel kernel, string label, bool isShadow = false)
	{
		m_Kernel = kernel;
		m_Types = kernel.GetTypes();
		m_Code = DZS_Random.Code(10);
		m_Label = label;
		m_History = new DZS_History();
		m_KnownOps = new map<string, bool>();
		m_Unconfirmed = new array<ref DZS_Operation>();
		m_UndoRedoOf = new map<string, DZS_Step>();
		m_IsShadow = isShadow;
	}

	string GetSessionCode() { return m_Code; }
	string GetLabel() { return m_Label; }
	bool IsShadow() { return m_IsShadow; }

	string NewElementId()
	{
		m_Counter++;
		return m_Code + "-" + m_Counter;
	}

	protected string NewOpId()
	{
		m_Counter++;
		return m_Code + "-" + m_Counter;
	}

	DZS_OpBuilder BeginOperation(string type, int typeVersion = 1)
	{
		return new DZS_OpBuilder(this, type, typeVersion);
	}

	// ---- the single inlet ----

	// A local user change: applied, recorded as (part of) an undo step, reported as event and to the sync slot.
	DZS_SubmitResult Submit(DZS_Operation draft, string nameDe, string nameEn, string mergeKey = "")
	{
		if (!draft || draft.CountChanges() == 0)
			return DZS_SubmitResult.Make(DZS_SubmitStatus.EMPTY);
		if (GateClosed())
			return Refused();
		DZS_Operation op = draft.CopyUnsealed();
		op.Seal(NewOpId(), m_Kernel.GetAuthorId(), m_Kernel.GetAuthorName(), DZS_Origin.LOCAL);
		if (TooLarge(op))
		{
			DZS_SubmitResult big = DZS_SubmitResult.Make(DZS_SubmitStatus.INVALID);
			big.m_ReasonEn = "step too large for the session";
			return big;
		}
		DZS_SubmitResult r = Apply(op, true, true);
		if (!r.Ok())
			return r;
		RecordStep(op, nameDe, nameEn, mergeKey);
		m_Kernel.GetEvents().FireHistoryChanged(this);
		return r;
	}

	// The estimated text form of a new step against the limit a plugin set.
	protected bool TooLarge(DZS_Operation op)
	{
		DZS_EditGate sizeGate = DZS_EditGate.Get();
		if (!sizeGate || m_IsShadow)
			return false;
		int limit = sizeGate.MaxStepBytes(this);
		if (limit <= 0)
			return false;
		int bytes = op.EstimateText();
		if (bytes <= limit)
			return false;
		DZS_Log.Info("submit refused: step of about " + bytes + " bytes over " + limit);
		sizeGate.StepRefused(this, bytes, limit);
		return true;
	}

	// The objects a step touched, for the selection after undo and redo.
	protected void FireStepObjects(DZS_Operation op, bool redo)
	{
		DZS_StepObjects so = new DZS_StepObjects();
		so.m_Inst = this;
		so.m_Redo = redo;
		map<string, bool> seen = new map<string, bool>();
		for (int i = 0; i < op.CountChanges(); i++)
		{
			string id = op.PeekChange(i).GetId();
			if (seen.Contains(id))
				continue;
			seen.Set(id, true);
			if (GetKind(id) == DZS_KIND_OBJECT || !Has(id))
				so.m_Ids.Insert(id);
		}
		m_Kernel.GetEvents().FirePluginEvent("core.stepObjects", so);
	}

	protected bool GateClosed()
	{
		return !m_IsShadow && DZS_EditGate.ChangesOff(this);
	}

	protected DZS_SubmitResult Refused()
	{
		DZS_SubmitResult r = DZS_SubmitResult.Make(DZS_SubmitStatus.INVALID);
		DZS_EditGate g = DZS_EditGate.Get();
		string de, en;
		if (g)
			g.ViewOnly(this, de, en);
		r.m_ReasonDe = de;
		r.m_ReasonEn = en;
		DZS_Log.Info("submit refused: " + en);
		return r;
	}

	// Existing objects a step changes (undo = the step before the cursor, redo = the one after it).
	array<string> StepObjects(bool redo)
	{
		array<string> ids = new array<string>();
		DZS_Step step = m_History.UndoStep();
		if (redo)
			step = m_History.RedoStep();
		if (!step)
			return ids;
		map<string, bool> seen = new map<string, bool>();
		foreach (DZS_Operation op : step.m_Ops)
		{
			for (int i = 0; i < op.CountChanges(); i++)
			{
				DZS_Change c = op.PeekChange(i);
				string id = c.GetId();
				if (seen.Contains(id) || !Has(id) || GetKind(id) != DZS_KIND_OBJECT)
					continue;
				seen.Set(id, true);
				ids.Insert(id);
			}
		}
		return ids;
	}

	// System changes (load, consequences of a save, silent reload): no undo step.
	DZS_SubmitResult SubmitSystem(DZS_Operation draft, bool isSaveResult = false, bool fromOutside = false)
	{
		if (fromOutside && m_NoExternalSystemOps)
		{
			DZS_SubmitResult blocked = DZS_SubmitResult.Make(DZS_SubmitStatus.INVALID);
			blocked.m_ReasonEn = "system operations from outside are disabled by the sync slot";
			return blocked;
		}
		if (!draft)
			return DZS_SubmitResult.Make(DZS_SubmitStatus.EMPTY);
		DZS_Operation op = draft.CopyUnsealed();
		op.Seal(NewOpId(), m_Kernel.GetAuthorId(), m_Kernel.GetAuthorName(), DZS_Origin.SYSTEM);
		DZS_SubmitResult r = Apply(op, true, true);
		if (r.Ok() && isSaveResult)
			MarkSavedNow();
		return r;
	}

	bool IsExternalSystemOpsBlocked() { return m_NoExternalSystemOps; }

	protected DZS_SubmitResult CheckTypes(DZS_Operation op)
	{
		if (!m_Types.KnowsOperation(op.GetType(), op.GetTypeVersion()))
		{
			DZS_SubmitResult r = DZS_SubmitResult.Make(DZS_SubmitStatus.UNKNOWN_TYPE, op.GetId());
			r.m_MissingPlugin = DZS_Str.PluginOfType(op.GetType());
			r.m_MissingVersion = op.GetTypeVersion();
			r.m_ReasonEn = "unknown operation type " + op.GetType() + " v" + op.GetTypeVersion();
			return r;
		}
		for (int i = 0; i < op.CountChanges(); i++)
		{
			DZS_Change ck = op.PeekChange(i);
			string kind = ck.GetSnapshotKind();
			if (kind != "" && !m_Types.KnowsKind(kind))
			{
				DZS_SubmitResult rk = DZS_SubmitResult.Make(DZS_SubmitStatus.UNKNOWN_TYPE, op.GetId());
				rk.m_MissingPlugin = DZS_Str.PluginOfType(kind);
				rk.m_ReasonEn = "unknown element kind " + kind;
				return rk;
			}
		}
		return null;
	}

	protected DZS_SubmitResult Apply(DZS_Operation op, bool checkDuplicate, bool notifySync)
	{
		if (checkDuplicate && m_KnownOps.Contains(op.GetId()))
			return DZS_SubmitResult.Make(DZS_SubmitStatus.DUPLICATE, op.GetId());
		DZS_SubmitResult bad = CheckTypes(op);
		if (bad)
			return bad;
		DZS_ApplyFailure fail = new DZS_ApplyFailure();
		if (!ApplyChanges(op, fail))
		{
			DZS_SubmitResult r = DZS_SubmitResult.Make(DZS_SubmitStatus.PRECONDITION, op.GetId());
			FailureText(fail, r);
			return r;
		}
		m_KnownOps.Set(op.GetId(), true);
		if (op.GetOrigin() == DZS_Origin.LOCAL)
			m_Unconfirmed.Insert(op);
		AfterApply(op, notifySync);
		return DZS_SubmitResult.Make(DZS_SubmitStatus.APPLIED, op.GetId());
	}

	protected void AfterApply(DZS_Operation op, bool notifySync)
	{
		if (m_Protocol)
			ProtocolWrite(op);
		if (m_Shadow)
			ShadowCheck(op);
		if (m_IsShadow)
			return;
		m_Kernel.GetEvents().FireOperation(this, op);
		if (notifySync && m_Sync && op.GetOrigin() != DZS_Origin.REMOTE)
		{
			array<string> t431 = op.ToText();
			m_Sync.OnOperation(t431, op.GetOrigin(), op.GetId());
		}
	}

	protected void FailureText(DZS_ApplyFailure fail, DZS_SubmitResult r)
	{
		if (fail.m_Code == "missing")
		{
			if (fail.m_Kind == DZS_KIND_GROUP)
			{
				r.m_ReasonDe = "Gruppe wurde gelöscht";
				r.m_ReasonEn = "Group was deleted";
			}
			else
			{
				r.m_ReasonDe = "Objekt wurde gelöscht";
				r.m_ReasonEn = "Object was deleted";
			}
		}
		else if (fail.m_Code == "changed")
		{
			r.m_ReasonDe = "Objekt wurde geändert";
			r.m_ReasonEn = "Object was changed";
		}
		else
		{
			r.m_ReasonDe = "Änderung passt nicht mehr";
			r.m_ReasonEn = "Change no longer fits";
		}
	}

	protected void RecordStep(DZS_Operation op, string nameDe, string nameEn, string mergeKey)
	{
		float nowMs = GetGame().GetTickTime() * 1000.0;
		int now = nowMs;
		DZS_Step last = m_History.LastStep();
		// "gesture:…" keys belong to one gesture of the user and merge however long it takes
		bool inTime = mergeKey.IndexOf("gesture:") == 0;
		if (last && now - last.m_LastTime < 1000)
			inTime = true;
		if (mergeKey != "" && last && last.m_MergeKey == mergeKey && inTime && m_History.Position() == m_History.Count())
		{
			last.m_Ops.Insert(op);
			last.m_LastTime = now;
			Touch(last, op);
			return;
		}
		DZS_Step step = new DZS_Step();
		string stepName = DZS_Str.StepName(DZS_Texts.Pick(nameDe, nameEn));
		step.m_NameDe = stepName;
		step.m_NameEn = stepName;
		step.m_MergeKey = mergeKey;
		step.m_LastTime = now;
		step.m_Ops.Insert(op);
		Touch(step, op);
		m_History.Push(step);
	}

	// Which groups and whether the manifest's project part a step changes (for "clean" checks).
	protected void Touch(DZS_Step step, DZS_Operation op)
	{
		for (int i = 0; i < op.CountChanges(); i++)
		{
			DZS_Change c = op.PeekChange(i);
			string id = c.GetId();
			string kind = c.GetSnapshotKind();
			if (kind == "")
				kind = GetKind(id);
			if (kind == DZS_KIND_GROUP || kind == DZS_KIND_PROJECT)
				step.m_Manifest = true;
			if (c.GetKind() == DZS_ChangeKind.SET_HIDDEN || c.GetKind() == DZS_ChangeKind.SET_LOCKED || c.GetKind() == DZS_ChangeKind.MOVE)
				step.m_Manifest = true;
			string g = FindAncestorOfKind(id, DZS_KIND_GROUP);
			DZS_ElementData snapOf = c.GetSnapshot();
			if (g == "" && snapOf)
			{
				string snapParent = snapOf.m_Parent;
				g = FindAncestorOfKind(snapParent, DZS_KIND_GROUP);
			}
			if (g == "" && kind == DZS_KIND_GROUP)
				g = id;
			if (g != "")
				step.m_Groups.Set(g, true);
			if (c.GetKind() == DZS_ChangeKind.MOVE)
			{
				string g2 = FindAncestorOfKind(c.GetFromParent(), DZS_KIND_GROUP);
				if (g2 != "")
					step.m_Groups.Set(g2, true);
			}
		}
	}

	// ---- undo / redo ----

	bool CanUndo() { return m_History.CanUndo(); }
	bool CanRedo() { return m_History.CanRedo(); }

	string GetUndoName(bool english = false)
	{
		DZS_Step s = m_History.UndoStep();
		if (!s)
			return "";
		if (english)
			return s.m_NameEn;
		return s.m_NameDe;
	}

	string GetRedoName(bool english = false)
	{
		DZS_Step s = m_History.RedoStep();
		if (!s)
			return "";
		if (english)
			return s.m_NameEn;
		return s.m_NameDe;
	}

	// Save state beyond the history: "" normal, "new" (never saved, shows
	// "Not saved yet"), "opened" ("Saved" without clock), "restored" (from a backup, unsaved
	// until saved), "manifest" (project file missing: one extra unsaved change).
	void SetSaveMode(string mode)
	{
		m_SaveMode = mode;
		m_Kernel.GetEvents().FireHistoryChanged(this);
	}

	string GetSaveMode() { return m_SaveMode; }
	bool IsSaving() { return m_Saving; }
	bool IsSaveFailed() { return m_SaveFailed; }

	void SetSaving(bool on)
	{
		m_Saving = on;
		m_Kernel.GetEvents().FireHistoryChanged(this);
	}

	void SetSaveFailed(bool on)
	{
		m_SaveFailed = on;
		m_Kernel.GetEvents().FireHistoryChanged(this);
	}

	// What a save that starts now writes; MarkSavedAt(mark) once it is written.
	DZS_SaveMark SaveMark()
	{
		DZS_SaveMark mk = new DZS_SaveMark();
		mk.m_Step = m_History.UndoStep();
		mk.m_StepSet = m_History.Position() > 0;
		mk.m_Foreign = m_Foreign;
		mk.m_Mode = m_SaveMode;
		return mk;
	}

	void MarkSavedAt(DZS_SaveMark mk)
	{
		int foreignAfter = m_Foreign - mk.m_Foreign;
		string modeNow = m_SaveMode;
		MarkSavedNow();
		m_History.MarkSavedAt(mk.m_Step, mk.m_StepSet);
		if (foreignAfter > 0)
			m_Foreign = foreignAfter;
		if (modeNow != mk.m_Mode)
			m_SaveMode = modeNow;
		m_Kernel.GetEvents().FireHistoryChanged(this);
	}

	void MarkSavedNow()
	{
		m_History.MarkSaved();
		m_Foreign = 0;
		m_ForeignTouched = null;
		m_SaveMode = "";
		m_SaveFailed = false;
		int hh, mm, ss;
		GetHourMinuteSecond(hh, mm, ss);
		m_SavedClock = hh.ToString() + ":" + mm.ToStringLen(2);
		m_Kernel.GetEvents().FireHistoryChanged(this);
	}

	bool IsUnsaved()
	{
		return m_SaveMode == "restored" || m_SaveMode == "manifest" || m_History.Unsaved() > 0 || m_Foreign > 0;
	}

	// The host's save state: operations of others since the last save count
	// as unsaved changes, and the groups they touch are not clean.
	void Sync_CountForeign(DZS_SyncSlot slot, array<string> chunks)
	{
		if (!Sync_Allowed(slot))
			return;
		string error;
		DZS_Operation op = DZS_Operation.FromText(chunks, error);
		if (!op)
			return;
		if (!m_ForeignTouched)
			m_ForeignTouched = new DZS_Step();
		Touch(m_ForeignTouched, op);
		m_Foreign++;
		m_Kernel.GetEvents().FireHistoryChanged(this);
	}

	int GetForeignUnsaved() { return m_Foreign; }

	int GetHistoryUnsaved() { return m_History.Unsaved(); }

	int GetUnsavedCount()
	{
		int n = m_History.Unsaved() + m_Foreign;
		if (m_SaveMode == "manifest")
			n++;
		return n;
	}
	// Clock time of the last save in this session ("10:42"); empty if never saved.
	string GetSavedClock() { return m_SavedClock; }
	bool IsGroupClean(string groupId)
	{
		if (m_ForeignTouched && m_ForeignTouched.m_Groups.Contains(groupId))
			return false;
		return m_SaveMode != "restored" && m_History.IsGroupClean(groupId);
	}

	bool IsManifestClean()
	{
		if (m_ForeignTouched && m_ForeignTouched.m_Manifest)
			return false;
		return m_SaveMode != "restored" && m_SaveMode != "manifest" && m_History.IsManifestClean();
	}
	int GetHistoryLength() { return m_History.Count(); }
	int GetHistoryPosition() { return m_History.Position(); }

	bool Undo()
	{
		DZS_Step step = m_History.UndoStep();
		if (!step)
			return false;
		if (GateClosed())
		{
			Refused();
			return false;
		}
		DZS_Operation op = new DZS_Operation("core:undo", 1);
		for (int i = step.m_Ops.Count() - 1; i >= 0; i--)
		{
			DZS_Operation part = step.m_Ops[i];
			for (int k = part.CountChanges() - 1; k >= 0; k--)
			{
				DZS_Change p605 = part.PeekChange(k);
				DZS_Change t605 = p605.Inverse();
				op.AddChange(t605);
			}
		}
		op.Seal(NewOpId(), m_Kernel.GetAuthorId(), m_Kernel.GetAuthorName(), DZS_Origin.LOCAL);
		if (TooLarge(op))
			return false;
		DZS_SubmitResult r = Apply(op, true, true);
		if (!r.Ok())
		{
			DropStep(step, r);
			return false;
		}
		m_UndoRedoOf.Set(op.GetId(), step);
		m_History.MovedBack();
		FireStepObjects(op, false);
		m_Kernel.GetEvents().FireNotice(new DZS_Notice("history", "Rückgängig: " + step.m_NameDe, "Undo: " + step.m_NameEn));
		m_Kernel.GetEvents().FireHistoryChanged(this);
		return true;
	}

	bool Redo()
	{
		DZS_Step step = m_History.RedoStep();
		if (!step)
			return false;
		if (GateClosed())
		{
			Refused();
			return false;
		}
		DZS_Operation op = new DZS_Operation("core:redo", 1);
		foreach (DZS_Operation part : step.m_Ops)
		{
			for (int k = 0; k < part.CountChanges(); k++)
			{
				DZS_Change t630 = part.PeekChange(k);
				op.AddChange(t630);
			}
		}
		op.Seal(NewOpId(), m_Kernel.GetAuthorId(), m_Kernel.GetAuthorName(), DZS_Origin.LOCAL);
		if (TooLarge(op))
			return false;
		DZS_SubmitResult r = Apply(op, true, true);
		if (!r.Ok())
		{
			DropStep(step, r);
			return false;
		}
		m_UndoRedoOf.Set(op.GetId(), step);
		m_History.MovedForward();
		FireStepObjects(op, true);
		m_Kernel.GetEvents().FireNotice(new DZS_Notice("history", "Wiederholt: " + step.m_NameDe, "Redo: " + step.m_NameEn));
		m_Kernel.GetEvents().FireHistoryChanged(this);
		return true;
	}

	protected void DropStep(DZS_Step step, DZS_SubmitResult r)
	{
		m_History.RemoveStep(step);
		m_Kernel.GetEvents().FireNotice(new DZS_Notice("history", "Schritt nicht mehr möglich: " + r.m_ReasonDe, "Step no longer possible: " + r.m_ReasonEn, "warning"));
		m_Kernel.GetEvents().FireHistoryChanged(this);
	}

	// ---- loading ----

	// Applies a whole state from its text form onto this (empty) instance as one system operation.
	bool LoadState(array<string> chunks, out string error)
	{
		if (!IsEmpty())
		{
			error = "instance is not empty";
			return false;
		}
		DZS_Operation load = DZS_Instance.BuildLoad(m_Types, chunks, error);
		if (!load)
			return false;
		DZS_SubmitResult r = SubmitSystem(load);
		if (!r.Ok())
		{
			error = "load failed: " + r.m_ReasonEn;
			return false;
		}
		m_History.Clear();
		return true;
	}

	// Applies a load built with BeginOperation("core:load") as one system operation; the history starts empty.
	bool LoadBuilt(DZS_OpBuilder b, out string error)
	{
		if (!IsEmpty())
		{
			error = "instance is not empty";
			return false;
		}
		DZS_SubmitResult r = SubmitSystem(b.GetOperation());
		if (!r.Ok())
		{
			error = "load failed: " + r.m_ReasonEn;
			return false;
		}
		m_History.Clear();
		return true;
	}

	// Part of a load (changes from … to-1) as one system operation; opening in slices applies a load over several
	// frames this way, the first part into the empty instance.
	bool LoadPart(DZS_Operation all, int from, int to, out string error)
	{
		if (from == 0 && !IsEmpty())
		{
			error = "instance is not empty";
			return false;
		}
		DZS_Operation op = new DZS_Operation("core:load", 1);
		for (int i = from; i < to; i++)
			op.AddChange(all.PeekChange(i));
		DZS_SubmitResult r = SubmitSystem(op);
		if (!r.Ok())
		{
			error = "load failed: " + r.m_ReasonEn;
			return false;
		}
		m_History.Clear();
		return true;
	}

	bool KnowsKind(string kind) { return m_Types.KnowsKind(kind); }

	static DZS_Operation BuildLoad(DZS_TypeRegistry types, array<string> chunks, out string error)
	{
		DZS_Json root = DZS_Json.Parse(chunks, error);
		if (!root)
			return null;
		if (!root.IsObj() || !root.Has("textform") || !root.Has("state"))
		{
			error = "not a project state";
			return null;
		}
		int tf = root.GetInt("textform");
		if (tf > DZS_TEXTFORM_VERSION)
		{
			error = "text form version " + tf + " is newer than " + DZS_TEXTFORM_VERSION;
			return null;
		}
		DZS_Json st = root.Get("state");
		DZS_Json elems = st.Get("elements");
		if (!elems || !elems.IsArr())
		{
			error = "state without elements";
			return null;
		}
		string missing = "";
		DZS_Json kinds = st.Get("kinds");
		if (kinds && kinds.IsObj())
		{
			for (int i = 0; i < kinds.Count(); i++)
			{
				string kn = kinds.m_Keys[i];
				if (!types.KnowsKind(kn))
					missing += " " + DZS_Str.PluginOfType(kn) + " (" + kn + " v" + kinds.GetInt(kn) + ")";
			}
		}
		DZS_Operation op = new DZS_Operation("core:load", 1);
		for (int k = 0; k < elems.Count(); k++)
		{
			DZS_ElementData e = DZS_ElementData.FromJson(elems.At(k), error);
			if (!e)
				return null;
			if (!types.KnowsKind(e.m_Kind) && !missing.Contains(e.m_Kind))
				missing += " " + DZS_Str.PluginOfType(e.m_Kind) + " (" + e.m_Kind + ")";
			DZS_Change t736 = DZS_Change.Create(e);
			op.AddChange(t736);
		}
		if (missing != "")
		{
			error = "fehlende Plugins:" + missing;
			return null;
		}
		return op;
	}

	// ---- sync slot ----

	bool AttachSync(DZS_SyncSlot slot)
	{
		if (m_Sync || !slot)
			return false;
		m_Sync = slot;
		m_Unconfirmed.Clear();
		slot.Bind(this);
		return true;
	}

	void DetachSync(DZS_SyncSlot slot)
	{
		if (m_Sync != slot)
			return;
		m_Sync = null;
		m_NoExternalSystemOps = false;
	}

	bool Sync_Allowed(DZS_SyncSlot slot)
	{
		return slot && (slot == m_Sync || slot == m_ShadowSlot);
	}

	void Sync_SetNoExternalSystemOps(DZS_SyncSlot slot, bool on)
	{
		if (Sync_Allowed(slot))
			m_NoExternalSystemOps = on;
	}

	DZS_SubmitResult Sync_SubmitRemote(DZS_SyncSlot slot, array<string> chunks)
	{
		if (!Sync_Allowed(slot))
			return DZS_SubmitResult.Make(DZS_SubmitStatus.INVALID);
		string error;
		DZS_Operation parsed = DZS_Operation.FromText(chunks, error);
		if (!parsed)
		{
			DZS_SubmitResult r = DZS_SubmitResult.Make(DZS_SubmitStatus.INVALID);
			r.m_ReasonEn = error;
			return r;
		}
		DZS_Operation op = parsed.CopyUnsealed();
		op.Seal(parsed.GetId(), parsed.GetAuthor(), parsed.GetAuthorName(), DZS_Origin.REMOTE);
		return Apply(op, true, false);
	}

	void Sync_Confirm(DZS_SyncSlot slot, string opId)
	{
		if (!Sync_Allowed(slot))
			return;
		for (int i = 0; i < m_Unconfirmed.Count(); i++)
		{
			if (m_Unconfirmed[i].GetId() == opId)
			{
				m_Unconfirmed.RemoveOrdered(i);
				return;
			}
		}
	}

	// Takes back all unconfirmed own operations, applies the remote ones, re-applies the own ones.
	// Returns the ids of own operations that failed their precondition and were dropped.
	array<string> Sync_Rebase(DZS_SyncSlot slot, array<ref array<string>> remoteOps)
	{
		array<string> dropped = new array<string>();
		if (!Sync_Allowed(slot))
			return dropped;
		array<ref DZS_Operation> own = new array<ref DZS_Operation>();
		foreach (DZS_Operation u : m_Unconfirmed)
			own.Insert(u);
		m_Unconfirmed.Clear();
		for (int i = own.Count() - 1; i >= 0; i--)
		{
			DZS_Operation rev = new DZS_Operation("core:revert", 1);
			for (int k = own[i].CountChanges() - 1; k >= 0; k--)
			{
				DZS_Operation o822 = own[i];
				DZS_Change p822 = o822.PeekChange(k);
				DZS_Change t822 = p822.Inverse();
				rev.AddChange(t822);
			}
			rev.Seal(NewOpId(), m_Kernel.GetAuthorId(), m_Kernel.GetAuthorName(), DZS_Origin.SYSTEM);
			DZS_SubmitResult rr = Apply(rev, false, false);
			if (!rr.Ok())
				DZS_Log.Error("rebase: revert of " + own[i].GetId() + " failed: " + rr.m_ReasonEn);
		}
		foreach (array<string> chunks : remoteOps)
		{
			DZS_SubmitResult rem = Sync_SubmitRemote(slot, chunks);
			if (rem.m_Status == DZS_SubmitStatus.UNKNOWN_TYPE || rem.m_Status == DZS_SubmitStatus.INVALID || rem.m_Status == DZS_SubmitStatus.PRECONDITION)
				slot.OnRejected(rem);
		}
		foreach (DZS_Operation again : own)
		{
			DZS_Operation re = again.CopyUnsealed();
			re.Seal(again.GetId(), again.GetAuthor(), again.GetAuthorName(), DZS_Origin.LOCAL);
			DZS_SubmitResult ar = Apply(re, false, false);
			if (ar.Ok())
			{
				ReplaceInHistory(again, re);
				continue;
			}
			dropped.Insert(again.GetId());
			DZS_Step s = m_History.FindStepOf(again);
			if (!s)
				s = m_UndoRedoOf.Get(again.GetId());
			if (s)
				DropStep(s, ar);
		}
		return dropped;
	}

	protected void ReplaceInHistory(DZS_Operation oldOp, DZS_Operation newOp)
	{
		DZS_Step s = m_History.FindStepOf(oldOp);
		if (!s)
			return;
		int idx = s.m_Ops.Find(oldOp);
		s.m_Ops.Set(idx, newOp);
	}

	array<string> Sync_ExportState(DZS_SyncSlot slot)
	{
		if (!Sync_Allowed(slot))
			return new array<string>();
		return ExportState();
	}

	// ---- protocol (developer switch) ----

	void EnableProtocol()
	{
		if (m_Protocol || m_IsShadow)
			return;
		m_Protocol = true;
		m_Shadow = new DZS_Instance(m_Kernel, m_Label + "-check", true);
		m_ShadowSlot = new DZS_SyncSlot();
		m_Shadow.AttachSync(m_ShadowSlot);
		if (!IsEmpty())
		{
			string error;
			array<string> t883 = ExportState();
			if (!m_Shadow.LoadState(t883, error))
				DZS_Log.Error("protocol: check instance failed to load: " + error);
		}
		DZS_Log.Info("protocol on for instance " + m_Label + " (" + m_Code + ")");
	}

	bool IsProtocolOn() { return m_Protocol; }

	protected void ProtocolWrite(DZS_Operation op)
	{
		m_ProtocolSeq++;
		array<string> chunks = op.ToText();
		for (int i = 0; i < chunks.Count(); i++)
			Print(DZS_TAG + " protocol " + m_Label + " #" + m_ProtocolSeq + " " + (i + 1) + "/" + chunks.Count() + " " + chunks[i]);
	}

	// Replays the operation from its text form in the check instance; any difference means a side path.
	protected void ShadowCheck(DZS_Operation op)
	{
		DZS_Operation copy = op.CopyUnsealed();
		copy.Seal(op.GetId() + "~" + m_ProtocolSeq, op.GetAuthor(), op.GetAuthorName(), op.GetOrigin());
		array<string> t904 = copy.ToText();
		DZS_SubmitResult r = m_ShadowSlot.SubmitRemote(t904);
		if (!r.Ok())
		{
			DZS_Log.Error("side path: check instance rejected " + op.GetId() + " (" + r.m_ReasonEn + ")");
			return;
		}
		array<string> a = ExportState();
		array<string> b = m_Shadow.ExportState();
		bool same = a.Count() == b.Count();
		for (int i = 0; same && i < a.Count(); i++)
			same = a[i] == b[i];
		if (!same)
			DZS_Log.Error("side path: state differs from replay after " + op.GetId());
	}
}
