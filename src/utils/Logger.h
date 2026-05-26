// Logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QDebug>

namespace CBCTAlign {

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    void init(const QString& logFile) {
        m_file.setFileName(logFile);
        m_file.open(QIODevice::WriteOnly | QIODevice::Append);
        m_stream.setDevice(&m_file);
    }
    
    void info(const QString& message) { log("INFO", message); }
    void warning(const QString& message) { log("WARN", message); }
    void error(const QString& message) { log("ERROR", message); }
    
private:
    Logger() = default;
    ~Logger() { m_file.close(); }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void log(const QString& level, const QString& message) {
        QMutexLocker locker(&m_mutex);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString line = QString("[%1] [%2] %3").arg(timestamp, level, message);
        
        qDebug() << line;
        if (m_file.isOpen()) {
            m_stream << line << "\n";
            m_stream.flush();
        }
    }
    
    QFile m_file;
    QTextStream m_stream;
    QMutex m_mutex;
};

} // namespace CBCTAlign

#endif // LOGGER_H
