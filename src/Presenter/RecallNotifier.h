#pragma once

#include <QObject>
#include <QDate>
#include <QTimer>
#include <vector>

#include "Model/Recall.h"

//Periodontal recalls which need the attention of the clinician (see RecallListRow::needsAttention):
//the number on the recall button and the notice shown once a day at startup.
//The number is always counted again from the database, after every change which may affect it
//(no polling; a timer only for the change of the day while QDento stays open).
class RecallNotifier : public QObject
{
	Q_OBJECT

	int m_count{ 0 };
	QDate m_countedOn;
	bool m_started{ false };

	QTimer m_dayTimer;

	RecallNotifier();

	void scheduleNextDay();
	void showStartupNotice();

public:
	static RecallNotifier& get();

	//the number of patients which need attention (distinct patients)
	static int attentionCount(const std::vector<RecallListRow>& rows, const QDate& today, int leadDays);

	int count() const { return m_count; }
	int leadDays() const;
	void setLeadDays(int days);

	//after the dentist signed in: counting, the day timer and the notice of the day
	void start();

	//counts again from the database and tells the badge and the recall list
	void refresh();

signals:
	//the recalls, the appointments, the lead time or the day changed
	void changed(int count, int leadDays);
};
