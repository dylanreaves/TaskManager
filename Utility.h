// Header files for various shared helper functions throughout the program
#ifndef UTILITY_H
#define UTILITY_H

#include <QColor>
#include <QDateTime>
#include <QString>

#include "Event.h"

QString priorityToString(Priority priority);
QString priorityToStorageString(Priority priority);
Priority priorityFromString(const QString &value);
QString eventTypeToString(EventType eventType);
EventType eventTypeFromString(const QString &value);
QString eventTypeDisplayText(EventType eventType);
QString eventTypeChipText(EventType eventType);
QColor eventTypeAccentColor(EventType eventType);
QString categoryDisplayText(const QString &category);
QString categoryTypeDisplayText(const QString &category, EventType eventType);
QString priorityMetadataText(Priority priority);
QString recurrenceSummaryForEvent(const Event &event);
QString recurrenceTypeToString(RecurrenceType recurrenceType);
RecurrenceType recurrenceTypeFromString(const QString &value);
QString formatDateTime(const QDateTime &dateTime);
QString formatDateTimeRange(const QDateTime &startDateTime, const QDateTime &endDateTime);
bool isHighPriority(Priority priority);
QColor categoryColor(const QString &category);
QColor translucentCategoryColor(const QString &category, int alpha = 40);
QString locationDisplayText(const QString &location);
QColor blendColors(const QColor &base, const QColor &overlay, qreal overlayAmount);
QString colorCss(const QColor &color);
QString rgbaCss(const QColor &color);
QString fontStyleCss(const QColor &color, int fontSize, int fontWeight);

#endif // UTILITY_H



