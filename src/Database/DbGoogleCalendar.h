#pragma once

#include <string>
#include <vector>
#include <optional>
#include <QDateTime>

class Db;

//Google Calendar synchronization data of the appointments (the appointment table stays the only record
//of an appointment; these are only its link to a Google Calendar event and the synchronization state).
//The functions with a Db parameter can run inside the transaction of a synchronization (nullptr: own connection).
namespace DbGoogleCalendar
{
	struct SyncAppointment
	{
		long long rowid{ 0 };
		long long dentist_rowid{ 0 };
		std::string summary;		//the name shown in the calendar (the only text sent to Google)
		QDateTime start;
		QDateTime end;

		std::string calendarId;		//Google calendar of the linked event
		std::string eventId;		//empty: not linked
		std::string etag;			//last known version of the Google event
		std::string updated;		//last known modification time of the Google event
		std::string status;			//synced, pending_create, pending_update, error, unlinked (empty: never synchronized)
		std::string origin;			//qdento or google: the last change came from there
		std::string lastSyncHash;	//state of the appointment when it was last synchronized

		//the synchronized state of this appointment now (its Google title and times)
		std::string hash() const;
	};

	//hash of the synchronized fields (title, start, end)
	std::string syncHash(const std::string& title, const QDateTime& start, const QDateTime& end);

	//the title of the Google event: the name of the appointment without a phone number
	//(older appointments have the phone after the name; it is never sent to Google)
	std::string googleTitle(const std::string& summary);

	//key / value state of the synchronization kept in the database (database_id, sync_token, ...)
	std::string state(const std::string& key, Db* db = nullptr);
	bool setState(const std::string& key, const std::string& value, Db* db = nullptr);
	//identifies the appointments of this database in the Google events
	std::string databaseId();

	std::optional<SyncAppointment> get(long long rowid, Db* db = nullptr);
	//the appointment linked to exactly this Google event (the event id is the identity, never the time or the name)
	std::optional<SyncAppointment> getByEvent(const std::string& calendarId, const std::string& eventId, Db* db = nullptr);

	//appointments of the dentist to be created in the calendar: not linked to an event of this calendar,
	//not unlinked on purpose, ending from the given date on
	std::vector<SyncAppointment> toCreate(long long dentist_rowid, const std::string& calendarId, const QDate& from);
	//linked appointments of the calendar changed in QDento (or which failed before)
	std::vector<SyncAppointment> toUpdate(long long dentist_rowid, const std::string& calendarId);
	//linked appointments of the calendar ending from the given date on
	std::vector<SyncAppointment> linked(long long dentist_rowid, const std::string& calendarId, const QDate& from);

	//the event was created / updated in Google Calendar with the given state of the appointment
	bool setSynced(long long rowid, const std::string& calendarId, const std::string& eventId,
		const std::string& etag, const std::string& updated, const std::string& hash, Db* db = nullptr);

	//a change made in Google Calendar is copied to the appointment; it is marked as synchronized,
	//so it is never sent back to Google
	bool applyGoogleChange(long long rowid, const std::string& summary, const QDateTime& start, const QDateTime& end,
		const std::string& etag, const std::string& updated, const std::string& hash, Db* db = nullptr);

	//a new event of Google Calendar becomes a new appointment of the dentist, linked to the event
	//(returns the rowid of the appointment, 0 on error)
	long long importEvent(long long dentist_rowid, const std::string& summary, const QDateTime& start, const QDateTime& end,
		const std::string& calendarId, const std::string& eventId, const std::string& etag, const std::string& updated,
		const std::string& hash, Db* db = nullptr);

	enum class RemoveResult { Removed, NotFound, Error };
	//the Google event was deleted: the appointment linked to exactly this event is deleted
	//(without a tombstone, so nothing is sent back to Google)
	RemoveResult removeDeletedInGoogle(long long rowid, const std::string& calendarId, const std::string& eventId, Db* db = nullptr);

	//only the version of the Google event changes (the content is the same on both sides)
	bool setEventVersion(long long rowid, const std::string& etag, const std::string& updated, Db* db = nullptr);

	bool setStatus(long long rowid, const std::string& status, Db* db = nullptr);

	//the user asked to create the event again (appointments unlinked by older versions)
	void createAgain(long long rowid);

	//events of deleted appointments, still to be deleted in Google Calendar
	struct DeletedEvent { long long rowid; std::string calendarId; std::string eventId; };
	std::vector<DeletedEvent> deletedEvents();
	bool isDeletedEvent(const std::string& eventId, Db* db = nullptr);
	void removeDeletedEvent(long long rowid);
}
