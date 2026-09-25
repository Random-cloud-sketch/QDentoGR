#include "SettingsDialog.h"

#include <QPainter>
#include <QFileDialog>
#include <QtGlobal>
#include <QInputDialog>
#include <QTabBar>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>

#include "Model/User.h"
#include "GlobalSettings.h"
#include "TableViewDialog.h"
#include "View/ModalDialogBuilder.h"
#include "Database/DbDiagnosis.h"
#include "View/Widgets/AboutDialog.h"
#include "View/uiComponents/UpperCaseValidator.h"
#include "GoogleCalendar/GoogleCalendarSettingsWidget.h"

SettingsDialog::SettingsDialog(QDialog* parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	
	setWindowTitle(tr("Settings"));
	setWindowFlag(Qt::WindowMaximizeButtonHint);
	setWindowIcon(QIcon(":/icons/icon_settings.png"));

	ui.sqlTable->setModel(&sql_table_model);
	ui.sqlTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	ui.sqlTable->hide();
	ui.sqlButton->hide();
	ui.sqlEdit->hide();

	ui.addButton->setIcon(CommonIcon::getPixmap(CommonIcon::Type::ADD));
	ui.removeButton->setIcon(CommonIcon::getPixmap(CommonIcon::Type::REMOVE));
	ui.addDiagnosisButton->setIcon(CommonIcon::getPixmap(CommonIcon::Type::ADD));
	ui.removeDiagnosisButton->setIcon(CommonIcon::getPixmap(CommonIcon::Type::REMOVE));
	ui.addDentist->setIcon(CommonIcon::getPixmap(CommonIcon::Type::ADD));
	ui.removeDentist->setIcon(CommonIcon::getPixmap(CommonIcon::Type::REMOVE));
	ui.removeTsButton->setIcon(CommonIcon::getPixmap(CommonIcon::Type::REMOVE));

	//company validators
	ui.ibanEdit->setInputValidator(&iban_validator);
	ui.bicEdit->setInputValidator(&bic_validator);

	ui.ibanEdit->setErrorLabel(ui.errorLabel);
	ui.bicEdit->setErrorLabel(ui.errorLabel);

	//IBAN and BIC are written in capital letters
	UpperCaseValidator::install(ui.ibanEdit);
	UpperCaseValidator::install(ui.bicEdit);

	//dentist validators
	ui.fNameEdit->setInputValidator(&not_empty_validator);
	ui.lNameEdit->setInputValidator(&not_empty_validator);

	ui.fNameEdit->setErrorLabel(ui.errorLabel);
	ui.lNameEdit->setErrorLabel(ui.errorLabel);
	
	connect(ui.addButton, &QPushButton::clicked, this, [&] { presenter.addProcedureElement(); });
	connect(ui.removeDentist, &QPushButton::clicked, this, [&] { presenter.removeDentist(ui.dentistList->currentRow()); });
	connect(ui.addDentist, &QPushButton::clicked, this, [&] { presenter.addDentist(); });
	connect(ui.priceListView, &ProcedureListView::codeDoubleClicked, this, [&](int row) { presenter.editProcedureElement(row); });
	connect(ui.removeButton, &QPushButton::clicked, this, [&] { presenter.removeProcedureElement(ui.priceListView->currentRow()); });
	connect(ui.cancelButton, &QPushButton::clicked, this, [&] { close(); });
	connect(ui.priceListView, &ProcedureListView::favouritesClicked, this, [&](int idx) { presenter.favClicked(idx); });
	connect(ui.okButton, &QPushButton::clicked, this, [&] {
		//since the validators are members of the view
		//we assume it is view's responsibility to check them
		if (!allFieldsAreValid()) return;
		presenter.okPressed();
	});

	connect(ui.sqlButton, &QPushButton::clicked, this, [&] {
		presenter.sqlCommandExec(ui.sqlEdit->text().toStdString());
	});

	connect(ui.sqlAgree, &QPushButton::clicked, this, [&] {
		ui.sqlButton->show();
		ui.sqlEdit->show();
		ui.sqlTable->show();
		ui.okButton->setDefault(false);
		ui.sqlButton->setDefault(true);
		ui.sqlWarning->hide();
		ui.sqlAgree->hide();
		ui.sqlEdit->setFocus();

		ui.sql->layout()->removeItem(ui.sqlSpacer1);
		ui.sql->layout()->removeItem(ui.sqlSpacer2);;

		delete ui.sqlSpacer1;
		delete ui.sqlSpacer2;

		presenter.sqlCommandExec("SELECT * FROM sqlite_master");

	});

	connect(ui.tsButton, &QPushButton::clicked, [&] {

		auto path = GlobalSettings::setTranslationPath();

		if (path != ui.tsEdit->text().toStdString()) {

			ui.tsEdit->setText(path.c_str());

			ModalDialogBuilder::showMessage(QObject::tr("Changes will take effect after restart").toStdString());
		}

	});

	connect(ui.removeTsButton, &QPushButton::clicked, [&] {
		GlobalSettings::removeTranslationPath();
		ui.tsEdit->clear();
        ModalDialogBuilder::showMessage(QObject::tr("Changes will take effect after restart").toStdString());
	});

	connect(ui.addDiagnosisButton, &QPushButton::clicked, [&] {

		auto result = ModalDialogBuilder::inputDialog("", QObject::tr("Add Diagnosis").toStdString(), {}, false, true);

		if (result.empty()) return;

		ui.diagnosisList->insertItem(ui.diagnosisList->count(), result.c_str());
	});

	connect(ui.removeDiagnosisButton, &QPushButton::clicked, [&] {
		
		int row = ui.diagnosisList->currentRow();

		if (row < 0) return;

		delete ui.diagnosisList->takeItem(row);
	});

	connect(ui.diagnosisList, &QListWidget::itemDoubleClicked, [&](QListWidgetItem* item) {
		if (!item) return;

		auto result = ModalDialogBuilder::inputDialog(
			"",
			QObject::tr("Edit Diagnosis").toStdString(),
			item->text().toStdString(),
			false, true
		);

		if (result.empty()) return;

		item->setText(result.c_str());
	});

	auto diagnoses = DbDiagnosis::getDiagnosisList();

	ui.diagnosisList->clear();

	for (const auto& d : diagnoses) 
	{
		ui.diagnosisList->addItem(QString::fromStdString(d));
	}

	ui.tsEdit->setText(GlobalSettings::getTranslationPath().c_str());

	//index 0 - Greek, index 1 - English
	ui.languageCombo->setCurrentIndex(GlobalSettings::getLanguage() == "en" ? 1 : 0);

	connect(ui.languageCombo, &QComboBox::currentIndexChanged, this, [&](int index) {
		GlobalSettings::setLanguage(index == 1 ? "en" : "el");
		ModalDialogBuilder::showMessage(QObject::tr("Changes will take effect after restart").toStdString());
	});

	User::ADA_num ? ui.adaButton->setChecked(true) : ui.fdiButton->setChecked(true);

	//required fields of the "New patient" dialog (saved immediately, like the language)
	{
		using namespace GlobalSettings;

		auto group = new QGroupBox(tr("Required fields of a new patient"), ui.generalTab);
		group->setToolTip(tr("The fields marked with * must be filled in when a new patient is created"));

		auto grid = new QGridLayout(group);

		const std::pair<const char*, QString> fields[]{
			{ RequiredField::FirstName, tr("First name") },
			{ RequiredField::LastName, tr("Last name") },
			{ RequiredField::Phone, tr("Phone") },
			{ RequiredField::Address, tr("Address") },
			{ RequiredField::ReferringDoctor, tr("Referring doctor") },
			{ RequiredField::DateOfBirth, tr("Date of birth") },
			{ RequiredField::Gender, tr("Sex") }
		};

		int i = 0;

		for (auto& [key, text] : fields)
		{
			auto check = new QCheckBox(text, group);
			check->setObjectName(QString("required_") + key);
			check->setChecked(isFieldRequired(key));

			std::string field = key;
			connect(check, &QCheckBox::toggled, this, [field](bool checked) { setFieldRequired(field, checked); });

			grid->addWidget(check, i / 4, i % 4);
			i++;
		}

		//after "Translation file", before the spacer
		ui.verticalLayout_3->insertWidget(2, group);
	}

	//Google Calendar synchronization of the appointments
	ui.tabWidget->addTab(new GoogleCalendarSettingsWidget(ui.tabWidget), "Google Calendar");

	//About QDento: the existing content, shown as the last page of the settings
	auto about = new AboutDialog(ui.tabWidget);
	about->setWindowFlags(Qt::Widget);
	ui.tabWidget->addTab(about, tr("About QDento"));

	//wide enough to show all the pages in the tab bar
	resize(std::max(width(), ui.tabWidget->tabBar()->sizeHint().width() + 40), height());

	presenter.setView(this);
}

void SettingsDialog::focusTab(SettingsTab tab)
{
	ui.tabWidget->setCurrentIndex(static_cast<int>(tab));
}

bool SettingsDialog::isADANumenclature()
{
	return ui.adaButton->isChecked();
}

void SettingsDialog::setAdminPriv(bool admin)
{
		ui.tabWidget->setTabEnabled(static_cast<int>(SettingsTab::PRACTICE), admin);
		ui.tabWidget->setTabEnabled(static_cast<int>(SettingsTab::COMPANY), admin);
		ui.tabWidget->setTabEnabled(static_cast<int>(SettingsTab::PROCEDURES), admin);
		ui.tabWidget->setTabEnabled(static_cast<int>(SettingsTab::SQL), admin);
}


QString SettingsDialog::getDentistName(const Dentist &d)
{
    if (d.rowID != User::dentist().rowID) {
        return QString::fromStdString(d.fname + " " + d.lname);
    }

	return ui.fNameEdit->text() + " " + ui.lNameEdit->text();
}

ProcedureListView* SettingsDialog::getProcedureView()
{
	return ui.priceListView;
}


void SettingsDialog::setSqlTable(const PlainTable& table)
{
	sql_table_model.setTableData(table);
	ui.sqlEdit->clear();
	ui.sqlEdit->setFocus();
}

void SettingsDialog::setCompany(const Company& company)
{
	ui.companyEdit->setText(QString::fromStdString(company.name));
	ui.identifierEdit->setText(QString::fromStdString(company.identifier));
	ui.addressEdit->setText(QString::fromStdString(company.address));
	ui.vatSpin->setValue(company.vat);
	ui.currencyEdit->setText(QString::fromStdString(company.currency));
	ui.bankEdit->setText(company.bank.c_str());
	ui.ibanEdit->setText(company.iban.c_str());
	ui.bicEdit->setText(company.bic.c_str());
}

void SettingsDialog::setDoctor(const Dentist& dentist)
{
	ui.doctorPassEdit->setEchoMode(QLineEdit::EchoMode::Password);

	ui.fNameEdit->set_Text(dentist.fname);
	ui.lNameEdit->set_Text(dentist.lname);
	ui.doctorPassEdit->set_Text(dentist.pass);

	ui.doctorPassEdit->setEchoMode(dentist.pass.empty() ? 
		QLineEdit::Normal
		:
		QLineEdit::Password
	);
}

void SettingsDialog::setDentistList(const std::vector<Dentist>& doctors)
{
	ui.dentistList->clear();

	for (auto& dentist : doctors)
	{
        ui.dentistList->addItem(getDentistName(dentist));
	}

	auto count = ui.dentistList->count();

	if (count) {
		ui.dentistList->setCurrentRow(count - 1);
		return;
	}

}

Company SettingsDialog::getCompany()
{
	Company p;

	p.identifier = ui.identifierEdit->getText();
	p.address = ui.addressEdit->getText();
	p.name = ui.companyEdit->getText();
	p.vat = ui.vatSpin->value();
	p.currency = ui.currencyEdit->getText();
	p.bank = ui.bankEdit->getText();
	p.iban = ui.ibanEdit->getText();
	p.bic = ui.bicEdit->getText();

	return p;
}

Dentist SettingsDialog::get()
{
	Dentist dentist;
	dentist.fname = ui.fNameEdit->getText();
	dentist.lname = ui.lNameEdit->getText();
	dentist.pass = ui.doctorPassEdit->getText();

	return dentist;
}


void SettingsDialog::replaceCurrentItem(const Dentist& d)
{
    ui.dentistList->currentItem()->setText(getDentistName(d));
}

bool SettingsDialog::allFieldsAreValid()
{
	constexpr int fieldsSize = 4;

	AbstractLineEdit* fields[fieldsSize] {
		ui.ibanEdit,
		ui.bicEdit,
		ui.fNameEdit,
		ui.lNameEdit
	};

	for (int i = 0; i < fieldsSize; i ++) {

		auto field = fields[i];

		if(!field->validateInput()) {

			//IBAN and BIC are in the invoice data, the names in the dentist data
			ui.tabWidget->setCurrentWidget(i < 2 ? ui.companySettings : ui.doctorSettings);

			field->set_focus();	
			return false;
		}
	}

	return true;
}


SettingsDialog::~SettingsDialog()
{
	std::set<std::string> diagnoses;

	for (int i = 0; i < ui.diagnosisList->count(); ++i) 
	{
		auto* item = ui.diagnosisList->item(i);

		diagnoses.insert(item->text().toStdString());
	}

	DbDiagnosis::setDiagnosisList(diagnoses);
}
