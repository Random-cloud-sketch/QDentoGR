#include "TabPresenter.h"
#include "Model/Patient.h"
#include "Model/Dental/DentalVisit.h"
#include "Model/TableRows.h"
#include "Database/DbPatient.h"
#include "VisitPresenter.h"
#include "PerioPresenter.h"
#include "PatientHistoryPresenter.h"
#include "CalendarPresenter.h"
#include "FinancialPresenter.h"
#include "Database/DbInvoice.h"
#include "View/Widgets/TabView.h"

TabPresenter TabPresenter::s_singleton;

void TabPresenter::setView(TabView* view)
{
    this->view = view;
}

TabInstance* TabPresenter::currentTab()
{
    if (!m_tabs.count(m_currentIndex))
    {
        return nullptr;
    }

    return m_tabs[m_currentIndex];
}

void TabPresenter::createNewTab(TabInstance* tabInstance, bool setFocus)
{
    m_indexCounter++;

    m_tabs[m_indexCounter] = tabInstance;

    tabInstance->setContainerIdx(m_indexCounter);

    view->newTab(m_indexCounter, m_tabs[m_indexCounter]->getTabName());

    //tabs size() > 1, because the first tab gets focused by the view automatically
    if (setFocus && m_tabs.size() > 1)
        view->focusTab(m_indexCounter);
}

void TabPresenter::setCurrentTab(int index)
{
    if (currentTab() != nullptr) currentTab()->prepareSwitch();

    m_currentIndex = index;

    if (index == -1)
    {
        //it's 100% empty, but just in case...
        if (m_tabs.empty()) {
            m_indexCounter = -1;
        }

        view->showWelcomeScreen();
        return;
    }

    m_tabs[index]->setCurrent();
    
}

void TabPresenter::closeTabRequested(int tabId)
{
    if (!m_tabs[tabId]->premissionToClose()) return;

    delete m_tabs[tabId];
    m_tabs.erase(tabId);
    view->removeTab(tabId);
}


bool TabPresenter::permissionToLogOut()
{
    for (auto& pair : m_tabs)
    {
        if (!m_tabs[pair.first]->premissionToClose())
            return false;
    }

    m_tabs.clear();
    view->removeAllTabs();

    return true;
}

std::shared_ptr<Patient> TabPresenter::getPatient_ptr(const Patient& patient)
{
    for (auto& [index, tabInstance] : m_tabs)
    {
        if (!tabInstance->patient) continue;

        if(tabInstance->patient->rowid == patient.rowid) return tabInstance->patient;
    }

    auto result = std::make_shared<Patient>(patient);

    return result;
}


void TabPresenter::refreshPatientTabNames(long long patientRowId)
{
    for (auto& tab : m_tabs) {

        if (tab.second->patient == nullptr ||
            tab.second->patient->rowid != patientRowId) {
            continue;
        }
        view->changeTabName(tab.second->getTabName(), tab.first);
    }
}

void TabPresenter::refreshVisitNumbers(long long patientRowId)
{
    for (auto& [index, tab] : m_tabs) {

        if (tab->type != TabType::DentalVisit ||
            tab->patient == nullptr ||
            tab->patient->rowid != patientRowId) {
            continue;
        }

        static_cast<VisitPresenter*>(tab)->refreshVisitNumber();
    }
}

void TabPresenter::openList(const Patient& patient)
{
    if (newListAlreadyOpened(patient)) return;

    createNewTab(new VisitPresenter(view, getPatient_ptr(patient)));
}

void TabPresenter::openPerio(const Patient& patient)
{
    createNewTab(new PerioPresenter(view, getPatient_ptr(patient)));
}

void TabPresenter::openInvoice(const Recipient& recipient)
{
    createNewTab(new FinancialPresenter(view, recipient));
}

void TabPresenter::openInvoice(long long patientRowId, const std::vector<Procedure>& procedures)
{
    
    createNewTab(new FinancialPresenter(
                view, 
                getPatient_ptr(
                    DbPatient::get(patientRowId)
                ),
                procedures
            )
    );
}

void TabPresenter::openCalendar(const CalendarEvent& event)
{
    open(RowInstance(TabType::Calendar), true);

    //set clipboard
    static_cast<CalendarPresenter*>(currentTab())->newAppointment(event);

}

void TabPresenter::openCalendar()
{
    open(RowInstance(TabType::Calendar), true);
}

bool TabPresenter::open(const RowInstance& row, bool setFocus)
{
    if (!row.permissionToOpen) return false;

    //checking if tab is already opened
    for (auto& [index, tab] : m_tabs)
    {
        if (tab->type == row.type &&
            tab->rowID() == row.rowID &&
            (tab->patient == nullptr || //financial tab or calendar
                tab->patient->rowid == row.patientRowId)
            )
        {
            view->focusTab(index);

            return true;
        }
    }

    Patient patient = DbPatient::get(row.patientRowId);

    if (row.type == TabType::PatientSummary) {
        PatientHistoryPresenter p(patient);
        p.openDialog();
        return true;
    }

    TabInstance* newTab{nullptr};

    switch (row.type)
    {
        case TabType::DentalVisit:
            if (!row.rowID && newListAlreadyOpened(patient)) return true;
            newTab = new VisitPresenter(view, getPatient_ptr(patient), row.rowID);
            break;

        case TabType::PerioStatus:
            newTab = new PerioPresenter(view, getPatient_ptr(patient), row.rowID);
            break;

        case TabType::PatientSummary:
            break;

        case TabType::Financial:
            newTab = row.rowID ? 
                new FinancialPresenter(view, row.rowID)
                :
                new FinancialPresenter(view, getPatient_ptr(patient));
            break;

        case TabType::Calendar:
            newTab = new CalendarPresenter(view);
            break;
    }

    createNewTab(newTab, setFocus);

    return true;
}

bool TabPresenter::newListAlreadyOpened(const Patient& patient)
{

    for (auto& [index, tabInstance] : m_tabs)
    {

        if (tabInstance->type != TabType::DentalVisit) continue;

        auto listPresenter = static_cast<VisitPresenter*>(tabInstance);

        auto ambSheetDate = listPresenter->m_visit.date;

        if (listPresenter->patient->id == patient.id &&
            ambSheetDate == Date::currentDate().to8601()
            )
        {

            view->focusTab(index);
            return true;
        }
    }

    return false;
}

bool TabPresenter::documentTabOpened(TabType type, long long rowID) const
{
    for (const auto& [index, tab] : m_tabs)
    {

        if (tab->type == type && tab->rowID() == rowID)
        {
            return true;
        }
    }

    //if user wants to delete the patient, check other types of documents related to the patient
    if (type == TabType::PatientSummary)
    {
        for (const auto& [index, tab] : m_tabs)
        {       
            if (tab->patient && tab->patient.get()->rowid == rowID)
            {
                return true;
            }
        }
    }

    return false;
}

bool TabPresenter::patientTabOpened(long long) const
{
    return false;
}
