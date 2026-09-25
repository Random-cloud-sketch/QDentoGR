#include "OptionalDateEdit.h"

#include <QCalendarWidget>
#include <QEvent>
#include <QHBoxLayout>
#include <QToolButton>

#include "View/uiComponents/DateEdit.h"

//the minimum date of the edit means "not set"
static const QDate notSet(1900, 1, 1);

OptionalDateEdit::OptionalDateEdit(QWidget* parent) : QWidget(parent)
{
	auto layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	m_edit = new DateEdit(this);
	m_edit->setMinimumDate(notSet);
	m_edit->setSpecialValueText(tr("Not set"));
	m_edit->setDisplayFormat("dd/MM/yyyy");
	m_edit->setFixedWidth(130);
	layout->addWidget(m_edit);

	m_clear = new QToolButton(this);
	m_clear->setText(QString::fromUtf8("✕"));
	m_clear->setToolTip(tr("Clear the date"));
	m_clear->setAutoRaise(true);
	layout->addWidget(m_clear);

	layout->addStretch();

	//a calendar opened on an empty date shows the current month
	if (m_edit->calendarWidget()) m_edit->calendarWidget()->installEventFilter(this);

	connect(m_clear, &QToolButton::clicked, this, [this] { setDate({}); });

	connect(m_edit, &QDateEdit::dateChanged, this, [this] {
		m_clear->setEnabled(date().isValid());
		emit dateChanged(date());
	});

	setDate({});
}

bool OptionalDateEdit::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::Show && watched == m_edit->calendarWidget() && !date().isValid())
	{
		auto today = QDate::currentDate();
		m_edit->calendarWidget()->setCurrentPage(today.year(), today.month());
	}

	return QWidget::eventFilter(watched, event);
}

void OptionalDateEdit::setDate(const QDate& date)
{
	m_edit->setDate(date.isValid() ? date : notSet);
	m_clear->setEnabled(date.isValid());
}

QDate OptionalDateEdit::date() const
{
	return m_edit->date() == notSet ? QDate() : m_edit->date();
}
