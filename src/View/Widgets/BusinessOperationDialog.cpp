#include "BusinessOperationDialog.h"
#include <QPainter>
#include "Database/DbProcedure.h"

QStringList s_completionList;

BuisnessOperationDialog::BuisnessOperationDialog(const std::optional<BusinessOperation>& op)
	: QDialog(nullptr), m_procedures(DbProcedure::getProcedureList())
{
	ui.setupUi(this);

	setWindowTitle(tr("Add Unit"));

	ui.nameEdit->setFocus();

	if (op.has_value()) {

		setWindowTitle(tr("Edit Unit"));

		ui.nameEdit->setText(op->activity_name.c_str());
		ui.codeEdit->setText(op->activity_code.c_str());
		ui.nameEdit->setCursorPosition(0);
		ui.priceSpinBox->setValue(op->unit_price);
		ui.quantitySpinBox->setValue(op->quantity);
		ui.priceSpinBox->setFocus();
	}

	ui.nameEdit->setInputValidator(&notEmptyValidator);

	if (s_completionList.isEmpty()) {

		
		s_completionList.reserve(m_procedures.size());

		for (auto& p : m_procedures)
		{
			QString descr = QString::fromStdString(p.name);

			s_completionList.push_back(descr);
		}
	}

	ui.nameEdit->setCompletions(s_completionList);

	ui.nameEdit->completer()->setFilterMode(Qt::MatchContains);

	connect(ui.nameEdit, &QLineEdit::textChanged, this, [&](const QString& text) {

		for (auto& p : m_procedures) {
			if (p.name == text.toStdString()) {
				ui.codeEdit->setText(p.code.c_str());
				ui.priceSpinBox->setValue(p.price);
			}
		}
	});

    connect(ui.cancelButton, &QPushButton::clicked, this, [&] { reject(); });
    connect(ui.okButton, &QPushButton::clicked, this, [&]
         {

			ui.nameEdit->validateInput();

			if (!ui.nameEdit->isValid()) return;

			auto price = ui.priceSpinBox->value();
			auto quantity = ui.quantitySpinBox->value();

			m_operation.emplace(
				ui.codeEdit->text().toStdString(),
				ui.nameEdit->text().toStdString(),
				price,
				quantity
			);

			accept();
		}
	);
}

void BuisnessOperationDialog::paintEvent(QPaintEvent* )
{
    QPainter painter(this);
	painter.fillRect(QRect(0, 0, width(), height()), Qt::white);
}


BuisnessOperationDialog::~BuisnessOperationDialog()
{
}
