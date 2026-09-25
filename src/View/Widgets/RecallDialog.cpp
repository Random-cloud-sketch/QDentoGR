#include "RecallDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Database/DbPatient.h"
#include "Database/DbRecall.h"
#include "View/Theme.h"
#include "View/uiComponents/OptionalDateEdit.h"

//preset intervals of the list (months); the last entry is "custom"
static const int presetIntervals[] = { 3, 4, 6, 9, 12 };

static QTableWidget* createTable(const QStringList& headers, QWidget* parent)
{
	auto table = new QTableWidget(0, headers.size(), parent);
	table->setHorizontalHeaderLabels(headers);
	table->verticalHeader()->setVisible(false);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setWordWrap(false);
	table->setTextElideMode(Qt::ElideRight);
	table->horizontalHeader()->setStretchLastSection(true);
	return table;
}

RecallDialog::RecallDialog(long long patient_rowid, bool activateNew, QWidget* parent)
	: QDialog(parent), m_patient(patient_rowid)
{
	auto patient = DbPatient::get(patient_rowid);

	setWindowTitle(tr("Periodontal recall") + " - " + QString::fromStdString(patient.firstLastName()));
	setWindowFlag(Qt::WindowMaximizeButtonHint);
	resize(900, 720);

	m_saved = DbRecall::get(patient_rowid);

	auto mainLayout = new QVBoxLayout(this);

	auto header = new QLabel(this);
	header->setText("<b>" + QString::fromStdString(patient.firstLastName()).toHtmlEscaped() + "</b>&nbsp;&nbsp;<span style=\"color:gray\">" +
		tr("Identifier:") + " " + QString::fromStdString(patient.id).toHtmlEscaped() + "</span>");
	mainLayout->addWidget(header);

	// ---------------------------------------------------------------- recall settings
	auto group = new QGroupBox(tr("Recall"), this);
	auto form = new QFormLayout(group);

	m_active = new QCheckBox(tr("Recall tracking active for this patient"), group);
	form->addRow(m_active);

	//next recall date: only changed by the clinician
	auto nextRow = new QHBoxLayout();

	m_nextDate = new OptionalDateEdit(group);
	nextRow->addWidget(m_nextDate);

	auto moveButton = new QPushButton(tr("Postpone / bring forward"), group);
	auto moveMenu = new QMenu(moveButton);
	moveMenu->setStyleSheet(Theme::getPopupMenuStylesheet());

	const std::pair<QString, int> moves[] = {
		{ tr("Postpone 1 week"), 7 }, { tr("Postpone 2 weeks"), 14 }, { tr("Postpone 1 month"), 30 },
		{ tr("Bring forward 1 week"), -7 }, { tr("Bring forward 2 weeks"), -14 }, { tr("Bring forward 1 month"), -30 },
	};

	for (auto& [label, days] : moves)
	{
		if (days == -7) moveMenu->addSeparator();

		moveMenu->addAction(label, this, [this, days = days] {
			auto date = m_nextDate->date();
			if (!date.isValid()) date = QDate::currentDate();
			m_nextDate->setDate(days == 30 ? date.addMonths(1) : days == -30 ? date.addMonths(-1) : date.addDays(days));
		});
	}

	moveButton->setMenu(moveMenu);
	nextRow->addWidget(moveButton);

	m_calculateButton = new QPushButton(tr("Calculate from the interval"), group);
	m_calculateButton->setToolTip(tr("Sets the next recall date from the interval - only when you choose it"));
	m_calculateButton->setMenu(new QMenu(m_calculateButton));
	m_calculateButton->menu()->setStyleSheet(Theme::getPopupMenuStylesheet());
	nextRow->addWidget(m_calculateButton);

	nextRow->addStretch();

	form->addRow(tr("Next recall:"), nextRow);

	//interval: changing it never changes the next recall date
	auto intervalRow = new QHBoxLayout();

	m_interval = new QComboBox(group);
	m_interval->addItem(tr("No interval"), 0);
	for (int months : presetIntervals) m_interval->addItem(RecallText::interval(months), months);
	m_interval->addItem(tr("Custom..."), -1);
	intervalRow->addWidget(m_interval);

	m_customInterval = new QSpinBox(group);
	m_customInterval->setRange(1, 120);
	m_customInterval->setSuffix(" " + tr("months"));
	m_customInterval->setValue(6);
	intervalRow->addWidget(m_customInterval);
	intervalRow->addStretch();

	form->addRow(tr("Recall interval:"), intervalRow);

	m_lastDate = new OptionalDateEdit(group);
	form->addRow(tr("Last recall:"), m_lastDate);

	m_notes = new QLineEdit(group);
	m_notes->setPlaceholderText(tr("Notes shown in the recall list"));
	form->addRow(tr("Notes:"), m_notes);

	m_warning = new QLabel(group);
	m_warning->setStyleSheet("color: rgb(200, 30, 30);");
	m_warning->setWordWrap(true);
	form->addRow(m_warning);

	mainLayout->addWidget(group);

	// ---------------------------------------------------------------- appointments
	auto appointmentsHeader = new QHBoxLayout();
	appointmentsHeader->addWidget(new QLabel("<b>" + tr("Recall appointments") + "</b>", this));
	appointmentsHeader->addStretch();

	auto bookButton = new QPushButton(QIcon(":/icons/icon_calendar.png"), tr("Book recall appointment"), this);
	bookButton->setToolTip(tr("Saves the recall and opens the calendar to choose the time"));
	appointmentsHeader->addWidget(bookButton);
	mainLayout->addLayout(appointmentsHeader);

	m_appointments = createTable({ tr("Date"), tr("Status") }, this);
	m_appointments->setColumnWidth(0, 160);
	m_appointments->setMaximumHeight(130);
	mainLayout->addWidget(m_appointments);

	// ---------------------------------------------------------------- history
	auto historyHeader = new QHBoxLayout();
	historyHeader->addWidget(new QLabel("<b>" + tr("Recall history") + "</b>", this));
	historyHeader->addStretch();

	auto noteButton = new QPushButton(QIcon(":/icons/icon_add.png"), tr("Add note"), this);
	historyHeader->addWidget(noteButton);
	mainLayout->addLayout(historyHeader);

	m_history = createTable({ tr("Time"), tr("Event"), tr("Details") }, this);
	m_history->setColumnWidth(0, 130);
	m_history->setColumnWidth(1, 320);
	mainLayout->addWidget(m_history, 1);

	// ---------------------------------------------------------------- buttons
	auto buttons = new QHBoxLayout();
	buttons->addStretch();

	auto okButton = new QPushButton(tr("OK"), this);
	auto cancelButton = new QPushButton(tr("Cancel"), this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	buttons->addWidget(cancelButton);
	mainLayout->addLayout(buttons);

	// ---------------------------------------------------------------- values
	Recall r = m_saved ? *m_saved : Recall{};

	m_active->setChecked(m_saved ? r.active : activateNew);
	m_nextDate->setDate(r.nextDate);
	setIntervalMonths(r.intervalMonths);
	m_lastDate->setDate(r.lastDate);
	m_notes->setText(QString::fromStdString(r.notes));

	updateCalculateMenu();

	connect(m_interval, &QComboBox::currentIndexChanged, this, [this] {
		m_customInterval->setVisible(m_interval->currentData().toInt() == -1);
		updateCalculateMenu();
	});

	connect(m_customInterval, &QSpinBox::valueChanged, this, [this] { updateCalculateMenu(); });
	connect(m_lastDate, &OptionalDateEdit::dateChanged, this, [this] { updateCalculateMenu(); });

	connect(m_nextDate, &OptionalDateEdit::dateChanged, this, [this](const QDate& date) {
		m_warning->setText(date.isValid() && date < QDate::currentDate() ? tr("The next recall date is in the past.") : QString());
	});
	emit m_nextDate->dateChanged(m_nextDate->date());

	connect(okButton, &QPushButton::clicked, this, [this] { if (saveChanges()) accept(); });
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

	connect(bookButton, &QPushButton::clicked, this, [this] {
		if (!saveChanges()) return;
		m_bookRequested = true;
		accept();
	});

	connect(noteButton, &QPushButton::clicked, this, [this] {

		bool ok = false;
		auto text = QInputDialog::getMultiLineText(this, tr("Add note"), tr("Note for the recall history:"), {}, &ok).trimmed();

		if (!ok || text.isEmpty()) return;

		if (!DbRecall::addHistory(m_patient, "note", {}, {}, 0, text.toStdString())) {
			QMessageBox::warning(this, windowTitle(), tr("The note could not be saved."));
			return;
		}

		m_changed = true;
		loadHistory();
	});

	loadAppointments();
	loadHistory();
}

int RecallDialog::intervalMonths() const
{
	int value = m_interval->currentData().toInt();

	return value == -1 ? m_customInterval->value() : value;
}

void RecallDialog::setIntervalMonths(int months)
{
	int index = m_interval->findData(months);

	if (months > 0 && index == -1) {
		index = m_interval->findData(-1);
		m_customInterval->setValue(months);
	}

	m_interval->setCurrentIndex(index == -1 ? 0 : index);
	m_customInterval->setVisible(m_interval->currentData().toInt() == -1);
}

void RecallDialog::updateCalculateMenu()
{
	auto menu = m_calculateButton->menu();
	menu->clear();

	Recall r;
	r.intervalMonths = intervalMonths();

	m_calculateButton->setEnabled(r.intervalMonths > 0);

	if (r.intervalMonths <= 0) return;

	auto addBase = [&](const QString& label, const QDate& base) {

		auto date = r.calculatedFrom(base);

		menu->addAction(label + " → " + RecallText::date(date), this, [this, date] { m_nextDate->setDate(date); });
	};

	addBase(tr("From today (%1)").arg(RecallText::date(QDate::currentDate())), QDate::currentDate());

	if (m_lastDate->date().isValid()) {
		addBase(tr("From the last recall (%1)").arg(RecallText::date(m_lastDate->date())), m_lastDate->date());
	}
}

Recall RecallDialog::current() const
{
	Recall r;
	r.patient_rowid = m_patient;
	r.active = m_active->isChecked();
	r.nextDate = m_nextDate->date();
	r.intervalMonths = intervalMonths();
	r.lastDate = m_lastDate->date();
	r.notes = m_notes->text().trimmed().toStdString();

	return r;
}

bool RecallDialog::saveChanges()
{
	auto r = current();

	//nothing changed: nothing is written (not even a new recall without any data)
	if (m_saved)
	{
		auto& s = *m_saved;

		if (s.active == r.active && s.nextDate == r.nextDate && s.intervalMonths == r.intervalMonths &&
			s.lastDate == r.lastDate && s.notes == r.notes) {
			return true;
		}
	}
	else if (!r.active && !r.nextDate.isValid() && !r.intervalMonths && !r.lastDate.isValid() && r.notes.empty()) {
		return true;
	}

	if (!DbRecall::save(r, m_saved)) {
		QMessageBox::warning(this, windowTitle(), tr("The recall could not be saved."));
		return false;
	}

	m_saved = r;
	m_changed = true;

	return true;
}

void RecallDialog::loadAppointments()
{
	auto appointments = DbRecall::appointments(m_patient);

	m_appointments->setRowCount(0);

	for (auto& a : appointments)
	{
		int row = m_appointments->rowCount();
		m_appointments->insertRow(row);

		QString status = RecallText::appointmentStatus(a.recallStatus);

		if (a.recallStatus.empty() && a.start < QDateTime::currentDateTime()) {
			status = tr("Past - not marked completed or missed");
		}

		m_appointments->setItem(row, 0, new QTableWidgetItem(RecallText::dateTime(a.start)));
		m_appointments->setItem(row, 1, new QTableWidgetItem(status));
	}

	if (appointments.empty()) {
		m_appointments->insertRow(0);
		auto item = new QTableWidgetItem(tr("No recall appointments"));
		item->setForeground(Qt::gray);
		m_appointments->setItem(0, 0, item);
		m_appointments->setSpan(0, 0, 1, 2);
	}
}

void RecallDialog::loadHistory()
{
	auto history = DbRecall::history(m_patient);

	m_history->setRowCount(0);

	for (auto& e : history)
	{
		int row = m_history->rowCount();
		m_history->insertRow(row);

		m_history->setItem(row, 0, new QTableWidgetItem(RecallText::dateTime(e.time)));
		m_history->setItem(row, 1, new QTableWidgetItem(RecallText::eventName(e.event)));
		auto details = new QTableWidgetItem(RecallText::details(e));
		details->setToolTip(details->text());
		m_history->setItem(row, 2, details);
	}

	m_history->scrollToBottom();
}
