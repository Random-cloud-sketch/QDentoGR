#pragma once

#include <QTableView>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QDateTime>
#include "View/Graphics/CalendarViewData.h"
#include <utility>
#include "Model/TabType.h"

class CalendarTable;
class QScrollArea;
class QTimer;

class EventDelegate : public QStyledItemDelegate
{
	Q_OBJECT

	CalendarViewData& data;
	CalendarTable* view{ nullptr };

	QString requestTime(int row) const;

public:
	std::pair<int, int> emptyHovered{ -1, -1 };

	EventDelegate(CalendarTable* view, CalendarViewData& data);
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index);

	signals:
		void clicked(int column, int row);
		
};

//placeholder model - it doesn't use anything. Everything is painted from scratch by event delegate
class CalendarTableModel : public QAbstractTableModel
{
	Q_OBJECT

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override { return QVariant(); };

public:
	CalendarTableModel() {};

	int m_rows = 96;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override { return m_rows; }

	//each row is 15 minutes of the shown part of the day
	void setRows(int rows)
	{
		if (rows > m_rows) {
			beginInsertRows(QModelIndex(), m_rows, rows - 1);
			m_rows = rows;
			endInsertRows();
		}
		else if (rows < m_rows) {
			beginRemoveRows(QModelIndex(), rows, m_rows - 1);
			m_rows = rows;
			endRemoveRows();
		}
	}
	int columnCount(const QModelIndex& parent = QModelIndex()) const override { return 7; }

	~CalendarTableModel() {};

};

class CalendarTable : public QTableView
{
	Q_OBJECT

	CalendarTableModel m_model;

	CalendarViewData m_data;

	EventDelegate* delegate_ptr = nullptr;

	int m_today_column = -1;

	//shown part of the day and size of the grid slots
	int m_firstHour = 0;
	int m_lastHour = 24;
	int m_slotMinutes = 15;

	void leaveEvent(QEvent* event) override;
	bool viewportEvent(QEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

	//moving / resizing an appointment with the mouse.
	//Nothing is changed until the appointment is dropped on a valid place.
	struct DragState
	{
		enum Mode { Move, ResizeTop, ResizeBottom };

		bool pending{ false }; //button pressed on an appointment, not moved enough yet
		bool active{ false };
		Mode mode{ Move };
		int eventIdx{ -1 };
		QPoint pressPos;
		int column{ -1 };		//original day column
		int firstRow{ -1 };		//original rows of the appointment
		int span{ 0 };
		int grabOffsetY{ 0 };	//distance of the cursor from the top of the appointment
		QDateTime start;		//original time
		QDateTime end;

		int targetColumn{ -1 };
		QDateTime newStart;		//time at the current position
		QDateTime newEnd;
		bool valid{ false };
	};

	DragState m_drag;
	std::vector<CalendarEvent> m_events;
	QTimer* m_autoScrollTimer{ nullptr };
	QRect m_feedbackRect; //painted drag preview, for repainting only the changed area

	DragState::Mode dragModeAt(const QPoint& pos, int column, int firstRow, int span) const;
	void updateDrag(const QPoint& pos);
	void finishDrag(bool drop);
	void autoScroll();
	QScrollArea* scrollArea() const;
	QRect minutesRect(int column, int fromMinute, int toMinute) const;
	QRect dragFeedbackRect() const;
	void paintDragFeedback(QPainter& painter);

	QMenu* context_menu{ nullptr };

	bool menu_click_guard = false;

	void menuRequested(int column, int row);

	void paintEvent(QPaintEvent*) override;

public:
	CalendarTable(QWidget* parent = nullptr);

	void cellClicked(int column, int row, bool leftClick);

	void setEvents(const std::vector<CalendarEvent>& list, const CalendarEvent& clipboardEvent = CalendarEvent());

	void setTodayColumn(int today);

	int todayColumn() const { return m_today_column; }

	bool isDragging() const { return m_drag.active; }

	static constexpr int minutesPerRow = 15;

	//shows the hours from firstHour to lastHour with a grid of slotMinutes (15, 30 or 60)
	void setTimeAxis(int firstHour, int lastHour, int slotMinutes);
	int firstHour() const { return m_firstHour; }
	int lastHour() const { return m_lastHour; }
	int slotMinutes() const { return m_slotMinutes; }
	int rowsPerSlot() const { return m_slotMinutes / minutesPerRow; }
	int unitHeight() const;

	QTime rowTime(int row) const;

	//y position of a time of the day, -1 when it is outside the shown hours
	int timeToY(const QTime& time) const;

	//the free rows of the grid slot containing the row (where a new appointment is placed)
	std::pair<int, int> freeSlotRows(int column, int row) const;

signals:

	void newDocRequested(int eventIndex, TabType type);
	void eventEditRequested(int eventIndex);
	void moveEventRequested(int eventIndex);
	void deleteEventRequested(int eventIndex);
	void eventAddRequested(const QTime& t, int daysFromMonday, int minDuration);
	void eventDurationChange(int eventIndx, int minDuration);
	//the appointment was dragged to another time or resized
	void eventTimeChangeRequested(int eventIdx, const QDateTime& start, const QDateTime& end, bool moved);
	//the Google Calendar event of the appointment was deleted: a new one is requested
	void googleEventAgainRequested(int eventIdx);
	void operationCanceled();

};