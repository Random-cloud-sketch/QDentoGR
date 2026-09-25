#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPainter>

#include "Presenter/PatientDialogPresenter.h"
#include "ui_PatientFormDialog.h"
#include "Model/Validators/DateValidator.h"
#include "Model/Validators/CommonValidators.h"

#include "Model/Patient.h"


struct PatientFormDialog : public QDialog
{

    QRegularExpressionValidator* phoneValidator;
    QRegularExpressionValidator* numValidator;
    QRegularExpressionValidator* nameValidator;

    NotEmptyValidator notEmpty_validator;
    DateValidator birthDate_validator;
    Ui::PatientFormDialog ui;

    void paintEvent(QPaintEvent* event) override;

    PatientDialogPresenter& presenter;

    //the identifier is not an input field: it is assigned automatically and never changes
    enum PatientField { fname, lname, phone, address, birthdate, size };

    std::array<AbstractUIElement*, PatientField::size> patientFields;

    //required fields of a new patient (configured in the settings)
    struct RequiredInput
    {
        const char* key;
        QLabel* label;
        QWidget* widget;
    };

    std::vector<RequiredInput> requiredFields;
    bool newPatient{ false };

    bool isRequired(const char* key) const;
    bool isEmpty(const RequiredInput& f) const;
    bool birthDateEntered() const;
    void updateRequiredIndicators();
    bool validateRequiredFields();
    void markFieldInvalid(QWidget* widget);
    void clearFieldError(QWidget* widget);
    void clearRequiredFieldErrors();

public:
    Q_OBJECT

public:

    PatientFormDialog(PatientDialogPresenter& p, QWidget* parent = 0);
    ~PatientFormDialog();

    //new patient: the required fields come from the settings (an existing patient keeps the usual checks)
    void setNewPatientMode(bool newPatient);
    void setTitle(const std::string& title);
    void resetFields();
    void setPatientId(const std::string& id);
    void setPatient(const Patient& patient);
    Patient getPatient();
    bool inputFieldsAreValid();
};
