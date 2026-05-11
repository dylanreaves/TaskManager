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



