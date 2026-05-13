// Header file for handling file i/o and stores a vector of "Event" class objects
#ifndef STORAGE_H
#define STORAGE_H

#include <QString>
#include <QVector>

#include "Event.h"

class Storage {
    public:
        static void saveToFile(const QVector<Event> &events, const QString &filename, QString *errorMessage = nullptr);
        static QVector<Event> loadFromFile(const QString &filename, QString *errorMessage = nullptr);
};

#endif // STORAGE_H



