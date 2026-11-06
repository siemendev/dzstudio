// Subscribe to kernel events by subclassing and registering with DZS_EventBus.Subscribe.
// Views (panels, scene, counters) follow these events, never the caller of a change.
class DZS_EventListener : Managed
{
	void OnOperationApplied(DZS_Instance inst, DZS_Operation op) {}
	void OnHistoryChanged(DZS_Instance inst) {}
	void OnNotice(DZS_Notice notice) {}
	void OnInstanceOpened(DZS_Instance inst) {}
	void OnInstanceClosing(DZS_Instance inst) {}
	void OnShownInstanceChanged(DZS_Instance inst) {}
	void OnSettingChanged(string key) {}
	// Events between plugins: a topic named "plugin.what" and an optional payload.
	void OnPluginEvent(string topic, Managed payload) {}
}

class DZS_EventBus : Managed
{
	protected ref array<DZS_EventListener> m_Listeners;

	void DZS_EventBus()
	{
		m_Listeners = new array<DZS_EventListener>();
	}

	void Subscribe(DZS_EventListener l)
	{
		if (l && m_Listeners.Find(l) < 0)
			m_Listeners.Insert(l);
	}

	void Unsubscribe(DZS_EventListener l)
	{
		int i = m_Listeners.Find(l);
		if (i >= 0)
			m_Listeners.RemoveOrdered(i);
	}

	protected array<DZS_EventListener> Snapshot()
	{
		array<DZS_EventListener> copy = new array<DZS_EventListener>();
		foreach (DZS_EventListener l : m_Listeners)
		{
			if (l)
				copy.Insert(l);
		}
		return copy;
	}

	void FireOperation(DZS_Instance inst, DZS_Operation op)
	{
		DZS_WorldLines.Moved();
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnOperationApplied(inst, op);
	}

	void FireHistoryChanged(DZS_Instance inst)
	{
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnHistoryChanged(inst);
	}

	void FireNotice(DZS_Notice n)
	{
		DZS_Log.Info("notice [" + n.m_Kind + "/" + n.m_Tone + "] " + n.m_De);
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnNotice(n);
	}

	void FireInstanceOpened(DZS_Instance inst)
	{
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnInstanceOpened(inst);
	}

	void FireInstanceClosing(DZS_Instance inst)
	{
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnInstanceClosing(inst);
	}

	void FireShownInstanceChanged(DZS_Instance inst)
	{
		DZS_WorldLines.Moved();
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnShownInstanceChanged(inst);
	}

	void FirePluginEvent(string topic, Managed payload)
	{
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnPluginEvent(topic, payload);
	}

	void FireSettingChanged(string key)
	{
		array<DZS_EventListener> ls = Snapshot();
		foreach (DZS_EventListener l : ls)
			l.OnSettingChanged(key);
	}
}
