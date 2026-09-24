#pragma once
#include "Model/CalendarStructs.h"

class QDate;

namespace DbAppointment
{
	long long insert(const CalendarEvent& e, long long dentist_rowid);
	void update(const CalendarEvent& e);
	//changes only the date / time of the appointment (drag and drop, undo)
	bool updateTime(long long rowid, const QDateTime& start, const QDateTime& end);
	void remove(long long rowid);
	std::vector<CalendarEvent> get(const QDate& from, const QDate& to, long long dentist_rowid);
}