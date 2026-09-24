#pragma once

#include <vector>
#include "Model/CalendarStructs.h"

//The new (or moved / resized / edited) appointment always has priority.
//An existing appointment which overlaps it is shortened, so it stays one continuous appointment:
// - overlapping its start or its end: the overlapping part is cut
// - the new one inside it: only the part before the new one is kept (it is never split in two)
// - completely covered by the new one: it is deleted
//Appointments which only touch each other do not overlap.
namespace AppointmentOverlap
{
	struct Change
	{
		enum Kind { Shorten, Remove };

		Kind kind{ Shorten };
		CalendarEvent before;	//the existing appointment as it was
		CalendarEvent after;	//its new time (for Shorten)
	};

	//the changes of the existing appointments, the one with the rowid of newEvent is skipped
	std::vector<Change> resolve(const CalendarEvent& newEvent, const std::vector<CalendarEvent>& existing);
}
