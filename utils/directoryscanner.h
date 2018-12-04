#ifndef DIRECTORYSCANNER_H
#define DIRECTORYSCANNER_H

#include "utils/parameters.h"

#include <QString>
#include <QObject>
#include <QFlags>
#include <QDir>
#include <QDirIterator>

#include <map>
#include <list>
#include <vector>


class DirectoryScanner : public QObject {
    Q_OBJECT

public:
    DirectoryScanner(std::list<QString> const& directories,
                     QString const& input_string,
                     std::map<parameters, bool> const& params);
    ~DirectoryScanner();

public slots:
    void scan_directories();

signals:
    void new_match(QString const& file_name,
                   std::list<std::pair<int, int>> const& coordinates,
                   bool first_match);
    void new_error(QString const& file_name);
    void progress(int index, double progress);
    void finished();

private:
    void scan_directory(QString const& directory_name, int index);
    bool substring_find(QString const& file_name,
                        QString const& relative_path);
    std::vector<int> z_function(QString const& str);

    std::list<QString> directories;
    QFlags<QDirIterator::IteratorFlag> iterator_flags;
    QFlags<QDir::Filter> directory_flags;
    bool first_match_flag;

    QString substring;
    std::vector<int> zf;
};

#endif // DIRECTORYSCANNER_H
