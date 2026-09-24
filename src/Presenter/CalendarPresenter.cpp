#include "CalendarPresenter.h"
#include "View/Widgets/CalendarView.h"
#include "Database/DbDentist.h"
#include "Database/DbPatient.h"
#include "Model/User.h"
#include "View/Widgets/TabView.h"
#include "Database/DbAppointment.h"
#include "View/Widgets/CalendarEventDialog.h"
#include "Presenter/TabPresenter.h"
#include "Presenter/PatientDialogPresenter.h"
#include "Model/TableRows.h"

CalendarPresenter::CalendarPresenter(TabView* tabView) :
    TabInstance(tabView, TabType::Calendar, nullptr),
    view(tabView->calendarView())
{
    view->setCalendarPresenter(this);

    shownWeek = getTodaysWeek();

    refreshView();

}

void CalendarPresenter::newAppointment(const CalendarEvent& event)
{
    clipboard_event = event;

    view->setEventList(events, clipboard_event);
}

void CalendarPresenter::setDataToView()
{
    view->setCalendarPresenter(this);
}

TabName CalendarPresenter::getTabName()
{
    return TabName{
        .header = QObject::tr("Appointments").toStdString(),
        .footer = "",
        .header_icon = CommonIcon::CALENDAR
        
    };
}

void CalendarPresenter::nextWeekRequested()
{
    shownWeek.first = shownWeek.first.addDays(7);
    shownWeek.second = shownWeek.second.addDays(7);

    refreshView();
}

void CalendarPresenter::prevWeekRequested()
{
    shownWeek.first = shownWeek.first.addDays(-7);
    shownWeek.second = shownWeek.second.addDays(-7);

    refreshView();
}

void CalendarPresenter::dateRequested(QDate date)
{
    if (date.year() == shownWeek.first.year() &&
        date.weekNumber() == shownWeek.first.weekNumber()) {

        return;
    }

    shownWeek.first = date.addDays(-date.dayOfWeek() + 1);
    shownWeek.second = date.addDays(-(date.dayOfWeek() - 7));

    refreshView();
}


void CalendarPresenter::currentWeekRequested()
{
    auto todaysWeek = getTodaysWeek();

    if (todaysWeek == shownWeek) return;

    shownWeek = todaysWeek;

    refreshView();
}

void CalendarPresenter::moveEvent(int index)
{
    setClipboard(events[index]);
}

void CalendarPresenter::newDocRequested(int index, TabType type)
{
    auto& event = events[index];

    if (type == TabType::Calendar) {

        CalendarEvent newEvent;

        newEvent.summary = event.summary;
        newEvent.patient_rowid = event.patient_rowid;

        setClipboard(newEvent);

        return;
    }

    RowInstance tab(type);

    tab.patientRowId = event.patient_rowid;

    if (!tab.patientRowId) {
        
        PatientDialogPresenter d(QObject::tr("New Patient").toStdString(), event.summary);

        auto result = d.open();

        if (!result) return;

        tab.patientRowId = result->rowid;

        //the appointment is linked to the patient, so next time the patient opens directly
        event.patient_rowid = result->rowid;
        DbAppointment::update(event);
    }

    TabPresenter::get().open(tab, true);

}

void CalendarPresenter::addEvent(const QTime& t, int daysFromMonday, int duration)
{
    QDateTime from(shownWeek.first.addDays(daysFromMonday), t);
    QDateTime to(from.addSecs(duration * 60));

    clipboard_event.start = from;
    clipboard_event.end = to;

    if (clipboard_event.rowid) { //existing event
        DbAppointment::update(clipboard_event);
        clipboard_event = CalendarEvent{};
        refreshView();
        return;
    }

    CalendarEventDialog d(clipboard_event);

    if (d.exec() != QDialog::Accepted) return;

    DbAppointment::insert(d.result(), User::dentist().rowID);

    refreshView();
}

void CalendarPresenter::editEvent(int index)
{
    auto& e = events[index];

    CalendarEventDialog d(e);

    if (d.exec() != QDialog::Accepted) return;

    DbAppointment::update(d.result());

    refreshView();
}

void CalendarPresenter::deleteEvent(int index)
{
    DbAppointment::remove(events[index].rowid);

    refreshView();
}

void CalendarPresenter::clearClipboard()
{
    clipboard_event = CalendarEvent{};

    view->setEventList(events, clipboard_event);
}

void CalendarPresenter::durationChange(int eventIdx, int duration)
{
    auto& event = events[eventIdx];

    event.end = event.start.addSecs(duration * 60);

    DbAppointment::update(event);

    refreshView();
}

void CalendarPresenter::cancelMove()
{
    clearClipboard();
}

CalendarPresenter::~CalendarPresenter()
{}

std::pair<QDate, QDate> CalendarPresenter::getTodaysWeek()
{
    auto currentDate = QDate::currentDate();
    int dayOfWeek = currentDate.dayOfWeek();
    return std::make_pair(currentDate.addDays(1 - dayOfWeek), currentDate.addDays(7 - dayOfWeek));
}

void CalendarPresenter::refreshView()
{
    view->updateWeekView(shownWeek.first, shownWeek.second, getCurrentDayColumn());

    events = DbAppointment::get(shownWeek.first, shownWeek.second, User::dentist().rowID);

    view->setEventList(events, clipboard_event);
}

void CalendarPresenter::setClipboard(const CalendarEvent& e)
{
    clipboard_event = e;

    view->setEventList(events, clipboard_event);
}

int CalendarPresenter::getCurrentDayColumn()
{
    QDate currentDate = QDate::currentDate();

    if (currentDate >= shownWeek.first && currentDate <= shownWeek.second) {
        return currentDate.dayOfWeek() - 1;
    }

    return -1;
}