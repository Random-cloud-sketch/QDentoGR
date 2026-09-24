#include "CalendarNavigator.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLocale>
#include <QGraphicsDropShadowEffect>
#include <functional>
#include <QCoreApplication>

#include "View/Theme.h"
#include "View/uiComponents/IconButton.h"
#include "GlobalSettings.h"

//Greek interface shows Greek month and day names; otherwise the default locale is used
static QLocale namesLocale()
{
	return GlobalSettings::isGreekUi() ? QLocale(QLocale::Greek, QLocale::Greece) : QLocale();
}

static QDate firstOfMonth(const QDate& date)
{
	return QDate(date.year(), date.month(), 1);
}

//One month as a grid of 7 equal columns (Monday first) and 6 weeks.
//The column width follows the width of the widget, every date is centered in its cell.
class NavigatorMonthGrid : public QWidget
{
	QDate m_month;
	const CalendarNavigator::Marks& m_marks;
	int m_hovered{ -1 };

	static constexpr int headerHeight = 22;
	static constexpr int rowHeight = 28;
	static constexpr int weeks = 6;

	QDate firstCell() const { return m_month.addDays(1 - m_month.dayOfWeek()); }

	qreal columnWidth() const { return width() / 7.0; }

	QRectF cellRect(int cell) const
	{
		return QRectF((cell % 7) * columnWidth(), headerHeight + (cell / 7) * rowHeight, columnWidth(), rowHeight);
	}

	int cellAt(const QPointF& pos) const
	{
		if (pos.y() < headerHeight || pos.x() < 0 || pos.x() >= width()) return -1;

		int row = int(pos.y() - headerHeight) / rowHeight;
		int column = int(pos.x() / columnWidth());

		if (row >= weeks || column > 6) return -1;

		return row * 7 + column;
	}

public:
	std::function<void(QDate)> clicked;

	NavigatorMonthGrid(const CalendarNavigator::Marks& marks, QWidget* parent) : QWidget(parent), m_marks(marks)
	{
		setMouseTracking(true);
		setCursor(Qt::PointingHandCursor);
		setFixedHeight(headerHeight + weeks * rowHeight);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	void setMonth(QDate month)
	{
		m_month = month;
		update();
	}

protected:
	void paintEvent(QPaintEvent*) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);

		//weekday names
		static const char* dayNames[] = {
			QT_TRANSLATE_NOOP("CalendarNavigator", "Mon"),
			QT_TRANSLATE_NOOP("CalendarNavigator", "Tue"),
			QT_TRANSLATE_NOOP("CalendarNavigator", "Wed"),
			QT_TRANSLATE_NOOP("CalendarNavigator", "Thu"),
			QT_TRANSLATE_NOOP("CalendarNavigator", "Fri"),
			QT_TRANSLATE_NOOP("CalendarNavigator", "Sat"),
			QT_TRANSLATE_NOOP("CalendarNavigator", "Sun")
		};

		QFont headerFont = font();
		headerFont.setPointSizeF(font().pointSizeF() * 0.85);
		p.setFont(headerFont);
		p.setPen(QColor(150, 150, 160));

		for (int column = 0; column < 7; column++)
		{
			QString name = QCoreApplication::translate("CalendarNavigator", dayNames[column]);

			p.drawText(QRectF(column * columnWidth(), 0, columnWidth(), headerHeight), Qt::AlignCenter, name);
		}

		QDate first = firstCell();
		QDate lastOfMonth = m_month.addDays(m_month.daysInMonth() - 1);

		auto inMonth = [&](const QDate& d) { return d >= m_month && d <= lastOfMonth; };

		//the shown week: one continuous band with rounded ends (only over the days of this month)
		for (int row = 0; row < weeks; row++)
		{
			int from = -1, to = -1;

			for (int column = 0; column < 7; column++)
			{
				QDate d = first.addDays(row * 7 + column);

				if (inMonth(d) && d >= m_marks.weekFrom && d <= m_marks.weekTo) {
					if (from == -1) from = column;
					to = column;
				}
			}

			if (from == -1) continue;

			QRectF start = cellRect(row * 7 + from);
			QRectF end = cellRect(row * 7 + to);
			QRectF band(start.left() + 2, start.top() + 2, end.right() - start.left() - 4, rowHeight - 4);

			QPainterPath path;
			path.addRoundedRect(band, band.height() / 2, band.height() / 2);
			p.fillPath(path, Theme::inactiveTabBG);
		}

		QFont dayFont = font();
		QFont boldFont = font();
		boldFont.setBold(true);

		QDate today = QDate::currentDate();
		qreal diameter = std::min<qreal>(columnWidth(), rowHeight) - 4;

		for (int cell = 0; cell < weeks * 7; cell++)
		{
			QDate d = first.addDays(cell);
			QRectF r = cellRect(cell);
			QRectF circle(r.center().x() - diameter / 2, r.center().y() - diameter / 2, diameter, diameter);

			bool selected = d == m_marks.selected && inMonth(d);
			bool isToday = d == today && inMonth(d);

			QColor textColor = inMonth(d) ? QColor(40, 40, 40) : QColor(175, 175, 185);

			if (selected) {
				p.setPen(Qt::NoPen);
				p.setBrush(Theme::fontTurquoise);
				p.drawEllipse(circle);
				textColor = Qt::white;
			}
			else if (cell == m_hovered) {
				p.setPen(Qt::NoPen);
				p.setBrush(QColor(0, 0, 0, 18));
				p.drawEllipse(circle);
			}

			if (isToday) {
				p.setPen(QPen(Theme::fontRed, 1.6));
				p.setBrush(Qt::NoBrush);
				p.drawEllipse(circle.adjusted(0.8, 0.8, -0.8, -0.8));
				if (!selected) textColor = Theme::fontRed;
			}

			p.setFont(selected || isToday ? boldFont : dayFont);
			p.setPen(textColor);
			p.drawText(r, Qt::AlignCenter, QString::number(d.day()));

			//days with appointments: a small dot under the number
			if (inMonth(d) && m_marks.busyDays.contains(d)) {
				p.setPen(Qt::NoPen);
				p.setBrush(selected ? QColor(Qt::white) : Theme::fontTurquoise);
				p.drawEllipse(QPointF(r.center().x(), r.center().y() + 9), 1.5, 1.5);
			}
		}
	}

	void mouseMoveEvent(QMouseEvent* e) override
	{
		int cell = cellAt(e->position());

		if (cell != m_hovered) {
			m_hovered = cell;
			update();
		}
	}

	void leaveEvent(QEvent*) override
	{
		m_hovered = -1;
		update();
	}

	void mouseReleaseEvent(QMouseEvent* e) override
	{
		if (e->button() != Qt::LeftButton) return;

		int cell = cellAt(e->position());

		if (cell != -1 && clicked) {
			clicked(firstCell().addDays(cell));
		}
	}
};

CalendarNavigator::CalendarNavigator(QWidget* parent) : QWidget(parent)
{
	setObjectName("calendarNavigator");
	setFixedWidth(256);

	//styles only for the navigator
	setStyleSheet(
		"#navMonthCard { background-color: white; border: 1px solid " + Theme::colorToString(Theme::border) + " border-radius: 10px; }"
		"#navMonthTitle { font-weight: bold; color: " + Theme::colorToString(Theme::fontTurquoise) + " }"
		"#navRangeTitle { font-weight: bold; font-size: 11pt; color: " + Theme::colorToString(Theme::fontTurquoise) + " }"
		"#navScroll, #navScrollContents { background: transparent; border: none; }"
	);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	//the header and the month cards are one block, vertically centered in the sidebar.
	//When the sidebar is not high enough the block scrolls instead of being squeezed.
	auto scroll = new QScrollArea(this);
	scroll->setObjectName("navScroll");
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setFrameShape(QFrame::NoFrame);

	auto contents = new QWidget(scroll);
	contents->setObjectName("navScrollContents");

	//the same gap between the header and the first card as between the cards
	auto block = new QVBoxLayout(contents);
	block->setContentsMargins(14, 12, 14, 12);
	block->setSpacing(12);

	//extra height goes above and below the block, the cards keep their natural height
	block->addStretch(1);

	//navigation between months: equal buttons on both sides keep the title centered
	auto header = new QHBoxLayout();
	header->setContentsMargins(0, 0, 0, 0);
	header->setSpacing(8);

	auto arrow = QPixmap(":/icons/icon_downArrow.png");

	auto makeButton = [&](int rotation, const QString& tooltip) {
		auto button = new IconButton(this);
		button->setIcon(QIcon(arrow.transformed(QTransform().rotate(rotation))));
		button->setFixedSize(30, 30);
		button->setHoverColor(Theme::inactiveTabBG);
		button->setToolTip(tooltip);
		return button;
	};

	prevButton = makeButton(90, tr("Previous month"));
	nextButton = makeButton(270, tr("Next month"));

	rangeLabel = new QLabel(this);
	rangeLabel->setObjectName("navRangeTitle");
	rangeLabel->setAlignment(Qt::AlignCenter);

	header->addWidget(prevButton);
	header->addWidget(rangeLabel, 1);
	header->addWidget(nextButton);

	block->addLayout(header);

	connect(prevButton, &QPushButton::clicked, this, [this] { showMonths(m_firstMonth.addMonths(-1)); });
	connect(nextButton, &QPushButton::clicked, this, [this] { showMonths(m_firstMonth.addMonths(1)); });

	//month cards
	for (int i = 0; i < monthCount; i++)
	{
		auto card = new QFrame(contents);
		card->setObjectName("navMonthCard");
		card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); //never stretched to fill the height

		auto shadow = new QGraphicsDropShadowEffect(card);
		shadow->setBlurRadius(12);
		shadow->setOffset(0, 1);
		shadow->setColor(QColor(0, 0, 0, 22));
		card->setGraphicsEffect(shadow);

		auto cardLayout = new QVBoxLayout(card);
		cardLayout->setContentsMargins(8, 8, 8, 6);
		cardLayout->setSpacing(4);

		auto title = new QLabel(card);
		title->setObjectName("navMonthTitle");
		title->setAlignment(Qt::AlignCenter);
		cardLayout->addWidget(title);
		monthLabels.push_back(title);

		auto grid = new NavigatorMonthGrid(m_marks, card);
		grid->clicked = [this](QDate date) { emit dateClicked(date); };
		cardLayout->addWidget(grid);
		months.push_back(grid);

		block->addWidget(card);
	}

	block->addStretch(1);

	scroll->setWidget(contents);
	scroll->viewport()->setAutoFillBackground(false);
	contents->setAutoFillBackground(false);

	layout->addWidget(scroll, 1);

	showMonths(firstOfMonth(QDate::currentDate()));
}

void CalendarNavigator::showMonths(QDate firstMonth)
{
	firstMonth = firstOfMonth(firstMonth);

	if (firstMonth == m_firstMonth) return;

	m_firstMonth = firstMonth;

	auto locale = namesLocale();

	for (int i = 0; i < monthCount; i++)
	{
		QDate month = m_firstMonth.addMonths(i);

		monthLabels[i]->setText(locale.standaloneMonthName(month.month()) + " " + QString::number(month.year()));
		months[i]->setMonth(month);
	}

	QDate last = lastDay();

	auto shortName = [&](const QDate& d) { return locale.standaloneMonthName(d.month(), QLocale::ShortFormat); };

	rangeLabel->setText(
		m_firstMonth.year() == last.year() ?
		shortName(m_firstMonth) + " - " + shortName(last) + " " + QString::number(last.year())
		:
		shortName(m_firstMonth) + " " + QString::number(m_firstMonth.year()) + " - " + shortName(last) + " " + QString::number(last.year())
	);

	emit monthsChanged();
}

void CalendarNavigator::setShownWeek(QDate from, QDate to, QDate selected)
{
	m_marks.weekFrom = from;
	m_marks.weekTo = to;
	m_marks.selected = selected;

	//the months follow the shown week when it moves outside of them
	if (selected < firstDay() || selected > lastDay()) {
		showMonths(selected);
	}

	refreshMarks();
}

void CalendarNavigator::setBusyDays(const QSet<QDate>& days)
{
	m_marks.busyDays = days;

	refreshMarks();
}

QDate CalendarNavigator::firstDay() const
{
	return m_firstMonth;
}

QDate CalendarNavigator::lastDay() const
{
	QDate lastMonth = m_firstMonth.addMonths(monthCount - 1);

	return lastMonth.addDays(lastMonth.daysInMonth() - 1);
}

void CalendarNavigator::refreshMarks()
{
	for (auto grid : months) grid->update();
}

void CalendarNavigator::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.fillRect(rect(), Theme::background);
	painter.setPen(Theme::border);
	painter.drawLine(0, 0, 0, height());
}
