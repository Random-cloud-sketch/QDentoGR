#include "GoogleCalendarSync.h"

#include <QApplication>
#include <QJsonArray>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPushButton>

#include "CredentialStore.h"
#include "GoogleCalendarLog.h"
#include "GoogleOAuth.h"
#include "GlobalSettings.h"
#include "Model/UpperCase.h"
#include "Model/User.h"
#include "Database/Database.h"

using DbGoogleCalendar::SyncAppointment;

//appointments sent to Google: from today on (and the ones already linked)
static QDate firstSyncedDay() { return QDate::currentDate(); }

//so many new events at once need a confirmation
static constexpr int confirmCreateCount = 5;

GoogleCalendarSync& GoogleCalendarSync::get()
{
	static GoogleCalendarSync instance;
	return instance;
}

GoogleCalendarSync::GoogleCalendarSync()
{
	m_network = new QNetworkAccessManager(this);
	m_auth = new GoogleOAuth(m_network, this);
	m_api = new GoogleCalendarApi(m_auth, m_network, this);

	m_autoTimer.setInterval(5 * 60 * 1000);
	connect(&m_autoTimer, &QTimer::timeout, this, [this] { run(false); });

	m_changeTimer.setSingleShot(true);
	m_changeTimer.setInterval(2000);
	connect(&m_changeTimer, &QTimer::timeout, this, [this] { run(false); });

	connect(m_auth, &GoogleOAuth::loggedIn, this, [this] {
		m_signedIn = true;
		m_api->resetAuthorizationState();
		setStatus(tr("Connected"));
		loadCalendars();
	});

	connect(m_auth, &GoogleOAuth::loginFailed, this, [this](const QString& message) {
		setStatus(message);
	});

	connect(m_api, &GoogleCalendarApi::reauthorizationNeeded, this, [this] {
		setStatus(tr("The connection to Google Calendar needs to be authorized again."));
		emit notice(tr("The connection to Google Calendar needs to be authorized again (Settings > Google Calendar)."));
	});

	m_signedIn = m_auth->hasRefreshToken();
	m_status = m_signedIn ? tr("Ready") : tr("Not connected");
}

bool GoogleCalendarSync::isAvailable() const
{
	return GoogleOAuth::isConfigured() && CredentialStore::isAvailable();
}

bool GoogleCalendarSync::isEnabled() const
{
	auto s = GlobalSettings::getGoogleCalendar();
	return m_signedIn && s.enabled && s.calendarId.size();
}

bool GoogleCalendarSync::isSigningIn() const
{
	return m_auth->isLoggingIn();
}

bool GoogleCalendarSync::needsReauthorization() const
{
	return m_api->needsReauthorization();
}

QString GoogleCalendarSync::lastSyncText() const
{
	auto time = QDateTime::fromString(QString::fromStdString(DbGoogleCalendar::state("last_sync_time")), Qt::ISODate);

	return time.isValid() ? time.toString("dd/MM/yyyy HH:mm") : QString("—");
}

void GoogleCalendarSync::setStatus(const QString& text)
{
	m_status = text;
	emit stateChanged();
}

void GoogleCalendarSync::start()
{
	m_signedIn = m_auth->hasRefreshToken();
	m_started = true;

	updateTimers();

	//synchronization when QDento starts (a little later, after the calendar is shown)
	if (isEnabled()) QTimer::singleShot(3000, this, [this] { run(false); });
}

void GoogleCalendarSync::updateTimers()
{
	if (m_started && isEnabled() && GlobalSettings::getGoogleCalendar().autoSync) {
		if (!m_autoTimer.isActive()) m_autoTimer.start();
	}
	else {
		m_autoTimer.stop();
		m_changeTimer.stop();
	}
}

// ---------------------------------------------------------------- settings page

void GoogleCalendarSync::signIn()
{
	setStatus(tr("Waiting for the Google sign-in in the browser..."));
	m_auth->login();
}

void GoogleCalendarSync::cancelSignIn()
{
	m_auth->cancelLogin();
}

void GoogleCalendarSync::signOut()
{
	m_autoTimer.stop();
	m_changeTimer.stop();

	m_auth->logout();
	m_signedIn = false;
	m_calendars.clear();

	//the appointments and the Google events stay; the links of the appointments are kept,
	//so connecting again to the same calendar does not create duplicates
	auto s = GlobalSettings::getGoogleCalendar();
	s.enabled = false;
	s.accountEmail.clear();
	s.calendarId.clear();
	s.calendarName.clear();
	GlobalSettings::setGoogleCalendar(s);

	DbGoogleCalendar::setState("sync_token", "");
	DbGoogleCalendar::setState("sync_calendar_id", "");

	GoogleCalendarLog::write("Disconnected");

	setStatus(tr("Not connected"));

	//the synchronization marks disappear from the calendar
	emit appointmentsChanged();
}

void GoogleCalendarSync::loadCalendars()
{
	loadCalendarsPage({}, {});
}

void GoogleCalendarSync::loadCalendarsPage(const QString& pageToken, std::vector<GoogleCalendarInfo> found)
{
	m_api->listCalendars(pageToken, [this, found](const GoogleCalendarApi::Result& r) mutable {

		if (!r.ok()) {
			setStatus(GoogleCalendarApi::errorText(r.error));
			return;
		}

		for (auto item : r.json["items"].toArray())
		{
			auto o = item.toObject();

			GoogleCalendarInfo c;
			c.id = o["id"].toString();
			c.summary = o["summaryOverride"].toString().size() ? o["summaryOverride"].toString() : o["summary"].toString();
			c.accessRole = o["accessRole"].toString();
			c.primary = o["primary"].toBool();

			found.push_back(c);
		}

		auto nextPage = r.json["nextPageToken"].toString();

		if (nextPage.size()) {
			loadCalendarsPage(nextPage, found);
			return;
		}

		m_calendars = found;

		//the id of the primary calendar is the e-mail address of the account (no extra permission needed)
		auto s = GlobalSettings::getGoogleCalendar();

		for (auto& c : m_calendars) {
			if (c.primary) s.accountEmail = c.id.toStdString();
		}

		GlobalSettings::setGoogleCalendar(s);

		GoogleCalendarLog::write(QString("Calendars loaded: %1").arg(m_calendars.size()));

		setStatus(s.calendarId.size() ? tr("Connected") : tr("Connected - choose a calendar"));

		emit calendarsLoaded();

		//connected again to the same calendar: synchronization goes on
		if (isEnabled()) {
			updateTimers();
			run(true);
		}
	});
}

void GoogleCalendarSync::selectCalendar(const QString& id, const QString& name)
{
	auto s = GlobalSettings::getGoogleCalendar();

	if (s.calendarId == id.toStdString() && s.enabled) return;

	s.calendarId = id.toStdString();
	s.calendarName = name.toStdString();
	s.enabled = id.size();
	s.dentistRowid = User::dentist().rowID;
	GlobalSettings::setGoogleCalendar(s);

	GoogleCalendarLog::write("Calendar selected: " + id);

	updateTimers();

	emit appointmentsChanged();

	if (isEnabled()) run(true);
	else emit stateChanged();
}

void GoogleCalendarSync::setAutoSync(bool enabled)
{
	auto s = GlobalSettings::getGoogleCalendar();
	s.autoSync = enabled;
	GlobalSettings::setGoogleCalendar(s);

	GoogleCalendarLog::write(enabled ? "Automatic synchronization on" : "Automatic synchronization off");

	updateTimers();
	emit stateChanged();
}

void GoogleCalendarSync::syncNow()
{
	run(true);
}

void GoogleCalendarSync::localChange()
{
	//without automatic synchronization the changes wait for "Synchronize now"
	if (!isEnabled() || !GlobalSettings::getGoogleCalendar().autoSync) return;

	m_changeTimer.start();
}

void GoogleCalendarSync::createAgain(long long rowid)
{
	DbGoogleCalendar::createAgain(rowid);

	GoogleCalendarLog::write(QString("Appointment %1: event to be created again").arg(rowid));

	emit appointmentsChanged();

	if (isEnabled()) run(true);
}

// ---------------------------------------------------------------- synchronization run

void GoogleCalendarSync::run(bool manual)
{
	if (!isEnabled()) return;

	//one run at a time: the request is remembered
	if (m_syncing) {
		m_runAgain = true;
		m_manual = m_manual || manual;
		return;
	}

	if (m_api->needsReauthorization()) {
		setStatus(tr("The connection to Google Calendar needs to be authorized again."));
		return;
	}

	auto s = GlobalSettings::getGoogleCalendar();

	m_calendarId = QString::fromStdString(s.calendarId);
	m_dentist = s.dentistRowid ? s.dentistRowid : User::dentist().rowID;
	m_databaseId = QString::fromStdString(DbGoogleCalendar::databaseId());

	if (m_databaseId.isEmpty()) {
		GoogleCalendarLog::write("Database without synchronization tables");
		setStatus(tr("Synchronization error"));
		return;
	}

	m_syncing = true;
	m_manual = manual;
	m_failed = false;
	m_error.clear();
	m_appointmentsChanged = false;
	m_total = {};
	m_conflictLater.clear();
	m_resolutions.clear();
	m_steps.clear();

	//the user may be asked again about the appointments not created before
	if (manual) m_declinedCreate.clear();

	//another calendar: its changes are read from the beginning
	if (DbGoogleCalendar::state("sync_calendar_id") != s.calendarId) {
		DbGoogleCalendar::setState("sync_token", "");
		DbGoogleCalendar::setState("sync_calendar_id", s.calendarId);
	}

	auto syncToken = QString::fromStdString(DbGoogleCalendar::state("sync_token"));

	m_fullSync = syncToken.isEmpty();

	GoogleCalendarLog::write(m_fullSync ? "Sync started (full)" : "Sync started");

	setStatus(tr("Synchronizing..."));

	pull(syncToken, {});
}

void GoogleCalendarSync::pull(const QString& syncToken, const QString& pageToken)
{
	//a new list: the changes read so far are dropped
	if (pageToken.isEmpty()) {
		m_pulled.clear();
		m_pulledIds.clear();
		m_nextSyncToken.clear();
	}

	m_api->listEvents(m_calendarId, syncToken, pageToken, [this, syncToken](const GoogleCalendarApi::Result& r) {

		//the sync token is not valid any more: everything is read again
		//(the stored token is replaced only when the full synchronization is saved)
		if (r.error == GoogleCalendarApi::Error::Gone && syncToken.size())
		{
			GoogleCalendarLog::write("Sync token expired, full synchronization");
			m_fullSync = true;
			pull({}, {});
			return;
		}

		//nothing was saved yet: the next run reads the same changes again
		if (!r.ok()) {
			fail(r.error);
			finish();
			return;
		}

		for (auto item : r.json["items"].toArray())
		{
			auto event = GoogleEvent::fromJson(item.toObject());

			if (event.id.isEmpty()) continue;

			m_pulled.push_back(event);
			m_pulledIds.insert(event.id);
		}

		auto nextPage = r.json["nextPageToken"].toString();

		if (nextPage.size()) {
			pull(syncToken, nextPage);
			return;
		}

		m_nextSyncToken = r.json["nextSyncToken"].toString();

		if (m_nextSyncToken.isEmpty()) {
			GoogleCalendarLog::write("Event list without a sync token");
			fail(GoogleCalendarApi::Error::Malformed);
			finish();
			return;
		}

		GoogleCalendarLog::write(QString("Read %1 changed events").arg(m_pulled.size()));

		if (m_fullSync) checkMissingEvents();
		else applyPulled();
	});
}

void GoogleCalendarSync::checkMissingEvents()
{
	//after a full list: a linked appointment whose event was not returned at all may have been deleted
	//long ago in Google (deleted events are not kept forever); the event itself tells
	for (auto& a : DbGoogleCalendar::linked(m_dentist, m_calendarId.toStdString(), firstSyncedDay()))
	{
		auto eventId = QString::fromStdString(a.eventId);

		if (m_pulledIds.count(eventId)) continue;

		m_steps.push_back([this, eventId] {
			m_api->getEvent(m_calendarId, eventId, [this, eventId](const GoogleCalendarApi::Result& r) {

				if (r.error == GoogleCalendarApi::Error::NotFound || r.error == GoogleCalendarApi::Error::Gone)
				{
					GoogleEvent deleted;
					deleted.id = eventId;
					deleted.status = "cancelled";
					m_pulled.push_back(deleted);
				}
				else if (r.ok()) {
					m_pulled.push_back(GoogleEvent::fromJson(r.json));
				}
				else {
					//not known whether it still exists: nothing is saved in this run
					fail(r.error);
					finish();
					return;
				}

				next();
			});
		});
	}

	m_steps.push_back([this] { applyPulled(); });

	next();
}

void GoogleCalendarSync::askConflicts()
{
	auto calendar = m_calendarId.toStdString();

	for (auto& event : m_pulled)
	{
		if (event.status == "cancelled" || !event.timed || m_resolutions.count(event.id)) continue;

		auto local = DbGoogleCalendar::getByEvent(calendar, event.id.toStdString());

		if (!local || event.etag.toStdString() == local->etag) continue;

		auto googleHash = DbGoogleCalendar::syncHash(UpperCase::convert(event.summary).trimmed().toStdString(), event.start, event.end);
		auto localHash = local->hash();

		//changed on both sides
		if (googleHash != localHash && localHash != local->lastSyncHash) {
			m_resolutions[event.id] = askConflict(*local, event);
		}
	}
}

void GoogleCalendarSync::applyPulled()
{
	//the user decides the conflicts first: no dialog waits while the transaction is open
	askConflicts();

	Db db;

	bool began = db.execute("BEGIN IMMEDIATE");

	m_db = &db;
	m_dbFailed = !began;
	m_batchChanged = false;
	m_batch = {};

	for (auto& event : m_pulled)
	{
		if (m_dbFailed) break;
		processEvent(event);
	}

	//the position in the changes of Google is saved together with the changes (never before them)
	if (!m_dbFailed) saved(DbGoogleCalendar::setState("sync_token", m_nextSyncToken.toStdString(), &db));

	if (!m_dbFailed && !db.execute("COMMIT")) m_dbFailed = true;

	m_db = nullptr;

	if (m_dbFailed)
	{
		if (began) db.execute("ROLLBACK");

		GoogleCalendarLog::write("Database error: the changes of Google Calendar were not saved (they are read again next time)");

		m_failed = true;
		if (m_error.isEmpty()) m_error = tr("The changes of Google Calendar could not be saved in the database.");

		finish();
		return;
	}

	m_total += m_batch;
	if (m_batchChanged) m_appointmentsChanged = true;

	m_pulled.clear();

	push();
}

void GoogleCalendarSync::processEvent(const GoogleEvent& event)
{
	auto calendar = m_calendarId.toStdString();

	//the appointment linked to exactly this event (deleted events of an incremental synchronization have only their id)
	auto local = DbGoogleCalendar::getByEvent(calendar, event.id.toStdString(), m_db);

	if (local)
	{
		if (event.status == "cancelled") {
			removeLocal(*local);
			return;
		}

		//the same version QDento knows (e.g. its own last change): nothing to do
		if (event.etag.toStdString() == local->etag) return;

		if (!event.timed) {
			GoogleCalendarLog::write(QString("Event %1 has no start / end time, ignored").arg(event.id));
			return;
		}

		reconcile(*local, event, false);
		return;
	}

	//no appointment is linked to this event

	//deleted: nothing to do (its appointment was removed before, or it never was one)
	if (event.status == "cancelled") return;

	//the event of an appointment deleted in QDento: it is deleted in Google by this run
	if (DbGoogleCalendar::isDeletedEvent(event.id.toStdString(), m_db)) return;

	//an event created by QDento for an appointment of this database which is not linked to any event
	//(e.g. the answer of Google was lost): it is linked again instead of creating a second event
	if (event.databaseId == m_databaseId && event.appointmentId.size())
	{
		auto owner = DbGoogleCalendar::get(event.appointmentId.toLongLong(), m_db);

		bool free = owner && owner->status != "unlinked" &&
			(owner->eventId.empty() || owner->calendarId != calendar);

		if (free)
		{
			//QDento's version is kept (it is sent if it differs)
			auto googleHash = DbGoogleCalendar::syncHash(UpperCase::convert(event.summary).trimmed().toStdString(), event.start, event.end);

			if (!saved(DbGoogleCalendar::setSynced(owner->rowid, calendar, event.id.toStdString(), event.etag.toStdString(),
				event.updated.toStdString(), googleHash, m_db))) return;

			if (owner->hash() != googleHash) saved(DbGoogleCalendar::setStatus(owner->rowid, "pending_update", m_db));

			GoogleCalendarLog::write(QString("Event %1 linked again to appointment %2").arg(event.id).arg(owner->rowid));
			changed();
			return;
		}
	}

	//any other event is a new appointment (its time never matters: a new event at the time of
	//another appointment is still a new appointment)
	importEvent(event);
}

void GoogleCalendarSync::importEvent(const GoogleEvent& event)
{
	if (!event.timed) {
		GoogleCalendarLog::write(QString("New event %1 has no start / end time (all-day event), not imported").arg(event.id));
		return;
	}

	if (event.recurring) {
		GoogleCalendarLog::write(QString("New event %1 is a repeating event, not imported").arg(event.id));
		return;
	}

	//like the appointments sent to Google: from today on
	if (event.end.date() < firstSyncedDay()) return;

	auto summary = UpperCase::convert(event.summary).trimmed().toStdString();
	auto hash = DbGoogleCalendar::syncHash(DbGoogleCalendar::googleTitle(summary), event.start, event.end);

	auto rowid = DbGoogleCalendar::importEvent(m_dentist, summary, event.start, event.end, m_calendarId.toStdString(),
		event.id.toStdString(), event.etag.toStdString(), event.updated.toStdString(), hash, m_db);

	if (!saved(rowid != 0)) return;

	GoogleCalendarLog::write(QString("New Google event %1 imported as appointment %2").arg(event.id).arg(rowid));

	counts().added++;
	changed();
}

void GoogleCalendarSync::removeLocal(const SyncAppointment& local)
{
	auto result = DbGoogleCalendar::removeDeletedInGoogle(local.rowid, local.calendarId, local.eventId, m_db);

	if (result == DbGoogleCalendar::RemoveResult::Error) {
		saved(false);
		return;
	}

	//removed before (e.g. the same deletion read again): nothing to do
	if (result == DbGoogleCalendar::RemoveResult::NotFound) return;

	GoogleCalendarLog::write(QString("Google event %1 was deleted, appointment %2 deleted")
		.arg(QString::fromStdString(local.eventId)).arg(local.rowid));

	counts().removed++;
	changed();
}

bool GoogleCalendarSync::saved(bool ok)
{
	if (!ok) {
		if (m_db) m_dbFailed = true;
		else {
			m_failed = true;
			if (m_error.isEmpty()) m_error = tr("The changes of Google Calendar could not be saved in the database.");
		}
	}

	return ok;
}

void GoogleCalendarSync::changed()
{
	if (m_db) m_batchChanged = true;
	else m_appointmentsChanged = true;
}

void GoogleCalendarSync::reconcile(const SyncAppointment& local, const GoogleEvent& google, bool fromPush)
{
	auto googleSummary = UpperCase::convert(google.summary).trimmed().toStdString();
	auto googleHash = DbGoogleCalendar::syncHash(googleSummary, google.start, google.end);
	auto localHash = local.hash();

	//the name of the appointment changes only if the title was changed in Google
	//(so the older "NAME PHONE" names keep their phone when only the time changed)
	auto newSummary = DbGoogleCalendar::googleTitle(local.summary) == googleSummary ? local.summary : googleSummary;

	//the same appointment on both sides: only the version is noted
	if (googleHash == localHash)
	{
		saved(DbGoogleCalendar::setSynced(local.rowid, local.calendarId, local.eventId, google.etag.toStdString(),
			google.updated.toStdString(), localHash, m_db));
		return;
	}

	bool localChanged = localHash != local.lastSyncHash;

	//changed only in Google: Google wins (and it is not sent back)
	if (!localChanged)
	{
		if (!saved(DbGoogleCalendar::applyGoogleChange(local.rowid, newSummary, google.start, google.end,
			google.etag.toStdString(), google.updated.toStdString(), googleHash, m_db))) return;

		GoogleCalendarLog::write(QString("Imported Google modification for appointment %1").arg(local.rowid));
		counts().updated++;
		changed();
		return;
	}

	//decided before the transaction of the pull, or asked now (push)
	auto decided = m_resolutions.find(google.id);
	auto resolution = decided != m_resolutions.end() ? decided->second : askConflict(local, google);

	switch (resolution)
	{
		case Resolution::KeepQDento:
			//the version of Google is noted, so that QDento's change can replace it
			saved(DbGoogleCalendar::setEventVersion(local.rowid, google.etag.toStdString(), google.updated.toStdString(), m_db));
			saved(DbGoogleCalendar::setStatus(local.rowid, "pending_update", m_db));
			GoogleCalendarLog::write(QString("Conflict of appointment %1: QDento kept").arg(local.rowid));
			if (fromPush) m_steps.push_front([this, rowid = local.rowid] { pushUpdate(rowid, true); });
			break;

		case Resolution::KeepGoogle:
			if (!saved(DbGoogleCalendar::applyGoogleChange(local.rowid, newSummary, google.start, google.end,
				google.etag.toStdString(), google.updated.toStdString(), googleHash, m_db))) break;
			GoogleCalendarLog::write(QString("Conflict of appointment %1: Google kept").arg(local.rowid));
			counts().updated++;
			changed();
			break;

		case Resolution::Later:
			m_conflictLater.insert(local.rowid);
			GoogleCalendarLog::write(QString("Conflict of appointment %1: decided later").arg(local.rowid));
			break;
	}
}

GoogleCalendarSync::Resolution GoogleCalendarSync::askConflict(const SyncAppointment& local, const GoogleEvent& google)
{
	auto times = [](const QDateTime& start, const QDateTime& end) {
		return start.toString("dd/MM/yyyy HH:mm") + QString::fromUtf8("–") + end.toString("HH:mm");
	};

	QString text =
		tr("The appointment of:") + "\n" + QString::fromStdString(local.summary) + "\n\n" +
		tr("was changed both in QDento and in Google Calendar.") + "\n\n" +
		"QDento:\n" + times(local.start, local.end) + "\n\n" +
		"Google Calendar:\n" + times(google.start, google.end) +
		(UpperCase::convert(google.summary).trimmed().toStdString() != DbGoogleCalendar::googleTitle(local.summary) ? "\n" + google.summary : QString());

	QMessageBox box(QMessageBox::Warning, tr("Synchronization conflict"), text, QMessageBox::NoButton, QApplication::activeWindow());

	auto keepQDento = box.addButton(tr("Keep QDento"), QMessageBox::AcceptRole);
	auto keepGoogle = box.addButton(tr("Keep Google"), QMessageBox::AcceptRole);
	auto later = box.addButton(tr("Cancel"), QMessageBox::RejectRole);

	box.setDefaultButton(later);
	box.exec();

	if (box.clickedButton() == keepQDento) return Resolution::KeepQDento;
	if (box.clickedButton() == keepGoogle) return Resolution::KeepGoogle;
	return Resolution::Later;
}

void GoogleCalendarSync::push()
{
	auto calendar = m_calendarId.toStdString();

	//events of deleted appointments
	for (auto& deleted : DbGoogleCalendar::deletedEvents()) {
		m_steps.push_back([this, deleted] { pushDelete(deleted); });
	}

	//changed appointments
	for (auto& a : DbGoogleCalendar::toUpdate(m_dentist, calendar)) {
		if (m_conflictLater.count(a.rowid)) continue;
		m_steps.push_back([this, rowid = a.rowid] { pushUpdate(rowid); });
	}

	//appointments without an event in this calendar
	std::vector<long long> create;

	for (auto& a : DbGoogleCalendar::toCreate(m_dentist, calendar, firstSyncedDay())) {
		if (!m_declinedCreate.count(a.rowid)) create.push_back(a.rowid);
	}

	if (create.size() >= confirmCreateCount)
	{
		auto answer = QMessageBox::question(QApplication::activeWindow(), "Google Calendar",
			tr("%1 appointments were found in QDento without a Google Calendar event.").arg(create.size()) + "\n\n" +
			tr("Do you want them to be created in Google Calendar?"));

		if (answer != QMessageBox::Yes) {
			m_declinedCreate.insert(create.begin(), create.end());
			GoogleCalendarLog::write(QString("Creation of %1 events declined").arg(create.size()));
			create.clear();
		}
	}

	for (auto rowid : create) {
		m_steps.push_back([this, rowid] { pushCreate(rowid); });
	}

	m_steps.push_back([this] { finish(); });

	next();
}

void GoogleCalendarSync::next()
{
	if (m_steps.empty()) return;

	auto step = m_steps.front();
	m_steps.pop_front();

	step();
}

void GoogleCalendarSync::fail(GoogleCalendarApi::Error error)
{
	m_failed = true;
	if (m_error.isEmpty()) m_error = GoogleCalendarApi::errorText(error);
}

void GoogleCalendarSync::pushDelete(const DbGoogleCalendar::DeletedEvent& deleted)
{
	auto calendar = deleted.calendarId.size() ? QString::fromStdString(deleted.calendarId) : m_calendarId;

	m_api->deleteEvent(calendar, QString::fromStdString(deleted.eventId), [this, deleted](const GoogleCalendarApi::Result& r) {

		bool gone = r.ok() || r.error == GoogleCalendarApi::Error::NotFound || r.error == GoogleCalendarApi::Error::Gone;

		if (gone) {
			DbGoogleCalendar::removeDeletedEvent(deleted.rowid);
			GoogleCalendarLog::write("Deleted event " + QString::fromStdString(deleted.eventId));
		}
		else fail(r.error);

		next();
	});
}

void GoogleCalendarSync::pushUpdate(long long rowid, bool retried)
{
	auto a = DbGoogleCalendar::get(rowid);

	if (!a || a->eventId.empty()) { next(); return; }

	//nothing really changed since the last synchronization: no request
	if (a->hash() == a->lastSyncHash) {
		saved(DbGoogleCalendar::setStatus(rowid, "synced"));
		changed();
		next();
		return;
	}

	auto body = GoogleCalendarApi::eventBody(QString::fromStdString(DbGoogleCalendar::googleTitle(a->summary)), a->start, a->end, a->rowid, m_databaseId);

	m_api->patchEvent(m_calendarId, QString::fromStdString(a->eventId), body, QString::fromStdString(a->etag),
		[this, a = *a, retried](const GoogleCalendarApi::Result& r) {

			if (r.ok())
			{
				auto event = GoogleEvent::fromJson(r.json);
				saved(DbGoogleCalendar::setSynced(a.rowid, a.calendarId, a.eventId, event.etag.toStdString(), event.updated.toStdString(), a.hash()));
				GoogleCalendarLog::write("Updated event " + QString::fromStdString(a.eventId));
				changed();
				next();
				return;
			}

			//changed in Google in the meantime: the current version decides
			if (r.error == GoogleCalendarApi::Error::Precondition && !retried)
			{
				m_api->getEvent(m_calendarId, QString::fromStdString(a.eventId), [this, a](const GoogleCalendarApi::Result& current) {

					if (current.ok()) {
						auto event = GoogleEvent::fromJson(current.json);
						if (event.status == "cancelled") removeLocal(a);
						else if (event.timed) reconcile(a, event, true);
					}
					else if (current.error == GoogleCalendarApi::Error::NotFound || current.error == GoogleCalendarApi::Error::Gone) {
						removeLocal(a);
					}
					else fail(current.error);

					next();
				});
				return;
			}

			//deleted in Google in the meantime: the deletion wins
			if (r.error == GoogleCalendarApi::Error::NotFound || r.error == GoogleCalendarApi::Error::Gone) {
				removeLocal(a);
				next();
				return;
			}

			saved(DbGoogleCalendar::setStatus(a.rowid, "error"));
			changed();
			fail(r.error);
			next();
		});
}

void GoogleCalendarSync::pushCreate(long long rowid)
{
	auto a = DbGoogleCalendar::get(rowid);

	//deleted, unlinked or linked in the meantime
	if (!a || a->status == "unlinked" || (a->eventId.size() && a->calendarId == m_calendarId.toStdString())) {
		next();
		return;
	}

	auto body = GoogleCalendarApi::eventBody(QString::fromStdString(DbGoogleCalendar::googleTitle(a->summary)), a->start, a->end, a->rowid, m_databaseId);

	m_api->insertEvent(m_calendarId, body, [this, a = *a](const GoogleCalendarApi::Result& r) {

		if (r.ok())
		{
			auto event = GoogleEvent::fromJson(r.json);
			saved(DbGoogleCalendar::setSynced(a.rowid, m_calendarId.toStdString(), event.id.toStdString(),
				event.etag.toStdString(), event.updated.toStdString(), a.hash()));
			GoogleCalendarLog::write(QString("Created event for appointment %1").arg(a.rowid));
			changed();
		}
		else
		{
			saved(DbGoogleCalendar::setStatus(a.rowid, "error"));
			changed();
			fail(r.error);
		}

		next();
	});
}

QString GoogleCalendarSync::resultText() const
{
	QStringList parts;

	if (m_total.added) parts << tr("new appointments: %1").arg(m_total.added);
	if (m_total.removed) parts << tr("deleted appointments: %1").arg(m_total.removed);
	if (m_total.updated) parts << tr("changed appointments: %1").arg(m_total.updated);

	return tr("Google Calendar synchronization completed") + " - " + parts.join(", ");
}

void GoogleCalendarSync::finish()
{
	m_steps.clear();
	m_pulled.clear();
	m_syncing = false;

	if (m_total.any()) {
		GoogleCalendarLog::write(QString("Saved changes: %1 added, %2 deleted, %3 changed")
			.arg(m_total.added).arg(m_total.removed).arg(m_total.updated));
	}

	if (m_failed)
	{
		GoogleCalendarLog::write("Sync failed: " + m_error);
		setStatus(tr("Synchronization error") + ": " + m_error);

		//a manual run always tells the error, the automatic runs only once
		if (m_manual || m_error != m_lastErrorNotice) {
			emit notice(tr("Google Calendar synchronization failed") + ": " + m_error);
		}

		m_lastErrorNotice = m_error;
	}
	else
	{
		m_lastErrorNotice.clear();

		DbGoogleCalendar::setState("last_sync_time", QDateTime::currentDateTime().toString(Qt::ISODate).toStdString());
		GoogleCalendarLog::write("Sync completed");
		setStatus(m_total.any() ? resultText() : tr("Synchronization completed"));
	}

	//only saved changes are told
	if (m_total.any() && !m_failed) emit notice(resultText());

	if (m_appointmentsChanged) emit appointmentsChanged();

	//a run was requested meanwhile
	if (m_runAgain) {
		m_runAgain = false;
		bool manual = m_manual;
		QTimer::singleShot(0, this, [this, manual] { run(manual); });
	}
}
