#include "RecallNotifier.h"

#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPushButton>

#include "Database/DbRecall.h"
#include "GlobalSettings.h"
#include "GoogleCalendar/GoogleCalendarSync.h"
#include "Presenter/RecallPresenter.h"
#include "Presenter/TabPresenter.h"

RecallNotifier& RecallNotifier::get()
{
	static RecallNotifier instance;
	return instance;
}

RecallNotifier::RecallNotifier()
{
	m_dayTimer.setSingleShot(true);

	//a new day: other recalls are due now
	connect(&m_dayTimer, &QTimer::timeout, this, [this] {
		refresh();
		scheduleNextDay();
	});
}

int RecallNotifier::attentionCount(const std::vector<RecallListRow>& rows, const QDate& today, int leadDays)
{
	//one row per patient (the recall is kept per patient), so the patients are counted once
	int count = 0;

	for (auto& row : rows) {
		if (row.needsAttention(today, leadDays)) count++;
	}

	return count;
}

int RecallNotifier::leadDays() const
{
	return GlobalSettings::getRecallLeadDays();
}

void RecallNotifier::setLeadDays(int days)
{
	if (days == leadDays()) return;

	//only the notification changes: the recall dates and intervals stay as they are
	GlobalSettings::setRecallLeadDays(days);

	refresh();
}

void RecallNotifier::refresh()
{
	auto today = QDate::currentDate();
	int lead = leadDays();

	m_count = attentionCount(DbRecall::list(today), today, lead);
	m_countedOn = today;

	emit changed(m_count, lead);
}

void RecallNotifier::scheduleNextDay()
{
	//a moment after the next midnight (local time)
	auto next = QDateTime(QDate::currentDate().addDays(1), QTime(0, 0, 5));
	auto ms = QDateTime::currentDateTime().msecsTo(next);

	m_dayTimer.start(int(std::clamp<qint64>(ms, 1000, 24LL * 3600 * 1000)));
}

void RecallNotifier::start()
{
	if (!m_started)
	{
		m_started = true;

		//appointments changed by the Google Calendar synchronization (deleted, moved, new)
		connect(&GoogleCalendarSync::get(), &GoogleCalendarSync::appointmentsChanged, this, [this] { refresh(); });

		//back to QDento: the day may have changed meanwhile (e.g. after sleep)
		connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
			if (state == Qt::ApplicationActive && m_countedOn != QDate::currentDate()) refresh();
		});
	}

	refresh();
	scheduleNextDay();

	//after the main window and the calendar are shown
	QTimer::singleShot(700, this, [this] { showStartupNotice(); });
}

void RecallNotifier::showStartupNotice()
{
	//counted again: the database may have changed since the start
	refresh();

	if (m_count <= 0) return;

	auto today = QDate::currentDate().toString(Qt::ISODate).toStdString();

	//at most once a day
	if (GlobalSettings::getRecallNoticeDate() == today) return;

	int lead = leadDays();

	QString text = m_count == 1 ?
		tr("There is 1 patient with a scheduled recall which is approaching or has passed and who has no booked appointment.") :
		tr("There are %1 patients with a scheduled recall which is approaching or has passed and who have no booked appointment.").arg(m_count);

	if (lead > 0) {
		text += "\n\n" + tr("Recalls due within the next %1 are included.").arg(RecallText::withinDays(lead));
	}

	QMessageBox box(QMessageBox::Information, tr("Pending periodontal recalls"), text, QMessageBox::NoButton, QApplication::activeWindow());

	auto openList = box.addButton(tr("Open list"), QMessageBox::AcceptRole);
	box.addButton(tr("Close"), QMessageBox::RejectRole);
	box.setDefaultButton(openList);

	//the notice is shown now: not again today (whatever the answer)
	GlobalSettings::setRecallNoticeDate(today);

	box.exec();

	if (box.clickedButton() == openList) {
		TabPresenter::get().openRecall(int(RecallPresenter::Filter::DueNotBooked));
	}
}
