#include "CalendarTable.h"

#include <QPainter>
#include <QHeaderView>
#include <QMouseEvent>
#include <QApplication>
#include <QAbstractTableModel>
#include <QMenu>
#include <QPainterPath>

#include "View/Theme.h"

EventDelegate::EventDelegate(CalendarTable* view, CalendarViewData& data) : data(data), view(view)
{
    view->setItemDelegate(this);
}

QString EventDelegate::requestTime(int row) const
{
    QString label;

    int hour = row / 4;

    int minutes = row % 4 * 15;

    if (hour < 10) {
        label += "0";
    }

    label += QString::number(hour);
    label += ":";
    
    if (minutes < 10) {
        label += "0";
    }
    
    label += QString::number(minutes);

    return label;
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
        painter->setPen(QColor(245, 245, 245));
        painter->drawRect(r);

        if (index.row() && index.row() % 4 == 0) {
            painter->setPen(QColor(224, 224, 224));
            painter->drawLine(r.topLeft(), r.topRight());
        }
    }

    //hovering empty cell
    if (column == emptyHovered.first && row == emptyHovered.second) {

        painter->fillRect(r, QColor(246, 245, 250));

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

        emptyHovered = std::make_pair(index.column(), index.row());

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

    connect(horizontalHeader(), &QHeaderView::sectionResized, this,
        [&](int logicalIndex, int oldSize, int newSize) {
            m_data.setPixelRatio(devicePixelRatioF());
            m_data.setCellSize(logicalIndex, newSize, rowHeight(1));
        });
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

    m_data.setEvents(list, clipboardEvent);

    auto rowSize = rowHeight(1);
    
    for (int i = 0; i < m_model.columnCount(); i++) {
        m_data.setCellSize(i, columnWidth(i), rowSize);
    }

    viewport()->repaint();
    
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
                emit eventAddRequested(QTime(row / 4, row % 4 * 15, 0), column, duration);

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

    if (m_today_column == -1) return;

    //painting current time marker

    QPainter painter(viewport());

    QPen pen(Qt::PenStyle::DotLine);

    pen.setWidth(2);

    pen.setColor(Qt::darkCyan);

    painter.setPen(pen);

    auto currentTime = QTime::currentTime();

    int currentMinutes = currentTime.hour() * 60 + currentTime.minute();

    double pixelsPerMinute = (double)height() / 1440; //minutes per day;

    int y = pixelsPerMinute * currentMinutes;

 //   painter.drawLine(0, y, width(), y);

    //painting non-dashed line on current day;

    pen.setStyle(Qt::PenStyle::SolidLine);

    painter.setPen(pen);

    int pixelsPerDay = width() / 7;

    painter.drawLine(pixelsPerDay * m_today_column, y, pixelsPerDay * (m_today_column + 1), y);

    QPainterPath path;

    path.addEllipse((pixelsPerDay * m_today_column)-5, y -5, 10, 10);

    painter.setRenderHint(QPainter::RenderHint::Antialiasing);

    painter.fillPath(path, Qt::darkCyan);
}
