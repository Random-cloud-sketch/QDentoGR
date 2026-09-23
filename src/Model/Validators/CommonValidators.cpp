#include "CommonValidators.h"
#include <QObject>

const std::string NotEmptyValidator::couldntBeEmpty{ QT_TRANSLATE_NOOP("LineEdit", "The field is mandatory") };

NotEmptyValidator::NotEmptyValidator()
{
    _errorMsg = &couldntBeEmpty;
}

bool NotEmptyValidator::validateInput(const std::string& text)
{
    return text.size() > 0;
}

const std::string mustBeNumber{ QT_TRANSLATE_NOOP("LineEdit", "The field should contain only digits") };

bool DigitsOnlyValidator::validateInput(const std::string& text)
{
    _errorMsg = &mustBeNumber;

    for (char c : text)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }

    return true;
}
