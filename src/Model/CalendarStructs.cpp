#include "CalendarStructs.h"
#include "Model/Patient.h"

CalendarEvent::CalendarEvent(const Patient& p)
{
	summary = p.firstLastName();
	phone = p.phone;

	patient_rowid = p.rowid;

}
