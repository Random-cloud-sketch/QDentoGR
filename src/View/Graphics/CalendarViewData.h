#pragma once
#include <vector>
#include <QString>
#include <map>
#include <QPixmap>
#include <memory>
#include "Model/CalendarStructs.h"

class CalendarViewData {

private:

	struct EventEntity {

		static inline qreal pixelRatio = 1;

		QString text;			//name (bold)
		QString phone;			//smaller line under the name, only when there is a phone
		QString description;	//bold, under the phone
		QString recallTag;		//periodontal recall appointment and its status (small, under the name)
		QColor recallColor;

		int column = -1;
		int row = -1;
		int span = -1;

		int event_list_index = -1;

		bool hasPatient = false;

		//Google Calendar synchronization state (invalid: not shown)
		QColor syncColor;

		QPixmap px = {};

		void setCellSize(int cell_width, int cell_height);
		QPixmap getPixmapPart(int row) const;

		//returns true if the appearence is different from the current one
		void setHovered(bool hovered);

		//returns column and rows
		std::vector<std::pair<int, int>> getSpan() const;

	private:
		int cell_width = -1;
		int cell_height = -1;
		void paintPixmap();
		bool m_hovered = false;
	};


	std::map<int, std::shared_ptr<EventEntity>> coordinatesMap;
	
	QString m_clipboardEventText;

	//each row is 15 minutes, the first row starts at m_firstMinute of the day
	int m_firstMinute = 0;
	int m_rowCount = 96;

	//ensures only one entity is hovered
	EventEntity* current_entity_hover = nullptr;
	

	static inline int getEventKey(int column, int row)
	{
		return column * 100 + row;
	}

	//returns nullptr if no event is found
	const EventEntity* const getEntity(int column, int row) const;
	EventEntity* getEntity(int column, int row);

	

public:

	CalendarViewData();

	void setEvents(const std::vector<CalendarEvent>& eventsList, const CalendarEvent& clipboardEvent);

	//a small dot on each appointment shows its Google Calendar synchronization state
	static inline bool showGoogleSync = false;

	//the shown part of the day: first minute of the first row and number of 15 minute rows
	void setTimeRange(int firstMinute, int rowCount);

	void setCellSize(int column, int cell_width, int cell_height);

	void setPixelRatio(qreal ratio) { EventEntity::pixelRatio = ratio; }
	const QString& clipboardEventText() const { return m_clipboardEventText; }

	QPixmap requestPixmap(int column, int row) const;

	//returns -1 if no entity exists
	int eventListIndex(int column, int row) const;

	bool hasPatient(int row, int column) const;

	//first row and number of rows of the appointment in the cell, false for an empty cell
	bool eventRows(int column, int row, int& firstRow, int& span) const;

	//returns the columns and rows which has to be updated
	std::vector<std::pair<int, int>> setHovered(int column, int row);

};