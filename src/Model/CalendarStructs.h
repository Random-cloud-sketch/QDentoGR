#pragma once
#include <string>
#include <QDateTime>

struct Patient;

struct CalendarEvent
{
	CalendarEvent(const Patient& p);
	CalendarEvent() {}
	long long rowid{ 0 };
	std::string summary;
	std::string description;
	std::string phone; //10 digits or empty
	//synchronization with Google Calendar: synced, pending_create, pending_update, error, unlinked (empty: never)
	std::string googleStatus;
	QDateTime start;
	QDateTime end;
	
	long long patient_rowid{ 0 };
};
