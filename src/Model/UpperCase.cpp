#include "UpperCase.h"

namespace {

	bool isGreek(char16_t c)
	{
		return (c >= 0x0370 && c <= 0x03FF) || (c >= 0x1F00 && c <= 0x1FFF);
	}

	bool isMark(QChar c)
	{
		return c.category() == QChar::Mark_NonSpacing;
	}

	constexpr char16_t capitalIota = 0x0399;
	constexpr char16_t capitalUpsilon = 0x03A5;
	constexpr char16_t dialytika = 0x0308;

	//the tonos of these vowels keeps a following Ι / Υ from forming a diphthong with them
	bool formsDiphthong(char16_t first, char16_t second)
	{
		switch (first)
		{
		case 0x0391: //Α
		case 0x0395: //Ε
		case 0x039F: //Ο
			return second == capitalIota || second == capitalUpsilon;
		case capitalUpsilon:
			return second == capitalIota;
		default:
			return false;
		}
	}
}

QString UpperCase::convert(const QString& text)
{
	if (text.isEmpty()) return text;

	//decomposed, so that the Greek accents are separate characters
	const QString s = text.toUpper().normalized(QString::NormalizationForm_D);

	QString result;
	result.reserve(s.size() + 2);

	QChar base;
	bool greekBase = false;
	qsizetype dialytikaAfter = -1;

	for (qsizetype i = 0; i < s.size(); i++)
	{
		QChar c = s[i];

		if (!isMark(c)) {
			result.append(c);
			base = c;
			greekBase = isGreek(c.unicode());
			if (i == dialytikaAfter) result.append(QChar(dialytika));
			continue;
		}

		//accents of other scripts are kept (e.g. É)
		if (!greekBase) {
			result.append(c);
			continue;
		}

		switch (c.unicode())
		{
		case 0x0301: //tonos / oxia
		case 0x0300: //varia
		case 0x0342: //perispomeni
		{
			qsizetype next = i + 1;
			while (next < s.size() && isMark(s[next])) next++;

			bool nextHasMarks = next + 1 < s.size() && isMark(s[next + 1]);

			if (next < s.size() && !nextHasMarks && formsDiphthong(base.unicode(), s[next].unicode())) {
				dialytikaAfter = next;
			}
			break;
		}
		case 0x0313: //psili
		case 0x0314: //dasia
			break;
		case 0x0345: //ypogegrammeni
			result.append(QChar(capitalIota));
			break;
		default:
			result.append(c);
		}
	}

	return result.normalized(QString::NormalizationForm_C);
}

std::string UpperCase::convert(const std::string& text)
{
	return convert(QString::fromStdString(text)).toStdString();
}

int UpperCase::convertedPosition(const QString& text, int position)
{
	return convert(text.left(position)).size();
}
