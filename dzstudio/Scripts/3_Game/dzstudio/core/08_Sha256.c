// SHA-256 over bytes as they are written (no second pass over a file). Add takes text pieces in
// order; Finish gives 64 lower-case hex digits. 32-bit ints wrap on addition; right shifts are arithmetic, hence the
// masks. Measured in DayZ 1.30 (-dzsShaTest).
class DZS_Sha256 : Managed
{
	protected static ref array<int> s_K;
	protected ref array<int> m_H = new array<int>();
	protected ref array<int> m_W = new array<int>();
	protected int m_Word;
	protected int m_InWord;
	protected int m_Words;
	protected int m_Len;
	// position for Feed: chunk and byte offset
	protected int m_Ci;
	protected int m_Off;

	void DZS_Sha256()
	{
		if (!s_K)
		{
			s_K = {1116352408, 1899447441, -1245643825, -373957723, 961987163, 1508970993, -1841331548, -1424204075, -670586216, 310598401, 607225278, 1426881987, 1925078388, -2132889090, -1680079193, -1046744716, -459576895, -272742522, 264347078, 604807628, 770255983, 1249150122, 1555081692, 1996064986, -1740746414, -1473132947, -1341970488, -1084653625, -958395405, -710438585, 113926993, 338241895, 666307205, 773529912, 1294757372, 1396182291, 1695183700, 1986661051, -2117940946, -1838011259, -1564481375, -1474664885, -1035236496, -949202525, -778901479, -694614492, -200395387, 275423344, 430227734, 506948616, 659060556, 883997877, 958139571, 1322822218, 1537002063, 1747873779, 1955562222, 2024104815, -2067236844, -1933114872, -1866530822, -1538233109, -1090935817, -965641998};
		}
		m_H = {1779033703, -1150833019, 1013904242, -1521486534, 1359893119, -1694144372, 528734635, 1541459225};
		for (int i = 0; i < 64; i++)
			m_W.Insert(0);
	}

	static string Of(array<string> chunks)
	{
		DZS_Sha256 h = new DZS_Sha256();
		foreach (string c : chunks)
			h.Add(c);
		return h.Finish();
	}

	static string OfText(string text)
	{
		DZS_Sha256 h = new DZS_Sha256();
		h.Add(text);
		return h.Finish();
	}

	// Hashes chunks in slices of 4000 bytes until about 'ms' milliseconds have passed; true when all are in.
	bool Feed(array<string> chunks, int ms)
	{
		int t0 = TickCount(0);
		while (m_Ci < chunks.Count())
		{
			string c = chunks[m_Ci];
			int n = c.Length();
			if (m_Off == 0 && n <= 4000)
			{
				Add(c);
				m_Ci++;
			}
			else
			{
				int take = Math.Min(4000, n - m_Off);
				string piece = c.Substring(m_Off, take);
				Add(piece);
				m_Off += take;
				if (m_Off >= n)
				{
					m_Ci++;
					m_Off = 0;
				}
			}
			if ((TickCount(0) - t0) / 10000 >= ms)
				return m_Ci >= chunks.Count();
		}
		return true;
	}

	void Add(string piece)
	{
		int n = piece.Length();
		array<int> w = m_W;
		int word = m_Word;
		int inWord = m_InWord;
		int words = m_Words;
		for (int i = 0; i < n; i++)
		{
			int b = piece.Get(i).ToAscii();
			word = (word << 8) | (b & 255);
			inWord++;
			if (inWord < 4)
				continue;
			w[words] = word;
			word = 0;
			inWord = 0;
			words++;
			if (words == 16)
			{
				Block();
				words = 0;
			}
		}
		m_Word = word;
		m_InWord = inWord;
		m_Words = words;
		m_Len += n;
	}

	protected void Byte(int b)
	{
		m_Word = (m_Word << 8) | b;
		m_InWord++;
		if (m_InWord < 4)
			return;
		m_W[m_Words] = m_Word;
		m_Word = 0;
		m_InWord = 0;
		m_Words++;
		if (m_Words == 16)
		{
			Block();
			m_Words = 0;
		}
	}

	// rotations inlined: ((x >> n) & mask of 32 - n bits) | (x << (32 - n))
	protected void Block()
	{
		array<int> w = m_W;
		array<int> k = s_K;
		for (int t = 16; t < 64; t++)
		{
			int x = w[t - 15];
			int y = w[t - 2];
			int s0 = (((x >> 7) & 33554431) | (x << 25)) ^ (((x >> 18) & 16383) | (x << 14)) ^ ((x >> 3) & 536870911);
			int s1 = (((y >> 17) & 32767) | (y << 15)) ^ (((y >> 19) & 8191) | (y << 13)) ^ ((y >> 10) & 4194303);
			w[t] = w[t - 16] + s0 + w[t - 7] + s1;
		}
		int a = m_H[0];
		int b = m_H[1];
		int c = m_H[2];
		int d = m_H[3];
		int e = m_H[4];
		int f = m_H[5];
		int g = m_H[6];
		int h = m_H[7];
		for (int i = 0; i < 64; i++)
		{
			int S1 = (((e >> 6) & 67108863) | (e << 26)) ^ (((e >> 11) & 2097151) | (e << 21)) ^ (((e >> 25) & 127) | (e << 7));
			int t1 = h + S1 + ((e & f) ^ (~e & g)) + k[i] + w[i];
			int S0 = (((a >> 2) & 1073741823) | (a << 30)) ^ (((a >> 13) & 524287) | (a << 19)) ^ (((a >> 22) & 1023) | (a << 10));
			int t2 = S0 + ((a & b) ^ (c & (a ^ b)));
			h = g;
			g = f;
			f = e;
			e = d + t1;
			d = c;
			c = b;
			b = a;
			a = t1 + t2;
		}
		m_H[0] = m_H[0] + a;
		m_H[1] = m_H[1] + b;
		m_H[2] = m_H[2] + c;
		m_H[3] = m_H[3] + d;
		m_H[4] = m_H[4] + e;
		m_H[5] = m_H[5] + f;
		m_H[6] = m_H[6] + g;
		m_H[7] = m_H[7] + h;
	}

	string Finish()
	{
		int bitsHi = (m_Len >> 29) & 7;
		int bitsLo = m_Len << 3;
		Byte(128);
		while (m_Words != 14 || m_InWord != 0)
			Byte(0);
		m_W[14] = bitsHi;
		m_W[15] = bitsLo;
		Block();
		m_Words = 0;
		string hex = "0123456789abcdef";
		string r = "";
		for (int i = 0; i < 8; i++)
		{
			int v = m_H[i];
			for (int k = 7; k >= 0; k--)
			{
				int nib = (v >> (k * 4)) & 15;
				r += hex.Get(nib);
			}
		}
		return r;
	}

	protected static string Ok(string a, string b)
	{
		if (a == b)
			return "yes";
		return "NO";
	}

	// -dzsShaTest: known values and the time for 1 MB and 10 MB in pieces of 4000 bytes (answers to section 11).
	static void SelfTest()
	{
		string e0 = OfText("");
		string e1 = OfText("abc");
		string e2 = OfText("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
		string e3 = OfText("Größe ä über die Grenze");
		DZS_Log.Info("sha256 '' " + e0 + " ok " + Ok(e0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
		DZS_Log.Info("sha256 abc " + e1 + " ok " + Ok(e1, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
		DZS_Log.Info("sha256 448 bits " + e2 + " ok " + Ok(e2, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
		DZS_Log.Info("sha256 utf8 " + e3);
		string unit = "";
		for (int i = 0; i < 100; i++)
			unit += "0123456789abcdefghijklmnopqrstuvwxyzABCD";
		int tb = TickCount(0);
		int sum = 0;
		for (int rr = 0; rr < 250; rr++)
		{
			for (int q = 0; q < 4000; q++)
			{
				int bq = unit.Get(q).ToAscii();
				sum += bq;
			}
		}
		DZS_Log.Info("sha256 probe: reading 1000000 bytes alone " + ((TickCount(0) - tb) / 10000) + " ms (" + sum + ")");
		DZS_Sha256 hb = new DZS_Sha256();
		int tk = TickCount(0);
		for (int blk = 0; blk < 15625; blk++)
			hb.Block();
		DZS_Log.Info("sha256 probe: 15625 blocks alone " + ((TickCount(0) - tk) / 10000) + " ms");
		array<int> sizes = {1000000, 10000000};
		foreach (int size : sizes)
		{
			DZS_Sha256 h = new DZS_Sha256();
			int done = 0;
			int t0 = TickCount(0);
			while (done < size)
			{
				int take = Math.Min(4000, size - done);
				if (take == 4000)
					h.Add(unit);
				else
					h.Add(unit.Substring(0, take));
				done += take;
			}
			string r = h.Finish();
			int ms = (TickCount(0) - t0) / 10000;
			DZS_Log.Info("sha256 " + size + " bytes in " + ms + " ms: " + r);
		}
	}
}
