#pragma once

#include <QLineEdit>
#include <QValidator>

#include "Model/UpperCase.h"

//Converts the text of a line edit to capital letters while the user types or pastes
//(and when the text is set), so the text that is saved is in capitals as well.
class UpperCaseValidator : public QValidator
{
public:
	using QValidator::QValidator;

	State validate(QString& input, int& pos) const override
	{
		auto converted = UpperCase::convert(input);

		if (converted != input) {
			pos = qMin(UpperCase::convertedPosition(input, pos), int(converted.size()));
			input = converted;
		}

		return Acceptable;
	}

	void fixup(QString& input) const override
	{
		input = UpperCase::convert(input);
	}

	static void install(QLineEdit* edit)
	{
		edit->setValidator(new UpperCaseValidator(edit));
	}
};
