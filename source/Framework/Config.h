#include "../main.h"

class Config
{
	private:
		string data;
		debugSettings debugCfg;
		connectionSettings portsCfg;
		emulatorSettings emuCfg;
		consoleSettings conCfg;

		string getValue(const char* key);
		bool setValue(string value, int& storage);
		bool setValue(string value, bool& storage);
		bool setValue(string value, string& storage, bool allow_empty = false);

		bool getValuesFromSection(const char* section);
		bool createDefaultConfig();
		bool storeValues();

	public:
		Config();
		bool loadConfigFile();
		debugSettings getDebugCfg();
		consoleSettings getConCfg();
		emulatorSettings getEmuCfg();
		connectionSettings getPortsCfg();
};
