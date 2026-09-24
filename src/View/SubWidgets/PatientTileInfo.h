#pragma once

#include <QWidget>
#include <optional>

#include "ui_PatientTileInfo.h"
#include "View/uiComponents/RoundedFrame.h"
#include "Model/MedicalHistory.h"

class QPushButton;
class QLabel;

struct PatientInfoPresenter;

class PatientTileInfo : public RoundedFrame
{
	Q_OBJECT

	PatientInfoPresenter* presenter{ nullptr };
    QMenu* context_menu;

    QPushButton* medicalHistoryButton;
    //first line (next to the button) and second line of the medical history summary
    QLabel* medicalHistoryLabels[2];
    QStringList medicalHistoryItems;

    void elideMedicalHistoryText();

public:
	PatientTileInfo(QWidget *parent = nullptr);
	void setPatient(const Patient& p, int age);
	void setPresenter(PatientInfoPresenter* p) { presenter = p; }
	//summary of the patient's medical history (allergies and risks are highlighted)
	void setMedicalHistory(const std::optional<MedicalHistory>& history);

protected:
	void resizeEvent(QResizeEvent* event) override;

public:

	~PatientTileInfo();

private:
	Ui::PatientTileInfoClass ui;
};
