// The sync slot of one instance. At most one plugin hooks in per open project by
// subclassing this and attaching it; the transport is the plugin's business.
class DZS_SyncSlot : Managed
{
	protected DZS_Instance m_Instance;

	void Bind(DZS_Instance inst)
	{
		m_Instance = inst;
	}

	DZS_Instance GetInstance() { return m_Instance; }

	// Every local operation and every system operation that arose in this instance, after applying, in order.
	void OnOperation(array<string> text, DZS_Origin origin, string opId)
	{
	}

	// A remote operation the instance did not apply (unknown type, newer version, broken text, precondition).
	void OnRejected(DZS_SubmitResult result)
	{
	}

	void SetNoExternalSystemOps(bool on)
	{
		if (m_Instance)
			m_Instance.Sync_SetNoExternalSystemOps(this, on);
	}

	DZS_SubmitResult SubmitRemote(array<string> text)
	{
		if (!m_Instance)
			return DZS_SubmitResult.Make(DZS_SubmitStatus.INVALID);
		DZS_SubmitResult r = m_Instance.Sync_SubmitRemote(this, text);
		if (r.m_Status == DZS_SubmitStatus.UNKNOWN_TYPE || r.m_Status == DZS_SubmitStatus.INVALID)
			OnRejected(r);
		return r;
	}

	void CountForeign(array<string> text)
	{
		if (m_Instance)
			m_Instance.Sync_CountForeign(this, text);
	}

	void Confirm(string opId)
	{
		if (m_Instance)
			m_Instance.Sync_Confirm(this, opId);
	}

	array<string> Rebase(array<ref array<string>> remoteOps)
	{
		if (!m_Instance)
			return new array<string>();
		return m_Instance.Sync_Rebase(this, remoteOps);
	}

	array<string> ExportState()
	{
		if (!m_Instance)
			return new array<string>();
		return m_Instance.Sync_ExportState(this);
	}

	void Detach()
	{
		if (m_Instance)
			m_Instance.DetachSync(this);
		m_Instance = null;
	}
}
