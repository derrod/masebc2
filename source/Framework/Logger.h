#pragma once
#include "../main.h"
#include <fstream>

class Logger
{
	private:
		bool logfile;
		ofstream fp;

		//console stuff
#if defined (_WIN32)
		SMALL_RECT Rect;
		WORD lastColor;
		HANDLE hstdout;
		COORD currentScreenBuffer;

		bool useNormalConsole;
		int screenMsgCount, fixedSBSize;
		unsigned short buildColor(int from, bool notification);
		void updateConsole(unsigned short color, const char* msg, int msgLength);
#else
		string lastColor;
		string buildColor(int from, bool notification);
		void updateConsole(string color, const char* msg, int msgLength);
#endif
		bool	useColor;
		int		fileNotificationLevel, fileWarningLevel;
		int		consoleNotificationLevel, consoleWarningLevel;
		int		msgCutOffLength;

		bool setFile(const char* filename);
		const char* buildName(int from);
#ifdef WIN_MAIN
		int hCrt;
		FILE* hf_out;
#endif

	public:
		Logger(debugSettings dbg, consoleSettings con);
		~Logger();

		void notification(int level, int from, const char* message, ...);
		void simpleNotification(int level, int from, const char* message);
		void warning(int level, int from, const char* message, ...);
		void error(int from, const char* message, ...);

		void updateTitle(char* title);
};
