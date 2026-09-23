#include "FinancialValidator.h"
#include <QObject>
#include <regex>
#include <sstream>

bool IbanValidator::validateInput(const std::string& text)
{
    static const std::string ibanError = { QT_TRANSLATE_NOOP("LineEdit", "Invalid IBAN") };

    _errorMsg = &ibanError;

    //allow empty
    if (text.empty()) return true;

    if (text.size() != 22) return false;

    if (!std::regex_match(text, std::regex("^[A-Z0-9]+"))) return false;
    
    std::string temp = text.substr(4, 18) + text.substr(0, 4);

    constexpr int asciiShift = 55;

    std::ostringstream stream;

    for(char c : temp)
    {
        if (!std::isdigit(c)) {
            stream << static_cast<int>(c - asciiShift);
        }
        else {
            stream << c;
        }
    }

    temp = stream.str();

    constexpr int charToIntOffset = 48;

    int checksum = static_cast<int>(temp[0] - 48);

    for (int i = 1; i < temp.size(); i++)
    {
        int v = static_cast<int>(temp[i] - 48);
        checksum *= 10;
        checksum += v;
        checksum %= 97;
    }

    return checksum == 1;
}

bool BICValidator::validateInput(const std::string& text)
{
    static const std::string error = { QT_TRANSLATE_NOOP("LineEdit", "Invalid BIC(SWIFT)") };

    _errorMsg = &error;

    //allow empty
    if (text.empty()) return true;

    if (text.size() != 8) return false;

    return std::regex_match(text, std::regex("^[A-Z0-9]+"));
}
