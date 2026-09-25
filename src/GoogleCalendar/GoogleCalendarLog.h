#pragma once

#include <QString>

//Technical log of the Google Calendar synchronization: google_calendar.log in the folder of config.json.
//Never write tokens, authorization codes, passwords or clinical information here
//(appointments are identified only by their number, events by their Google id).
namespace GoogleCalendarLog
{
	void write(const QString& message);
}
