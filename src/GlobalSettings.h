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
}