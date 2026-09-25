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
#include "GoogleCalendar/GoogleCalendarSync.h"
#include "View/Graphics/CalendarViewData.h"
#include "Presenter/RecallPresenter.h"
#include "Presenter/RecallNotifier.h"

CalendarPresenter::CalendarPresenter(TabView* tabView) :
    TabInstance(tabView, TabType::Calendar, nullptr),
    view(tabView->calendarView())
{
    view->setCalendarPresenter(this);

    shownWeek = getTodaysWeek();

    //opening the calendar shows the current time / the start of the working day
    view->requestScrollToWorkingTime();

    //changes made in Google Calendar are shown at once (or when the calendar is shown again)
    auto& sync = GoogleCalendarSync::get();

    m_syncConnections.push_back(QObject::connect(&sync, &GoogleCalendarSync::appointmentsChanged, [this] {

        //the appointments may have been changed in Google: the last drag and drop cannot be undone any more
        clearUndo();

        if (isCurrent()) refreshView();
        else m_refreshWhenShown = true;
    }));

    m_syncConnections.push_back(QObject::connect(&sync, &GoogleCalendarSync::notice, [this](const QString& text) {
        if (isCurrent()) view->showSyncNotice(text);
    }));

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

    if (m_refreshWhenShown) {
        m_refreshWhenShown = false;
        refreshView();
    }
}

void CalendarPresenter::appointmentsWritten()
{
    GoogleCalendarSync::get().localChange();

    //a recall appointment may have been booked, moved or cancelled
    RecallNotifier::get().refresh();
}

void CalendarPresenter::createGoogleEventAgain(int index)
{
    if (index < 0 || index >= int(events.size())) return;

    GoogleCalendarSync::get().createAgain(events[index].rowid);
}

void CalendarPresenter::recallAction(int index, int action)
{
    if (index < 0 || index >= int(events.size())) return;

    clearUndo();

    //a copy: the list of the appointments is read again
    auto event = events[index];

    bool changed = false;

    switch (RecallAction(action))
    {
        case RecallAction::Complete: changed = RecallActions::completeAppointment(event); break;
        case RecallAction::Missed: changed = RecallActions::setAppointmentStatus(event, "missed"); break;
        case RecallAction::ResetStatus: changed = RecallActions::setAppointmentStatus(event, ""); break;
        case RecallAction::Mark: changed = RecallActions::setRecallAppointment(event, true); break;
        case RecallAction::Unmark: changed = RecallActions::setRecallAppointment(event, false); break;
        case RecallAction::EditRecall: RecallActions::editRecall(event.patient_rowid); break;
    }

    //the recall does not change the appointment in Google Calendar: only the calendar is shown again
    if (changed) refreshView();
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
    clearUndo();

    setClipboard(events[index]);
}

void CalendarPresenter::newDocRequested(int index, TabType type)
{
    clearUndo();

    auto& event = events[index];

    if (type == TabType::Calendar) {

        CalendarEvent newEvent;

        newEvent.summary = event.summary;
        newEvent.phone = event.phone;
        newEvent.patient_rowid = event.patient_rowid;

        setClipboard(newEvent);

        return;
    }

    RowInstance tab(type);

    tab.patientRowId = event.patient_rowid;

    if (!tab.patientRowId) {
        
        //the phone of the appointment fills the phone of the new patient
        PatientDialogPresenter d(QObject::tr("New Patient").toStdString(),
            event.phone.size() ? event.summary + " " + event.phone : event.summary);

        auto result = d.open();

        if (!result) return;

        tab.patientRowId = result->rowid;

        //the appointment is linked to the patient, so next time the patient opens directly
        event.patient_rowid = result->rowid;
        DbAppointment::update(event);
        appointmentsWritten();
    }

    TabPresenter::get().open(tab, true);

}

void CalendarPresenter::addEvent(const QTime& t, int daysFromMonday, int duration)
{
    clearUndo();

    QDateTime from(shownWeek.first.addDays(daysFromMonday), t);
    QDateTime to(from.addSecs(duration * 60));

    clipboard_event.start = from;
    clipboard_event.end = to;

    if (clipboard_event.rowid) { //existing event
        DbAppointment::update(clipboard_event);
        auto overlaps = resolveOverlaps(clipboard_event);
        appointmentsWritten();
        clipboard_event = CalendarEvent{};
        refreshView();
        showOverlapNotice(overlaps);
        return;
    }

    CalendarEventDialog d(clipboard_event);

    if (d.exec() != QDialog::Accepted) return;

    auto newEvent = d.result();

    newEvent.rowid = DbAppointment::insert(newEvent, User::dentist().rowID);

    auto overlaps = newEvent.rowid ? resolveOverlaps(newEvent) : std::vector<AppointmentOverlap::Change>{};

    appointmentsWritten();

    refreshView();

    showOverlapNotice(overlaps);
}

void CalendarPresenter::editEvent(int index)
{
    clearUndo();

    auto& e = events[index];

    CalendarEventDialog d(e);

    if (d.exec() != QDialog::Accepted) return;

    DbAppointment::update(d.result());

    auto overlaps = resolveOverlaps(d.result());

    appointmentsWritten();

    refreshView();

    showOverlapNotice(overlaps);
}

void CalendarPresenter::deleteEvent(int index)
{
    clearUndo();

    DbAppointment::remove(events[index].rowid);

    appointmentsWritten();

    refreshView();
}

void CalendarPresenter::clearClipboard()
{
    clipboard_event = CalendarEvent{};

    view->setEventList(events, clipboard_event);
}

void CalendarPresenter::durationChange(int eventIdx, int duration)
{
    clearUndo();

    auto& event = events[eventIdx];

    event.end = event.start.addSecs(duration * 60);

    DbAppointment::update(event);

    auto overlaps = resolveOverlaps(event);

    appointmentsWritten();

    refreshView();

    showOverlapNotice(overlaps);
}

void CalendarPresenter::cancelMove()
{
    clearClipboard();
}

CalendarPresenter::~CalendarPresenter()
{
    for (auto& c : m_syncConnections) QObject::disconnect(c);
}

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

    CalendarViewData::showGoogleSync = GoogleCalendarSync::get().isEnabled();

    view->setEventList(events, clipboard_event);

    refreshBusyDays();
}

void CalendarPresenter::rescheduleEvent(int index, const QDateTime& start, const QDateTime& end, bool moved)
{
    if (index < 0 || index >= int(events.size())) return;

    auto& e = events[index];

    //nothing is changed when the new time is not valid
    if (!start.isValid() || !end.isValid() || end <= start) return;

    UndoEntry undo{ e.rowid, e.start, e.end };

    if (!DbAppointment::updateTime(e.rowid, start, end)) return;

    CalendarEvent changed = e;
    changed.start = start;
    changed.end = end;

    undo.overlaps = resolveOverlaps(changed);

    appointmentsWritten();

    m_undo = undo;

    refreshView();

    int shortened = 0, removed = 0;

    for (auto& c : undo.overlaps) {
        (c.kind == AppointmentOverlap::Change::Remove ? removed : shortened)++;
    }

    view->showChangeNotice(start, end, moved, shortened, removed);
}

void CalendarPresenter::undoLastChange()
{
    if (!m_undo.rowid) return;

    auto undo = m_undo;

    m_undo = UndoEntry{};

    //only the date and time of the appointment are restored
    DbAppointment::updateTime(undo.rowid, undo.start, undo.end);

    //the appointments it had shortened or deleted come back as they were
    for (auto& c : undo.overlaps)
    {
        if (c.kind == AppointmentOverlap::Change::Remove) {
            DbAppointment::insert(c.before, User::dentist().rowID);
        }
        else {
            DbAppointment::updateTime(c.before.rowid, c.before.start, c.before.end);
        }
    }

    appointmentsWritten();

    refreshView();

    view->showUndoneNotice();
}

void CalendarPresenter::clearUndo()
{
    if (!m_undo.rowid) return;

    m_undo = UndoEntry{};

    view->hideChangeNotice();
}

std::vector<AppointmentOverlap::Change> CalendarPresenter::resolveOverlaps(const CalendarEvent& event)
{
    if (!event.start.isValid() || !event.end.isValid() || event.end <= event.start) return {};

    //appointments starting the day before can reach into this one
    auto existing = DbAppointment::get(event.start.date().addDays(-1), event.end.date(), User::dentist().rowID);

    auto changes = AppointmentOverlap::resolve(event, existing);

    for (auto& c : changes)
    {
        if (c.kind == AppointmentOverlap::Change::Remove) {
            DbAppointment::remove(c.before.rowid);
        }
        else {
            DbAppointment::updateTime(c.before.rowid, c.after.start, c.after.end);
        }
    }

    return changes;
}

void CalendarPresenter::showOverlapNotice(const std::vector<AppointmentOverlap::Change>& changes)
{
    if (changes.empty()) return;

    int shortened = 0, removed = 0;

    for (auto& c : changes) {
        (c.kind == AppointmentOverlap::Change::Remove ? removed : shortened)++;
    }

    view->showOverlapNotice(shortened, removed);
}

void CalendarPresenter::navigatorMonthsChanged()
{
    refreshBusyDays();
}

void CalendarPresenter::refreshBusyDays()
{
    QSet<QDate> days;

    for (auto& e : DbAppointment::get(view->navigatorFirstDay(), view->navigatorLastDay(), User::dentist().rowID)) {
        days.insert(e.start.date());
    }

    view->setBusyDays(days);
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