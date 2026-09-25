#pragma once

#include <optional>
#include <vector>

#include "Model/Recall.h"
#include "Model/CalendarStructs.h"

class Db;

//Periodontal recall of the patients (table recall) and its history (table recall_history).
//Linked to the patient by the rowid of the patient; independent of the periodontal charts.
//The recall appointments are appointments with recall = 1; their booking, rescheduling and cancelling
//is written to the history by database triggers (whatever changes the appointment).
namespace DbRecall
{
	std::optional<Recall> get(long long patient_rowid);

	//saves the recall and writes the differences to the previous state into the history (one transaction)
	bool save(const Recall& recall, const std::optional<Recall>& previous, const std::string& reason = {});

	std::vector<RecallHistoryEntry> history(long long patient_rowid);
	bool addHistory(long long patient_rowid, const std::string& event, const std::string& value = {},
		const std::string& previousValue = {}, long long appointment_rowid = 0, const std::string& note = {}, Db* db = nullptr);

	//all the patients with a recall, with the state of their recall appointments on the given day
	//(a scheduled recall appointment of that day or later counts as booked)
	std::vector<RecallListRow> list(const QDate& today);

	//the recall appointments of the patient (newest first)
	std::vector<CalendarEvent> appointments(long long patient_rowid);
	std::optional<CalendarEvent> appointment(long long appointment_rowid);

	//the appointment is a recall appointment or not (booking / removal are written by the triggers)
	bool setRecallAppointment(long long appointment_rowid, bool recall);

	//completed / missed / "" (scheduled) - the recall dates are not changed by this
	bool setAppointmentStatus(const CalendarEvent& appointment, const std::string& status, const std::string& note = {});
}
