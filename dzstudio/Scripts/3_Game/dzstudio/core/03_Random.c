class DZS_Random
{
	protected static bool s_Seeded;

	// Mixed from wall clock and the tick counter at first use, so every game start draws differently.
	static void EnsureSeeded()
	{
		if (s_Seeded)
			return;
		int y, mo, d, h, mi, s;
		GetYearMonthDay(y, mo, d);
		GetHourMinuteSecond(h, mi, s);
		int seed = TickCount(0);
		seed = seed * 31 + ((((y * 13 + mo) * 32 + d) * 24 + h) * 60 + mi) * 60 + s;
		seed = seed ^ (seed << 13);
		seed = seed ^ (seed >> 17);
		Math.Randomize(seed & 0x7fffffff);
		s_Seeded = true;
	}

	static string Code(int length)
	{
		EnsureSeeded();
		string alphabet = "0123456789abcdefghijklmnopqrstuvwxyz";
		string r = "";
		for (int i = 0; i < length; i++)
			r += alphabet.Get(Math.RandomInt(0, 36));
		return r;
	}
}
