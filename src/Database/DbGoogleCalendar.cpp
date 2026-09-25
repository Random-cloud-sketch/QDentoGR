#include "DbGoogleCalendar.h"

#include <QCryptographicHash>
#include <QRegularExpression>

#include "Database/Database.h"

static const char* columns =
	"rowid, dentist_rowid, summary, start, end, IFNULL(google_calendar_id, ''), IFNULL(google_event_id, ''), "
	"IFNULL(google_event_etag, ''), IFNULL(google_event_updated, ''), IFNULL(google_sync_status, ''), "
	"IFNULL(google_sync_origin, ''), IFNULL(google_last_sync_hash, '') ";

static std::string iso(const QDateTime& dt)
{
	return dt.toString(Qt::ISODate).toStdString();
}

static DbGoogleCalendar::SyncAppointment read(Db& db)
{
	DbGoogleCalendar::SyncAppointment a;

	a.rowid = db.asRowId(0);
	a.dentist_rowid = db.asRowId(1);
	a.summary = db.asString(2);
	a.start = QDateTime::fromString(db.asString(3).c_str(), Qt::ISODate);
	a.end = QDateTime::fromString(db.asString(4).c_str(), Qt::ISODate);
	a.calendarId = db.asString(5);
	a.eventId = db.asString(6);
	a.etag = db.asString(7);
	a.updated = db.asString(8);
	a.status = db.asString(9);
	a.origin = db.asString(10);
	a.lastSyncHash = db.asString(11);

	return a;
}

std::string DbGoogleCalendar::googleTitle(const std::string& summary)
{
	static const QRegularExpression phone(R"(\s*\+?\d[\d ]{5,}\d\s*$)");

	return QString::fromStdString(summary).remove(phone).trimmed().toStdString();
}

std::string DbGoogleCalendar::syncHash(const std::string& title, const QDateTime& start, const QDateTime& end)
{
	QByteArray data = QByteArray::fromStdString(title) + '|' +
		start.toString(Qt::ISODate).toUtf8() + '|' + end.toString(Qt::ISODate).toUtf8();

	return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().toStdString();
}

std::string DbGoogleCalendar::SyncAppointment::hash() const
{
	return syncHash(googleTitle(summary), start, end);
}

std::string DbGoogleCalendar::state(const std::string& key, Db* connection)
{
	Db db("SELECT IFNULL(value, '') FROM google_calendar_state WHERE key=?", connection);

	db.bind(1, key);

	while (db.hasRows()) return db.asString(0);

	return {};
}

bool DbGoogleCalendar::setState(const std::string& key, const std::string& value, Db* connection)
{
	Db db("INSERT OR REPLACE INTO google_calendar_state (key, value) VALUES (?,?)", connection);

	db.bind(1, key);
	db.bind(2, value);

	return db.execute();
}

std::string DbGoogleCalendar::databaseId()
{
	return state("database_id");
}

std::optional<DbGoogleCalendar::SyncAppointment> DbGoogleCalendar::get(long long rowid, Db* connection)
{
	Db db(std::string("SELECT ") + columns + "FROM appointment WHERE rowid=?", connection);

	db.bind(1, rowid);

	while (db.hasRows()) return read(db);

	return {};
}

std::optional<DbGoogleCalendar::SyncAppointment> DbGoogleCalendar::getByEvent(const std::string& calendarId, const std::string& eventId, Db* connection)
{
	Db db(std::string("SELECT ") + columns + "FROM appointment WHERE google_calendar_id=? AND google_event_id=?", connection);

	db.bind(1, calendarId);
	db.bind(2, eventId);

	while (db.hasRows()) return read(db);

	return {};
}

std::vector<DbGoogleCalendar::SyncAppointment> DbGoogleCalendar::toCreate(long long dentist_rowid, const std::string& calendarId, const QDate& from)
{
	Db db(std::string("SELECT ") + columns + "FROM appointment WHERE dentist_rowid=? "
		"AND (IFNULL(google_event_id, '') = '' OR IFNULL(google_calendar_id, '') <> ?) "
		"AND IFNULL(google_sync_status, '') <> 'unlinked' "
		"AND date(end) >= date(?) ORDER BY start");

	db.bind(1, dentist_rowid);
	db.bind(2, calendarId);
	db.bind(3, from.toString(Qt::ISODate).toStdString());

	std::vector<SyncAppointment> result;

	while (db.hasRows()) result.push_back(read(db));

	return result;
}

std::vector<DbGoogleCalendar::SyncAppointment> DbGoogleCalendar::toUpdate(long long dentist_rowid, const std::string& calendarId)
{
	Db db(std::string("SELECT ") + columns + "FROM appointment WHERE dentist_rowid=? "
		"AND IFNULL(google_event_id, '') <> '' AND google_calendar_id = ? "
		"AND google_sync_status IN ('pending_update', 'error') ORDER BY start");

	db.bind(1, dentist_rowid);
	db.bind(2, calendarId);

	std::vector<SyncAppointment> result;

	while (db.hasRows()) result.push_back(read(db));

	return result;
}

std::vector<DbGoogleCalendar::SyncAppointment> DbGoogleCalendar::linked(long long dentist_rowid, const std::string& calendarId, const QDate& from)
{
	Db db(std::string("SELECT ") + columns + "FROM appointment WHERE dentist_rowid=? "
		"AND IFNULL(google_event_id, '') <> '' AND google_calendar_id = ? "
		"AND date(end) >= date(?) ORDER BY start");

	db.bind(1, dentist_rowid);
	db.bind(2, calendarId);
	db.bind(3, from.toString(Qt::ISODate).toStdString());

	std::vector<SyncAppointment> result;

	while (db.hasRows()) result.push_back(read(db));

	return result;
}

bool DbGoogleCalendar::setSynced(long long rowid, const std::string& calendarId, const std::string& eventId,
	const std::string& etag, const std::string& updated, const std::string& hash, Db* connection)
{
	Db db("UPDATE appointment SET google_calendar_id=?, google_event_id=?, google_event_etag=?, google_event_updated=?, "
		"google_last_sync_hash=?, google_sync_status='synced' WHERE rowid=?", connection);

	db.bind(1, calendarId);
	db.bind(2, eventId);
	db.bind(3, etag);
	db.bind(4, updated);
	db.bind(5, hash);
	db.bind(6, rowid);

	return db.execute();
}

bool DbGoogleCalendar::applyGoogleChange(long long rowid, const std::string& summary, const QDateTime& start, const QDateTime& end,
	const std::string& etag, const std::string& updated, const std::string& hash, Db* connection)
{
	Db db("UPDATE appointment SET summary=?, start=?, end=?, google_event_etag=?, google_event_updated=?, "
		"google_last_sync_hash=?, google_sync_status='synced', google_sync_origin='google' WHERE rowid=?", connection);

	db.bind(1, summary);
	db.bind(2, iso(start));
	db.bind(3, iso(end));
	db.bind(4, etag);
	db.bind(5, updated);
	db.bind(6, hash);
	db.bind(7, rowid);

	return db.execute();
}

long long DbGoogleCalendar::importEvent(long long dentist_rowid, const std::string& summary, const QDateTime& start, const QDateTime& end,
	const std::string& calendarId, const std::string& eventId, const std::string& etag, const std::string& updated,
	const std::string& hash, Db* connection)
{
	Db db("INSERT INTO appointment (dentist_rowid, patient_rowid, start, end, summary, description, phone, "
		"google_calendar_id, google_event_id, google_event_etag, google_event_updated, google_last_sync_hash, "
		"google_sync_status, google_sync_origin) VALUES (?,NULL,?,?,?,'','',?,?,?,?,?,'synced','google')", connection);

	db.bind(1, dentist_rowid);
	db.bind(2, iso(start));
	db.bind(3, iso(end));
	db.bind(4, summary);
	db.bind(5, calendarId);
	db.bind(6, eventId);
	db.bind(7, etag);
	db.bind(8, updated);
	db.bind(9, hash);

	if (!db.execute()) return 0;

	return db.lastInsertedRowID();
}

DbGoogleCalendar::RemoveResult DbGoogleCalendar::removeDeletedInGoogle(long long rowid, const std::string& calendarId, const std::string& eventId, Db* connection)
{
	Db db(connection);

	//only the appointment still linked to exactly this event; the link is removed first,
	//so the delete trigger does not queue the (already deleted) event for deletion in Google
	db.newStatement("UPDATE appointment SET google_event_id=NULL WHERE rowid=? AND google_calendar_id=? AND google_event_id=?");
	db.bind(1, rowid);
	db.bind(2, calendarId);
	db.bind(3, eventId);

	if (!db.execute()) return RemoveResult::Error;

	if (db.rowsAffected() == 0) return RemoveResult::NotFound;

	db.newStatement("DELETE FROM appointment WHERE rowid=?");
	db.bind(1, rowid);

	if (!db.execute()) return RemoveResult::Error;

	return db.rowsAffected() ? RemoveResult::Removed : RemoveResult::NotFound;
}

bool DbGoogleCalendar::setEventVersion(long long rowid, const std::string& etag, const std::string& updated, Db* connection)
{
	Db db("UPDATE appointment SET google_event_etag=?, google_event_updated=? WHERE rowid=?", connection);

	db.bind(1, etag);
	db.bind(2, updated);
	db.bind(3, rowid);

	return db.execute();
}

bool DbGoogleCalendar::setStatus(long long rowid, const std::string& status, Db* connection)
{
	Db db("UPDATE appointment SET google_sync_status=? WHERE rowid=?", connection);

	db.bind(1, status);
	db.bind(2, rowid);

	return db.execute();
}

void DbGoogleCalendar::createAgain(long long rowid)
{
	Db db("UPDATE appointment SET google_event_id=NULL, google_event_etag=NULL, google_event_updated=NULL, "
		"google_last_sync_hash=NULL, google_sync_status='pending_create', google_sync_origin='qdento' WHERE rowid=?");

	db.bind(1, rowid);

	db.execute();
}

std::vector<DbGoogleCalendar::DeletedEvent> DbGoogleCalendar::deletedEvents()
{
	Db db("SELECT rowid, calendar_id, event_id FROM google_calendar_deleted ORDER BY rowid");

	std::vector<DeletedEvent> result;

	while (db.hasRows()) {
		result.push_back({ db.asRowId(0), db.asString(1), db.asString(2) });
	}

	return result;
}

bool DbGoogleCalendar::isDeletedEvent(const std::string& eventId, Db* connection)
{
	Db db("SELECT COUNT(*) FROM google_calendar_deleted WHERE event_id=?", connection);

	db.bind(1, eventId);

	while (db.hasRows()) return db.asInt(0) > 0;

	return false;
}

void DbGoogleCalendar::removeDeletedEvent(long long rowid)
{
	Db db("DELETE FROM google_calendar_deleted WHERE rowid=?");

	db.bind(1, rowid);

	db.execute();
}
