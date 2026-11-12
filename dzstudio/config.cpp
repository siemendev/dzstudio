class CfgPatches
{
	class dzstudio
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
	};
};

class CfgMods
{
	class dzstudio
	{
		dir = "dzstudio";
		name = "dzStudio";
		version = "0.8.0";
		type = "mod";
		dependencies[] = {"Game", "World", "Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"dzstudio/Scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"dzstudio/Scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"dzstudio/Scripts/5_Mission"};
			};
		};
	};
};
