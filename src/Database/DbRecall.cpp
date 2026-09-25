#include "DbRecall.h"

#include "Database/Database.h"

static std::string iso(const QDate& date)
{
	return date.isValid() ? date.toString(Qt::ISODate).toStdString() : std::string();
}

static QDate toDate(const std::string& text)
{
	return QDate::fromString(QString::fromStdString(text), Qt::ISODate);
}

static std::string nowText()
{
	return QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss").toStdString();
}

static CalendarEvent readAppointment(Db& db)
{
	CalendarEvent e;

	e.rowid = db.asRowId(0);
	e.patient_rowid = db.asRowId(1);
	e.start = QDateTime::fromString(db.asString(2).c_str(), Qt::ISODate);
	e.end = QDateTime::fromString(db.asString(3).c_str(), Qt::ISODate);
	e.summary = db.asString(4);
	e.recall = db.asBool(5);
	e.recallStatus = db.asString(6);

	return e;
}

static const char* appointmentColumns = "rowid, patient_rowid, start, end, summary, recall, IFNULL(recall_status, '') ";

std::optional<Recall> DbRecall::get(long long patient_rowid)
{
	Db db("SELECT active, next_date, interval_months, last_date, notes FROM recall WHERE patient_rowid=?");

	db.bind(1, patient_rowid);

	while (db.hasRows())
	{
		Recall r;
		r.patient_rowid = patient_rowid;
		r.active = db.asBool(0);
		r.nextDate = toDate(db.asString(1));
		r.intervalMonths = db.asInt(2);
		r.lastDate = toDate(db.asString(3));
		r.notes = db.asString(4);

		return r;
	}

	return {};
}

bool DbRecall::addHistory(long long patient_rowid, const std::string& event, const std::string& value,
	const std::string& previousValue, long long appointment_rowid, const std::string& note, Db* connection)
{
	Db db("INSERT INTO recall_history (patient_rowid, time, event, value, previous_value, appointment_rowid, note) "
		"VALUES (?,?,?,?,?,?,?)", connection);

	db.bind(1, patient_rowid);
	db.bind(2, nowText());
	db.bind(3, event);
	value.size() ? db.bind(4, value) : db.bindNull(4);
	previousValue.size() ? db.bind(5, previousValue) : db.bindNull(5);
	appointment_rowid ? db.bind(6, appointment_rowid) : db.bindNull(6);
	note.size() ? db.bind(7, note) : db.bindNull(7);

	return db.execute();
}

bool DbRecall::save(const Recall& r, const std::optional<Recall>& previous, const std::string& reason)
{
	Db db;

	if (!db.execute("BEGIN IMMEDIATE")) return false;

	bool ok = true;

	{
		Db query("INSERT OR REPLACE INTO recall (patient_rowid, active, next_date, interval_months, last_date, notes) VALUES (?,?,?,?,?,?)", &db);

		query.bind(1, r.patient_rowid);
		query.bind(2, int(r.active));
		r.nextDate.isValid() ? query.bind(3, iso(r.nextDate)) : query.bindNull(3);
		r.intervalMonths > 0 ? query.bind(4, r.intervalMonths) : query.bindNull(4);
		r.lastDate.isValid() ? query.bind(5, iso(r.lastDate)) : query.bindNull(5);
		query.bind(6, r.notes);

		ok = query.execute();
	}

	//the history keeps every change (nothing of it is ever overwritten)
	auto log = [&](const std::string& event, const std::string& value, const std::string& previousValue) {
		if (ok) ok = addHistory(r.patient_rowid, event, value, previousValue, 0, reason, &db);
	};

	Recall before = previous ? *previous : Recall{};

	if (!previous || before.active != r.active) {
		log(r.active ? "activated" : "deactivated", {}, {});
	}

	if (before.nextDate != r.nextDate) log("date_changed", iso(r.nextDate), iso(before.nextDate));

	if (before.intervalMonths != r.intervalMonths) {
		log("interval_changed", std::to_string(r.intervalMonths), std::to_string(before.intervalMonths));
	}

	if (before.lastDate != r.lastDate) log("last_date_changed", iso(r.lastDate), iso(before.lastDate));

	if (before.notes != r.notes) log("notes_changed", r.notes, before.notes);

	if (ok && db.execute("COMMIT")) return true;

	db.execute("ROLLBACK");
	return false;
}

std::vector<RecallHistoryEntry> DbRecall::history(long long patient_rowid)
{
	Db db("SELECT rowid, time, event, IFNULL(value, ''), IFNULL(previous_value, ''), IFNULL(appointment_rowid, 0), IFNULL(note, '') "
		"FROM recall_history WHERE patient_rowid=? ORDER BY time, rowid");

	db.bind(1, patient_rowid);

	std::vector<RecallHistoryEntry> result;

	while (db.hasRows())
	{
		RecallHistoryEntry e;
		e.rowid = db.asRowId(0);
		e.time = QDateTime::fromString(db.asString(1).c_str(), Qt::ISODate);
		e.event = db.asString(2);
		e.value = db.asString(3);
		e.previousValue = db.asString(4);
		e.appointment_rowid = db.asRowId(5);
		e.note = db.asString(6);

		result.push_back(e);
	}

	return result;
}

std::vector<RecallListRow> DbRecall::list(const QDate& today)
{
	Db db(
		"SELECT r.patient_rowid, r.active, IFNULL(r.next_date, ''), IFNULL(r.interval_months, 0), IFNULL(r.last_date, ''), IFNULL(r.notes, ''), "
		"p.fname, p.lname, p.id, IFNULL(p.phone, ''), "
		"(SELECT MIN(a.start) FROM appointment a WHERE a.patient_rowid = r.patient_rowid AND a.recall = 1 "
			"AND IFNULL(a.recall_status, '') = '' AND a.start >= ?1), "
		"(SELECT MAX(a.start) FROM appointment a WHERE a.patient_rowid = r.patient_rowid AND a.recall = 1 "
			"AND IFNULL(a.recall_status, '') = '' AND a.start < ?1) "
		"FROM recall r JOIN patient p ON p.rowid = r.patient_rowid"
	);

	//calendar days: the appointments of today are booked, even when their time has passed
	db.bind(1, today.toString("yyyy-MM-dd").toStdString() + "T00:00:00");

	std::vector<RecallListRow> result;

	while (db.hasRows())
	{
		RecallListRow row;

		row.recall.patient_rowid = db.asRowId(0);
		row.recall.active = db.asBool(1);
		row.recall.nextDate = toDate(db.asString(2));
		row.recall.intervalMonths = db.asInt(3);
		row.recall.lastDate = toDate(db.asString(4));
		row.recall.notes = db.asString(5);

		row.patientName = db.asString(6) + " " + db.asString(7);
		row.patientId = db.asString(8);
		row.phone = db.asString(9);

		row.bookedAppointment = QDateTime::fromString(db.asString(10).c_str(), Qt::ISODate);
		row.unmarkedAppointment = QDateTime::fromString(db.asString(11).c_str(), Qt::ISODate);

		result.push_back(row);
	}

	return result;
}

std::vector<CalendarEvent> DbRecall::appointments(long long patient_rowid)
{
	Db db(std::string("SELECT ") + appointmentColumns + "FROM appointment WHERE patient_rowid=? AND recall=1 ORDER BY start DESC");

	db.bind(1, patient_rowid);

	std::vector<CalendarEvent> result;

	while (db.hasRows()) result.push_back(readAppointment(db));

	return result;
}

std::optional<CalendarEvent> DbRecall::appointment(long long appointment_rowid)
{
	Db db(std::string("SELECT ") + appointmentColumns + "FROM appointment WHERE rowid=?");

	db.bind(1, appointment_rowid);

	while (db.hasRows()) return readAppointment(db);

	return {};
}

bool DbRecall::setRecallAppointment(long long appointment_rowid, bool recall)
{
	Db db("UPDATE appointment SET recall=?, recall_status=NULL WHERE rowid=?");

	db.bind(1, int(recall));
	db.bind(2, appointment_rowid);

	return db.execute();
}

bool DbRecall::setAppointmentStatus(const CalendarEvent& a, const std::string& status, const std::string& note)
{
	Db db;

	if (!db.execute("BEGIN IMMEDIATE")) return false;

	bool ok;

	{
		Db query("UPDATE appointment SET recall_status=? WHERE rowid=? AND recall=1", &db);

		status.size() ? query.bind(1, status) : query.bindNull(1);
		query.bind(2, a.rowid);

		ok = query.execute() && db.rowsAffected() == 1;
	}

	auto event = status == "completed" ? "completed" : status == "missed" ? "missed" : "status_reset";

	if (ok) ok = addHistory(a.patient_rowid, event, a.start.toString("yyyy-MM-ddTHH:mm:ss").toStdString(), {}, a.rowid, note, &db);

	if (ok && db.execute("COMMIT")) return true;

	db.execute("ROLLBACK");
	return false;
}
