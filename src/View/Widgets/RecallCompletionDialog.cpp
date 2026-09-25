#include "RecallCompletionDialog.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "View/uiComponents/OptionalDateEdit.h"

RecallCompletionDialog::RecallCompletionDialog(const CalendarEvent& appointment, const std::optional<Recall>& recall, QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Recall visit completed"));

	Recall r = recall ? *recall : Recall{};

	//the interval is counted from the day of the recall visit
	m_calculatedDate = r.calculatedFrom(appointment.start.date());

	auto layout = new QVBoxLayout(this);

	auto info = new QLabel(this);
	info->setWordWrap(true);
	info->setText(
		"<b>" + QString::fromStdString(appointment.summary).toHtmlEscaped() + "</b><br>" +
		tr("Recall appointment of %1").arg(RecallText::dateTime(appointment.start)) + "<br><br>" +
		tr("Current next recall date: %1").arg(r.nextDate.isValid() ? RecallText::date(r.nextDate) : tr("not set")) + "<br>" +
		tr("Saved interval: %1").arg(RecallText::interval(r.intervalMonths)) + "<br><br>" +
		tr("What should happen to the next recall date?")
	);
	layout->addWidget(info);

	auto group = new QButtonGroup(this);

	//A. entered by the clinician
	m_manual = new QRadioButton(tr("A. Enter the next recall date manually:"), this);
	group->addButton(m_manual);

	auto manualRow = new QHBoxLayout();
	manualRow->addWidget(m_manual);
	m_manualDate = new OptionalDateEdit(this);
	m_manualDate->setDate(r.nextDate.isValid() && r.nextDate > appointment.start.date() ? r.nextDate : m_calculatedDate);
	manualRow->addWidget(m_manualDate);
	manualRow->addStretch();
	layout->addLayout(manualRow);

	//B. from the saved interval
	m_calculated = new QRadioButton(this);
	group->addButton(m_calculated);

	if (m_calculatedDate.isValid()) {
		m_calculated->setText(tr("B. Calculate it from the saved interval (%1 from %2): %3")
			.arg(RecallText::interval(r.intervalMonths), RecallText::date(appointment.start.date()), RecallText::date(m_calculatedDate)));
	}
	else {
		m_calculated->setText(tr("B. Calculate it from the saved interval (no interval is saved)"));
		m_calculated->setEnabled(false);
	}

	layout->addWidget(m_calculated);

	//C. nothing changes
	m_unchanged = new QRadioButton(tr("C. Leave the next recall date unchanged"), this);
	group->addButton(m_unchanged);
	layout->addWidget(m_unchanged);

	//nothing is chosen for the clinician: the date stays unless another option is chosen
	m_unchanged->setChecked(true);

	connect(m_manualDate, &OptionalDateEdit::dateChanged, this, [this] { m_manual->setChecked(true); });

	layout->addSpacing(8);

	auto noteRow = new QFormLayout();
	m_note = new QLineEdit(this);
	m_note->setPlaceholderText(tr("Optional note for the recall history"));
	noteRow->addRow(tr("Note:"), m_note);
	layout->addLayout(noteRow);

	auto buttons = new QHBoxLayout();
	buttons->addStretch();
	auto ok = new QPushButton(tr("Mark completed"), this);
	auto cancel = new QPushButton(tr("Cancel"), this);
	ok->setDefault(true);
	buttons->addWidget(ok);
	buttons->addWidget(cancel);
	layout->addLayout(buttons);

	connect(ok, &QPushButton::clicked, this, [this] {

		if (m_manual->isChecked() && !m_manualDate->date().isValid())
		{
			auto answer = QMessageBox::question(this, windowTitle(), tr("No date is entered: the next recall date will be cleared. Continue?"));
			if (answer != QMessageBox::Yes) return;
		}

		accept();
	});

	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}

RecallCompletionDialog::Choice RecallCompletionDialog::choice() const
{
	if (m_manual->isChecked()) return Choice::Manual;
	if (m_calculated->isChecked()) return Choice::Calculated;
	return Choice::Unchanged;
}

QDate RecallCompletionDialog::nextDate() const
{
	switch (choice())
	{
		case Choice::Manual: return m_manualDate->date();
		case Choice::Calculated: return m_calculatedDate;
		default: return {};
	}
}

std::string RecallCompletionDialog::note() const
{
	return m_note->text().trimmed().toStdString();
}
