#include "PatientFormDialog.h"
#include "Model/User.h"
#include "Model/UpperCase.h"
#include "View/uiComponents/UpperCaseValidator.h"
#include "GlobalSettings.h"
#include <QMessageBox>
#include <QStyle>

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

    //required fields of a new patient: red border while a required field is missing
    setStyleSheet(
        "QLineEdit[requiredInvalid=\"true\"], QDateEdit[requiredInvalid=\"true\"], QComboBox[requiredInvalid=\"true\"]"
        "{ border: 1px solid #E74C3C; }"
    );

    using namespace GlobalSettings;

    requiredFields = {
        { RequiredField::FirstName, ui.label, ui.fNameEdit },
        { RequiredField::LastName, ui.label_3, ui.lNameEdit },
        { RequiredField::Phone, ui.label_5, ui.phoneEdit },
        { RequiredField::Address, ui.addressLabel, ui.addressEdit },
        { RequiredField::ReferringDoctor, ui.referringDoctorLabel, ui.referringDoctorEdit },
        { RequiredField::DateOfBirth, ui.label_7, ui.birthEdit },
        { RequiredField::Gender, ui.label_8, ui.sexCombo }
    };

    //the label texts without "*"
    for (auto& f : requiredFields) f.label->setProperty("baseText", f.label->text());

    //a corrected field is no longer marked (no message while typing)
    for (auto edit : { static_cast<QLineEdit*>(ui.fNameEdit), static_cast<QLineEdit*>(ui.lNameEdit),
                       static_cast<QLineEdit*>(ui.phoneEdit), static_cast<QLineEdit*>(ui.addressEdit),
                       static_cast<QLineEdit*>(ui.referringDoctorEdit) })
    {
        connect(edit, &QLineEdit::textChanged, this, [this, edit](const QString& text) {
            if (!text.trimmed().isEmpty()) clearFieldError(edit);
        });
    }

    connect(ui.birthEdit, &QDateEdit::dateChanged, this, [this] {
        if (birthDateEntered()) clearFieldError(ui.birthEdit);
    });

    connect(ui.sexCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) clearFieldError(ui.sexCombo);
    });

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

void PatientFormDialog::setNewPatientMode(bool isNew)
{
    newPatient = isNew;

    if (!newPatient) return;

    //the fixed checks of the names and of the date of birth are replaced by the configured ones
    ui.fNameEdit->setInputValidator(nullptr);
    ui.lNameEdit->setInputValidator(nullptr);
    ui.birthEdit->setInputValidator(nullptr);

    //a required sex has to be chosen (otherwise "Male" stays the default)
    if (isRequired(GlobalSettings::RequiredField::Gender)) ui.sexCombo->setCurrentIndex(-1);

    updateRequiredIndicators();
}

bool PatientFormDialog::isRequired(const char* key) const
{
    return GlobalSettings::isFieldRequired(key);
}

bool PatientFormDialog::birthDateEntered() const
{
    //01.01.1900 (the minimum of the field) is the "no date of birth" value of the program
    return ui.birthEdit->date() >= QDate(1901, 1, 1);
}

bool PatientFormDialog::isEmpty(const RequiredInput& f) const
{
    if (f.widget == ui.birthEdit) return !birthDateEntered();

    if (f.widget == ui.sexCombo) return ui.sexCombo->currentIndex() < 0;

    return static_cast<QLineEdit*>(f.widget)->text().trimmed().isEmpty();
}

void PatientFormDialog::updateRequiredIndicators()
{
    for (auto& f : requiredFields)
    {
        //always built from the original text, so the "*" is never repeated
        QString base = f.label->property("baseText").toString();

        if (!isRequired(f.key)) {
            f.label->setText(base);
            continue;
        }

        f.label->setText(base.endsWith(':') ? base.chopped(1) + "*:" : base + "*");
    }
}

bool PatientFormDialog::validateRequiredFields()
{
    clearRequiredFieldErrors();

    QStringList missing;
    QWidget* first = nullptr;

    for (auto& f : requiredFields)
    {
        if (!isRequired(f.key) || !isEmpty(f)) continue;

        missing << f.label->property("baseText").toString().remove(':').trimmed();

        markFieldInvalid(f.widget);

        if (!first) first = f.widget;
    }

    if (missing.size())
    {
        QMessageBox::warning(this, tr("Required fields"),
            tr("Please fill in the following required fields:") + "\n\n" + missing.join("\n"));

        first->setFocus();

        return false;
    }

    //an optional date of birth, when entered, must still be valid
    if (birthDateEntered() && ui.birthEdit->date() >= QDate::currentDate())
    {
        markFieldInvalid(ui.birthEdit);
        QMessageBox::warning(this, tr("Required fields"), tr("Invalid birthdate"));
        ui.birthEdit->setFocus();
        return false;
    }

    return true;
}

void PatientFormDialog::markFieldInvalid(QWidget* widget)
{
    widget->setProperty("requiredInvalid", true);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void PatientFormDialog::clearFieldError(QWidget* widget)
{
    if (!widget->property("requiredInvalid").toBool()) return;

    widget->setProperty("requiredInvalid", false);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void PatientFormDialog::clearRequiredFieldErrors()
{
    for (auto& f : requiredFields) clearFieldError(f.widget);
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
    //a new patient whose sex is required has to have it chosen
    if (!newPatient || !isRequired(GlobalSettings::RequiredField::Gender)) {
        ui.sexCombo->setCurrentIndex(patient.sex);
    }

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
    if (newPatient) return validateRequiredFields();

    for (auto& f : patientFields) {
        f->validateInput();

        if (!f->isValid()) {
            f->set_focus();
            return false;
        }
    }

    return true;
}



