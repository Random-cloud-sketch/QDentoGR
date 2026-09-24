#include "AppointmentOverlap.h"

std::vector<AppointmentOverlap::Change> AppointmentOverlap::resolve(const CalendarEvent& newEvent, const std::vector<CalendarEvent>& existing)
{
	std::vector<Change> result;

	for (auto& old : existing)
	{
		//the appointment itself (when it is moved or resized)
		if (newEvent.rowid && old.rowid == newEvent.rowid) continue;

		//touching at a boundary is not an overlap
		if (!(old.start < newEvent.end && old.end > newEvent.start)) continue;

		Change change;
		change.before = old;
		change.after = old;

		if (old.start >= newEvent.start && old.end <= newEvent.end) {
			//completely covered
			change.kind = Change::Remove;
		}
		else if (old.start < newEvent.start) {
			//overlapping the start of the new one, or the new one inside it: the part before is kept
			change.after.end = newEvent.start;
		}
		else {
			//overlapping the end of the new one: the part after is kept
			change.after.start = newEvent.end;
		}

		result.push_back(change);
	}

	return result;
}
