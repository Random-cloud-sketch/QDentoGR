#include "CalendarEventDialog.h"
#include "Database/DbPatient.h"
#include "Model/UpperCase.h"
#include "View/uiComponents/UpperCaseValidator.h"
#include <QCompleter>
#include <QPainter>
#include <QAbstractItemView>
#include <unordered_map>

std::unordered_map<QString, long long> s_completer;

CalendarEventDialog::CalendarEventDialog(const CalendarEvent& event, QWidget *parent) :
	m_result(event),
	QDialog(parent)
{
	ui.setupUi(this);

	setWindowTitle(event.rowid ?
		tr("Edit Appointment")
		:
		tr("New Appointment")
	);


	connect(ui.okButton, &QPushButton::clicked, this, [&] {

		QString summary = UpperCase::convert(ui.summaryEdit->text());

		m_result.summary = summary.toStdString();
		m_result.description = UpperCase::convert(ui.descriptionEdit->text()).toStdString();
		m_result.start = ui.startDateTimeEdit->dateTime();
		m_result.end = ui.endDateTimeEdit->dateTime();

		if (s_completer.count(summary)) {

			m_result.patient_rowid = s_completer.at(summary);
		}

		accept();

	});

	connect(ui.summaryEdit, &QLineEdit::textChanged, this, [&](const QString& text) {

		ui.iconLabel->setText(
			s_completer.count(text) ?
			"<font color=\"Green\">✓</font>"
			:
			""
		);

	});

	//setting autocomplete patients

	s_completer.clear();

	QStringList completerList;

	for (auto p : DbPatient::getPatientList())
	{
		//the patient name is typed in capitals, so the list is in capitals as well
		QString summary = UpperCase::convert(QString::fromStdString(p.summary));

		completerList.push_back(summary);
		s_completer[summary] = p.rowid;
	}

	auto new_completer = new QCompleter(completerList, this);
	new_completer->setCaseSensitivity(Qt::CaseInsensitive);
	new_completer->setCompletionMode(QCompleter::PopupCompletion);

	QFont f;
	//f.setPixelSize(10);
	new_completer->popup()->setFont(f);
	new_completer->setMaxVisibleItems(10);
	new_completer->setModelSorting(QCompleter::UnsortedModel);
	ui.summaryEdit->setCompleter(new_completer);

	UpperCaseValidator::install(ui.summaryEdit);
	UpperCaseValidator::install(ui.descriptionEdit);

	ui.summaryEdit->setText(event.summary.c_str());
	ui.descriptionEdit->setText(event.description.c_str());
	ui.startDateTimeEdit->setDateTime(event.start);
	ui.endDateTimeEdit->setDateTime(event.end);

	ui.summaryEdit->setFocus();

	if (event.summary.size()) {
		ui.descriptionEdit->setFocus();
	}

}

void CalendarEventDialog::paintEvent(QPaintEvent* e)
{
	QPainter p(this);
	p.fillRect(rect(), Qt::white);
}

CalendarEventDialog::~CalendarEventDialog()
{}
