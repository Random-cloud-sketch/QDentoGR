#include "CalendarTable.h"

#include <QPainter>
#include <QHeaderView>
#include <QMouseEvent>
#include <QApplication>
#include <QAbstractTableModel>
#include <QMenu>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QHelpEvent>
#include <QToolTip>
#include <QTimer>
#include <QLocale>
#include <QKeyEvent>
#include <cmath>

#include "GlobalSettings.h"

#include "View/Theme.h"
#include "Model/Recall.h"

EventDelegate::EventDelegate(CalendarTable* view, CalendarViewData& data) : data(data), view(view)
{
    view->setItemDelegate(this);
}

QString EventDelegate::requestTime(int row) const
{
    return view->rowTime(row).toString("HH:mm");
}


void EventDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    int row = index.row();
    int column = index.column();

    QRect r = option.rect;//getting the rect of the cell

    painter->fillRect(r, Qt::white);

    //filling cells of today column
    if (column == view->todayColumn()) {
        painter->fillRect(r, QColor(250, 250, 250));
    }

    auto px = data.requestPixmap(column, row);
    
    if (!px.isNull()) {
        painter->drawPixmap(r, px, px.rect());

        return;
    }
    else {
        //light lines around the grid slots, darker line every hour
        painter->setPen(QColor(245, 245, 245));
        painter->drawLine(r.topLeft(), r.bottomLeft());
        painter->drawLine(r.topRight(), r.bottomRight());

        if (row % view->rowsPerSlot() == 0) {
            painter->drawLine(r.topLeft(), r.topRight());
        }

        if (index.row() && index.row() % 4 == 0) {
            painter->setPen(QColor(224, 224, 224));
            painter->drawLine(r.topLeft(), r.topRight());
        }
    }

    if (column != emptyHovered.first || emptyHovered.second < 0 || view->isDragging()) return;

    //hovering the free part of a grid slot
    auto [firstRow, lastRow] = view->freeSlotRows(column, emptyHovered.second);

    if (row >= firstRow && row <= lastRow) {

        painter->fillRect(r, QColor(246, 245, 250));

        //the time is written once, in the first row of the slot
        if (row != firstRow) return;

        auto font = painter->font();

        font.setBold(true);

        painter->setFont(font);
        
        painter->setPen(Qt::lightGray);

        QString text = requestTime(row);
    
        QTextOption textOption(Qt::Alignment(Qt::AlignVCenter | Qt::AlignHCenter));

        if (data.clipboardEventText().size()) {

            text = "   " + text;
            text += " - " + data.clipboardEventText();
            textOption.setWrapMode(QTextOption::WrapMode::ManualWrap);
            textOption.setAlignment(Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter));
        }

        painter->drawText(r, text, textOption);
    }
    
}

bool EventDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{

    if (event->type() == QEvent::MouseMove) {

        if (view->isDragging()) return false;

        //the hover covers a whole grid slot, so the old and the new slot are repainted
        auto updateSlot = [&](std::pair<int, int> cell) {
            if (cell.first < 0 || cell.second < 0) return;
            auto [first, last] = view->freeSlotRows(cell.first, cell.second);
            for (int r = first; r <= last; r++) {
                view->update(view->model()->index(r, cell.first));
            }
        };

        auto newHovered = std::make_pair(index.column(), index.row());

        if (newHovered != emptyHovered) {
            updateSlot(emptyHovered);
            updateSlot(newHovered);
        }

        emptyHovered = newHovered;

        auto idxToUpdate = data.setHovered(index.column(), index.row());
        
        if (idxToUpdate.empty()) return false;

        view->setUpdatesEnabled(false);

        for (auto& [column, row] : idxToUpdate) {
            view->update(view->model()->index(row, column));
        }

        view->setUpdatesEnabled(true);

        return false;
    }
    if (event->type() == QEvent::MouseButtonPress) {

        auto button = static_cast<QMouseEvent*>(event)->button();

            view->cellClicked(index.column(), index.row(), button == Qt::LeftButton);

    }

    return false;
}


CalendarTable::CalendarTable(QWidget* parent) : QTableView(parent)
{
    setModel(&m_model);
    
    delegate_ptr = new EventDelegate(this, m_data);

    //appointments are moved with the mouse by the table itself
    setDragEnabled(false);
    setDragDropMode(QAbstractItemView::NoDragDrop);

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(16);
    connect(m_autoScrollTimer, &QTimer::timeout, this, [this] { autoScroll(); });

    connect(horizontalHeader(), &QHeaderView::sectionResized, this,
        [&](int logicalIndex, int oldSize, int newSize) {
            m_data.setPixelRatio(devicePixelRatioF());
            m_data.setCellSize(logicalIndex, newSize, unitHeight());
        });
}

bool CalendarTable::viewportEvent(QEvent* event)
{
    //the Google Calendar synchronization state of the appointment under the mouse
    if (event->type() == QEvent::ToolTip && CalendarViewData::showGoogleSync)
    {
        auto help = static_cast<QHelpEvent*>(event);
        auto index = indexAt(help->pos());
        int eventIdx = index.isValid() ? m_data.eventListIndex(index.column(), index.row()) : -1;

        if (eventIdx >= 0 && eventIdx < int(m_events.size()))
        {
            auto& s = m_events[eventIdx].googleStatus;

            QString text =
                s == "synced" ? tr("Synchronized with Google Calendar") :
                s == "pending_create" || s == "pending_update" ? tr("Waiting for synchronization with Google Calendar") :
                s == "error" ? tr("Google Calendar synchronization error") :
                s == "unlinked" ? tr("The Google Calendar event of this appointment was deleted") :
                tr("Not synchronized with Google Calendar");

            QToolTip::showText(help->globalPos(), text, viewport());
            return true;
        }

        QToolTip::hideText();
        return true;
    }

    return QTableView::viewportEvent(event);
}

void CalendarTable::leaveEvent(QEvent* event)
{
    if(!context_menu || !context_menu->isVisible()) {

        delegate_ptr->emptyHovered = { -1, -1 };

        viewport()->repaint();
    }

    QWidget::leaveEvent(event);
}

void CalendarTable::mouseDoubleClickEvent(QMouseEvent* event)
{
    m_drag.pending = false;

    //double click on an appointment opens the patient (a new dental visit, or the one already opened)
    if (event->button() == Qt::LeftButton) {

        auto index = indexAt(event->position().toPoint());

        auto eventIdx = index.isValid() ? m_data.eventListIndex(index.column(), index.row()) : -1;

        if (eventIdx != -1) {
            emit newDocRequested(eventIdx, TabType::DentalVisit);
            return;
        }
    }

    QTableView::mouseDoubleClickEvent(event);
}

void CalendarTable::setEvents(const std::vector<CalendarEvent>& list, const CalendarEvent& clipboardEvent)
{
    setUpdatesEnabled(true);

    //the indexes of a drag in progress are not valid for a new list
    if (m_drag.active || m_drag.pending) finishDrag(false);

    m_events = list;

    m_data.setEvents(list, clipboardEvent);

    auto rowSize = unitHeight();
    
    for (int i = 0; i < m_model.columnCount(); i++) {
        m_data.setCellSize(i, columnWidth(i), rowSize);
    }

    viewport()->repaint();
    
}

int CalendarTable::unitHeight() const
{
    //a larger slot makes the day more compact
    switch (m_slotMinutes)
    {
    case 30: return 20;
    case 60: return 16;
    default: return 24;
    }
}

void CalendarTable::setTimeAxis(int firstHour, int lastHour, int slotMinutes)
{
    m_firstHour = firstHour;
    m_lastHour = lastHour;
    m_slotMinutes = slotMinutes;

    int rows = (lastHour - firstHour) * 60 / minutesPerRow;

    m_model.setRows(rows);
    m_data.setTimeRange(firstHour * 60, rows);

    int unit = unitHeight();

    verticalHeader()->setMinimumSectionSize(1);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(unit);

    for (int i = 0; i < rows; i++) {
        setRowHeight(i, unit);
    }

    setMinimumHeight(0);
    setFixedHeight(rows * unit);

    delegate_ptr->emptyHovered = { -1, -1 };

    viewport()->update();
}

QTime CalendarTable::rowTime(int row) const
{
    int minutes = m_firstHour * 60 + row * minutesPerRow;

    return QTime(0, 0).addSecs(std::min(minutes, 24 * 60 - 1) * 60);
}

int CalendarTable::timeToY(const QTime& time) const
{
    int minutes = time.hour() * 60 + time.minute() - m_firstHour * 60;

    if (minutes < 0 || minutes > (m_lastHour - m_firstHour) * 60) return -1;

    return minutes * unitHeight() / minutesPerRow;
}

std::pair<int, int> CalendarTable::freeSlotRows(int column, int row) const
{
    //no free part when the row itself is an appointment
    if (m_data.eventListIndex(column, row) != -1) return { row + 1, row };

    int first = row - row % rowsPerSlot();
    int last = first + rowsPerSlot() - 1;

    //an appointment inside the slot limits the free part
    for (int r = first; r < row; r++) {
        if (m_data.eventListIndex(column, r) != -1) first = r + 1;
    }

    for (int r = row + 1; r <= last; r++) {
        if (m_data.eventListIndex(column, r) != -1) {
            last = r - 1;
            break;
        }
    }

    return { first, std::min(last, m_model.rowCount() - 1) };
}

void CalendarTable::setTodayColumn(int today)
{
    if (today > 6 || today < 0) {
        m_today_column = -1;
    }

    m_today_column = today;

    viewport()->update();
}

void CalendarTable::cellClicked(int column, int row, bool leftClick)
{

    auto idx = m_data.eventListIndex(column, row);

    //appointments: a single left click does nothing,
    //the menu is shown with the right click and the patient is opened with a double click
    if (idx != -1 && leftClick) return;

    if (idx == -1 && menu_click_guard && !leftClick){
        menu_click_guard = false;
        return;
    }
    else {
        menuRequested(column, row);
    }
}

void CalendarTable::menuRequested(int column, int row)
{
    menu_click_guard = true;

    if (context_menu) {
        delete context_menu;
    }

    context_menu = new QMenu(this);

    context_menu->setStyleSheet(Theme::getPopupMenuStylesheet());

    auto eventIdx = m_data.eventListIndex(column, row);

    m_data.setHovered(column, row);

    if (eventIdx == -1) {
        delegate_ptr->emptyHovered = { column, row };
        //refreshing the data completely
        viewport()->repaint();
    }

    connect(context_menu, &QMenu::aboutToHide, context_menu, [&] {
        //refreshing the mouse hover

        delegate_ptr->emptyHovered = {-1, -1};

        auto index = indexAt(QCursor::pos());

        m_data.setHovered(index.column(), index.row());

        viewport()->repaint();
    });

    std::pair<QString, int> labelDurationPair[] =
    {
        {tr("15 min."), 15},
        {tr("30 min."), 30},
        {tr("45 min."), 45},
        {tr("1 hour"), 60},
        {tr("1 hour and a half"), 90},
        {tr("2 hours"), 120}
    };

    //initializing the entity menu
    if (eventIdx != -1) {

        QAction* action;

        QMenu* subMenu;

        bool isPatientSpecific = true;//m_data.hasPatient(row, column);
      
        //initializing patient specific menu
        if (isPatientSpecific) {

            subMenu = new QMenu(tr("Open"), context_menu);
            subMenu->setIcon(QIcon(":/icons/icon_open.png"));

            action = (new QAction(tr("New Dental Visit"), subMenu));
            connect(action, &QAction::triggered, this, [=, this] { emit newDocRequested(eventIdx, TabType::DentalVisit); });
            action->setIcon(QIcon(":/icons/icon_sheet.png"));
            subMenu->addAction(action);

            action = (new QAction(tr("New Periodontal Measurment"), subMenu));
            connect(action, &QAction::triggered, this, [=, this] { emit newDocRequested(eventIdx, TabType::PerioStatus); });
            action->setIcon(QIcon(":/icons/icon_periosheet.png"));
            subMenu->addAction(action);

            action = (new QAction(tr("New Financial Document"), subMenu));
            connect(action, &QAction::triggered, this, [=, this] { emit newDocRequested(eventIdx, TabType::Financial); });
            action->setIcon(QIcon(":/icons/icon_invoice.png"));
            subMenu->addAction(action);

            //invoices are not used for now: the menu entry is only hidden (delete this line to show it again)
            action->setVisible(false);

            action = (new QAction(tr("Patient History"), subMenu));
            connect(action, &QAction::triggered, this, [=, this] { emit newDocRequested(eventIdx, TabType::PatientSummary); });
            action->setIcon(QIcon(":/icons/icon_history.png"));
            subMenu->addAction(action);

            context_menu->addMenu(subMenu);
        }

        action = (new QAction(tr("Edit"), context_menu));
        action->setIcon(QIcon(":/icons/icon_edit.png"));

        connect(action, &QAction::triggered, context_menu, [=, this] {
            emit eventEditRequested(eventIdx);
        });
        context_menu->addAction(action);

        action = (new QAction(tr("Move"), context_menu));
        action->setIcon(QIcon(":/icons/icon_copy.png"));

        connect(action, &QAction::triggered, context_menu, [=, this] {
            emit moveEventRequested(eventIdx);
        });
        context_menu->addAction(action);

        subMenu = new QMenu(tr("Duration"), context_menu);

        context_menu->addMenu(subMenu);

        for (auto& [label, duration] : labelDurationPair){
            
            action = new QAction(label, subMenu);

            connect(action, &QAction::triggered, context_menu, [=, this] {
                emit eventDurationChange(eventIdx, duration);
            });
            subMenu->addAction(action);

        }

        action = (new QAction(tr("Schedule Next Appointment"), context_menu));
        connect(action, &QAction::triggered, this, [=, this] { emit newDocRequested(eventIdx, TabType::Calendar); });
        action->setIcon(QIcon(":/icons/icon_calendar.png"));
        context_menu->addAction(action);
  
        //the Google Calendar event of the appointment was deleted
        if (CalendarViewData::showGoogleSync && eventIdx < int(m_events.size()) && m_events[eventIdx].googleStatus == "unlinked")
        {
            action = (new QAction(tr("Create again in Google Calendar"), context_menu));
            connect(action, &QAction::triggered, context_menu, [=, this] { emit googleEventAgainRequested(eventIdx); });
            context_menu->addAction(action);
        }

        //periodontal recall
        if (eventIdx < int(m_events.size()) && m_events[eventIdx].patient_rowid)
        {
            auto& e = m_events[eventIdx];

            auto recallAction = [&](QMenu* menu, const QString& text, RecallAction a) {
                auto act = new QAction(text, menu);
                connect(act, &QAction::triggered, this, [=, this] { emit recallActionRequested(eventIdx, int(a)); });
                menu->addAction(act);
                return act;
            };

            if (e.recall)
            {
                subMenu = new QMenu(tr("Periodontal recall"), context_menu);
                subMenu->setIcon(QIcon(":/icons/icon_sync.png"));

                if (e.recallStatus != "completed") recallAction(subMenu, tr("Mark completed..."), RecallAction::Complete);
                if (e.recallStatus != "missed") recallAction(subMenu, tr("Mark missed"), RecallAction::Missed);
                if (e.recallStatus.size()) recallAction(subMenu, tr("Set back to scheduled"), RecallAction::ResetStatus);

                subMenu->addSeparator();
                recallAction(subMenu, tr("Recall of the patient..."), RecallAction::EditRecall);
                recallAction(subMenu, tr("Not a recall appointment"), RecallAction::Unmark);

                context_menu->addMenu(subMenu);
            }
            else
            {
                recallAction(context_menu, tr("Mark as recall appointment"), RecallAction::Mark)->setIcon(QIcon(":/icons/icon_sync.png"));
            }
        }

        action = (new QAction(tr("Cancel"), context_menu));
        action->setIcon(QIcon(":/icons/icon_remove.png"));
        connect(action, &QAction::triggered, context_menu, [=, this] {

                emit deleteEventRequested(eventIdx);
        });
        context_menu->addAction(action);
    }
    else {

        QAction* action;

        for (auto& [label, duration] : labelDurationPair) {

            action = new QAction(tr("Set ") + label, context_menu);
            connect(action, &QAction::triggered, context_menu, [=, this] {
                emit eventAddRequested(rowTime(freeSlotRows(column, row).first), column, duration);

            });
            context_menu->addAction(action);
        }

        if (m_data.clipboardEventText().size()) {
            
            action = new QAction(tr("Cancel"), context_menu);
            action->setIcon(QIcon(":/icons/icon_remove.png"));
            connect(action, &QAction::triggered, context_menu, [=, this] { emit operationCanceled(); });
            context_menu->addAction(action);

        }

    }

    context_menu->popup(QCursor::pos());
}

void CalendarTable::paintEvent(QPaintEvent* e)
{
    QTableView::paintEvent(e);

    QPainter painter(viewport());

    //current time marker on today's column
    int y = m_today_column == -1 ? -1 : timeToY(QTime::currentTime());

    if (y >= 0) {

        QPen pen(Qt::darkCyan);
        pen.setWidth(2);
        painter.setPen(pen);

        int pixelsPerDay = width() / 7;

        painter.drawLine(pixelsPerDay * m_today_column, y, pixelsPerDay * (m_today_column + 1), y);

        QPainterPath path;

        path.addEllipse((pixelsPerDay * m_today_column) - 5, y - 5, 10, 10);

        painter.setRenderHint(QPainter::RenderHint::Antialiasing);

        painter.fillPath(path, Qt::darkCyan);
    }

    paintDragFeedback(painter);
}

//---------------------------------------------------------------- moving and resizing appointments with the mouse

static QDateTime dateTimeAt(const QDate& date, int minuteOfDay)
{
    return QDateTime(date.addDays(minuteOfDay / (24 * 60)), QTime(0, 0).addSecs((minuteOfDay % (24 * 60)) * 60));
}

static int minuteOfDay(const QDateTime& dateTime, const QDate& day)
{
    return day.daysTo(dateTime.date()) * 24 * 60 + dateTime.time().hour() * 60 + dateTime.time().minute();
}

CalendarTable::DragState::Mode CalendarTable::dragModeAt(const QPoint& pos, int column, int firstRow, int span) const
{
    QRect top = visualRect(m_model.index(firstRow, column));
    QRect bottom = visualRect(m_model.index(firstRow + span - 1, column));

    //thin handles at the top and at the bottom edge, the rest of the appointment moves it
    int handle = std::min(6, (bottom.bottom() - top.top()) / 4);

    if (pos.y() <= top.top() + handle) return DragState::ResizeTop;
    if (pos.y() >= bottom.bottom() - handle) return DragState::ResizeBottom;

    return DragState::Move;
}

void CalendarTable::mousePressEvent(QMouseEvent* event)
{
    auto pos = event->position().toPoint();
    auto index = indexAt(pos);

    int firstRow = -1, span = 0;

    if (event->button() == Qt::LeftButton && index.isValid() &&
        m_data.eventRows(index.column(), index.row(), firstRow, span))
    {
        int eventIdx = m_data.eventListIndex(index.column(), index.row());

        if (eventIdx >= 0 && eventIdx < int(m_events.size()))
        {
            //the drag starts only after the mouse moves a little, a click stays a click
            m_drag = DragState{};
            m_drag.pending = true;
            m_drag.mode = dragModeAt(pos, index.column(), firstRow, span);
            m_drag.eventIdx = eventIdx;
            m_drag.pressPos = pos;
            m_drag.column = index.column();
            m_drag.firstRow = firstRow;
            m_drag.span = span;
            m_drag.grabOffsetY = pos.y() - visualRect(m_model.index(firstRow, index.column())).top();
            m_drag.start = m_events[eventIdx].start;
            m_drag.end = m_events[eventIdx].end;
        }
    }

    QTableView::mousePressEvent(event);
}

void CalendarTable::mouseMoveEvent(QMouseEvent* event)
{
    auto pos = event->position().toPoint();

    if (m_drag.pending && !m_drag.active &&
        (pos - m_drag.pressPos).manhattanLength() >= QApplication::startDragDistance())
    {
        m_drag.active = true;
        m_drag.pending = false;

        delegate_ptr->emptyHovered = { -1, -1 };

        setFocus(); //Esc cancels the drag
        viewport()->setCursor(m_drag.mode == DragState::Move ? Qt::ClosedHandCursor : Qt::SizeVerCursor);

        m_autoScrollTimer->start();
    }

    if (m_drag.active) {
        updateDrag(pos);
        return;
    }

    //cursor over the appointments: move or resize
    if (!(event->buttons() & Qt::LeftButton))
    {
        auto index = indexAt(pos);
        int firstRow = -1, span = 0;

        if (index.isValid() && m_data.eventRows(index.column(), index.row(), firstRow, span)) {
            viewport()->setCursor(dragModeAt(pos, index.column(), firstRow, span) == DragState::Move ?
                Qt::OpenHandCursor : Qt::SizeVerCursor);
        }
        else {
            viewport()->unsetCursor();
        }
    }

    QTableView::mouseMoveEvent(event);
}

void CalendarTable::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_drag.active) {
        updateDrag(event->position().toPoint());
        finishDrag(true);
        return;
    }

    m_drag.pending = false;

    QTableView::mouseReleaseEvent(event);
}

void CalendarTable::keyPressEvent(QKeyEvent* event)
{
    if (m_drag.active && event->key() == Qt::Key_Escape) {
        finishDrag(false);
        return;
    }

    QTableView::keyPressEvent(event);
}

QRect CalendarTable::minutesRect(int column, int fromMinute, int toMinute) const
{
    int first = m_firstHour * 60;

    int top = (fromMinute - first) * unitHeight() / minutesPerRow;
    int bottom = (toMinute - first) * unitHeight() / minutesPerRow;

    return QRect(columnViewportPosition(column), top, columnWidth(column), std::max(bottom - top, unitHeight()));
}

void CalendarTable::updateDrag(const QPoint& pos)
{
    auto& d = m_drag;

    int firstMinute = m_firstHour * 60;
    int lastMinute = m_lastHour * 60;
    int slot = m_slotMinutes;
    QDate day = d.start.date();

    int startMinute = minuteOfDay(d.start, day);
    int endMinute = minuteOfDay(d.end, day);
    int duration = endMinute - startMinute;

    //the time under the cursor, rounded to the grid slots of the calendar
    auto snappedMinute = [&](int y) {
        double minute = firstMinute + double(y) * minutesPerRow / unitHeight();
        return int(std::lround((minute - firstMinute) / slot)) * slot + firstMinute;
    };

    //dropping outside the days of the week is not possible
    d.valid = pos.x() >= 0 && pos.x() < viewport()->width();

    d.targetColumn = d.column;

    switch (d.mode)
    {
    case DragState::Move:
    {
        int column = columnAt(std::clamp(pos.x(), 0, viewport()->width() - 1));
        if (column >= 0) d.targetColumn = column;

        int newStart = snappedMinute(pos.y() - d.grabOffsetY);

        //the whole appointment stays in the shown hours of the day
        newStart = std::clamp(newStart, firstMinute, std::max(firstMinute, lastMinute - duration));

        if (duration > lastMinute - firstMinute) d.valid = false;

        QDate newDay = day.addDays(d.targetColumn - d.column);

        d.newStart = dateTimeAt(newDay, newStart);
        d.newEnd = dateTimeAt(newDay, newStart + duration);
        break;
    }
    case DragState::ResizeTop:
    {
        int newStart = std::clamp(snappedMinute(pos.y()), firstMinute, endMinute - minutesPerRow);

        d.newStart = dateTimeAt(day, newStart);
        d.newEnd = d.end;
        break;
    }
    case DragState::ResizeBottom:
    {
        int newEnd = std::clamp(snappedMinute(pos.y()), startMinute + minutesPerRow, std::max(lastMinute, startMinute + minutesPerRow));

        d.newStart = d.start;
        d.newEnd = dateTimeAt(day, newEnd);
        break;
    }
    }

    viewport()->setCursor(!d.valid ? Qt::ForbiddenCursor : d.mode == DragState::Move ? Qt::ClosedHandCursor : Qt::SizeVerCursor);

    //only the old and the new preview are repainted
    QRect feedback = dragFeedbackRect();

    viewport()->update(m_feedbackRect.united(feedback));

    m_feedbackRect = feedback;
}

QRect CalendarTable::dragFeedbackRect() const
{
    auto& d = m_drag;

    if (!d.active) return QRect();

    //the original place, the preview and the day column which receives the appointment
    QRect original = visualRect(m_model.index(d.firstRow, d.column)).united(
        visualRect(m_model.index(d.firstRow + d.span - 1, d.column)));

    QRect column(columnViewportPosition(d.targetColumn), 0, columnWidth(d.targetColumn), viewport()->height());

    return original.united(column).adjusted(-2, -2, 2, 2);
}

void CalendarTable::paintDragFeedback(QPainter& painter)
{
    auto& d = m_drag;

    if (!d.active) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    //the original place stays visible, faded with a dashed outline
    QRect original = visualRect(m_model.index(d.firstRow, d.column)).united(
        visualRect(m_model.index(d.firstRow + d.span - 1, d.column))).adjusted(2, 2, -2, -2);

    painter.fillRect(original, QColor(255, 255, 255, 150));
    painter.setPen(QPen(Theme::fontTurquoise, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(original, 7, 7);

    if (d.valid)
    {
        QDate day = d.newStart.date();

        //the day receiving the appointment
        painter.fillRect(QRect(columnViewportPosition(d.targetColumn), 0, columnWidth(d.targetColumn), viewport()->height()),
            QColor(170, 215, 220, 45));

        int from = minuteOfDay(d.newStart, day);
        int to = minuteOfDay(d.newEnd, day);

        QRect ghost = minutesRect(d.targetColumn, from, to).adjusted(2, 2, -2, -2);

        //semi-transparent preview at the new place
        QPainterPath path;
        path.addRoundedRect(QRectF(ghost), 7, 7);
        QColor fill = Theme::inactiveTabBGHover;
        fill.setAlpha(210);
        painter.fillPath(path, fill);
        painter.setPen(QPen(Theme::fontTurquoise, 2));
        painter.drawPath(path);

        //the resulting day and time, then the appointment text
        QLocale locale = GlobalSettings::isGreekUi() ? QLocale(QLocale::Greek, QLocale::Greece) : QLocale();

        QString time = locale.toString(day, "ddd d/M") + "  " +
            d.newStart.toString("HH:mm") + " - " + (to == 24 * 60 ? QString("24:00") : d.newEnd.toString("HH:mm"));

        auto& e = m_events[d.eventIdx];
        QString text = QString::fromStdString(e.summary);
        if (e.phone.size()) text += "\n" + QCoreApplication::translate("CalendarViewData", "TEL.") + " " + QString::fromStdString(e.phone);
        if (e.description.size()) text += "\n" + QString::fromStdString(e.description);

        QFont bold = painter.font();
        bold.setBold(true);
        painter.setFont(bold);

        QRect textRect = ghost.adjusted(5, ghost.height() < 24 ? 0 : 3, -5, -2);

        painter.setPen(Theme::fontRed);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, time);

        painter.setPen(Theme::fontTurquoise);
        painter.drawText(textRect.adjusted(0, painter.fontMetrics().height(), 0, 0), Qt::AlignLeft | Qt::AlignTop, text);
    }

    painter.restore();
}

void CalendarTable::finishDrag(bool drop)
{
    auto d = m_drag;

    m_drag = DragState{};
    m_autoScrollTimer->stop();
    viewport()->unsetCursor();
    viewport()->update(m_feedbackRect);
    m_feedbackRect = QRect();

    if (!drop || !d.active || !d.valid) return;

    if (d.newStart == d.start && d.newEnd == d.end) return;

    emit eventTimeChangeRequested(d.eventIdx, d.newStart, d.newEnd, d.mode == DragState::Move);
}

QScrollArea* CalendarTable::scrollArea() const
{
    for (auto w = parentWidget(); w; w = w->parentWidget()) {
        if (auto area = qobject_cast<QScrollArea*>(w)) return area;
    }

    return nullptr;
}

void CalendarTable::autoScroll()
{
    auto area = scrollArea();

    if (!m_drag.active || !area) return;

    //faster when the cursor is closer to (or beyond) the edge of the visible part
    constexpr int edge = 50;
    constexpr int maxStep = 24;

    int y = area->viewport()->mapFromGlobal(QCursor::pos()).y();
    int height = area->viewport()->height();

    int step = 0;

    if (y < edge) step = -std::min(maxStep, 2 + (edge - y) * maxStep / edge);
    else if (y > height - edge) step = std::min(maxStep, 2 + (y - (height - edge)) * maxStep / edge);

    if (!step) return;

    auto bar = area->verticalScrollBar();
    int before = bar->value();
    bar->setValue(before + step);

    //the preview follows the cursor while the calendar scrolls
    if (bar->value() != before) {
        updateDrag(viewport()->mapFromGlobal(QCursor::pos()));
    }
}
