#ifndef TRIGRAMMANAGER_H
#define TRIGRAMMANAGER_H

#include "trigramworker.h"
#include "parameters.h"

#include <QObject>
#include <QDir>
#include <QDirIterator>

#include <vector>
#include <map>
#include <set>


class TrigramManager : public QObject {
    Q_OBJECT

public:
    explicit TrigramManager(QObject *parent = nullptr);
    TrigramManager(std::set<QString> const& directories, std::map<parameters, bool> const& params);
    ~TrigramManager();

signals:
    void result(std::map<QString, std::map<QString, std::set<int64_t>>>* result);
    void throw_progress(QString const& directory, double progress);
    void throw_error(QString const& file_name);
    void finished();
    void cancel();

public slots:
    void manage_trigrams();
    void canceled();

private slots:
    void ready(std::map<QString, std::map<QString, std::set<int64_t>>>* res);
    void catch_error(QString const& file_name);

private:
    void progress(QString const& directory);
    TrigramWorker* make_worker(size_t index, size_t quantity);

    QFlags<QDirIterator::IteratorFlag> iterator_flags;
    QFlags<QDir::Filter> directory_flags;
    std::map<QString, int> sizes;
    std::map<QString, int> scan_progress;

    std::map<parameters, bool> params;
    std::vector<std::pair<int64_t, std::pair<QString, QString>>> files;
    std::set<QString> directories;
    std::map<QString, std::map<QString, std::set<int64_t>>>* trigrams = nullptr;
    std::vector<TrigramWorker*> worker;
    size_t workers_ready = 0;
};

#endif // TRIGRAMMANAGER_H
