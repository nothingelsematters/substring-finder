#ifndef TRIGRAMWORKER_H
#define TRIGRAMWORKER_H

#include <QObject>
#include <QString>

#include <map>
#include <set>

class TrigramWorker : public QObject
{
    Q_OBJECT
public:
    explicit TrigramWorker(QObject *parent = nullptr);
    ~TrigramWorker();

signals:
    void files_processed(std::map<QString, std::map<QString, std::set<int64_t>>>* result);
    void throw_progress(QString const& directory);
    void throw_error(QString const& file_name);

public slots:
    void process_files();

public:
    std::list<std::pair<QString, QString>> files;
    std::map<QString, std::map<QString, std::set<int64_t>>> trigrams;

private:
    void process_file(std::pair<QString, QString> const& file_directory);
};

#endif // TRIGRAMWORKER_H
