#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "GoogleCalendarApi.h"
#include "Database/DbGoogleCalendar.h"

class QNetworkAccessManager;
class GoogleOAuth;
class Db;

//Two-way synchronization of the QDento appointments with one Google calendar.
//Google only receives the name, the times and "QDento appointment". An appointment and its Google event
//are linked only by the calendar id and the event id (never by the time or the name).
//A run first reads all the changes of Google (pull) and saves them in one database transaction together
//with the new sync token, then sends the changes of QDento (push). Events deleted in Google delete their
//appointment, new events of Google become appointments. Runs never overlap and the network part is
//asynchronous (the interface is never blocked).
class GoogleCalendarSync : public QObject
{
	Q_OBJECT

public:
	static GoogleCalendarSync& get();

	//after the dentist signed in to QDento: timers and the first synchronization
	void start();

	//an OAuth client is built in and the system has a credential store
	bool isAvailable() const;
	bool isSignedIn() const { return m_signedIn; }
	//signed in and a calendar is chosen
	bool isEnabled() const;
	bool isSyncing() const { return m_syncing; }
	bool isSigningIn() const;
	bool needsReauthorization() const;

	QString statusText() const { return m_status; }
	QString lastSyncText() const;
	const std::vector<GoogleCalendarInfo>& calendars() const { return m_calendars; }

	//settings page
	void signIn();
	void cancelSignIn();
	void signOut();
	void loadCalendars();
	void selectCalendar(const QString& id, const QString& name);
	void setAutoSync(bool enabled);
	void syncNow();

	//QDento changed appointments (sent after a short delay when automatic synchronization is on)
	void localChange();
	//an appointment whose Google event was deleted gets a new event
	void createAgain(long long rowid);

signals:
	void stateChanged();
	void calendarsLoaded();
	//changes of Google Calendar were saved in the appointments (or their synchronization state changed)
	void appointmentsChanged();
	//message for the user in the calendar (result or error of a synchronization)
	void notice(const QString& text);

private:
	GoogleCalendarSync();

	enum class Resolution { KeepQDento, KeepGoogle, Later };

	//appointments changed in QDento by a synchronization (only saved changes are counted)
	struct Counts
	{
		int added{ 0 };
		int removed{ 0 };
		int updated{ 0 };
		Counts& operator+=(const Counts& other) { added += other.added; removed += other.removed; updated += other.updated; return *this; }
		bool any() const { return added || removed || updated; }
	};

	QNetworkAccessManager* m_network;
	GoogleOAuth* m_auth;
	GoogleCalendarApi* m_api;

	QTimer m_autoTimer;		//every 5 minutes
	QTimer m_changeTimer;	//short delay after a change in QDento

	std::vector<GoogleCalendarInfo> m_calendars;
	QString m_status;
	bool m_signedIn{ false };
	bool m_started{ false };

	//current run
	bool m_syncing{ false };
	bool m_runAgain{ false };
	bool m_manual{ false };
	QString m_calendarId;
	long long m_dentist{ 0 };
	QString m_databaseId;
	bool m_fullSync{ false };
	bool m_failed{ false };
	QString m_error;
	bool m_appointmentsChanged{ false };
	Counts m_total;
	QString m_lastErrorNotice;				//an automatic run shows the same error only once

	//changes read from Google, saved together at the end of the pull
	std::vector<GoogleEvent> m_pulled;
	std::set<QString> m_pulledIds;
	QString m_nextSyncToken;
	std::map<QString, Resolution> m_resolutions;	//conflicts decided before the changes are saved (by event id)

	//transaction of the pull (nullptr: every change is saved on its own)
	Db* m_db{ nullptr };
	bool m_dbFailed{ false };
	bool m_batchChanged{ false };
	Counts m_batch;

	std::set<long long> m_conflictLater;	//conflicts the user decides later (not sent in this run)
	std::set<long long> m_declinedCreate;	//appointments the user did not want to create (until a manual run)
	std::deque<std::function<void()>> m_steps;

	void setStatus(const QString& text);
	void updateTimers();

	void run(bool manual);
	void pull(const QString& syncToken, const QString& pageToken);
	void checkMissingEvents();
	void applyPulled();
	void askConflicts();
	void processEvent(const GoogleEvent& event);
	void importEvent(const GoogleEvent& event);
	void push();
	void next();
	void finish();
	void fail(GoogleCalendarApi::Error error);

	void pushDelete(const DbGoogleCalendar::DeletedEvent& deleted);
	void pushUpdate(long long rowid, bool retried = false);
	void pushCreate(long long rowid);

	//both sides changed the appointment: the user decides
	Resolution askConflict(const DbGoogleCalendar::SyncAppointment& local, const GoogleEvent& google);
	//the Google event differs from the synchronized state: which side wins
	void reconcile(const DbGoogleCalendar::SyncAppointment& local, const GoogleEvent& google, bool fromPush);

	//the Google event was deleted: its appointment is deleted
	void removeLocal(const DbGoogleCalendar::SyncAppointment& local);

	//result of a database change of the synchronization (a failed one cancels the transaction of the pull)
	bool saved(bool ok);
	//an appointment was changed by the synchronization
	void changed();
	Counts& counts() { return m_db ? m_batch : m_total; }
	QString resultText() const;

	void loadCalendarsPage(const QString& pageToken, std::vector<GoogleCalendarInfo> found);
};
