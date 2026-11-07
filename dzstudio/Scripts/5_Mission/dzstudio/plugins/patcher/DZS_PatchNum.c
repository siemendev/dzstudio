// Decimal numbers of the patcher as text, exact to six decimals: no float32 on the way, so keys, comparisons and
// written values are the same in every implementation. Rounding is half away from zero.
class DZS_PatchNum
{
	// Splits a number text into sign, integer digits and fraction digits (exponent applied); false if it is no number.
	static bool Split(string t, out bool neg, out string ip, out string fp)
	{
		neg = false;
		ip = "";
		fp = "";
		int n = t.Length();
		int i = 0;
		if (n == 0)
			return false;
		string c0 = t.Get(0);
		if (c0 == "-" || c0 == "+")
		{
			neg = c0 == "-";
			i = 1;
		}
		bool dot = false;
		bool any = false;
		while (i < n)
		{
			string c = t.Get(i);
			if (c == ".")
			{
				if (dot)
					return false;
				dot = true;
			}
			else if (c == "e" || c == "E")
				break;
			else if (DZS_Dec.IsDigit(c))
			{
				any = true;
				if (dot)
					fp += c;
				else
					ip += c;
			}
			else
				return false;
			i++;
		}
		if (!any)
			return false;
		if (i < n)
		{
			string ex = t.Substring(i + 1, n - i - 1);
			if (ex == "")
				return false;
			bool eneg = false;
			string e0 = ex.Get(0);
			if (e0 == "-" || e0 == "+")
			{
				eneg = e0 == "-";
				ex = ex.Substring(1, ex.Length() - 1);
			}
			if (ex == "" || ex.Length() > 3)
				return false;
			for (int k = 0; k < ex.Length(); k++)
			{
				if (!DZS_Dec.IsDigit(ex.Get(k)))
					return false;
			}
			int shift = ex.ToInt();
			while (shift > 0)
			{
				if (eneg)
				{
					if (ip.Length() > 0)
					{
						fp = ip.Get(ip.Length() - 1) + fp;
						ip = ip.Substring(0, ip.Length() - 1);
					}
					else
						fp = "0" + fp;
				}
				else
				{
					if (fp.Length() > 0)
					{
						ip += fp.Get(0);
						fp = fp.Substring(1, fp.Length() - 1);
					}
					else
						ip += "0";
				}
				shift--;
			}
		}
		while (ip.Length() > 1 && ip.Get(0) == "0")
			ip = ip.Substring(1, ip.Length() - 1);
		if (ip == "")
			ip = "0";
		return true;
	}

	// Integer digits and exactly six fraction digits, rounded.
	static bool Round6(string t, out bool neg, out string ip, out string f6)
	{
		string fp;
		if (!Split(t, neg, ip, fp))
			return false;
		bool up = false;
		if (fp.Length() > 6)
		{
			int d7 = fp.Get(6).ToInt();
			up = d7 >= 5;
			fp = fp.Substring(0, 6);
		}
		while (fp.Length() < 6)
			fp += "0";
		if (up)
		{
			string all = Inc(ip + fp);
			ip = all.Substring(0, all.Length() - 6);
			f6 = all.Substring(all.Length() - 6, 6);
		}
		else
			f6 = fp;
		if (ip == "")
			ip = "0";
		if (ip == "0" && f6 == "000000")
			neg = false;
		return true;
	}

	protected static string Inc(string digits)
	{
		string r = "";
		int carry = 1;
		for (int i = digits.Length() - 1; i >= 0; i--)
		{
			int d = digits.Get(i).ToInt() + carry;
			carry = 0;
			if (d == 10)
			{
				d = 0;
				carry = 1;
			}
			r = d.ToString() + r;
		}
		if (carry == 1)
			r = "1" + r;
		return r;
	}

	static bool IsNumber(string t)
	{
		bool neg;
		string ip, fp;
		return Split(t, neg, ip, fp);
	}

	// Coordinates and angles: always six decimals.
	static string Fixed6(string t)
	{
		bool neg;
		string ip, f6;
		if (!Round6(t, neg, ip, f6))
			return t;
		string r = ip + "." + f6;
		if (neg)
			r = "-" + r;
		return r;
	}

	// Other decimals: rounded to six places, trailing zeros and a bare point dropped.
	static string Canon(string t)
	{
		bool neg;
		string ip, f6;
		if (!Round6(t, neg, ip, f6))
			return t;
		int len = f6.Length();
		while (len > 0 && f6.Get(len - 1) == "0")
			len--;
		string r = ip;
		if (len > 0)
			r += "." + f6.Substring(0, len);
		if (neg)
			r = "-" + r;
		return r;
	}

	// Digits only, with an optional minus: what an integer field accepts.
	static bool IsInt(string t)
	{
		int n = t.Length();
		int i = 0;
		if (n > 0 && t.Get(0) == "-")
			i = 1;
		if (i >= n || n - i > 10)
			return false;
		for (int k = i; k < n; k++)
		{
			if (!DZS_Dec.IsDigit(t.Get(k)))
				return false;
		}
		return true;
	}

	// Value as whole units (floor) and millionths 0 … 999999.
	static bool Micro(string t, out int hi, out int lo)
	{
		bool neg;
		string ip, f6;
		hi = 0;
		lo = 0;
		if (!Round6(t, neg, ip, f6) || ip.Length() > 9)
			return false;
		int whole = ip.ToInt();
		int frac = f6.ToInt();
		if (!neg)
		{
			hi = whole;
			lo = frac;
		}
		else if (frac == 0)
			hi = -whole;
		else
		{
			hi = -whole - 1;
			lo = 1000000 - frac;
		}
		return true;
	}

	static bool Same(string a, string b)
	{
		return Near(a, b, 0);
	}

	// |a - b| <= tol millionths; false when either is no number.
	static bool Near(string a, string b, int tol)
	{
		int ah, al, bh, bl;
		if (!Micro(a, ah, al) || !Micro(b, bh, bl))
			return false;
		int dh = ah - bh;
		if (dh > 2 || dh < -2)
			return false;
		int d = dh * 1000000 + al - bl;
		if (d < 0)
			d = -d;
		return d <= tol;
	}
}
