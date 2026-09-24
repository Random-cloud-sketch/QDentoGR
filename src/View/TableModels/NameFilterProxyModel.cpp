#include "NameFilterProxyModel.h"
#include "Model/UpperCase.h"


bool NameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	auto str = sourceModel()->index(sourceRow, filterKeyColumn()).data().toString();

	//compared in capitals without tonos, as the names are saved
	str = UpperCase::convert(str);

	for (auto& name : m_names) {
		if (!str.contains(name)) return false;
	}

	return true;
}

void NameFilterProxyModel::setName(const QString& name)
{
	m_names = name.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

	for (auto& name : m_names) {
		name = UpperCase::convert(name);
	}

	invalidateRowsFilter();
}
