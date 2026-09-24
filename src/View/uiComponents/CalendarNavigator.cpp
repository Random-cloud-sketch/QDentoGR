#include "CalendarNavigator.h"

#include <QCalendarWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCharFormat>
#include <QPainter>
#include <QLocale>
#include <QAbstractItemView>

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

CalendarNavigator::CalendarNavigator(QWidget* parent) : QWidget(parent)
{
	setFixedWidth(250);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 12, 10, 10);
	layout->setSpacing(4);

	//navigation between months
	auto header = new QHBoxLayout();
	header->setSpacing(4);

	auto arrow = QPixmap(":/icons/icon_downArrow.png");

	prevButton = new IconButton(this);
	prevButton->setIcon(QIcon(arrow.transformed(QTransform().rotate(90))));
	prevButton->setFixedSize(28, 28);
	prevButton->setHoverColor(Theme::mainBackgroundColor);
	prevButton->setToolTip(tr("Previous month"));

	nextButton = new IconButton(this);
	nextButton->setIcon(QIcon(arrow.transformed(QTransform().rotate(270))));
	nextButton->setFixedSize(28, 28);
	nextButton->setHoverColor(Theme::mainBackgroundColor);
	nextButton->setToolTip(tr("Next month"));

	rangeLabel = new QLabel(this);
	rangeLabel->setAlignment(Qt::AlignCenter);
	rangeLabel->setStyleSheet("font-weight: bold; color: " + Theme::colorToString(Theme::fontTurquoise) + ";");

	header->addWidget(prevButton);
	header->addWidget(rangeLabel, 1);
	header->addWidget(nextButton);

	layout->addLayout(header);

	connect(prevButton, &QPushButton::clicked, this, [this] { showMonths(m_firstMonth.addMonths(-1)); });
	connect(nextButton, &QPushButton::clicked, this, [this] { showMonths(m_firstMonth.addMonths(1)); });

	QTextCharFormat headerFormat;
	headerFormat.setBackground(Qt::white);
	headerFormat.setForeground(Qt::gray);

	QTextCharFormat dayFormat;
	dayFormat.setBackground(Qt::white);
	dayFormat.setForeground(Qt::black);

	for (int i = 0; i < monthCount; i++)
	{
		auto label = new QLabel(this);
		label->setStyleSheet("font-weight: bold; color: " + Theme::colorToString(Theme::fontTurquoise) + "; padding-top: 6px;");
		layout->addWidget(label);
		monthLabels.push_back(label);

		auto calendar = new QCalendarWidget(this);
		calendar->setLocale(namesLocale());
		calendar->setFirstDayOfWeek(Qt::Monday);
		calendar->setNavigationBarVisible(false);
		calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
		calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
		calendar->setGridVisible(false);
		calendar->setFocusPolicy(Qt::NoFocus);
		calendar->setFixedHeight(165);
		calendar->setHeaderTextFormat(headerFormat);

		for (int day = Qt::Monday; day <= Qt::Sunday; day++) {
			calendar->setWeekdayTextFormat(Qt::DayOfWeek(day), dayFormat);
		}

		//the selection is shown with the date formats, so it is the same in all months
		calendar->setStyleSheet(
			"QCalendarWidget QAbstractItemView {"
			"background-color: white; selection-background-color: transparent; selection-color: black;"
			"outline: 0; border: 1px solid " + Theme::colorToString(Theme::border) + "; border-radius: 4px; }"
		);

		//each month keeps its own selected date, which is hidden (the selection is painted with the date formats)
		auto view = calendar->findChild<QAbstractItemView*>();
		view->setSelectionMode(QAbstractItemView::NoSelection);
		view->setFocusPolicy(Qt::NoFocus);
		monthViews.push_back(view);

		connect(calendar, &QCalendarWidget::clicked, this, [this](QDate date) { emit dateClicked(date); });

		layout->addWidget(calendar);
		months.push_back(calendar);
	}

	layout->addStretch();

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

		//days of the neighbouring months are shown disabled, so each date is clickable only once
		months[i]->setDateRange(month, month.addDays(month.daysInMonth() - 1));
		months[i]->setCurrentPage(month.year(), month.month());
	}

	QDate last = lastDay();

	rangeLabel->setText(
		m_firstMonth.year() == last.year() ?
		locale.standaloneMonthName(m_firstMonth.month(), QLocale::ShortFormat) + " - " +
		locale.standaloneMonthName(last.month(), QLocale::ShortFormat) + " " + QString::number(last.year())
		:
		locale.standaloneMonthName(m_firstMonth.month(), QLocale::ShortFormat) + " " + QString::number(m_firstMonth.year()) + " - " +
		locale.standaloneMonthName(last.month(), QLocale::ShortFormat) + " " + QString::number(last.year())
	);

	refreshFormats();

	emit monthsChanged();
}

void CalendarNavigator::setShownWeek(QDate from, QDate to, QDate selected)
{
	m_weekFrom = from;
	m_weekTo = to;
	m_selected = selected;

	//the months follow the shown week when it moves outside of them
	if (m_selected < firstDay() || m_selected > lastDay()) {
		showMonths(m_selected);
	}

	refreshFormats();
}

void CalendarNavigator::setBusyDays(const QSet<QDate>& days)
{
	m_busyDays = days;

	refreshFormats();
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

void CalendarNavigator::refreshFormats()
{
	QDate today = QDate::currentDate();

	for (int i = 0; i < monthCount; i++)
	{
		auto calendar = months[i];

		calendar->setDateTextFormat(QDate(), QTextCharFormat());

		QDate month = m_firstMonth.addMonths(i);

		for (int d = 0; d < month.daysInMonth(); d++)
		{
			QDate date = month.addDays(d);

			QTextCharFormat format;
			format.setForeground(Qt::black);
			format.setBackground(Qt::white);

			if (date >= m_weekFrom && date <= m_weekTo) {
				format.setBackground(Theme::inactiveTabBG);
			}

			if (m_busyDays.contains(date)) {
				format.setFontWeight(QFont::Bold);
			}

			if (date == today) {
				format.setForeground(Theme::fontRed);
				format.setFontWeight(QFont::Bold);
			}

			if (date == m_selected) {
				format.setBackground(Theme::mainBackgroundColor);
				format.setFontWeight(QFont::Bold);
			}

			calendar->setDateTextFormat(date, format);
		}

		//the current cell of the calendar would be marked in every month
		if (monthViews[i]->selectionModel()) {
			monthViews[i]->selectionModel()->clearSelection();
			monthViews[i]->selectionModel()->clearCurrentIndex();
		}
	}
}

void CalendarNavigator::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.fillRect(rect(), Theme::background);
	painter.setPen(Theme::border);
	painter.drawLine(0, 0, 0, height());
}
