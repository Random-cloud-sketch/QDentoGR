#include "ProcedureInput.h"

#include <QCompleter>
#include <QAbstractItemView>
#include "View/Widgets/TableViewDialog.h"
#include "Presenter/ProcedureCreator.h"
#include "Resources.h"
#include "View/ModalDialogBuilder.h"
#include "Model/User.h"
#include "Model/Dental/ToothUtils.h"
#include "Database/DbDiagnosis.h"

ProcedureInput::ProcedureInput(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	setWindowFlag(Qt::WindowMaximizeButtonHint);

	connect(ui.rangeCheck, &QCheckBox::toggled, this, [&](bool checked) {
		ui.rangeGroup->setHidden(!checked);
		ui.hyperdonticCheckBox->setHidden(checked);
	});

	ui.priceSpin->setSuffix(QString(" ") + User::company().currency.c_str());

	for (int i = 0; i < 32; i++)
	{
		ui.beginCombo->addItem(ToothUtils::getNomenclature(i, false).c_str());
		ui.endCombo->addItem(ToothUtils::getNomenclature(i, false).c_str());
	}

	QStringList completionList;

	for (auto& d : DbDiagnosis::getDiagnosisList(true)) 
	{
		completionList.append(d.c_str());
	}

	ui.diagnosisEdit->setCompletions(completionList);
	ui.diagnosisEdit->completer()->setFilterMode(Qt::MatchContains);
}

void ProcedureInput::setParamMinHeight(int height)
{
	ui.paramFrame->setMinimumHeight(height);
}

void ProcedureInput::setCommonData(const CommonData& data)
{
	ui.diagnosisGroup->show();
	ui.notesGroup->show();
	ui.priceGroup->show();
	ui.diagnosisEdit->setText(data.diagnosis.c_str());
	ui.diagnosisEdit->setCursorPosition(0);
	ui.notesEdit->setPlainText(data.notes.c_str());
	ui.priceSpin->setValue(data.price);
	ui.errorLabel->clear();
}

void ProcedureInput::setParameterData()
{
	ui.paramFrame->show();

	for (auto o : ui.paramFrame->children()) {
		if (o->isWidgetType()) {
			static_cast<QWidget*>(o)->hide();
		}
	}
}

void ProcedureInput::setParameterData(bool supernumeral)
{
	setParameterData();

	ui.rangeCheck->hide();
	ui.toothFrame->show();
	ui.hyperdonticCheckBox->setChecked(supernumeral);
}

void ProcedureInput::setParameterData(bool supernumeral, RestorationData restoration)
{
	setParameterData(supernumeral);

	ui.surfaceGroup->show();

	ui.o_check->setChecked(restoration.surfaces[0]);
	ui.m_check->setChecked(restoration.surfaces[1]);
	ui.d_check->setChecked(restoration.surfaces[2]);
	ui.b_check->setChecked(restoration.surfaces[3]);
	ui.l_check->setChecked(restoration.surfaces[4]);
	ui.c_check->setChecked(restoration.surfaces[5]);
	ui.postCheck->setChecked(restoration.post);

	ui.postCheck->setDisabled(m_postDisabled);
}

void ProcedureInput::setParameterData(ConstructionRange range, bool allowSingle)
{
	setParameterData();

	m_allow_singleRange = allowSingle;

	ui.rangeGroup->show();

	ui.beginCombo->setCurrentIndex(range.toothFrom);
	ui.endCombo->setCurrentIndex(range.toothTo);
}

void ProcedureInput::setParameterData(bool supernumeral, ConstructionRange range, bool preferSingle)
{
	m_allow_singleRange = false;

	setParameterData();

	ui.hyperdonticCheckBox->setChecked(supernumeral);
	ui.beginCombo->setCurrentIndex(range.toothFrom);
	ui.endCombo->setCurrentIndex(range.toothTo);
	ui.toothFrame->show();
	ui.rangeCheck->show();

	ui.rangeCheck->setChecked(!preferSingle);
	emit ui.rangeCheck->toggled(!preferSingle); //in case it is already checked
}

void ProcedureInput::setParameterData(bool supernumeral, ConstructionRange range, RestorationData r, int preferedIndex)
{
	m_allow_singleRange = false;

	setParameterData(supernumeral, r);
	ui.beginCombo->setCurrentIndex(range.toothFrom);
	ui.endCombo->setCurrentIndex(range.toothTo);

	setParameterData(); //hiding everything...
}

ProcedureInput::ResultData ProcedureInput::getResult()
{
	ResultData result;

	result.diagnosis = ui.diagnosisEdit->text().toStdString();

	result.notes = ui.notesEdit->getText();

	result.price = ui.priceSpin->value();

	if (ui.hyperdonticCheckBox->isVisible()) {
		result.parameters = ui.hyperdonticCheckBox->isChecked();
	}

	if (ui.surfaceGroup->isVisible()) {
		result.parameters = std::make_pair(ui.hyperdonticCheckBox->isChecked(), getRestorationData());
	}

	if (ui.rangeGroup->isVisible()) {
		result.parameters = getConstructionRange();
	}

	return result;
}

void ProcedureInput::setErrorMsg(const std::string& errorMsg)
{
	for (auto child : children()) {

		if (child->isWidgetType()) {
			static_cast<QWidget*>(child)->setHidden(true);
		}
	}

	ui.errorLabel->setHidden(false);
	ui.errorLabel->setText(tr(errorMsg.c_str()));
}

void ProcedureInput::disablePost()
{
	m_postDisabled = true;
	ui.postCheck->hide();
}

std::string ProcedureInput::isValid()
{
	if (ui.surfaceGroup->isVisible()) {

		if (!getRestorationData().isValid()){ return tr("Select at least one surface").toStdString(); }
	}

	if (ui.rangeGroup->isVisible())
	{

		auto constructionRange = getConstructionRange();

		if (!constructionRange.isFromSameJaw()) {

			return tr("Teeth must be from the same jaw").toStdString();
		}

		if (!m_allow_singleRange && constructionRange.getTeethCount() == 1)
		{
			return tr("The range of the procedure must include multiple teeth").toStdString();
		}

	}

	return std::string();
}

ConstructionRange ProcedureInput::getConstructionRange()
{
	return ConstructionRange{
				ui.beginCombo->currentIndex(),
				ui.endCombo->currentIndex()
	};
}

RestorationData ProcedureInput::getRestorationData()
{
	return RestorationData{
		{
			ui.o_check->isChecked(),
			ui.m_check->isChecked(),
			ui.d_check->isChecked(),
			ui.b_check->isChecked(),
			ui.l_check->isChecked(),
			ui.c_check->isChecked()
		},
		ui.postCheck->isChecked()
	};
}

ProcedureInput::~ProcedureInput()
{
}

