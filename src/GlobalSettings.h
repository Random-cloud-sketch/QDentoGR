#pragma once
#include <string>
#include <vector>

namespace GlobalSettings
{
	void createCfgIfNotExists();

	std::string getDbBackupFilepath();

	std::string getDbPath();
	std::string setDbPath();

	bool isADANum();

	void setToothNum(bool ADA);

	std::string getTranslationPath();
	void removeTranslationPath();
	std::string setTranslationPath();

	//built-in interface language: "el" (Greek, default) or "en" (original English)
	std::string getLanguage();
	void setLanguage(const std::string& language);

	//true when the built-in Greek translation was loaded at startup
	bool isGreekUi();
	void setGreekUi(bool greek);

	//time axis of the appointments calendar
	struct CalendarAxis
	{
		int startHour{ 8 };		//start of the working day
		int endHour{ 20 };		//end of the working day (24 = midnight)
		int slotMinutes{ 15 };	//15, 30 or 60
		bool fullDay{ false };	//show all 24 hours instead of the working day
	};

	CalendarAxis getCalendarAxis();
	void setCalendarAxis(const CalendarAxis& axis);
}