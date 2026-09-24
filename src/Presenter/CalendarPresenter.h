#pragma once
#include "Presenter/TabInstance.h"
#include "Model/CalendarStructs.h"
#include <QString>
#include <QDate>

class CalendarView;
class TabView;

class CalendarPresenter : public TabInstance
{
	CalendarView* view;

	std::pair<QDate, QDate> shownWeek;

	std::vector<CalendarEvent> events;

	CalendarEvent clipboard_event;

	static std::pair<QDate, QDate> getTodaysWeek();

	void refreshView();

	int getCurrentDayColumn();

	void setClipboard(const CalendarEvent& e);

	//marks the days with appointments in the month navigator of the view
	void refreshBusyDays();

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

	~CalendarPresenter();
};