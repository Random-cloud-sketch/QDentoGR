#pragma once
#include "Presenter/TabInstance.h"
#include "Model/CalendarStructs.h"
#include "Model/AppointmentOverlap.h"
#include <QString>
#include <QDate>
#include <QMetaObject>

class CalendarView;
class TabView;

class CalendarPresenter : public TabInstance
{
	CalendarView* view;

	std::pair<QDate, QDate> shownWeek;

	std::vector<CalendarEvent> events;

	CalendarEvent clipboard_event;

	//the last drag and drop change, which can be undone
	struct UndoEntry
	{
		long long rowid{ 0 };
		QDateTime start;
		QDateTime end;
		std::vector<AppointmentOverlap::Change> overlaps; //other appointments shortened / deleted by the change
	} m_undo;

	//the appointment has priority: the appointments of the dentist which overlap it are shortened or deleted
	std::vector<AppointmentOverlap::Change> resolveOverlaps(const CalendarEvent& event);
	void showOverlapNotice(const std::vector<AppointmentOverlap::Change>& changes);

	//any other change to the appointments ends the possibility to undo
	void clearUndo();

	static std::pair<QDate, QDate> getTodaysWeek();

	void refreshView();

	int getCurrentDayColumn();

	void setClipboard(const CalendarEvent& e);

	//marks the days with appointments in the month navigator of the view
	void refreshBusyDays();

	//the appointments were changed in QDento: Google Calendar synchronization
	void appointmentsWritten();

	//connections to the Google Calendar synchronization (removed with the presenter)
	std::vector<QMetaObject::Connection> m_syncConnections;
	bool m_refreshWhenShown{ false };

public:

	CalendarPresenter(TabView* view);
	void newAppointment(const CalendarEvent& event);

	// Inherited via TabInstance
	void setDataToView() override;
	bool isNew() override { return false; }
	TabName getTabName() override;
	bool save() override { return true; }
	long long rowID() const override { return 0; }

	void nextWeekRequested();
	void prevWeekRequested();
	void dateRequested(QDate date);
	void currentWeekRequested();
	void moveEvent(int index);
	void newDocRequested(int index, TabType type);
	void addEvent(const QTime& t, int daysFromMonday, int duration);
	void editEvent(int index);
	void deleteEvent(int index);
	void clearClipboard();
	void durationChange(int eventIdx, int duration);
	void cancelMove();
	void navigatorMonthsChanged();
	void rescheduleEvent(int index, const QDateTime& start, const QDateTime& end, bool moved);
	void undoLastChange();
	void createGoogleEventAgain(int index);

	~CalendarPresenter();
};