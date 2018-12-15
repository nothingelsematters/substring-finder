#ifndef DIRECTORYSCANNER_H
#define DIRECTORYSCANNER_H

#include "parameters.h"
#include "qcharhash.cpp"

#include <QString>
#include <QObject>
#include <QFlags>
#include <QDir>
#include <QDirIterator>

#include <map>
#include <vector>
#include <set>
#include <functional>
#include <cstdint>
#include <algorithm>


class DirectoryScanner : public QObject {
    Q_OBJECT

public:
    DirectoryScanner(std::map<parameters, bool> const& params,
                     std::map<QString, std::map<QString, std::set<int64_t>>>* trigrams);

    ~DirectoryScanner();

    void add_scan_properties(QString const& input_string);
    void add_directories(std::list<QString> const& directories);
    void add_directories(std::set<QString> const& directories);


public slots:
    void scan_directories();

signals:
    void new_match(QString const& file_name,
                   std::list<int> const& coordinates,
                   bool first_match);
    void new_error(QString const& file_name);
    void progress(QString const& directory, double progress);
    void finished();

private:
    void scan_directory(QString const& directory_name);
    bool substring_find(QString const& file_name, QString const& relative_path);

    void directory_dfs(QString const& directory_name,
                       std::function<bool(DirectoryScanner&, QString const&, QString const&)> process);

    std::list<QString> directories;
    QFlags<QDirIterator::IteratorFlag> iterator_flags;
    QFlags<QDir::Filter> directory_flags;
    std::map<parameters, bool> params;

    QString substring;
    std::boyer_moore_horspool_searcher<QChar*, std::hash<QChar>, std::equal_to<void>>* preprocess = nullptr;
    std::map<QString, std::map<QString, std::set<int64_t>>>* trigrams = nullptr;
};

#endif // DIRECTORYSCANNER_H
