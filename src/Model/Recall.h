#pragma once

#include <string>
#include <QDate>
#include <QDateTime>
#include <QString>

//Periodontal recall of a patient. It is kept by the clinician only: nothing is derived from the
//periodontal charts, and the next recall date is changed only by an explicit action of the clinician.
struct Recall
{
	long long patient_rowid{ 0 };
	bool active{ true };
	QDate nextDate;			//invalid: not set
	int intervalMonths{ 0 };	//0: no interval
	QDate lastDate;			//date of the last completed recall (invalid: none)
	std::string notes;

	//the next date calculated from the interval (invalid without an interval or a start date)
	QDate calculatedFrom(const QDate& start) const;
};

//An entry of the recall history (never changed afterwards)
struct RecallHistoryEntry
{
	long long rowid{ 0 };
	QDateTime time;
	std::string event;			//see RecallText::eventName
	std::string value;			//new value (date, interval, appointment time ...)
	std::string previousValue;	//previous value, if any
	long long appointment_rowid{ 0 };
	std::string note;
};

//A row of the recall dashboard
struct RecallListRow
{
	Recall recall;
	std::string patientName;
	std::string patientId;		//the identifier of the patient (UUID)
	std::string phone;

	QDateTime bookedAppointment;	//first scheduled recall appointment from today on (invalid: none)
	QDateTime unmarkedAppointment;	//a recall appointment of an earlier day, not yet marked completed / missed

	//the recall needs the attention of the clinician: active, with a next recall date up to
	//today + leadDays (overdue ones always included) and without a booked recall appointment
	bool needsAttention(const QDate& today, int leadDays) const;
};

//Recall actions of a recall appointment in the calendar
enum class RecallAction { Complete, Missed, ResetStatus, Mark, Unmark, EditRecall };

//Texts of the recall (shared by the dashboard, the dialogs and the calendar)
namespace RecallText
{
	QString date(const QDate& date);
	QString dateTime(const QDateTime& dateTime);
	QString interval(int months);
	//a number of days after "within" (1 day / N days)
	QString withinDays(int days);
	QString eventName(const std::string& event);
	//the details of a history entry (dates of the values in the local format)
	QString details(const RecallHistoryEntry& entry);
	QString appointmentStatus(const std::string& recallStatus);
}
