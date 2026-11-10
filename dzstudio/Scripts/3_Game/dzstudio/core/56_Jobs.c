// Long work over several frames: a job does a few milliseconds per frame (DZS_Jobs.SLICE_MS) and says how far it is,
// so the game keeps rendering. Every job shows the same progress plate (what it does, the 20 blocks of the shared
// progress bar, the exact number) once it runs longer than a moment, with a cancel knob where the job allows one —
// never while it writes. A job that must finish at once (a test, a caller that cannot wait) runs through RunNow.
class DZS_Job : Managed
{
	// what it does, shown on the plate ("Projekt wird gespeichert")
	string m_De;
	string m_En;
	// the current step, after the text ("Loot-Dateien"), "" for none
	string m_StepDe;
	string m_StepEn;
	float m_Share;
	bool m_Cancelable;
	// no plate: the work is short or its owner shows its own progress
	bool m_Quiet;
	bool m_Done;
	bool m_Cancelled;
	// called once with the job when it is complete or cancelled
	ref ScriptInvoker m_OnDone = new ScriptInvoker();
	string m_Id;
	float m_Age;
	int m_Frames;
	float m_Slowest;
	float m_Ms;

	// One slice of about 'ms' milliseconds (a single indivisible piece may take longer); true when the work is complete.
	bool Step(float ms)
	{
		return true;
	}

	// The job was cancelled before it was complete (drop what it made).
	void OnCancel() {}

	// Whether the job may be cancelled right now (false while it writes).
	bool CanCancelNow()
	{
		return m_Cancelable;
	}

	// Whether stopping it now would leave files half written: such a job runs to its end when the session ends.
	bool MustFinish()
	{
		return false;
	}

	string Label()
	{
		string t = DZS_Texts.Pick(m_De, m_En);
		string s = DZS_Texts.Pick(m_StepDe, m_StepEn);
		if (s != "")
			t += " · " + s;
		return t + " …";
	}

	void SetStep(string de, string en)
	{
		m_StepDe = de;
		m_StepEn = en;
	}

	// Whether 'ms' milliseconds since 't0' (TickCount) are used up.
	static bool Over(int t0, float ms)
	{
		return (TickCount(0) - t0) / 10000.0 >= ms;
	}
}

class DZS_JobsListener : DZS_MessageListener
{
	override void OnMessageButton(string kind)
	{
		int at = kind.IndexOf(":");
		if (at <= 0)
			return;
		DZS_Jobs.CancelId(kind.Substring(0, at));
	}
}

class DZS_Jobs
{
	static const float SLICE_MS = 4;
	// what all jobs together may take in one frame; the first job always gets its slice
	static const float FRAME_MS = 8;
	// short work does not flash a plate
	static const float SHOW_AFTER = 0.3;
	static const int PLATE_ORDER = 30;

	protected static ref array<ref DZS_Job> s_Jobs;
	protected static ref DZS_JobsListener s_Listener;
	protected static int s_Seq;
	protected static int s_Turn;

	static void Start(DZS_Job j)
	{
		if (!j || j.m_Done)
			return;
		if (!s_Jobs)
			s_Jobs = new array<ref DZS_Job>();
		if (s_Jobs.Find(j) >= 0)
			return;
		s_Seq++;
		j.m_Id = "job." + s_Seq;
		s_Jobs.Insert(j);
		DZS_Log.Info("job " + j.m_Id + " started: " + j.m_En);
	}

	static bool Running(DZS_Job j)
	{
		return j && s_Jobs && s_Jobs.Find(j) >= 0;
	}

	static int Count()
	{
		if (!s_Jobs)
			return 0;
		return s_Jobs.Count();
	}

	// Runs a job to its end in this frame (started or not): for callers that need the result now.
	static void RunNow(DZS_Job j)
	{
		if (!j || j.m_Done)
			return;
		int t0 = TickCount(0);
		while (!j.Step(1000000))
		{
		}
		j.m_Ms += (TickCount(0) - t0) / 10000.0;
		j.m_Frames++;
		End(j, false);
	}

	static void Cancel(DZS_Job j)
	{
		if (!j || j.m_Done)
			return;
		j.m_Cancelled = true;
		j.OnCancel();
		End(j, true);
	}

	static void CancelId(string id)
	{
		if (!s_Jobs)
			return;
		foreach (DZS_Job j : s_Jobs)
		{
			if (j.m_Id == id && j.CanCancelNow())
			{
				DZS_Log.Info("job " + id + " cancelled by the user");
				Cancel(j);
				return;
			}
		}
	}

	// Whether a job runs that must not stop now (it writes files).
	static bool Writing()
	{
		if (!s_Jobs)
			return false;
		foreach (DZS_Job j : s_Jobs)
		{
			if (!j.m_Done && j.MustFinish())
				return true;
		}
		return false;
	}

	// Every job gone without its callbacks (the session ends); one that writes files runs to its end first.
	static void Clear()
	{
		if (!s_Jobs)
			return;
		array<ref DZS_Job> left = s_Jobs;
		s_Jobs = null;
		foreach (DZS_Job j : left)
		{
			if (j.m_Done || !j.MustFinish())
				continue;
			DZS_Log.Info("job " + j.m_Id + " finished at the session end: " + j.m_En);
			while (!j.Step(1000000))
			{
			}
		}
	}

	// From the kernel, once per frame.
	static void Update(float dt)
	{
		if (!s_Jobs || s_Jobs.Count() == 0)
			return;
		array<DZS_Job> turn = new array<DZS_Job>();
		int n = s_Jobs.Count();
		s_Turn++;
		for (int i = 0; i < n; i++)
		{
			DZS_Job tj = s_Jobs[(i + s_Turn) % n];
			turn.Insert(tj);
		}
		int t0 = TickCount(0);
		int ran = 0;
		foreach (DZS_Job j : turn)
		{
			if (!j || j.m_Done || !s_Jobs || s_Jobs.Find(j) < 0)
				continue;
			if (ran > 0 && DZS_Job.Over(t0, FRAME_MS))
				break;
			ran++;
			int s0 = TickCount(0);
			bool done = j.Step(SLICE_MS);
			float took = (TickCount(0) - s0) / 10000.0;
			j.m_Ms += took;
			j.m_Frames++;
			if (took > j.m_Slowest)
				j.m_Slowest = took;
			if (done)
				End(j, false);
		}
		if (!s_Jobs)
			return;
		foreach (DZS_Job pj : s_Jobs)
		{
			pj.m_Age += dt;
			if (!pj.m_Quiet && pj.m_Age >= SHOW_AFTER)
				Plate(pj.m_Id, pj.Label(), pj.m_Share, pj.CanCancelNow());
		}
	}

	// The shared progress plate, also for work an owner steps itself (the save plan): shown or updated, every frame.
	static void Plate(string id, string label, float share, bool cancel)
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k || !k.GetScreen())
			return;
		if (!s_Listener)
			s_Listener = new DZS_JobsListener();
		DZS_MessageListener l = null;
		if (cancel)
			l = s_Listener;
		k.GetScreen().ShowProgress(id, PLATE_ORDER, DZS_W.Literal(label), share, l);
	}

	static void HidePlate(string id)
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (k && k.GetScreen())
			k.GetScreen().HideBanner(id);
	}

	protected static void End(DZS_Job j, bool cancelled)
	{
		if (j.m_Done)
			return;
		// held here: the list may have been its only owner
		DZS_Job keep = j;
		keep.m_Done = true;
		if (s_Jobs)
		{
			int at = s_Jobs.Find(j);
			if (at >= 0)
			{
				HidePlate(j.m_Id);
				s_Jobs.RemoveOrdered(at);
			}
		}
		string how = "done";
		if (cancelled)
			how = "cancelled";
		// work run at once (RunNow) is named only when it was long
		if (j.m_Id != "" || j.m_Ms >= 50)
		{
			string line = "job " + j.m_Id + " " + how + ": " + j.m_En;
			line += ", " + j.m_Frames + " frame(s), " + DZS_Num.Fixed(j.m_Ms, 1);
			line += " ms of work, slowest " + DZS_Num.Fixed(j.m_Slowest, 1) + " ms";
			DZS_Log.Info(line);
		}
		keep.m_OnDone.Invoke(keep);
	}
}

// The shared progress look: 20 square blocks up to the share in accent, the exact number beside them (the blocks
// round to 5 %).
class DZS_ProgressBar : Managed
{
	protected Widget m_Frame;
	protected ref array<Widget> m_Blocks = new array<Widget>();
	protected TextWidget m_Num;
	protected int m_Pct = -1;

	void DZS_ProgressBar(Widget parent)
	{
		m_Frame = DZS_W.Frame(parent);
		for (int i = 0; i < 20; i++)
			m_Blocks.Insert(DZS_W.Panel(m_Frame, DZS_Color.Get("blockEmpty")));
		m_Num = DZS_W.Text(m_Frame, DZS_Font.MONO, 12, DZS_Color.Get("textMuted"), false, true);
		SetShare(0);
	}

	static float BlocksWidth()
	{
		return DZS_Scale.Px(20 * 6 + 19 * 2);
	}

	static float NumberWidth()
	{
		return DZS_Scale.Px(44);
	}

	static float Width()
	{
		return BlocksWidth() + DZS_Scale.Px(6) + NumberWidth();
	}

	static float Height()
	{
		return DZS_Scale.Px(18);
	}

	void Place(float x, float y)
	{
		float h = Height();
		DZS_W.Place(m_Frame, x, y, Width(), h);
		float b = DZS_Scale.Px(6);
		float gap = DZS_Scale.Px(2);
		for (int i = 0; i < m_Blocks.Count(); i++)
			DZS_W.Place(m_Blocks[i], i * (b + gap), (h - b) / 2, b, b);
		float th = DZS_W.TextHeight(m_Num);
		DZS_W.Place(m_Num, BlocksWidth() + DZS_Scale.Px(6), (h - th) / 2, NumberWidth(), th);
	}

	void SetShare(float share)
	{
		float s = Math.Clamp(share, 0, 1);
		int pct = Math.Floor(s * 100);
		if (pct == m_Pct)
			return;
		m_Pct = pct;
		int filled = Math.Floor(s * 20);
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			if (i < filled)
				m_Blocks[i].SetColor(DZS_Color.Get("accent"));
			else
				m_Blocks[i].SetColor(DZS_Color.Get("blockEmpty"));
		}
		m_Num.SetText(pct.ToString() + " %");
	}

	void Show(bool on)
	{
		m_Frame.Show(on);
	}

	void Destroy()
	{
		if (m_Frame)
			m_Frame.Unlink();
	}
}
