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
    connect(ui.idLineEdit, &QLineEdit::textEdited, this, [&](const QString& text) {presenter.searchDbForPatient(text.toStdString()); });

    patientFields[id] = ui.idLineEdit;
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

void PatientFormDialog::setEditMode(bool editMode)
{
    ui.idLineEdit->setReadOnly(editMode);
    ui.idLineEdit->setFocus();
    ui.idLineEdit->selectAll();
}

void PatientFormDialog::setTitle(const std::string& title)
{
    setWindowTitle(tr(title.c_str()));
}

void PatientFormDialog::resetFields()
{
    ui.idLineEdit->reset();
    ui.idLineEdit->setValidAppearence(true);
    ui.birthEdit->reset();
    ui.fNameEdit->reset();
    ui.lNameEdit->reset();
    ui.phoneEdit->reset();
    ui.addressEdit->reset();
    ui.referringDoctorEdit->reset();
    ui.sexCombo->setCurrentIndex(0);
}

void PatientFormDialog::setPatient(const Patient& patient)
{
    ui.idLineEdit->blockSignals(true);
    ui.idLineEdit->QLineEdit::setText(QString::fromStdString(patient.id));
    ui.idLineEdit->blockSignals(false);

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
        .id = ui.idLineEdit->text().toStdString(),
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



