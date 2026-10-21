#pragma once

// zipstore.h — minimal ZIP reader/writer, STORED entries only (no compression).
//
// Self-contained on purpose: a .qnb notebook is a zip file, and depending on
// Qt's private QZipReader/QZipWriter would tie us to private headers. Stored
// (uncompressed) entries keep this ~200 lines, notebooks are mostly PNGs
// (already compressed), and any unzip tool can still open the result.

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class ZipStore
{
public:
    // Write all entries (name -> bytes) to filePath, replacing it.
    // Entry names use '/' separators. Returns false and sets *error on failure.
    static bool writeFile(const QString& filePath, const QMap<QString, QByteArray>& entries, QString* error);

    // Read all entries from filePath. Returns false and sets *error on failure
    // (not a zip, truncated central directory, bad CRC, ...).
    static bool readFile(const QString& filePath, QMap<QString, QByteArray>* entries, QString* error);

    // Just the entry names, in central-directory order.
    static bool listEntries(const QString& filePath, QStringList* names, QString* error);

    static quint32 crc32(const QByteArray& data);
};
