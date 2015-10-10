#pragma once
#include "../main.h"

class Stats
{
	private:
		string email, persona, path, dogtagPath, data, dogtags;

	public:
		Stats();
		Stats(string email, string persona, bool createNew = false, bool initOnly = false);

		struct dogtag
		{
			string id;
			string data;
		};

		void saveStats();
		string getKey(string key);
		bool setKey(string key, string value, bool ignore_warning = false);
		bool isEmpty();

		int loadDogtags();					// returns number of dogtags for persona
		dogtag getDogtag(int lineNumber);	// returns a dogtag value from the specified line number in the file
		void addDogtag(dogtag newDogtag);
		void saveDogtags();

		void deleteStats();
};
