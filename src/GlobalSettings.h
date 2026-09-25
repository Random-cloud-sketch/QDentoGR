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

	//fields which must be filled in when a new patient is created ("required_fields" in config.json)
	namespace RequiredField
	{
		inline constexpr const char* FirstName = "first_name";
		inline constexpr const char* LastName = "last_name";
		inline constexpr const char* Phone = "phone";
		inline constexpr const char* Address = "address";
		inline constexpr const char* ReferringDoctor = "referring_doctor";
		inline constexpr const char* DateOfBirth = "date_of_birth";
		inline constexpr const char* Gender = "gender";
	}

	//not configured yet: the fields which were already required (names and date of birth)
	bool isFieldRequired(const std::string& field);
	void setFieldRequired(const std::string& field, bool required);

	//Google Calendar synchronization ("google_calendar" in config.json; the OAuth tokens are never stored here)
	struct GoogleCalendar
	{
		bool enabled{ false };		//connected and a calendar is chosen
		std::string accountEmail;
		std::string calendarId;
		std::string calendarName;
		bool autoSync{ true };
		long long dentistRowid{ 0 };	//the appointments of this dentist are synchronized

		//only for testing against a local mock of the Google services (empty: the real Google addresses)
		std::string testApiUrl;
		std::string testOAuthUrl;
	};

	GoogleCalendar getGoogleCalendar();
	void setGoogleCalendar(const GoogleCalendar& settings);

	//folder of config.json (logs of the program are written there)
	std::string getDataFolder();
}