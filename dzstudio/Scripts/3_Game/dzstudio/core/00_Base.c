const int DZS_API_VERSION = 1;
const string DZS_VERSION = "0.4.1";
const string DZS_WORKSHOP_URL = "";
const int DZS_TEXTFORM_VERSION = 1;
const int DZS_CHUNK_BYTES = 1000;
const string DZS_TAG = "[dzstudio]";
const string DZS_PROJECT_ID = "project";

enum DZS_Origin
{
	LOCAL,
	REMOTE,
	SYSTEM
}

enum DZS_ChangeKind
{
	CREATE,
	DELETE,
	SET_FIELD,
	SET_HIDDEN,
	SET_LOCKED,
	MOVE,
	SET_COMPANION
}

enum DZS_SubmitStatus
{
	APPLIED,
	DUPLICATE,
	PRECONDITION,
	UNKNOWN_TYPE,
	INVALID,
	EMPTY
}

class DZS_Log
{
	static void Info(string msg)
	{
		Print(DZS_TAG + " " + msg);
	}

	static void Warn(string msg)
	{
		Print(DZS_TAG + " WARN " + msg);
	}

	static void Error(string msg)
	{
		Print(DZS_TAG + " ERROR " + msg);
	}
}

class DZS_Str
{
	static string OriginName(DZS_Origin o)
	{
		if (o == DZS_Origin.LOCAL)
			return "local";
		if (o == DZS_Origin.REMOTE)
			return "remote";
		return "system";
	}

	static bool OriginFromName(string s, out DZS_Origin o)
	{
		if (s == "local")
			o = DZS_Origin.LOCAL;
		else if (s == "remote")
			o = DZS_Origin.REMOTE;
		else if (s == "system")
			o = DZS_Origin.SYSTEM;
		else
			return false;
		return true;
	}

	static string Bool(bool b)
	{
		if (b)
			return "true";
		return "false";
	}

	// Splits "plugin:typ" into plugin name; empty if malformed.
	static string PluginOfType(string typeName)
	{
		int i = typeName.IndexOf(":");
		if (i <= 0)
			return "";
		return typeName.Substring(0, i);
	}

	// Cuts a display name to 60 characters (UTF-8 aware) and ends it with an ellipsis.
	static string StepName(string name)
	{
		int chars = 0;
		int n = name.Length();
		for (int i = 0; i < n; i++)
		{
			if (!DZS_Utf8.IsContinuation(name, i))
			{
				chars++;
				if (chars == 60)
				{
					int end = i + DZS_Utf8.CharLen(name, i);
					if (end < n)
						return name.Substring(0, i) + "…";
				}
			}
		}
		return name;
	}
}

class DZS_Utf8
{
	// Substring without the game's cut at 8191 bytes: longer parts are copied in pieces.
	static string Part(string s, int start, int len)
	{
		if (len <= 8000)
			return s.Substring(start, len);
		string r = "";
		while (len > 0)
		{
			int take = Math.Min(8000, len);
			r += s.Substring(start, take);
			start += take;
			len -= take;
		}
		return r;
	}

	// Bytes come back from string.Get().ToAscii() signed: -128..-65 are continuation bytes.
	static int Byte(string s, int i)
	{
		int b = s.Get(i).ToAscii();
		if (b < 0)
			b += 256;
		return b;
	}

	static bool IsContinuation(string s, int i)
	{
		int b = Byte(s, i);
		return b >= 128 && b < 192;
	}

	static int CharLen(string s, int i)
	{
		int b = Byte(s, i);
		if (b < 128)
			return 1;
		if (b >= 240)
			return 4;
		if (b >= 224)
			return 3;
		if (b >= 192)
			return 2;
		return 1;
	}

	// Largest cut position <= pos that does not split a UTF-8 character.
	static int SafeCut(string s, int pos)
	{
		if (pos >= s.Length())
			return s.Length();
		while (pos > 0 && IsContinuation(s, pos))
			pos--;
		return pos;
	}

	static string FromCodepoint(int cp)
	{
		if (cp < 128)
			return cp.AsciiToString();
		if (cp < 2048)
			return (192 + (cp >> 6)).AsciiToString() + (128 + (cp & 63)).AsciiToString();
		if (cp < 65536)
			return (224 + (cp >> 12)).AsciiToString() + (128 + ((cp >> 6) & 63)).AsciiToString() + (128 + (cp & 63)).AsciiToString();
		return (240 + (cp >> 18)).AsciiToString() + (128 + ((cp >> 12) & 63)).AsciiToString() + (128 + ((cp >> 6) & 63)).AsciiToString() + (128 + (cp & 63)).AsciiToString();
	}
}
