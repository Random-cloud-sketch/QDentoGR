#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;

//Page «Google Calendar» of the settings: connection, calendar, automatic synchronization.
//Every action takes effect immediately (like the language of the program).
class GoogleCalendarSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit GoogleCalendarSettingsWidget(QWidget* parent = nullptr);

private:
	QLabel* m_accountLabel;
	QLabel* m_connectionLabel;
	QPushButton* m_connectButton;
	QPushButton* m_disconnectButton;
	QComboBox* m_calendarCombo;
	QCheckBox* m_autoSyncCheck;
	QLabel* m_lastSyncLabel;
	QPushButton* m_syncButton;
	QLabel* m_statusLabel;

	void refresh();
	void fillCalendars();
};
