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

    //IBAN of any country (ISO 13616), e.g. GR16 0110 1250 0000 0001 2300 695 (27 characters in Greece);
    //the spaces of the printed form are allowed
    std::string iban;

    for (char c : text) {
        if (c != ' ') iban += c;
    }

    //country code, check digits, account number of up to 30 characters
    if (!std::regex_match(iban, std::regex("^[A-Z]{2}[0-9]{2}[A-Z0-9]{11,30}$"))) return false;

    //the first 4 characters are moved to the end, letters become 10..35, the number modulo 97 must be 1
    std::string rearranged = iban.substr(4) + iban.substr(0, 4);

    int remainder = 0;

    for (char c : rearranged)
    {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            remainder = (remainder * 10 + (c - '0')) % 97;
        }
        else {
            int value = c - 'A' + 10;
            remainder = (remainder * 100 + value) % 97;
        }
    }

    return remainder == 1;
}

bool BICValidator::validateInput(const std::string& text)
{
    static const std::string error = { QT_TRANSLATE_NOOP("LineEdit", "Invalid BIC(SWIFT)") };

    _errorMsg = &error;

    //allow empty
    if (text.empty()) return true;

    //8 characters, or 11 with the branch code (e.g. ETHNGRAA, ETHNGRAAXXX)
    if (text.size() != 8 && text.size() != 11) return false;

    return std::regex_match(text, std::regex("^[A-Z0-9]+"));
}
