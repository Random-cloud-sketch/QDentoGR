#include "MedicalHistoryDialog.h"

#include <algorithm>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDateTime>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "Database/DbMedicalHistory.h"
#include "Model/User.h"
#include "View/ModalDialogBuilder.h"
#include "View/uiComponents/DateEdit.h"

using Section = MedicalHistoryItems::Section;

static QString qs(const std::string& s) { return QString::fromStdString(s); }

MedicalHistoryDialog::MedicalHistoryDialog(long long patientRowid, const QString& patientName, QWidget* parent)
	: QDialog(parent), m_patientRowid(patientRowid)
{
	setWindowTitle(tr("Medical history") + " - " + patientName);
	setWindowFlag(Qt::WindowMaximizeButtonHint);
	resize(980, 720);

	auto mainLayout = new QVBoxLayout(this);

	m_headerLabel = new QLabel(this);
	m_headerLabel->setWordWrap(true);
	mainLayout->addWidget(m_headerLabel);

	m_alertLabel = new QLabel(this);
	m_alertLabel->setWordWrap(true);
	m_alertLabel->setStyleSheet("color: rgb(200, 30, 30); font-weight: bold;");
	mainLayout->addWidget(m_alertLabel);

	auto centralLayout = new QHBoxLayout();
	mainLayout->addLayout(centralLayout, 1);

	//positive findings: the row / field gets the property positiveFinding=true
	setStyleSheet(
		"QFrame#medicalHistoryRow[positiveFinding=\"true\"] { background-color: #FFF9F0; border-radius: 4px; }"
		"QLineEdit[positiveFinding=\"true\"], QPlainTextEdit[positiveFinding=\"true\"] { background-color: #FFF9F0; border: 1px solid #E67E22; border-radius: 3px; }"
	);

	auto navigation = new QListWidget(this);
	navigation->setFixedWidth(260);
	centralLayout->addWidget(navigation);
	m_navigation = navigation;

	auto pages = new QStackedWidget(this);
	centralLayout->addWidget(pages, 1);

	auto addPage = [&](const QString& title, QWidget* page) {
		auto item = new QListWidgetItem(QString::number(navigation->count() + 1) + ". " + title, navigation);
		item->setData(OriginalTitleRole, item->text());
		item->setData(PositiveFindingsRole, false);
		pages->addWidget(page);
	};

	QVBoxLayout* layout;

	//1. General information
	auto page = createPage(layout);
	{
		auto form = new QFormLayout();

		m_dateEdit = new DateEdit(page);
		m_dateEdit->setFixedWidth(130);
		form->addRow(tr("Date of medical history:"), m_dateEdit);

		auto healthFrame = new QFrame(page);
		healthFrame->setObjectName("medicalHistoryRow");
		auto healthRow = new QHBoxLayout(healthFrame);
		healthRow->setContentsMargins(0, 0, 0, 0);
		m_healthGroup = new QButtonGroup(this);

		const std::pair<int, QString> health[]{
			{ MedicalHistory::Good, tr("Good") },
			{ MedicalHistory::Fair, tr("Fair") },
			{ MedicalHistory::Poor, tr("Poor") }
		};

		for (auto& [id, text] : health) {
			auto radio = new QRadioButton(text, healthFrame);
			m_healthGroup->addButton(radio, id);
			healthRow->addWidget(radio);

			if (id == MedicalHistory::Poor) {
				addFinding({ .kind = Finding::Answer, .section = currentSection(), .button = radio, .highlight = { healthFrame } });
			}
		}

		healthRow->addStretch();
		form->addRow(tr("General health status:"), healthFrame);

		m_physicianEdit = new QLineEdit(page);
		form->addRow(tr("Primary physician:"), m_physicianEdit);

		m_physicianContactEdit = new QLineEdit(page);
		m_physicianContactEdit->setPlaceholderText(tr("Phone, address, e-mail"));
		form->addRow(tr("Physician contact information:"), m_physicianContactEdit);

		layout->addLayout(form);
		layout->addStretch();
	}
	addPage(tr("General information"), page);

	//2. Medical conditions
	page = createPage(layout);
	layout->addWidget(new QLabel("<b>" + tr("Does the patient have, or has had, any of the following conditions?") + "</b>", page));
	addYesNoRows(layout, Section::Condition, tr("Details (type, date of diagnosis, treatment)"));
	layout->addStretch();
	addPage(tr("Medical conditions"), page);

	//3. Medications
	page = createPage(layout);
	layout->addWidget(new QLabel("<b>" + tr("Current medications") + "</b>", page));
	m_medicationTable = createTable(layout,
		{ tr("Name"), tr("Dose"), tr("Frequency"), tr("Reason / indication"), tr("Notes") },
		tr("Add medication"), tr("Remove medication")
	);
	addFinding({ .kind = Finding::Table, .section = currentSection(), .table = m_medicationTable });
	addPage(tr("Medications"), page);

	//4. Allergies
	page = createPage(layout);
	layout->addWidget(new QLabel("<b>" + tr("Does the patient have any allergies?") + "</b>", page));
	addYesNoRows(layout, Section::Allergy, tr("Allergen (e.g. penicillin)"));
	m_allergyReactionEdit = addTextField(layout, tr("Description of reaction:"), 90);
	addFinding({ .kind = Finding::Text, .section = currentSection(), .text = m_allergyReactionEdit, .highlight = { m_allergyReactionEdit } });
	layout->addStretch();
	addPage(tr("Allergies"), page);

	//5. Previous surgeries and hospitalizations
	page = createPage(layout);
	layout->addWidget(new QLabel("<b>" + tr("Previous surgeries and hospitalizations") + "</b>", page));
	m_surgeryTable = createTable(layout,
		{ tr("Surgery / procedure"), tr("Date"), tr("Reason"), tr("Hospital"), tr("Notes") },
		tr("Add entry"), tr("Remove entry")
	);
	addFinding({ .kind = Finding::Table, .section = currentSection(), .table = m_surgeryTable });
	addPage(tr("Previous surgeries and hospitalizations"), page);

	//6. Bleeding and medical risk
	page = createPage(layout);
	layout->addWidget(new QLabel("<b>" + tr("Bleeding and medical risk") + "</b>", page));
	addYesNoRows(layout, Section::Risk, tr("Details"));
	layout->addStretch();
	addPage(tr("Bleeding and medical risk"), page);

	//7. Lifestyle and social history
	page = createPage(layout);
	{
		layout->addWidget(new QLabel("<b>" + tr("Smoking") + "</b>", page));

		auto smokingFrame = new QFrame(page);
		smokingFrame->setObjectName("medicalHistoryRow");
		auto smokingRow = new QHBoxLayout(smokingFrame);
		smokingRow->setContentsMargins(0, 0, 0, 0);
		m_smokingGroup = new QButtonGroup(this);

		const std::pair<int, QString> smoking[]{
			{ MedicalHistory::NonSmoker, tr("Non-smoker") },
			{ MedicalHistory::FormerSmoker, tr("Former smoker") },
			{ MedicalHistory::Smoker, tr("Smoker") }
		};

		for (auto& [id, text] : smoking) {
			auto radio = new QRadioButton(text, smokingFrame);
			m_smokingGroup->addButton(radio, id);
			smokingRow->addWidget(radio);

			if (id == MedicalHistory::Smoker) {
				addFinding({ .kind = Finding::Answer, .section = currentSection(), .button = radio, .highlight = { smokingFrame } });
			}
		}

		smokingRow->addStretch();
		layout->addWidget(smokingFrame);

		auto labeledRow = [&](const QString& label, QWidget* field) {
			auto row = new QWidget(page);
			auto rowLayout = new QHBoxLayout(row);
			rowLayout->setContentsMargins(20, 0, 0, 0);
			auto l = new QLabel(label, row);
			l->setMinimumWidth(200);
			rowLayout->addWidget(l);
			field->setParent(row);
			rowLayout->addWidget(field);
			rowLayout->addStretch();
			layout->addWidget(row);
			return row;
		};

		m_cigarettesSpin = new QSpinBox();
		m_cigarettesSpin->setRange(0, 200);
		m_cigarettesRow = labeledRow(tr("Cigarettes per day:"), m_cigarettesSpin);

		m_smokingYearsSpin = new QSpinBox();
		m_smokingYearsSpin->setRange(0, 100);
		m_smokingYearsRow = labeledRow(tr("Years of smoking:"), m_smokingYearsSpin);

		m_smokingQuitEdit = new QLineEdit();
		m_smokingQuitEdit->setMinimumWidth(300);
		m_smokingQuitEdit->setPlaceholderText(tr("e.g. 2018, or 5 years ago"));
		m_smokingQuitRow = labeledRow(tr("Smoking cessation:"), m_smokingQuitEdit);

		connect(m_smokingGroup, &QButtonGroup::idToggled, this, [this] { updateSmokingFields(); });
		updateSmokingFields();

		layout->addSpacing(10);
		addYesNoRows(layout, Section::Lifestyle, tr("Details (quantity, frequency)"));

		m_lifestyleOtherEdit = addTextField(layout, tr("Other relevant lifestyle information:"), 90);
		addFinding({ .kind = Finding::Text, .section = currentSection(), .text = m_lifestyleOtherEdit, .highlight = { m_lifestyleOtherEdit } });
		layout->addStretch();
	}
	addPage(tr("Lifestyle and social history"), page);

	//8. Family history
	page = createPage(layout);
	layout->addWidget(new QLabel("<b>" + tr("Is there a family history of:") + "</b>", page));
	addYesNoRows(layout, Section::Family, tr("Relative, details"));
	layout->addStretch();
	addPage(tr("Family history"), page);

	//9. Dental history
	page = createPage(layout);
	layout->addWidget(new QLabel("<b>" + tr("Dental history") + "</b>", page));
	addYesNoRows(layout, Section::Dental, tr("Details (when, what)"));
	m_oralHygieneEdit = addTextField(layout, tr("Oral hygiene habits:"), 70);
	m_oralHygieneEdit->setPlaceholderText(tr("e.g. brushing twice a day, interdental brushes, floss"));
	m_dentalOtherEdit = addTextField(layout, tr("Other relevant dental history:"), 70);
	addFinding({ .kind = Finding::Text, .section = currentSection(), .text = m_dentalOtherEdit, .highlight = { m_dentalOtherEdit } });
	layout->addStretch();
	addPage(tr("Dental history"), page);

	//10. Clinical notes
	page = createPage(layout);
	m_notesEdit = addTextField(layout, tr("Clinical notes:"), 0);
	addPage(tr("Clinical notes"), page);

	//11. Versions
	page = createPage(layout);
	{
		layout->addWidget(new QLabel("<b>" + tr("Saved versions") + "</b>", page));

		auto splitter = new QSplitter(Qt::Vertical, page);
		m_versionList = new QListWidget(splitter);
		m_versionView = new QTextBrowser(splitter);
		splitter->setStretchFactor(0, 1);
		splitter->setStretchFactor(1, 3);
		layout->addWidget(splitter, 1);

		connect(m_versionList, &QListWidget::currentRowChanged, this, [this](int row) { showVersion(row); });
	}
	addPage(tr("Change history"), page);

	connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
	navigation->setCurrentRow(0);

	//wide enough for the longest title shown with the warning sign in semi-bold
	{
		QFont flagged = navigation->font();
		flagged.setWeight(QFont::DemiBold);
		QFontMetrics metrics(flagged);

		int needed = 0;

		for (int i = 0; i < navigation->count(); i++) {
			needed = std::max(needed, metrics.horizontalAdvance(QString(QChar(0x26A0)) + " " + navigation->item(i)->text()));
		}

		navigation->setFixedWidth(std::max(navigation->width(), needed + 2 * navigation->frameWidth() + 30));
	}

	//buttons
	auto buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();

	auto saveButton = new QPushButton(tr("Save"), this);
	saveButton->setDefault(true);
	buttonLayout->addWidget(saveButton);

	auto closeButton = new QPushButton(tr("Close"), this);
	buttonLayout->addWidget(closeButton);

	mainLayout->addLayout(buttonLayout);

	connect(saveButton, &QPushButton::clicked, this, [this] { if (save()) accept(); });
	connect(closeButton, &QPushButton::clicked, this, &MedicalHistoryDialog::reject);

	//data
	m_current = DbMedicalHistory::getCurrent(m_patientRowid);

	setHistory(m_current ? m_current.value() : MedicalHistory{});

	loadVersions();
}

QWidget* MedicalHistoryDialog::createPage(QVBoxLayout*& layout)
{
	auto scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);

	auto content = new QWidget(scroll);
	layout = new QVBoxLayout(content);
	scroll->setWidget(content);

	return scroll;
}

QGridLayout* MedicalHistoryDialog::addYesNoRows(QVBoxLayout* layout, Section section, const QString& placeholder)
{
	auto parent = layout->parentWidget();
	auto grid = new QGridLayout();
	grid->setColumnStretch(3, 1);
	grid->setHorizontalSpacing(12);

	int row = 0;

	for (auto& d : MedicalHistoryItems::ofSection(section))
	{
		//background of the whole question row, highlighted when the answer is "Yes"
		//(created first, so that it stays below the widgets of the row)
		auto rowFrame = new QFrame(parent);
		rowFrame->setObjectName("medicalHistoryRow");
		grid->addWidget(rowFrame, row, 0, 1, 4);

		auto label = new QLabel(MedicalHistoryItems::label(d), parent);
		auto no = new QRadioButton(tr("No"), parent);
		auto yes = new QRadioButton(tr("Yes"), parent);
		auto details = new QLineEdit(parent);
		details->setPlaceholderText(placeholder);
		details->setMinimumWidth(250);

		//the two answers of each question are exclusive only between themselves
		auto group = new QButtonGroup(this);
		group->addButton(no);
		group->addButton(yes);

		//details are shown only when the answer is "Yes"
		details->setVisible(false);
		connect(yes, &QRadioButton::toggled, details, &QLineEdit::setVisible);

		grid->addWidget(label, row, 0);
		grid->addWidget(no, row, 1);
		grid->addWidget(yes, row, 2);
		grid->addWidget(details, row, 3);
		grid->setRowMinimumHeight(row, 28);

		m_rows.push_back(YesNoRow{ d.key, no, yes, details });

		//"No" is never a finding
		addFinding({ .kind = Finding::Answer, .section = currentSection(), .button = yes, .highlight = { rowFrame, details } });

		row++;
	}

	layout->addLayout(grid);

	return grid;
}

QTableWidget* MedicalHistoryDialog::createTable(QVBoxLayout* layout, const QStringList& headers, const QString& addText, const QString& removeText)
{
	auto parent = layout->parentWidget();

	auto table = new QTableWidget(0, headers.size(), parent);
	table->setHorizontalHeaderLabels(headers);
	table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	table->verticalHeader()->setVisible(false);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	layout->addWidget(table, 1);

	auto buttons = new QHBoxLayout();

	auto addButton = new QPushButton(QIcon(":/icons/icon_add.png"), addText, parent);
	auto removeButton = new QPushButton(QIcon(":/icons/icon_remove.png"), removeText, parent);

	buttons->addWidget(addButton);
	buttons->addWidget(removeButton);
	buttons->addStretch();
	layout->addLayout(buttons);

	connect(addButton, &QPushButton::clicked, table, [table] {
		int row = table->rowCount();
		table->insertRow(row);
		for (int c = 0; c < table->columnCount(); c++) table->setItem(row, c, new QTableWidgetItem());
		table->setCurrentCell(row, 0);
		table->editItem(table->item(row, 0));
	});

	connect(removeButton, &QPushButton::clicked, table, [table] {
		if (table->currentRow() >= 0) table->removeRow(table->currentRow());
	});

	return table;
}

QPlainTextEdit* MedicalHistoryDialog::addTextField(QVBoxLayout* layout, const QString& label, int height)
{
	auto parent = layout->parentWidget();

	layout->addSpacing(8);
	layout->addWidget(new QLabel(label, parent));

	auto edit = new QPlainTextEdit(parent);

	if (height) {
		edit->setFixedHeight(height);
		layout->addWidget(edit);
	}
	else {
		layout->addWidget(edit, 1);
	}

	return edit;
}

static void setTableRows(QTableWidget* table, const std::vector<std::vector<std::string>>& rows)
{
	table->setRowCount(0);

	for (auto& values : rows) {
		int row = table->rowCount();
		table->insertRow(row);
		for (int c = 0; c < table->columnCount(); c++) {
			table->setItem(row, c, new QTableWidgetItem(c < (int)values.size() ? qs(values[c]) : QString()));
		}
	}
}

static std::vector<std::vector<std::string>> getTableRows(const QTableWidget* table)
{
	std::vector<std::vector<std::string>> result;

	for (int row = 0; row < table->rowCount(); row++)
	{
		std::vector<std::string> values;
		bool empty = true;

		for (int c = 0; c < table->columnCount(); c++) {
			auto item = table->item(row, c);
			auto text = item ? item->text().trimmed().toStdString() : std::string();
			if (text.size()) empty = false;
			values.push_back(text);
		}

		if (!empty) result.push_back(values);
	}

	return result;
}

void MedicalHistoryDialog::setHistory(const MedicalHistory& h)
{
	//the intermediate states while the fields are filled are not evaluated
	m_loading = true;

	m_dateEdit->set_Date(h.date);

	if (auto b = m_healthGroup->button(h.generalHealth)) b->setChecked(true);

	m_physicianEdit->setText(qs(h.physician));
	m_physicianContactEdit->setText(qs(h.physicianContact));

	for (auto& row : m_rows) {
		auto a = h.answer(row.key);
		if (a.value == MedicalHistoryAnswer::Yes) row.yes->setChecked(true);
		if (a.value == MedicalHistoryAnswer::No) row.no->setChecked(true);
		row.details->setText(qs(a.details));
	}

	std::vector<std::vector<std::string>> medications;
	for (auto& m : h.medications) medications.push_back({ m.name, m.dose, m.frequency, m.indication, m.notes });
	setTableRows(m_medicationTable, medications);

	std::vector<std::vector<std::string>> surgeries;
	for (auto& s : h.surgeries) surgeries.push_back({ s.surgery, s.date, s.reason, s.hospital, s.notes });
	setTableRows(m_surgeryTable, surgeries);

	m_allergyReactionEdit->setPlainText(qs(h.allergyReaction));

	if (auto b = m_smokingGroup->button(h.smoking)) b->setChecked(true);
	m_cigarettesSpin->setValue(h.cigarettesPerDay);
	m_smokingYearsSpin->setValue(h.smokingYears);
	m_smokingQuitEdit->setText(qs(h.smokingQuit));
	m_lifestyleOtherEdit->setPlainText(qs(h.lifestyleOther));

	m_oralHygieneEdit->setPlainText(qs(h.oralHygiene));
	m_dentalOtherEdit->setPlainText(qs(h.dentalOther));

	m_notesEdit->setPlainText(qs(h.notes));

	updateSmokingFields();

	m_loading = false;

	evaluateAllSections();

	auto alerts = h.alerts();
	QStringList escaped;
	for (auto& a : alerts) escaped.append(a.toHtmlEscaped());
	m_alertLabel->setText(escaped.join("<br>"));
	m_alertLabel->setVisible(alerts.size());
}

MedicalHistory MedicalHistoryDialog::collect() const
{
	MedicalHistory h;

	h.patientRowid = m_patientRowid;
	h.date = m_dateEdit->getDate();
	h.generalHealth = std::max(0, m_healthGroup->checkedId());
	h.physician = m_physicianEdit->text().trimmed().toStdString();
	h.physicianContact = m_physicianContactEdit->text().trimmed().toStdString();

	for (auto& row : m_rows)
	{
		MedicalHistoryAnswer a;

		if (row.yes->isChecked()) {
			a.value = MedicalHistoryAnswer::Yes;
			a.details = row.details->text().trimmed().toStdString();
		}
		else if (row.no->isChecked()) {
			a.value = MedicalHistoryAnswer::No;
		}

		if (a.value != MedicalHistoryAnswer::NotAnswered) h.answers[row.key] = a;
	}

	for (auto& v : getTableRows(m_medicationTable)) h.medications.push_back(Medication{ v[0], v[1], v[2], v[3], v[4] });
	for (auto& v : getTableRows(m_surgeryTable)) h.surgeries.push_back(PastSurgery{ v[0], v[1], v[2], v[3], v[4] });

	h.allergyReaction = m_allergyReactionEdit->toPlainText().trimmed().toStdString();

	h.smoking = std::max(0, m_smokingGroup->checkedId());

	//only the fields relevant to the selected smoking status are kept
	if (h.smoking == MedicalHistory::Smoker) h.cigarettesPerDay = m_cigarettesSpin->value();
	if (h.smoking == MedicalHistory::Smoker || h.smoking == MedicalHistory::FormerSmoker) h.smokingYears = m_smokingYearsSpin->value();
	if (h.smoking == MedicalHistory::FormerSmoker) h.smokingQuit = m_smokingQuitEdit->text().trimmed().toStdString();

	h.lifestyleOther = m_lifestyleOtherEdit->toPlainText().trimmed().toStdString();
	h.oralHygiene = m_oralHygieneEdit->toPlainText().trimmed().toStdString();
	h.dentalOther = m_dentalOtherEdit->toPlainText().trimmed().toStdString();
	h.notes = m_notesEdit->toPlainText().trimmed().toStdString();

	return h;
}

bool MedicalHistoryDialog::hasUnsavedChanges() const
{
	return !MedicalHistory::differences(m_current ? m_current.value() : MedicalHistory{}, collect()).isEmpty();
}

bool MedicalHistoryDialog::save()
{
	auto h = collect();

	//nothing changed since the last saved version
	if (m_current && MedicalHistory::differences(m_current.value(), h).isEmpty()) return true;

	h.dentistRowid = User::dentist().rowID;
	h.saved = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();

	if (!DbMedicalHistory::insertVersion(h)) {
		ModalDialogBuilder::showError(tr("The medical history could not be saved").toStdString());
		return false;
	}

	m_saved = true;
	m_current = DbMedicalHistory::getCurrent(m_patientRowid);

	return true;
}

void MedicalHistoryDialog::reject()
{
	if (hasUnsavedChanges())
	{
		switch (ModalDialogBuilder::YesNoCancelDailog(tr("Do you want to save the changes to the medical history?").toStdString()))
		{
		case DialogAnswer::Yes: if (!save()) return; break;
		case DialogAnswer::Cancel: return;
		default: break;
		}
	}

	QDialog::reject();
}

void MedicalHistoryDialog::loadVersions()
{
	auto versions = DbMedicalHistory::getVersions(m_patientRowid);

	m_versionList->clear();

	for (auto& v : versions)
	{
		auto text = MedicalHistory::savedToLocalFormat(v.saved);
		auto dentist = User::getNameFromRowid(v.dentistRowid);
		if (dentist.size()) text += "  -  " + qs(dentist);

		auto item = new QListWidgetItem(text, m_versionList);
		item->setData(Qt::UserRole, v.rowid);
	}

	if (versions.empty()) {
		m_headerLabel->setText(tr("No medical history has been recorded for this patient yet."));
		m_versionView->setHtml(tr("No saved versions."));
		return;
	}

	m_headerLabel->setText(
		tr("Created: %1    Last updated: %2    Versions: %3")
		.arg(MedicalHistory::savedToLocalFormat(versions.back().saved))
		.arg(MedicalHistory::savedToLocalFormat(versions.front().saved))
		.arg(versions.size())
	);

	m_versionList->setCurrentRow(0);
}

void MedicalHistoryDialog::showVersion(int listIndex)
{
	if (listIndex < 0 || listIndex >= m_versionList->count()) return;

	auto version = DbMedicalHistory::get(m_versionList->item(listIndex)->data(Qt::UserRole).toLongLong());

	QString html = "<h3>" + tr("Version of %1").arg(MedicalHistory::savedToLocalFormat(version.saved)) + "</h3>";

	//the list is ordered from the newest to the oldest version
	if (listIndex + 1 < m_versionList->count())
	{
		auto previous = DbMedicalHistory::get(m_versionList->item(listIndex + 1)->data(Qt::UserRole).toLongLong());

		html += "<b>" + tr("Changes compared to the previous version:") + "</b><ul>";

		for (auto& change : MedicalHistory::differences(previous, version)) {
			html += "<li>" + change.toHtmlEscaped() + "</li>";
		}

		html += "</ul>";
	}
	else {
		html += "<b>" + tr("First record of the medical history.") + "</b><br>";
	}

	html += "<hr>" + version.toHtml();

	m_versionView->setHtml(html);
}

int MedicalHistoryDialog::currentSection() const
{
	//the page being built is added to the sidebar after its widgets
	return m_navigation->count();
}

void MedicalHistoryDialog::addFinding(const Finding& finding)
{
	m_findings.push_back(finding);

	//each control is connected once, when the page is built; only its own section is re-evaluated
	int section = finding.section;

	auto reevaluate = [this, section] { if (!m_loading) evaluateSection(section); };

	switch (finding.kind)
	{
	case Finding::Answer:
		connect(finding.button, &QAbstractButton::toggled, this, reevaluate);
		break;

	case Finding::Text:
		if (auto line = qobject_cast<QLineEdit*>(finding.text)) {
			connect(line, &QLineEdit::textChanged, this, reevaluate);
		}
		else if (auto plain = qobject_cast<QPlainTextEdit*>(finding.text)) {
			connect(plain, &QPlainTextEdit::textChanged, this, reevaluate);
		}
		break;

	case Finding::Table:
		//rows are added and removed by the user
		connect(finding.table, &QTableWidget::itemChanged, this, reevaluate);
		connect(finding.table->model(), &QAbstractItemModel::rowsInserted, this, reevaluate);
		connect(finding.table->model(), &QAbstractItemModel::rowsRemoved, this, reevaluate);
		break;
	}
}

static bool tableRowHasText(const QTableWidget* table, int row)
{
	for (int c = 0; c < table->columnCount(); c++) {
		auto item = table->item(row, c);
		if (item && !item->text().trimmed().isEmpty()) return true;
	}

	return false;
}

bool MedicalHistoryDialog::isPositive(const Finding& f)
{
	switch (f.kind)
	{
	case Finding::Answer:
		return f.button->isChecked();

	case Finding::Text:
		//placeholder text is not part of text() / toPlainText()
		if (auto line = qobject_cast<QLineEdit*>(f.text)) return !line->text().trimmed().isEmpty();
		if (auto plain = qobject_cast<QPlainTextEdit*>(f.text)) return !plain->toPlainText().trimmed().isEmpty();
		return false;

	case Finding::Table:
		for (int row = 0; row < f.table->rowCount(); row++) {
			if (tableRowHasText(f.table, row)) return true;
		}
		return false;
	}

	return false;
}

bool MedicalHistoryDialog::hasPositiveFindings(int section) const
{
	for (auto& f : m_findings) {
		if (f.section == section && isPositive(f)) return true;
	}

	return false;
}

void MedicalHistoryDialog::evaluateSection(int section)
{
	bool positive = false;

	for (auto& f : m_findings)
	{
		if (f.section != section) continue;

		bool findingPositive = isPositive(f);

		updateHighlights(f, findingPositive);

		positive = positive || findingPositive;
	}

	updateSidebarStatus(section, positive);
}

void MedicalHistoryDialog::evaluateAllSections()
{
	for (int section = 0; section < m_navigation->count(); section++) {
		evaluateSection(section);
	}
}

void MedicalHistoryDialog::updateSidebarStatus(int section, bool positive)
{
	auto item = m_navigation->item(section);

	if (!item) return;

	item->setData(PositiveFindingsRole, positive);

	//the displayed text is always built from the original title, so the sign is never repeated
	auto title = item->data(OriginalTitleRole).toString();

	auto font = item->font();
	font.setWeight(positive ? QFont::DemiBold : QFont::Normal);
	item->setFont(font);

	if (positive) {
		item->setText(QString(QChar(0x26A0)) + " " + title);
		item->setForeground(QColor(0xC0, 0x39, 0x2B));
		item->setBackground(QColor(0xFD, 0xF1, 0xEE));
	}
	else {
		//the default colours of the list are used again
		item->setText(title);
		item->setData(Qt::ForegroundRole, QVariant());
		item->setData(Qt::BackgroundRole, QVariant());
	}
}

void MedicalHistoryDialog::updateHighlights(const Finding& f, bool positive)
{
	for (auto w : f.highlight) setHighlighted(w, positive);

	if (f.kind != Finding::Table) return;

	//every row with an entry is highlighted (itemChanged is not emitted for the colour change)
	QSignalBlocker blocker(f.table);

	for (int row = 0; row < f.table->rowCount(); row++)
	{
		bool rowPositive = tableRowHasText(f.table, row);

		for (int c = 0; c < f.table->columnCount(); c++)
		{
			auto item = f.table->item(row, c);

			if (!item) continue;

			if (rowPositive) item->setBackground(QColor(0xFF, 0xF9, 0xF0));
			else item->setData(Qt::BackgroundRole, QVariant());
		}
	}
}

void MedicalHistoryDialog::setHighlighted(QWidget* widget, bool on)
{
	if (widget->property("positiveFinding").toBool() == on) return;

	//only this property changes: the stylesheet rules of the dialog do the rest
	widget->setProperty("positiveFinding", on);
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
	widget->update();
}

void MedicalHistoryDialog::updateSmokingFields()
{
	auto status = m_smokingGroup->checkedId();

	m_cigarettesRow->setVisible(status == MedicalHistory::Smoker);
	m_smokingYearsRow->setVisible(status == MedicalHistory::Smoker || status == MedicalHistory::FormerSmoker);
	m_smokingQuitRow->setVisible(status == MedicalHistory::FormerSmoker);
}
