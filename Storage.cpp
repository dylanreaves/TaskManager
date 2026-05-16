// Implements JSON file i/o for Event data and includes load/save error handling
#include "Storage.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "Utility.h"

namespace {

// Returns a string from a JSON object
QString jsonString(const QJsonObject &object, const QString &key) {
    QString returnVal = object.value(key).toString();
    return returnVal;
}

// Returns a bool from a JSON object
bool jsonBool(const QJsonObject &object, const QString &key, bool defaultValue = false) {
    bool returnVal = object.value(key).toBool(defaultValue);
    return returnVal;
}

// Returns just a date from a JSON object
QDate jsonDate(const QJsonObject &object, const QString &key) {
    const QString value = jsonString(object, key);
    QDate returnVal;
    if (!value.isEmpty()) {
        returnVal = QDate::fromString(value, Qt::ISODate);
    }
    return returnVal;
}

// Reads an ISO date/time string from a JSON object
QDateTime jsonDateTime(const QJsonObject &object, const QString &key) {
    const QString value = jsonString(object, key);
    QDateTime returnVal;
    if (!value.isEmpty()) {
        returnVal = QDateTime::fromString(value, Qt::ISODate);
    }
    return returnVal;
}

QJsonArray repeatDaysToJson(const QList<Qt::DayOfWeek> &repeatDays) {
    QJsonArray returnVal;
    for (Qt::DayOfWeek day : repeatDays) {
        returnVal.append(static_cast<int>(day));
    }
    return returnVal;
}

QList<Qt::DayOfWeek> repeatDaysFromJson(const QJsonValue &value) {
    QList<Qt::DayOfWeek> returnVal;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            const bool entryIsNumber = entry.isDouble();
            if (entryIsNumber) {
                const int numericDay = entry.toInt();
                const bool validDay = numericDay >= static_cast<int>(Qt::Monday)
                                      && numericDay <= static_cast<int>(Qt::Sunday);
                if (validDay) {
                    returnVal.append(static_cast<Qt::DayOfWeek>(numericDay));
                }
            }
        }
    }
    return returnVal;
}

// Converts an Event into the JSON object stored in file.
QJsonObject eventToJson(const Event &event) {
    const QString startDateTimeValue = event.getStartDateTime().isValid()
        ? event.getStartDateTime().toString(Qt::ISODate)
        : QString();    // Empty string if no start date
    const QString endDateTimeValue = event.getEndDateTime().isValid()
        ? event.getEndDateTime().toString(Qt::ISODate)
        : QString();    // Empty string if no end date
    const QString recurrenceUntilValue = event.getRecurrenceUntil().isValid()
        ? event.getRecurrenceUntil().toString(Qt::ISODate)
        : QString();    // Empty string if no recurrence until
    const QString completedAtValue = event.getCompletedAt().isValid()
        ? event.getCompletedAt().toString(Qt::ISODate)
        : QString();    // Empty string if completed at

    QJsonObject returnVal;
    returnVal.insert(QStringLiteral("id"), event.getId());
    returnVal.insert(QStringLiteral("name"), event.getName());
    returnVal.insert(QStringLiteral("description"), event.getDescription());
    returnVal.insert(QStringLiteral("location"), event.getLocation());
    returnVal.insert(QStringLiteral("startDateTime"), startDateTimeValue);
    returnVal.insert(QStringLiteral("endDateTime"), endDateTimeValue);
    returnVal.insert(QStringLiteral("priority"), priorityToStorageString(event.getPriority()));
    returnVal.insert(QStringLiteral("eventType"), eventTypeToString(event.getEventType()));
    returnVal.insert(QStringLiteral("allDay"), event.isAllDay());
    returnVal.insert(QStringLiteral("recurrenceType"), recurrenceTypeToString(event.getRecurrenceType()));
    returnVal.insert(QStringLiteral("recurrenceInterval"), event.getRecurrenceInterval());
    returnVal.insert(QStringLiteral("recurrenceUntil"), recurrenceUntilValue);
    returnVal.insert(QStringLiteral("repeatDays"), repeatDaysToJson(event.getRepeatDays()));
    returnVal.insert(QStringLiteral("category"), event.getCategory());
    returnVal.insert(QStringLiteral("notified"), event.isNotified());
    returnVal.insert(QStringLiteral("completed"), event.isCompleted());
    returnVal.insert(QStringLiteral("completedAt"), completedAtValue);
    returnVal.insert(QStringLiteral("autoCompleteSuppressed"), event.isAutoCompleteSuppressed());
    return returnVal;
}

// Builds an Event from a saved JSON object when the required fields are valid.
bool parseEventObject(const QJsonObject &object, Event *event) {
    bool returnVal = false;

    const bool hasEventPointer = event != nullptr;
    const bool hasValidId = object.value(QStringLiteral("id")).isDouble();
    const bool hasValidName = object.value(QStringLiteral("name")).isString();
    const bool canParseEvent = hasEventPointer && hasValidId && hasValidName;

    if (canParseEvent) {
        const int eventId = object.value(QStringLiteral("id")).toInt();
        const QString eventName = jsonString(object, QStringLiteral("name"));
        const QString description = jsonString(object, QStringLiteral("description"));
        const QString location = jsonString(object, QStringLiteral("location"));
        const QString category = jsonString(object, QStringLiteral("category"));
        const QString priorityValue = jsonString(object, QStringLiteral("priority"));
        const QDateTime startDateTime = jsonDateTime(object, QStringLiteral("startDateTime"));
        const QDateTime endDateTime = jsonDateTime(object, QStringLiteral("endDateTime"));
        const QDateTime completedAt = jsonDateTime(object, QStringLiteral("completedAt"));
        const QDate recurrenceUntil = jsonDate(object, QStringLiteral("recurrenceUntil"));
        const Priority priority = priorityFromString(priorityValue);

        QString eventTypeValue = jsonString(object, QStringLiteral("eventType"));
        if (eventTypeValue.isEmpty()) {
            // Fall back to older storage keys when reading older save files.
            eventTypeValue = jsonString(object, QStringLiteral("type"));
        }

        EventType eventType = EventType::Event;
        if (!eventTypeValue.isEmpty()) {
            eventType = eventTypeFromString(eventTypeValue);
        }

        QString recurrenceTypeValue;
        if (object.contains(QStringLiteral("recurrenceType"))) {
            recurrenceTypeValue = jsonString(object, QStringLiteral("recurrenceType"));
        } else if (object.contains(QStringLiteral("recurrenceKind"))) {
            recurrenceTypeValue = jsonString(object, QStringLiteral("recurrenceKind"));
        } else if (jsonBool(object, QStringLiteral("recurring"), false)) {
            recurrenceTypeValue = jsonString(object, QStringLiteral("recurrenceType"));
        }

        RecurrenceType recurrenceType = RecurrenceType::None;
        if (!recurrenceTypeValue.isEmpty()) {
            recurrenceType = recurrenceTypeFromString(recurrenceTypeValue);
        }

        Event parsedEvent(eventId, eventName);
        parsedEvent.setDescription(description);
        parsedEvent.setLocation(location);
        parsedEvent.setStartDateTime(startDateTime);
        parsedEvent.setEndDateTime(endDateTime);
        parsedEvent.setPriority(priority);
        parsedEvent.setCategory(category);
        parsedEvent.setNotified(jsonBool(object, QStringLiteral("notified"), false));
        parsedEvent.setCompleted(jsonBool(object, QStringLiteral("completed"), false));
        parsedEvent.setCompletedAt(completedAt);
        parsedEvent.setAutoCompleteSuppressed(jsonBool(object, QStringLiteral("autoCompleteSuppressed"), false));
        parsedEvent.setEventType(eventType);
        parsedEvent.setAllDay(jsonBool(object, QStringLiteral("allDay"), false));
        parsedEvent.setRecurrenceType(recurrenceType);
        parsedEvent.setRecurrenceInterval(object.value(QStringLiteral("recurrenceInterval")).toInt(1));
        parsedEvent.setRecurrenceUntil(recurrenceUntil);

        QList<Qt::DayOfWeek> repeatDays =
            repeatDaysFromJson(object.value(QStringLiteral("repeatDays")));

        // Older weekly saves may omit repeat days, so the start day is reused.
        if (repeatDays.isEmpty()
            && recurrenceType == RecurrenceType::Weekly
            && startDateTime.isValid()) {
            repeatDays.append(static_cast<Qt::DayOfWeek>(startDateTime.date().dayOfWeek()));
        }

        parsedEvent.setRepeatDays(repeatDays);
        *event = parsedEvent;
        returnVal = true;
    }
    return returnVal;
}

} // namespace

// Saves the current events to a file and reports any storage error.
void Storage::saveToFile(const QVector<Event> &events, const QString &filename, QString *errorMessage) {
    QString saveError;

    try {
        QFileInfo fileInfo(filename);
        QDir directory = fileInfo.dir();

        // Create the directory exists if it does not exist
        if (!directory.exists()) {
            const bool directoryCreated = directory.mkpath(QStringLiteral("."));

            if (!directoryCreated){
                throw QStringLiteral("Unable to create the storage directory for %1.").arg(filename);
            }
        }

        // Convert each Event object into JSON format
        QJsonArray array;
        for (const Event &event : events) {
            array.append(eventToJson(event));
        }

        const QJsonDocument document(array);
        const QByteArray jsonData = document.toJson(QJsonDocument::Indented);

        // Protect original file by creating a temporary file first
        QSaveFile file(filename);
        const bool fileOpened = file.open(QIODevice::WriteOnly);
        if (!fileOpened) {
            throw QStringLiteral("Unable to open %1 for writing: %2").arg(filename, file.errorString());
        }

        const qint64 bytesWritten = file.write(jsonData);
        const bool writeSuccessful = bytesWritten == jsonData.size();
        if (!writeSuccessful) {
            throw QStringLiteral("Unable to write all event data to %1.").arg(filename);
        }

        // Replace the original file only if the temporary save succeeds
        const bool commited = file.commit();
        if (!commited) {
            throw QStringLiteral("Unable to save %1: %2").arg(filename, file.errorString());
        }
    }
    catch (const QString &error) {
        saveError = error;
    }

    // Store error message or clear it if there were no errors.
    if (errorMessage != nullptr) {
        if (saveError.isEmpty()) {
            errorMessage->clear();
        } else {
            *errorMessage = saveError;
        }
    }
}

// Loads saved events from a file while keeping the current file format compatible.
QVector<Event> Storage::loadFromFile(const QString &filename, QString *errorMessage) {
    QVector<Event> returnVal;
    QString loadError;

    try {
        QFileInfo fileInfo(filename);

        // Missing file is not an error, it just means there are no saved events yet.
        if (fileInfo.exists()) {
            QFile file(filename);
            const bool fileOpened = file.open(QIODevice::ReadOnly);

            if (!fileOpened) {
                throw QStringLiteral("Unable to open %1 for reading: %2").arg(filename, file.errorString());
            }

            // Read all the saved JSON data from the file
            const QByteArray data = file.readAll();
            file.close();

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
            const bool parseSuccessful = parseError.error == QJsonParseError::NoError;
            const bool documentIsArray = document.isArray();
            const bool documentIsValid = (parseSuccessful && documentIsArray);

            if (!documentIsValid) {
                throw QStringLiteral("Unable to parse %1 as event data.").arg(filename);
            }

            const QJsonArray array = document.array();
            returnVal.reserve(array.size());

            // Convert each JSON object into an Event object
            for (const QJsonValue &value : array) {
                const bool valueIsObject = value.isObject();
                if (valueIsObject) {
                    Event event;
                    const bool parsed = parseEventObject(value.toObject(), &event);
                    if (parsed) {
                        returnVal.append(event);
                    }
                }
            }
        }
    }
    catch (const QString &error) {
        loadError = error;
        returnVal.clear();
    }

    if (errorMessage != nullptr) {
        if (loadError.isEmpty()) {
            errorMessage->clear();
        } else {
            *errorMessage = loadError;
        }
    }

    return returnVal;
}
