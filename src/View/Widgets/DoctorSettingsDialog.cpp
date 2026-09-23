#include "DoctorSettingsDialog.h"
#include "Presenter/DoctorDialogPresenter.h"
#include <QPainter>
#include "TableViewDialog.h"

void DoctorSettingsDialog::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.fillRect(rect(), QColor(Qt::white));
}

DoctorSettingsDialog::DoctorSettingsDialog(DentistDialogPresenter& presenter, QWidget *parent)
	: QDialog(parent), presenter(presenter)
{
	ui.setupUi(this);

	setWindowTitle(tr("Dentist"));
	setWindowIcon(QIcon{ ":/icons/icon_user.png" });

	ui.fNameEdit->setInputValidator(&fieldValidator);
	ui.lNameEdit->setInputValidator(&fieldValidator);

	ui.fNameEdit->setErrorLabel(ui.errorLabel);
	ui.lNameEdit->setErrorLabel(ui.errorLabel);

    connect(ui.okButton, &QPushButton::clicked, [&] {
		
		if (!ui.fNameEdit->validateInput()) {
			ui.fNameEdit->setFocus();
			return;
		}

		if (!ui.lNameEdit->validateInput()){
			ui.lNameEdit->setFocus();
			return;
		}
		presenter.okPressed();
		
	});

    presenter.setView(this);
}

void DoctorSettingsDialog::setDoctor(const Dentist& dentist)
{
	ui.passEdit->setEchoMode(QLineEdit::EchoMode::Password);

	ui.fNameEdit->set_Text(dentist.fname);
	ui.lNameEdit->set_Text(dentist.lname);
	ui.passEdit->set_Text(dentist.pass);

	if (dentist.pass.empty()) ui.passEdit->setEchoMode(QLineEdit::Normal);
}

Dentist DoctorSettingsDialog::get()
{
	return Dentist{
		.fname = ui.fNameEdit->getText(),
		.lname = ui.lNameEdit->getText(),
		.pass = ui.passEdit->getText()
	};

}

void DoctorSettingsDialog::setToReadOnly()
{
	ui.fNameEdit->setReadOnly(true);
	ui.lNameEdit->setReadOnly(true);
	ui.passEdit->setReadOnly(true);

	ui.passEdit->setEchoMode(QLineEdit::EchoMode::Password);
}


DoctorSettingsDialog::~DoctorSettingsDialog()
{
}
