#include "PatientFormDialog.h"
#include "Model/User.h"
#include "Model/UpperCase.h"
#include "View/uiComponents/UpperCaseValidator.h"

PatientFormDialog::PatientFormDialog(PatientDialogPresenter& p, QWidget* parent)
    : QDialog(parent),
    presenter(p)
{
    ui.setupUi(this);

    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("New document"));

    birthDate_validator.setMaxDate(Date::currentDate().yesterday());
    birthDate_validator.setMinDate(Date(1, 1, 1901));

    birthDate_validator.setMaxErrorMsg(tr("Invalid birthdate").toStdString());
    birthDate_validator.setMinErrorMsg(tr("Invalid birthdate").toStdString());

    numValidator = new QRegularExpressionValidator(QRegularExpression("[0-9]+"), this);

    phoneValidator = new QRegularExpressionValidator(QRegularExpression("[0-9-+]+"), this);
    ui.phoneEdit->QLineEdit::setValidator(phoneValidator);


    //names, address and referring doctor are typed and saved in capital letters
    UpperCaseValidator::install(ui.fNameEdit);
    UpperCaseValidator::install(ui.lNameEdit);
    UpperCaseValidator::install(ui.addressEdit);
    UpperCaseValidator::install(ui.referringDoctorEdit);

    ui.fNameEdit->setInputValidator(&notEmpty_validator);
    ui.lNameEdit->setInputValidator(&notEmpty_validator);
    ui.birthEdit->setInputValidator(&birthDate_validator);

    connect(ui.okButton, &QPushButton::clicked, this, [&] { presenter.accept(); });

    //the identifier is shown read-only (it can be selected and copied), wide enough for a whole UUID
    ui.idLineEdit->setMinimumWidth(
        ui.idLineEdit->fontMetrics().horizontalAdvance("7f3c9b2e-6e2a-4c91-9b7a-3a1f8d52c614") + 16
    );

    QPalette idPalette = ui.idLineEdit->palette();
    idPalette.setColor(QPalette::Base, QColor(0xf0, 0xf0, 0xf0));
    idPalette.setColor(QPalette::Text, QColor(0x50, 0x50, 0x50));
    ui.idLineEdit->setPalette(idPalette);

    patientFields[fname] = ui.fNameEdit;
    patientFields[lname] = ui.lNameEdit;
    patientFields[address] = ui.addressEdit;
    patientFields[phone] = ui.phoneEdit;
    patientFields[birthdate] = ui.birthEdit;

    for (int i = 0; i < PatientField::birthdate; i++)
    {
        static_cast<LineEdit*>(patientFields[i])->setErrorLabel(ui.errorLabel);
    }

    ui.birthEdit->setErrorLabel(ui.errorLabel);

    //wide enough for the whole identifier
    resize(QDialog::size().expandedTo(sizeHint()));

    presenter.setView(this);
}

void PatientFormDialog::paintEvent(QPaintEvent*)
{
    QPainter painter;
    painter.begin(this);
    painter.fillRect(QRect(0, 0, width(), height()), Qt::white);
    painter.end();
}

PatientFormDialog::~PatientFormDialog()
{
}

void PatientFormDialog::setTitle(const std::string& title)
{
    setWindowTitle(tr(title.c_str()));
}

void PatientFormDialog::resetFields()
{
    ui.birthEdit->reset();
    ui.fNameEdit->reset();
    ui.lNameEdit->reset();
    ui.phoneEdit->reset();
    ui.addressEdit->reset();
    ui.referringDoctorEdit->reset();
    ui.sexCombo->setCurrentIndex(0);
}

void PatientFormDialog::setPatientId(const std::string& id)
{
    ui.idLineEdit->setText(QString::fromStdString(id));
    ui.idLineEdit->setCursorPosition(0);
}

void PatientFormDialog::setPatient(const Patient& patient)
{
    ui.sexCombo->setCurrentIndex(patient.sex);

    auto& date = patient.birth;
    ui.birthEdit->setDate(QDate(date.year, date.month, date.day));

    if (!patient.rowid) return;

    ui.fNameEdit->QLineEdit::setText(QString::fromStdString(patient.firstName));
    ui.lNameEdit->QLineEdit::setText(QString::fromStdString(patient.lastName));

    ui.addressEdit->QLineEdit::setText(QString::fromStdString(patient.address));
    ui.phoneEdit->QLineEdit::setText(QString::fromStdString(patient.phone));
    ui.referringDoctorEdit->QLineEdit::setText(QString::fromStdString(patient.referringDoctor));

    ui.colorPicker->setColor(QColor(patient.colorNameRgb.c_str()));
}

Patient PatientFormDialog::getPatient()
{
    auto color = ui.colorPicker->color();

    auto colorName = color.isValid() ? color.name().toStdString() : std::string();

    return Patient
    {
        .rowid = 0,
        .id = {}, //set by the presenter, never taken from the view
        .birth = ui.birthEdit->getDate(),
        .sex = Patient::Sex(ui.sexCombo->currentIndex()),
        .firstName = UpperCase::convert(ui.fNameEdit->text()).toStdString(),
        .lastName = UpperCase::convert(ui.lNameEdit->text()).toStdString(),
        .address = UpperCase::convert(ui.addressEdit->text()).toStdString(),
        .phone = ui.phoneEdit->text().toStdString(),
        .referringDoctor = UpperCase::convert(ui.referringDoctorEdit->text().trimmed()).toStdString(),
        .colorNameRgb = colorName
    };
}

bool PatientFormDialog::inputFieldsAreValid()
{
    for (auto& f : patientFields) {
        f->validateInput();

        if (!f->isValid()) {
            f->set_focus();
            return false;
        }
    }

    return true;
}



