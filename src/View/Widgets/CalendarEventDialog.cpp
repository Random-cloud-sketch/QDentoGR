#include "CalendarEventDialog.h"
#include "Database/DbPatient.h"
#include "Model/UpperCase.h"
#include "View/uiComponents/UpperCaseValidator.h"
#include <QCheckBox>
#include <QCompleter>
#include <QPainter>
#include <QAbstractItemView>
#include <QRegularExpressionValidator>
#include <QTimer>
#include <unordered_map>

//a patient in the autocomplete list
struct CompleterPatient
{
	long long rowid{ 0 };
	QString name;
	QString phone;
};

std::unordered_map<QString, CompleterPatient> s_completer;

namespace {

	//Greek phone number: up to 10 digits while typing, the spaces of a pasted number are removed
	class PhoneValidator : public QRegularExpressionValidator
	{
	public:
		PhoneValidator(QObject* parent) : QRegularExpressionValidator(QRegularExpression("\\d{0,10}"), parent) {}

		State validate(QString& input, int& pos) const override
		{
			pos -= input.left(pos).count(' ');
			input.remove(' ');

			return QRegularExpressionValidator::validate(input, pos);
		}
	};

	QString normalizedPhone(QString phone)
	{
		return phone.remove(QRegularExpression("\\s"));
	}
}

bool CalendarEventDialog::isLinked(const QString& summary) const
{
	return (m_linkedRowid && summary == m_linkedName) || s_completer.count(summary);
}

void CalendarEventDialog::showPhoneError(const QString& error)
{
	ui.phoneErrorLabel->setText(error);
	ui.phoneLineEdit->setStyleSheet(error.isEmpty() ? "" : "QLineEdit{ border: 1px solid red; }");
}

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
		QString phone = normalizedPhone(ui.phoneLineEdit->text());
		bool linked = isLinked(summary);

		//the phone is optional, but when it is given it has all 10 digits
		if (phone.size() && !QRegularExpression("^\\d{10}$").match(phone).hasMatch()) {
			showPhoneError(tr("The phone number must have 10 digits"));
			ui.phoneLineEdit->setFocus();
			return;
		}

		//a name typed together with the phone, as in the list of patients
		if (s_completer.count(summary)) {

			auto& patient = s_completer.at(summary);

			m_result.patient_rowid = patient.rowid;
			summary = patient.name;

			if (phone.isEmpty()) phone = patient.phone;
		}
		else if (m_linkedRowid && summary == m_linkedName) {

			m_result.patient_rowid = m_linkedRowid;
		}

		//a recall appointment needs a patient of the list
		m_result.recall = m_recallBox->isChecked() && linked && m_result.patient_rowid;

		m_result.summary = summary.toStdString();
		m_result.phone = phone.toStdString();
		m_result.description = UpperCase::convert(ui.descriptionEdit->text()).toStdString();
		m_result.start = ui.startDateTimeEdit->dateTime();
		m_result.end = ui.endDateTimeEdit->dateTime();

		accept();

	});

	m_recallBox = new QCheckBox(tr("Periodontal recall appointment"), this);
	m_recallBox->setToolTip(tr("Only for an appointment of a patient of the list"));
	ui.verticalLayout_4->insertWidget(ui.verticalLayout_4->count() - 1, m_recallBox);

	connect(ui.summaryEdit, &QLineEdit::textChanged, this, [&](const QString& text) {

		ui.iconLabel->setText(
			isLinked(text) ?
			"<font color=\"Green\">✓</font>"
			:
			""
		);

		m_recallBox->setEnabled(isLinked(UpperCase::convert(text)));

	});

	//setting autocomplete patients

	s_completer.clear();

	QStringList completerList;

	for (auto p : DbPatient::getPatientList())
	{
		//the patient name is typed in capitals, so the list is in capitals as well
		QString summary = UpperCase::convert(QString::fromStdString(p.summary));

		completerList.push_back(summary);
		s_completer[summary] = CompleterPatient{
			p.rowid,
			UpperCase::convert(QString::fromStdString(p.name)),
			QString::fromStdString(p.phone)
		};
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

	//choosing a patient fills the name and the phone separately
	connect(new_completer, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString& text) {

		if (!s_completer.count(text)) return;

		auto patient = s_completer.at(text);

		//after the completer has written its text
		QTimer::singleShot(0, this, [this, patient] {
			m_linkedRowid = patient.rowid;
			m_linkedName = patient.name;
			ui.summaryEdit->setText(patient.name);
			ui.phoneLineEdit->setText(patient.phone);
			showPhoneError("");
		});
	});

	//phone
	ui.phoneLineEdit->setValidator(new PhoneValidator(ui.phoneLineEdit));
	ui.phoneLineEdit->setPlaceholderText(tr("e.g. 6912345678"));

	connect(ui.phoneLineEdit, &QLineEdit::textEdited, this, [this] { showPhoneError(""); });

	UpperCaseValidator::install(ui.summaryEdit);
	UpperCaseValidator::install(ui.descriptionEdit);

	//older appointments have the phone at the end of the name
	QString summary = QString::fromStdString(event.summary);
	QString phone = QString::fromStdString(event.phone);

	auto oldFormat = QRegularExpression("^(.*\\S)\\s+(\\d{10})$").match(summary);

	if (phone.isEmpty() && oldFormat.hasMatch()) {
		summary = oldFormat.captured(1);
		phone = oldFormat.captured(2);
	}

	if (event.patient_rowid) {
		m_linkedRowid = event.patient_rowid;
		m_linkedName = UpperCase::convert(summary);
	}

	m_recallBox->setChecked(event.recall);

	ui.summaryEdit->setText(summary);
	ui.phoneLineEdit->setText(phone);
	m_recallBox->setEnabled(isLinked(UpperCase::convert(summary)));
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
