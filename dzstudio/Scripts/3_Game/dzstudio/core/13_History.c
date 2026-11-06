// One undo step: one or more consecutive own operations from one user action, with its name.
class DZS_Step : Managed
{
	string m_NameDe;
	string m_NameEn;
	string m_MergeKey;
	int m_LastTime;
	ref array<ref DZS_Operation> m_Ops;
	ref map<string, bool> m_Groups;
	bool m_Manifest;

	void DZS_Step()
	{
		m_Ops = new array<ref DZS_Operation>();
		m_Groups = new map<string, bool>();
	}
}

// Linear, unbounded history with a position and a save point; lives only while the project is open.
class DZS_History : Managed
{
	protected ref array<ref DZS_Step> m_Steps;
	protected int m_Pos;
	protected int m_SavePoint;
	protected bool m_SaveLost;
	protected int m_LostOffset;

	void DZS_History()
	{
		m_Steps = new array<ref DZS_Step>();
	}

	int Count() { return m_Steps.Count(); }
	int Position() { return m_Pos; }
	bool CanUndo() { return m_Pos > 0; }
	bool CanRedo() { return m_Pos < m_Steps.Count(); }

	DZS_Step StepAt(int i)
	{
		if (i < 0 || i >= m_Steps.Count())
			return null;
		return m_Steps[i];
	}

	DZS_Step UndoStep() { return StepAt(m_Pos - 1); }
	DZS_Step RedoStep() { return StepAt(m_Pos); }
	DZS_Step LastStep()
	{
		if (m_Pos != m_Steps.Count())
			return null;
		return StepAt(m_Pos - 1);
	}

	void Push(DZS_Step step)
	{
		if (m_Pos < m_Steps.Count())
		{
			if (m_SavePoint > m_Pos)
			{
				m_LostOffset += m_SavePoint - m_Pos;
				m_SavePoint = m_Pos;
				m_SaveLost = true;
			}
			for (int i = m_Steps.Count() - 1; i >= m_Pos; i--)
				m_Steps.RemoveOrdered(i);
		}
		m_Steps.Insert(step);
		m_Pos++;
	}

	void MovedBack() { m_Pos--; }
	void MovedForward() { m_Pos++; }

	void RemoveStep(DZS_Step step)
	{
		int idx = m_Steps.Find(step);
		if (idx < 0)
			return;
		m_Steps.RemoveOrdered(idx);
		if (idx < m_Pos)
			m_Pos--;
		if (idx < m_SavePoint)
			m_SavePoint--;
	}

	void MarkSaved()
	{
		m_SavePoint = m_Pos;
		m_SaveLost = false;
		m_LostOffset = 0;
	}

	// Save point at a step taken earlier (UndoStep() then): changes made since stay unsaved.
	void MarkSavedAt(DZS_Step last, bool lastSet)
	{
		int pos = 0;
		if (lastSet)
			pos = m_Steps.Find(last) + 1;
		if (lastSet && pos == 0)
		{
			m_SavePoint = m_Pos;
			m_SaveLost = true;
			m_LostOffset = 1;
			return;
		}
		m_SavePoint = pos;
		m_SaveLost = false;
		m_LostOffset = 0;
	}

	void Clear()
	{
		m_Steps.Clear();
		m_Pos = 0;
		MarkSaved();
	}

	int Unsaved()
	{
		return m_LostOffset + Math.AbsInt(m_Pos - m_SavePoint);
	}

	bool IsGroupClean(string groupId)
	{
		if (m_SaveLost)
			return false;
		for (int i = Math.Min(m_Pos, m_SavePoint); i < Math.Max(m_Pos, m_SavePoint); i++)
		{
			if (m_Steps[i].m_Groups.Contains(groupId))
				return false;
		}
		return true;
	}

	bool IsManifestClean()
	{
		if (m_SaveLost)
			return false;
		for (int i = Math.Min(m_Pos, m_SavePoint); i < Math.Max(m_Pos, m_SavePoint); i++)
		{
			if (m_Steps[i].m_Manifest)
				return false;
		}
		return true;
	}

	DZS_Step FindStepOf(DZS_Operation op)
	{
		foreach (DZS_Step s : m_Steps)
		{
			if (s.m_Ops.Find(op) >= 0)
				return s;
		}
		return null;
	}
}
