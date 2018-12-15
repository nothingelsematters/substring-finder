#include "directoryscanner.h"

#include <unordered_map>
#include <QString>

#include <QtCore/QThread>
#include <QDebug>


DirectoryScanner::DirectoryScanner(std::map<parameters, bool> const& params,
                                   std::map<QString, std::map<QString, std::set<int64_t>>>* trigrams)
    : params(params),
      trigrams(trigrams) {

    iterator_flags = params.at(parameters::Recursive) ?
         QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    directory_flags = {QDir::NoDotAndDotDot, QDir::Files};
    if (params.at(parameters::Hidden)) {
        directory_flags |= QDir::Hidden;
    }
}

DirectoryScanner::~DirectoryScanner() {
    delete preprocess;
}

void DirectoryScanner::add_scan_properties(QString const& input_string) {
    substring = input_string;
    preprocess = new std::boyer_moore_horspool_searcher<QChar*, std::hash<QChar>,
            std::equal_to<void>>(substring.begin(), substring.end());
}

void DirectoryScanner::add_directories(std::list<QString> const& directories) {
    this->directories = directories;
}

void DirectoryScanner::add_directories(std::set<QString> const& directories) {
    this->directories.clear();
    for (auto i: directories) {
        this->directories.push_back(i);
    }
}

bool DirectoryScanner::substring_find(QString const& directory_name, QString const& file_name) {
    size_t directory_prefix = directory_name.size() - QDir(directory_name).dirName().size();
    QString relative_path = file_name.right(file_name.size() - directory_prefix);
    QFile file(file_name);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }
    const int size = substring.size();
    const int BUFFER_SIZE = 1 << 18;
    std::list<int> coordinates;
    QTextStream stream(&file);
    QString buffer = stream.read(BUFFER_SIZE);
    int index = 0;
    while (buffer.size() > size - 1) {
        auto it = buffer.begin();
        while ((it = std::search(it, buffer.end(), *preprocess)) != buffer.end()) {
            coordinates.push_back(index + (it++ - buffer.begin()));

            if (QThread::currentThread()->isInterruptionRequested() || params[parameters::FirstMatch]) {
                break;
            }
        }
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
        index += buffer.size() - size + 1;
        buffer = buffer.mid(buffer.size() - substring.size() + 1);
        buffer += stream.read(BUFFER_SIZE);
    }

    qRegisterMetaType<std::list<int>>("coordinates");
    if (coordinates.size() > 0) {
        emit new_match(relative_path, coordinates, params[parameters::FirstMatch]);
    }
    return true;
}

void DirectoryScanner::scan_directory(QString const& directory_name) {
    size_t directory_prefix = directory_name.size() - QDir(directory_name).dirName().size();
    size_t directory_size = 0;
    size_t current = 0;
    std::list<QString> files;
    for (auto i: (*trigrams)[directory_name]) {
        bool accept = true;
        int64_t needed = (((int64_t) substring[0].unicode()) << 16) +
                         (((int64_t) substring[1].unicode()) << 32);
        for (int j = 2; j < substring.size(); ++j) {
            needed = (needed >> 16) + (((int64_t) substring[j].unicode()) << 32);
            if (i.second.find(needed) == i.second.end()) {
                accept = false;
                break;
            }
        }

        if (accept) {
            files.push_back(i.first);
            directory_size += QFileInfo(i.first).size();
        }
    }
    for (auto i: files) {
        QString relative_path = i.right(i.size() - directory_prefix);
        if (!substring_find(directory_name, i)) {
            emit new_error(relative_path);
        }
        if (QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
        current += QFileInfo(i).size();
        emit progress(directory_name, ((double) current * 100 / directory_size));
    }
    emit progress(directory_name, 100);
}

void DirectoryScanner::scan_directories() {
    if (params[parameters::Preprocess] && substring.size() >= 3) {
        for (auto i: *trigrams) {
            scan_directory(i.first);
            if (QThread::currentThread()->isInterruptionRequested()) {
                break;
            }
        }
    } else {
        for (auto i: directories) {
            directory_dfs(i, &DirectoryScanner::substring_find);
            if (QThread::currentThread()->isInterruptionRequested()) {
                break;
            }
        }
    }
    emit finished();
}

void DirectoryScanner::directory_dfs(QString const& directory_name,
                                     std::function<bool(DirectoryScanner&, QString const&,
                                                        QString const&)> process) {
    size_t directory_prefix = directory_name.size() - QDir(directory_name).dirName().size();
    size_t directory_size = 0;
    size_t current = 0;

    for (QDirIterator it(directory_name, directory_flags, iterator_flags); it.hasNext(); ) {
        it.next();
        directory_size += it.fileInfo().size();
    }

    if (directory_size == 0) {
        emit progress(directory_name, 100);
        return;
    }

    for (QDirIterator it(directory_name, directory_flags, iterator_flags); it.hasNext(); ) {
        it.next();
        QString file_path = it.filePath();
        QString relative_path = file_path.right(file_path.size() - directory_prefix);
        if (!process(*this, directory_name, file_path)) {
            emit new_error(relative_path);
        }
        current += it.fileInfo().size();
        emit progress(directory_name, ((double) current * 100 / directory_size));
        if (QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
    }
}
