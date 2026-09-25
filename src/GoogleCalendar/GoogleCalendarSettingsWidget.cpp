#include "GoogleCalendarSettingsWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "GoogleCalendarSync.h"
#include "GlobalSettings.h"

GoogleCalendarSettingsWidget::GoogleCalendarSettingsWidget(QWidget* parent)
	: QWidget(parent)
{
	auto& sync = GoogleCalendarSync::get();

	auto layout = new QVBoxLayout(this);

	auto group = new QGroupBox("Google Calendar", this);
	auto form = new QFormLayout(group);

	m_accountLabel = new QLabel(group);
	form->addRow(tr("Google account:"), m_accountLabel);

	m_connectionLabel = new QLabel(group);
	form->addRow(tr("Connection:"), m_connectionLabel);

	auto buttons = new QHBoxLayout();
	m_connectButton = new QPushButton(group);
	m_disconnectButton = new QPushButton(tr("Disconnect Google"), group);
	buttons->addWidget(m_connectButton);
	buttons->addWidget(m_disconnectButton);
	buttons->addStretch();
	form->addRow(QString(), buttons);

	m_calendarCombo = new QComboBox(group);
	m_calendarCombo->setMinimumWidth(280);
	m_calendarCombo->setPlaceholderText(tr("Choose a calendar"));
	form->addRow(tr("Calendar:"), m_calendarCombo);

	m_autoSyncCheck = new QCheckBox(tr("Automatic synchronization"), group);
	m_autoSyncCheck->setToolTip(tr("Every 5 minutes, when QDento starts and after every change of an appointment"));
	form->addRow(QString(), m_autoSyncCheck);

	auto syncRow = new QHBoxLayout();
	m_lastSyncLabel = new QLabel(group);
	m_syncButton = new QPushButton(tr("Synchronize now"), group);
	syncRow->addWidget(m_lastSyncLabel);
	syncRow->addSpacing(20);
	syncRow->addWidget(m_syncButton);
	syncRow->addStretch();
	form->addRow(tr("Last synchronization:"), syncRow);

	m_statusLabel = new QLabel(group);
	m_statusLabel->setWordWrap(true);
	form->addRow(tr("Status:"), m_statusLabel);

	layout->addWidget(group);

	auto privacy = new QLabel(tr("Only the name and the time of each appointment are sent to Google Calendar, "
		"never medical or financial information. "
		"New events of Google Calendar become appointments and events deleted in Google Calendar delete their appointment."), this);
	privacy->setWordWrap(true);
	privacy->setStyleSheet("color: gray;");
	layout->addWidget(privacy);

	layout->addStretch();

	connect(m_connectButton, &QPushButton::clicked, this, [this] {
		auto& sync = GoogleCalendarSync::get();
		if (sync.isSigningIn()) sync.cancelSignIn();
		else sync.signIn();
		refresh();
	});

	connect(m_disconnectButton, &QPushButton::clicked, this, [this] {

		auto answer = QMessageBox::question(this, "Google Calendar",
			tr("Disconnect QDento from Google Calendar?") + "\n\n" +
			tr("The appointments of QDento and the events of Google Calendar are not deleted."));

		if (answer != QMessageBox::Yes) return;

		GoogleCalendarSync::get().signOut();
		m_calendarCombo->clear();
		refresh();
	});

	connect(m_calendarCombo, &QComboBox::activated, this, [this](int index) {
		if (index < 0) return;
		GoogleCalendarSync::get().selectCalendar(m_calendarCombo->itemData(index).toString(), m_calendarCombo->itemText(index));
	});

	connect(m_autoSyncCheck, &QCheckBox::toggled, this, [](bool checked) {
		GoogleCalendarSync::get().setAutoSync(checked);
	});

	connect(m_syncButton, &QPushButton::clicked, this, [] {
		GoogleCalendarSync::get().syncNow();
	});

	connect(&sync, &GoogleCalendarSync::stateChanged, this, [this] { refresh(); });
	connect(&sync, &GoogleCalendarSync::calendarsLoaded, this, [this] { fillCalendars(); refresh(); });

	//the calendars of the account are read again when the page is opened
	if (sync.isAvailable() && sync.isSignedIn()) {
		if (sync.calendars().empty()) sync.loadCalendars();
		else fillCalendars();
	}

	refresh();
}

void GoogleCalendarSettingsWidget::fillCalendars()
{
	QSignalBlocker blocker(m_calendarCombo);

	m_calendarCombo->clear();

	for (auto& c : GoogleCalendarSync::get().calendars()) {
		m_calendarCombo->addItem(c.primary ? c.summary + " " + tr("(main)") : c.summary, c.id);
	}

	auto selected = QString::fromStdString(GlobalSettings::getGoogleCalendar().calendarId);

	m_calendarCombo->setCurrentIndex(m_calendarCombo->findData(selected));
}

void GoogleCalendarSettingsWidget::refresh()
{
	auto& sync = GoogleCalendarSync::get();
	auto settings = GlobalSettings::getGoogleCalendar();

	bool available = sync.isAvailable();
	bool signedIn = sync.isSignedIn();

	if (!available)
	{
		m_accountLabel->setText(QString::fromUtf8("—"));
		m_connectionLabel->setText(tr("Google Calendar is not set up in this copy of QDento (no OAuth client)."));
		m_connectButton->setText(tr("Connect with Google"));

		for (QWidget* w : std::initializer_list<QWidget*>{ m_connectButton, m_disconnectButton, m_calendarCombo, m_autoSyncCheck, m_syncButton }) {
			w->setEnabled(false);
		}

		m_lastSyncLabel->setText(QString::fromUtf8("—"));
		m_statusLabel->setText(QString());
		return;
	}

	m_accountLabel->setText(signedIn && settings.accountEmail.size() ? QString::fromStdString(settings.accountEmail) : QString::fromUtf8("—"));

	m_connectionLabel->setText(
		sync.needsReauthorization() ? tr("Needs authorization again") :
		signedIn ? tr("Connected") : tr("Not connected"));

	m_connectButton->setText(
		sync.isSigningIn() ? tr("Cancel sign-in") :
		sync.needsReauthorization() ? tr("Connect again") : tr("Connect with Google"));

	m_connectButton->setEnabled(!signedIn || sync.needsReauthorization() || sync.isSigningIn());
	m_disconnectButton->setEnabled(signedIn);
	m_calendarCombo->setEnabled(signedIn && !sync.isSyncing());

	{
		QSignalBlocker blocker(m_autoSyncCheck);
		m_autoSyncCheck->setChecked(settings.autoSync);
	}

	m_autoSyncCheck->setEnabled(signedIn);
	m_syncButton->setEnabled(sync.isEnabled() && !sync.isSyncing());
	m_lastSyncLabel->setText(sync.lastSyncText());
	m_statusLabel->setText(sync.statusText());
}
