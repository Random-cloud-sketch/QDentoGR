#include "DbAppointment.h"
#include "Database/Database.h"
#include "Model/UpperCase.h"

//a changed appointment which is linked to a Google Calendar event is sent again by the next synchronization
//(whether it really changed is decided there by comparing with the synchronized state)
#define GOOGLE_PENDING_UPDATE 	"google_sync_status = CASE WHEN IFNULL(google_event_id, '') <> '' AND IFNULL(google_sync_status, '') <> 'unlinked' " 	"THEN 'pending_update' ELSE google_sync_status END, google_sync_origin = 'qdento'"

long long DbAppointment::insert(const CalendarEvent& e, long long dentist_rowid)
{
	//a new appointment is sent to Google Calendar by the next synchronization (when it is connected)
	auto query = "INSERT INTO appointment (dentist_rowid, patient_rowid, start, end, summary, description, phone, "
		"google_sync_status, google_sync_origin) VALUES (?,?,?,?,?,?,?,'pending_create','qdento')";

	Db db(query);

	db.bind(1, dentist_rowid);
	e.patient_rowid ? db.bind(2, e.patient_rowid) : db.bindNull(2);
	db.bind(3, e.start.toString(Qt::ISODate).toStdString());
	db.bind(4, e.end.toString(Qt::ISODate).toStdString());
	db.bind(5, UpperCase::convert(e.summary));
	db.bind(6, UpperCase::convert(e.description));
	db.bind(7, e.phone);

	db.execute();

	return db.lastInsertedRowID();
}

void DbAppointment::update(const CalendarEvent& e)
{
	auto query = "UPDATE appointment SET patient_rowid=?, start=?, end=?, summary=?, description=?, phone=?, "
		GOOGLE_PENDING_UPDATE " WHERE rowid=?";

	Db db(query);

	e.patient_rowid ? db.bind(1, e.patient_rowid) : db.bindNull(1);
	db.bind(2, e.start.toString(Qt::ISODate).toStdString());
	db.bind(3, e.end.toString(Qt::ISODate).toStdString());
	db.bind(4, UpperCase::convert(e.summary));
	db.bind(5, UpperCase::convert(e.description));
	db.bind(6, e.phone);
	db.bind(7, e.rowid);

	db.execute();

}

bool DbAppointment::updateTime(long long rowid, const QDateTime& start, const QDateTime& end)
{
	Db db("UPDATE appointment SET start=?, end=?, " GOOGLE_PENDING_UPDATE " WHERE rowid=?");

	db.bind(1, start.toString(Qt::ISODate).toStdString());
	db.bind(2, end.toString(Qt::ISODate).toStdString());
	db.bind(3, rowid);

	return db.execute();
}

void DbAppointment::remove(long long rowid)
{
	Db db("DELETE FROM appointment WHERE rowid=?");

	db.bind(1, rowid);

	db.execute();
}

std::vector<CalendarEvent> DbAppointment::get(const QDate& from, const QDate& to, long long dentist_rowid)
{
	auto fromDate = from.toString(Qt::ISODate).toStdString();
	auto toDate = to.toString(Qt::ISODate).toStdString();

	auto query = "SELECT rowid, patient_rowid, start, end, summary, description, phone, IFNULL(google_sync_status, '') FROM appointment WHERE dentist_rowid =? AND strftime('%Y-%m-%d', start) BETWEEN ? AND ? ORDER BY start ASC";

	Db db(query);

	db.bind(1, dentist_rowid);
	db.bind(2, fromDate);
	db.bind(3, toDate);

	std::vector<CalendarEvent> result;

	while (db.hasRows())
	{
		CalendarEvent e;

		e.rowid = db.asRowId(0);
		e.patient_rowid = db.asRowId(1);
		e.start = QDateTime::fromString(db.asString(2).c_str(), Qt::DateFormat::ISODate);
		e.end = QDateTime::fromString(db.asString(3).c_str(), Qt::DateFormat::ISODate);
		e.summary = db.asString(4);
		e.description = db.asString(5);
		e.phone = db.asString(6);
		e.googleStatus = db.asString(7);

		result.push_back(e);
	}

	return result;
}
