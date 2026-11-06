// Exact decimal arithmetic for float32 values: the shortest text that reads back as the same float32
// and an exact reader for decimal text.
// The game's own float-to-text gives only 6 significant digits.

// A non-negative decimal 0.d1d2d3… × 10^m_Exp; m_Digits has no leading or trailing zeros ("" = zero).
class DZS_Dec : Managed
{
	string m_Digits;
	int m_Exp;

	void DZS_Dec(string digits = "", int exp = 0)
	{
		m_Digits = digits;
		m_Exp = exp;
		Trim();
	}

	protected void Trim()
	{
		int n = m_Digits.Length();
		int lead = 0;
		while (lead < n && m_Digits.Get(lead) == "0")
			lead++;
		if (lead == n)
		{
			m_Digits = "";
			m_Exp = 0;
			return;
		}
		int end = n;
		while (end > lead && m_Digits.Get(end - 1) == "0")
			end--;
		m_Digits = m_Digits.Substring(lead, end - lead);
		m_Exp -= lead;
	}

	bool IsZero() { return m_Digits == ""; }

	// -1, 0, 1
	static int Compare(DZS_Dec a, DZS_Dec b)
	{
		if (a.IsZero() || b.IsZero())
		{
			if (a.IsZero() && b.IsZero())
				return 0;
			if (a.IsZero())
				return -1;
			return 1;
		}
		if (a.m_Exp != b.m_Exp)
		{
			if (a.m_Exp > b.m_Exp)
				return 1;
			return -1;
		}
		int n = Math.Max(a.m_Digits.Length(), b.m_Digits.Length());
		for (int i = 0; i < n; i++)
		{
			int da = 0;
			int db = 0;
			if (i < a.m_Digits.Length())
				da = a.m_Digits.Get(i).ToAscii() - 48;
			if (i < b.m_Digits.Length())
				db = b.m_Digits.Get(i).ToAscii() - 48;
			if (da != db)
			{
				if (da > db)
					return 1;
				return -1;
			}
		}
		return 0;
	}

	// Exact value of n × 2^e (n ≥ 0, below 2^31).
	static DZS_Dec FromBinary(int n, int e)
	{
		array<int> limbs = new array<int>();
		limbs.Insert(n % 10000);
		limbs.Insert((n / 10000) % 10000);
		limbs.Insert(n / 100000000);
		int point = 0;
		if (e >= 0)
		{
			int left = e;
			while (left > 0)
			{
				int k = Math.Min(left, 13);
				MulSmall(limbs, 1 << k);
				left -= k;
			}
		}
		else
		{
			int fives = -e;
			point = fives;
			while (fives > 0)
			{
				int f = Math.Min(fives, 5);
				int mul = 1;
				for (int j = 0; j < f; j++)
					mul *= 5;
				MulSmall(limbs, mul);
				fives -= f;
			}
		}
		string s = "";
		for (int i = limbs.Count() - 1; i >= 0; i--)
			s += limbs[i].ToStringLen(4);
		// s is the integer N; value = N × 10^-point
		DZS_Dec d = new DZS_Dec(s, s.Length() - point);
		return d;
	}

	protected static void MulSmall(array<int> limbs, int mul)
	{
		int carry = 0;
		for (int i = 0; i < limbs.Count(); i++)
		{
			int v = limbs[i] * mul + carry;
			limbs[i] = v % 10000;
			carry = v / 10000;
		}
		while (carry > 0)
		{
			limbs.Insert(carry % 10000);
			carry /= 10000;
		}
	}

	// Rounds to p significant digits; dir 0 = nearest (ties down), 1 = up, -1 = down (truncate).
	DZS_Dec Round(int p, int dir)
	{
		if (IsZero() || m_Digits.Length() <= p)
			return new DZS_Dec(m_Digits, m_Exp);
		string head = m_Digits.Substring(0, p);
		bool up = false;
		if (dir > 0)
			up = true;
		else if (dir == 0)
		{
			int next = m_Digits.Get(p).ToAscii() - 48;
			if (next > 5)
				up = true;
			else if (next == 5 && m_Digits.Length() > p + 1)
				up = true;
		}
		if (!up)
			return new DZS_Dec(head, m_Exp);
		return Increment(head, m_Exp);
	}

	// head + one unit in its last place
	static DZS_Dec Increment(string head, int exp)
	{
		string r = "";
		int carry = 1;
		for (int i = head.Length() - 1; i >= 0; i--)
		{
			int d = head.Get(i).ToAscii() - 48 + carry;
			carry = d / 10;
			r = (d % 10).ToString() + r;
		}
		if (carry > 0)
		{
			r = "1" + r;
			exp++;
		}
		return new DZS_Dec(r, exp);
	}

	// Plain text without exponent; whole values end in ".0".
	string ToPlain()
	{
		if (IsZero())
			return "0.0";
		int p = m_Digits.Length();
		if (m_Exp <= 0)
		{
			string z = "";
			for (int i = 0; i < -m_Exp; i++)
				z += "0";
			return "0." + z + m_Digits;
		}
		if (m_Exp >= p)
		{
			string t = "";
			for (int k = 0; k < m_Exp - p; k++)
				t += "0";
			return m_Digits + t + ".0";
		}
		string ip = m_Digits.Substring(0, m_Exp);
		string fp = m_Digits.Substring(m_Exp, p - m_Exp);
		return ip + "." + fp;
	}

	// Reads JSON number text (sign handled by the caller); false if it is not a number.
	static bool Parse(string text, out DZS_Dec result, out bool negative)
	{
		negative = false;
		int n = text.Length();
		int i = 0;
		if (i < n && text.Get(i) == "-")
		{
			negative = true;
			i++;
		}
		string digits = "";
		int intDigits = 0;
		bool any = false;
		while (i < n && IsDigit(text.Get(i)))
		{
			digits += text.Get(i);
			intDigits++;
			i++;
			any = true;
		}
		if (i < n && text.Get(i) == ".")
		{
			i++;
			while (i < n && IsDigit(text.Get(i)))
			{
				digits += text.Get(i);
				i++;
				any = true;
			}
		}
		if (!any)
			return false;
		int exp = 0;
		string ce = "";
		if (i < n)
			ce = text.Get(i);
		if (ce == "e" || ce == "E")
		{
			i++;
			bool eneg = false;
			string cs = "";
			if (i < n)
				cs = text.Get(i);
			if (cs == "+" || cs == "-")
			{
				eneg = text.Get(i) == "-";
				i++;
			}
			bool eany = false;
			while (i < n && IsDigit(text.Get(i)))
			{
				if (exp < 100000)
					exp = exp * 10 + text.Get(i).ToAscii() - 48;
				i++;
				eany = true;
			}
			if (!eany)
				return false;
			if (eneg)
				exp = -exp;
		}
		if (i != n)
			return false;
		result = new DZS_Dec(digits, intDigits + exp);
		return true;
	}

	// Text for the game's own reader (a first guess only); at most 12 digits, with exponent.
	string ToPlainShort()
	{
		if (IsZero())
			return "0";
		string d = m_Digits;
		if (d.Length() > 12)
			d = d.Substring(0, 12);
		return "0." + d + "e" + m_Exp.ToString();
	}

	static bool IsDigit(string c)
	{
		int b = c.ToAscii();
		return b >= 48 && b <= 57;
	}
}

class DZS_F32
{
	static const float MAX = FLT_MAX;

	// f = m × 2^e with 2^23 ≤ m < 2^24 (or subnormal with e = -149); f ≥ 0, finite.
	static void Decompose(float f, out int m, out int e)
	{
		e = 0;
		if (f == 0)
		{
			m = 0;
			return;
		}
		while (f >= 16777216.0)
		{
			f *= 0.5;
			e++;
		}
		while (f < 8388608.0 && e > -149)
		{
			f *= 2.0;
			e--;
		}
		m = f;
	}

	static float Compose(int m, int e)
	{
		float f = m;
		while (e > 0)
		{
			f *= 2.0;
			e--;
		}
		while (e < 0)
		{
			f *= 0.5;
			e++;
		}
		return f;
	}

	static bool IsFinite(float f)
	{
		return f == f && f <= MAX && f >= -MAX;
	}

	// Bounds of the decimals that read as m × 2^e; inclusive when m is even (ties to even).
	protected static void Bounds(int m, int e, out DZS_Dec lo, out DZS_Dec hi)
	{
		if (m == 8388608 && e > -149)
			lo = DZS_Dec.FromBinary(4 * m - 1, e - 2);
		else if (m == 0)
			lo = new DZS_Dec();
		else
			lo = DZS_Dec.FromBinary(2 * m - 1, e - 1);
		hi = DZS_Dec.FromBinary(2 * m + 1, e - 1);
	}

	protected static bool Inside(DZS_Dec d, DZS_Dec lo, DZS_Dec hi, bool inclusive)
	{
		int a = DZS_Dec.Compare(d, lo);
		int b = DZS_Dec.Compare(d, hi);
		if (inclusive)
			return a >= 0 && b <= 0;
		return a > 0 && b < 0;
	}

	// Shortest decimal text that reads back as exactly this float32, never with an exponent.
	static string ToText(float f)
	{
		if (!IsFinite(f))
			return "0.0";
		if (f == 0)
			return "0.0";
		bool neg = f < 0;
		if (neg)
			f = -f;
		int m, e;
		Decompose(f, m, e);
		DZS_Dec exact = DZS_Dec.FromBinary(m, e);
		DZS_Dec lo, hi;
		Bounds(m, e, lo, hi);
		bool inclusive = (m % 2) == 0;
		DZS_Dec best = exact;
		// If any text of up to 6 digits reads back, it is the 6-digit rounding (the interval is far
		// narrower than the 6-digit grid); otherwise 7 to 9 digits are needed.
		// Subnormal values have a fixed absolute spacing, so they always try every length.
		DZS_Dec six = exact.Round(6, 0);
		int firstP = 7;
		if (m < 8388608)
			firstP = 1;
		if (firstP == 7 && Inside(six, lo, hi, inclusive))
			best = six;
		else
		{
			for (int p = firstP; p <= 9; p++)
			{
				DZS_Dec near = exact.Round(p, 0);
				if (Inside(near, lo, hi, inclusive))
				{
					best = near;
					break;
				}
				DZS_Dec down = exact.Round(p, -1);
				DZS_Dec up = exact.Round(p, 1);
				bool okDown = Inside(down, lo, hi, inclusive);
				bool okUp = Inside(up, lo, hi, inclusive);
				if (okDown && !okUp)
				{
					best = down;
					break;
				}
				if (okUp && !okDown)
				{
					best = up;
					break;
				}
				if (okUp && okDown)
				{
					best = near;
					break;
				}
			}
		}
		string s = best.ToPlain();
		if (neg)
			return "-" + s;
		return s;
	}

	// Exact float32 of JSON number text, rounded to nearest-even. False if not a number or not finite.
	static bool FromText(string text, out float result)
	{
		DZS_Dec d;
		bool neg;
		if (!DZS_Dec.Parse(text, d, neg))
			return false;
		if (d.IsZero())
		{
			result = 0;
			return true;
		}
		DZS_Dec maxHi = DZS_Dec.FromBinary(2 * 16777215 + 1, 104 - 1);
		int cmpMax = DZS_Dec.Compare(d, maxHi);
		if (cmpMax >= 0)
			return false;
		float guess = d.ToPlainShort().ToFloat();
		if (guess < 0)
			guess = -guess;
		if (!IsFinite(guess))
			guess = MAX;
		int m, e;
		Decompose(guess, m, e);
		for (int step = 0; step < 64; step++)
		{
			DZS_Dec lo, hi;
			Bounds(m, e, lo, hi);
			bool inclusive = (m % 2) == 0;
			if (Inside(d, lo, hi, inclusive))
				break;
			if (DZS_Dec.Compare(d, lo) <= 0)
			{
				if (m == 0)
					break;
				m--;
				if (m < 8388608 && e > -149)
				{
					m = m * 2 + 1;
					e--;
				}
			}
			else
			{
				m++;
				if (m >= 16777216)
				{
					m = m / 2;
					e++;
				}
			}
		}
		result = Compose(m, e);
		if (neg)
			result = -result;
		return true;
	}
}
