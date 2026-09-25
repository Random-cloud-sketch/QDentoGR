#pragma once

#include "TabInstance.h"
#include "Model/CalendarStructs.h"

struct DentalVisit;
struct Patient;
struct RowInstance;
struct Recipient;
class TabView;

class TabPresenter
{

	std::unordered_map<int, TabInstance*> m_tabs;

	int m_indexCounter{ -1 };
	int m_currentIndex{ -1 };

	TabView* view{ nullptr };

	bool newListAlreadyOpened(const Patient& patient);

	void createNewTab(TabInstance* tabInstance, bool setFocus = true);

	std::shared_ptr<Patient> getPatient_ptr(const Patient& patient);

	static TabPresenter s_singleton;

	TabPresenter() {};
public:

	void setView(TabView* view);

	TabInstance* currentTab();
	void setCurrentTab(int index);

	void refreshPatientTabNames(long long patientRowId);
	//the numbers of the patient's open dental visits are recalculated (a visit was saved or deleted)
	void refreshVisitNumbers(long long patientRowId);

	bool open(const RowInstance& row, bool setFocus = false);
	void openList(const Patient& patient);
	void openPerio(const Patient& patient);
	void openInvoice(const Recipient& recipient);
	void openInvoice(long long patientRowId, const std::vector<Procedure>& procedures = {});
	//the calendar with a new appointment to place (shown at the week of the date, if valid)
	void openCalendar(const CalendarEvent& event, const QDate& week = QDate());
	void openCalendar();
	//the recall list (showing the recalls of the filter, if given)
	void openRecall(int filter = -1);

	bool documentTabOpened(TabType type, long long rowID) const;
	bool patientTabOpened(long long patientRowid) const;

	void closeTabRequested(int tabId);


	//returns false if not all of the tabs are removed(a.k.a. user breaks the operation)
	bool permissionToLogOut();

	static TabPresenter& get() { return s_singleton; }
};

