#include "CalendarViewData.h"
#include <QPainter>
#include <QPainterPath>
#include "View/Theme.h"
#include <QApplication>
#include <QCoreApplication>

CalendarViewData::CalendarViewData()
{}

void CalendarViewData::setEvents(const std::vector<CalendarEvent>& eventsList, const CalendarEvent& clipboardEvent)
{
	coordinatesMap.clear();
	current_entity_hover = nullptr;
	m_clipboardEventText = clipboardEvent.summary.c_str();

	if (clipboardEvent.rowid && m_clipboardEventText.isEmpty()) {
		m_clipboardEventText = "???";
	}

	for (int i = 0; i < eventsList.size(); i++)
	{
		auto& event = eventsList[i];
		
		if (event.rowid == clipboardEvent.rowid) { //wont paint the clipboard event
			continue;
		}
		
		auto entity = std::make_shared<EventEntity>();

		entity->hasPatient = event.patient_rowid;

		if (showGoogleSync)
		{
			auto& s = event.googleStatus;

			if (s == "synced") entity->syncColor = QColor(46, 160, 110);
			else if (s == "pending_create" || s == "pending_update") entity->syncColor = QColor(230, 160, 40);
			else if (s == "error") entity->syncColor = QColor(200, 50, 40);
			else if (s == "unlinked") entity->syncColor = QColor(150, 150, 150);
		}
		
		if (entity->hasPatient) {

			entity->text += "• ";
		}

		entity->text += event.summary.c_str();

		if (event.phone.size()) {
			entity->phone = QCoreApplication::translate("CalendarViewData", "TEL.") + " " + QString::fromStdString(event.phone);
		}

		entity->description = event.description.c_str();

		if (event.recall)
		{
			auto& s = event.recallStatus;

			entity->recallTag = QString::fromUtf8(s == "completed" ? "✓ " : s == "missed" ? "✗ " : "↻ ") +
				(s == "completed" ? QCoreApplication::translate("CalendarViewData", "RECALL - COMPLETED") :
				 s == "missed" ? QCoreApplication::translate("CalendarViewData", "RECALL - MISSED") :
				 QCoreApplication::translate("CalendarViewData", "PERIODONTAL RECALL"));

			entity->recallColor = s == "completed" ? QColor(46, 140, 90) : s == "missed" ? QColor(200, 50, 40) : QColor(120, 70, 170);
		}

		if (entity->text.isEmpty() && entity->phone.isEmpty() && entity->description.isEmpty()) {
			entity->text = "???";
		}

		entity->span = (event.start.secsTo(event.end) / 60 / 15);
		
		entity->event_list_index = i;

		//for events under 15 minutes
		if (!entity->span) {
			entity->span = 1;
		}

		entity->column = event.start.date().dayOfWeek() - 1;

		int startMinute = QTime(0, 0, 0).secsTo(event.start.time()) / 60;

		entity->row = startMinute >= m_firstMinute ?
			(startMinute - m_firstMinute) / 15
			:
			-((m_firstMinute - startMinute + 14) / 15);

		//only the part inside the shown hours is painted
		if (entity->row < 0) {
			entity->span += entity->row;
			entity->row = 0;
		}

		entity->span = std::min(entity->span, m_rowCount - entity->row);

		if (entity->span <= 0) {
			continue;
		}

		for (int y = entity->row; y < entity->row + entity->span; y++) {

			int key = getEventKey(entity->column, y);

			coordinatesMap[key] = entity;
		}

	}
}

void CalendarViewData::setTimeRange(int firstMinute, int rowCount)
{
	m_firstMinute = firstMinute;
	m_rowCount = rowCount;
}

void CalendarViewData::setCellSize(int column, int cell_width, int cell_height)
{
	for (auto& [key, ptr] : coordinatesMap) {
		
		if (ptr->column == column) {
			ptr->setCellSize(cell_width, cell_height);
		}
	}
}


QPixmap CalendarViewData::requestPixmap(int column, int row) const
{
	auto entity = getEntity(column, row);

	if (!entity) {
		return QPixmap();
	}

	return entity->getPixmapPart(row);
}

int CalendarViewData::eventListIndex(int column, int row) const
{
	auto key = getEventKey(column, row);

	if (coordinatesMap.count(key)) {
		return coordinatesMap.at(key)->event_list_index;
	}

	return -1;
}

bool CalendarViewData::hasPatient(int row, int column) const
{
	auto key = getEventKey(column, row);

	if (coordinatesMap.count(key)) {
		return coordinatesMap.at(key)->hasPatient;
	}

	return false;
}

bool CalendarViewData::eventRows(int column, int row, int& firstRow, int& span) const
{
	auto entity = getEntity(column, row);

	if (!entity) return false;

	firstRow = entity->row;
	span = entity->span;

	return true;
}

std::vector<std::pair<int, int>> CalendarViewData::setHovered(int column, int row)
{
	std::vector<std::pair<int, int>> result;
	
	auto newHoveredEntity = getEntity(column, row);

	//hovered over empty cell
	if (!newHoveredEntity) {
		
		//hovering out of entity to empty cell
		if (current_entity_hover)
		{
			//we have to de-hover the already hovered entity
			result = current_entity_hover->getSpan();
			current_entity_hover->setHovered(false);
			current_entity_hover = nullptr;
		}

		return result;
	}


	//no need to update anything - returning empty result
	if (newHoveredEntity == current_entity_hover) {
		return result;
	}

	//going from one entity to another
	if (current_entity_hover) {
		//adding the span of the last
		result = current_entity_hover->getSpan();

		current_entity_hover->setHovered(false);
	}

	//hovering from one entity to another:
	current_entity_hover = newHoveredEntity;

	current_entity_hover->setHovered(true);

	for (auto& idx : newHoveredEntity->getSpan()) {
		result.push_back(idx);
	}

	return result;
}

CalendarViewData::EventEntity* CalendarViewData::getEntity(int column, int row)
{

	auto key = getEventKey(column, row);

	if (!coordinatesMap.count(key)) return nullptr;

	return coordinatesMap.at(key).get();
}

const CalendarViewData::EventEntity* const CalendarViewData::getEntity(int column, int row) const
{
	auto key = getEventKey(column, row);

	if (!coordinatesMap.count(key)) return nullptr;

	return coordinatesMap.at(key).get();
}


void CalendarViewData::EventEntity::setCellSize(int width, int height)
{
	cell_width = width;
	cell_height = height;
	
	paintPixmap();
}

void CalendarViewData::EventEntity::paintPixmap()
{
	int eventHeight = cell_height * span;

	px = QPixmap(cell_width*pixelRatio, eventHeight*pixelRatio);
	px.setDevicePixelRatio(pixelRatio);
	px.fill(Qt::transparent);

	QPainter p(&px);

	p.setRenderHint(QPainter::Antialiasing);
	
	auto cellRect = QRect(0, 0, cell_width, eventHeight);

	p.setPen(QColor(245, 245, 245));
	p.drawRect(cellRect);
	
	if (row % 4 == 0) {
		p.setPen(QColor(224, 224, 224));
		p.drawLine(cellRect.topLeft(), cellRect.topRight());
	}

	QPainterPath path;
	path.addRoundedRect(QRectF(2,2,cell_width-4, eventHeight-4), 7, 7);

	
	p.fillPath(path, m_hovered ? Theme::inactiveTabBGHover : Theme::inactiveTabBG);
	p.setPen(Theme::mainBackgroundColor);
	p.drawPath(path);
	QFont font = p.font();
	font.setBold(true);

	//short appointments in the compact (30 / 60 minute) grid have little room for the text
	int textTop = eventHeight < 24 ? 1 : 4;

	//room for the synchronization dot
	int syncRoom = syncColor.isValid() ? 8 : 0;

	QRect textRect(5, textTop, cell_width - 10 - syncRoom, eventHeight-textTop-2);

	if (syncColor.isValid()) {
		p.setPen(Qt::NoPen);
		p.setBrush(syncColor);
		p.drawEllipse(QPointF(cell_width - 9, textTop + 5), 3, 3);
		p.setBrush(Qt::NoBrush);
	}

	p.setRenderHint(QPainter::RenderHint::TextAntialiasing);

	//the lines follow each other, whatever does not fit in the appointment is cut
	p.setClipRect(textRect);

	int y = textRect.top();

	auto drawPart = [&](const QString& part, const QFont& partFont, const QColor& color) {

		if (part.isEmpty() || y >= textRect.bottom()) return;

		p.setFont(partFont);
		p.setPen(color);

		QRect r(textRect.left(), y, textRect.width(), textRect.bottom() - y);
		QRect used;

		p.drawText(r, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, part, &used);

		y += used.height();
	};

	drawPart(text, font, Theme::fontTurquoise);

	if (recallTag.size()) {
		QFont small = font;
		if (font.pointSizeF() > 0) small.setPointSizeF(font.pointSizeF() * 0.85);
		else if (font.pixelSize() > 0) small.setPixelSize(qMax(8, int(font.pixelSize() * 0.85)));
		drawPart(recallTag, small, recallColor);
	}
	drawPart(phone, font, Theme::fontTurquoise); //same as the name, as in the drag preview
	drawPart(description, font, Theme::fontTurquoise);
}

QPixmap CalendarViewData::EventEntity::getPixmapPart(int row) const
{
	int section = row - this->row;

	int pointY = cell_height * section;

	QRectF sourceRect(0, pixelRatio* pointY, pixelRatio * cell_width, pixelRatio * cell_height);

	QRectF target(0, 0, pixelRatio * cell_width, pixelRatio * cell_height);

	QPixmap result(pixelRatio * cell_width, pixelRatio * cell_height);

	result.fill(Qt::transparent);
	
	QPainter p(&result);

	p.drawPixmap(target, this->px, sourceRect);

	return result;
}

void CalendarViewData::EventEntity::setHovered(bool hovered)
{
	m_hovered = hovered;
	paintPixmap();
}

std::vector<std::pair<int, int>> CalendarViewData::EventEntity::getSpan() const
{
	std::vector<std::pair<int, int>> result;

	for (int i = row; i < row + span; i++) {
		result.push_back(std::make_pair(column, i));
	}

	return result;
}
