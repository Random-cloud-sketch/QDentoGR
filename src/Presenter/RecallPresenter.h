#pragma once

#include <vector>
#include <QString>
#include <QMetaObject>

#include "Presenter/TabInstance.h"
#include "Model/Recall.h"
#include "Model/CalendarStructs.h"

class RecallView;
class TabView;

//The periodontal recall list (a tab like the calendar)
class RecallPresenter : public TabInstance
{
	RecallView* view;

	//the recalls changed (dialogs, calendar, Google Calendar, lead time, new day)
	QMetaObject::Connection m_notifierConnection;

public:
	enum class Filter { Active, DueToday, Next7, Next30, Next90, Overdue, DueNotBooked, Inactive, All, Count };

	RecallPresenter(TabView* tabView);

	void setDataToView() override;
	bool isNew() override { return false; }
	TabName getTabName() override;
	bool save() override { return true; }
	long long rowID() const override { return 0; }

	//reads the recalls again and shows the ones of the filter / search of the view
	void refresh();

	//shows the recalls of the filter (the search is cleared)
	void showFilter(Filter filter);

	void addPatientRequested();
	void editRequested(long long patient_rowid);
	void bookRequested(long long patient_rowid);
	void openPatientRequested(long long patient_rowid);

	//leadDays: for DueNotBooked (the recalls which need attention, see RecallListRow::needsAttention)
	static bool matches(const RecallListRow& row, Filter filter, const QDate& today, int leadDays);
	//name or identifier of the patient (case and accents do not matter)
	static bool matchesSearch(const RecallListRow& row, const QString& search);
	//text for searching: without accents, case folded
	static QString searchable(const QString& text);

	~RecallPresenter();
};

//Recall actions shared by the recall list, the patient tile and the calendar
namespace RecallActions
{
	//opens the recall of the patient; returns true if something was saved
	bool editRecall(long long patient_rowid, bool activateNew = false);
	//opens the calendar to choose the time of a recall appointment of the patient
	void bookAppointment(long long patient_rowid);
	void openPatient(long long patient_rowid);

	//actions of a recall appointment; return true if the appointment / recall changed
	bool completeAppointment(const CalendarEvent& appointment);
	bool setAppointmentStatus(const CalendarEvent& appointment, const std::string& status);
	bool setRecallAppointment(const CalendarEvent& appointment, bool recall);
}
