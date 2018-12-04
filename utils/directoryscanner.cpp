#include "directoryscanner.h"
#include "filereader.h"

#include <QtCore/QThread>

DirectoryScanner::DirectoryScanner(std::list<QString> const& directories,
                                   QString const& input_string,
                                   std::map<parameters, bool> const& params)
    : directories(directories),
      substring(input_string) {
    iterator_flags = params.at(parameters::Recursive) ?
         QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    directory_flags = {QDir::NoDotAndDotDot, QDir::Files};
    if (params.at(parameters::Hidden)) {
        directory_flags |= QDir::Hidden;
    }
    first_match_flag = params.at(parameters::FirstMatch);
    zf = z_function(input_string);
}

DirectoryScanner::~DirectoryScanner() {}

bool DirectoryScanner::substring_find(QString const& file_name, QString const& relative_path) {
    FileReader file_reader(file_name, substring.size());
    if (!file_reader.open()) {
        return false;
    }
    const int size = substring.size();
    int left = 0;
    int right = 0;
    int line_number = 1;
    int last_line = -1;
    std::vector<int> z_cache(size, 0);
    std::list<std::pair<int, int>> coordinates;

    for (int i = 0; file_reader.readable(i); ++i) {
        int cur = i % size;
        z_cache[cur] = std::max(0, std::min(right - i, zf[i - left]));
        while (file_reader.readable(i) && z_cache[cur] < size &&
               file_reader.readable(i + z_cache[cur]) &&
               file_reader[i + z_cache[cur]] ==
                    substring[z_cache[cur]]) {
            z_cache[cur]++;
        }
        if (i + z_cache[cur] > right) {
            left = i;
            right = i + z_cache[cur];
        }
        if (z_cache[cur] == substring.size()) {
            coordinates.push_back({line_number, i - last_line});
            if (first_match_flag) {
                break;
            }
        }

        if (file_reader[i] == QChar('\n')) {
            ++line_number;
            last_line = i;
        }


        if (QThread::currentThread()->isInterruptionRequested()) {
            return true;
        }
    }

    qRegisterMetaType<std::list<std::pair<int, int>>>("coordinates");
    if (coordinates.size() > 0) {
        emit new_match(relative_path, coordinates, first_match_flag);
    }
    return true;
}

void DirectoryScanner::scan_directory(QString const& directory_name, int index) {
    size_t directory_prefix = directory_name.size() - QDir(directory_name).dirName().size();
    size_t directory_size = 0;
    size_t current = 0;

    for (QDirIterator it(directory_name, directory_flags, iterator_flags); it.hasNext(); ) {
        it.next();
        directory_size += it.fileInfo().size();
    }

    if (directory_size == 0) {
        emit progress(index, 100);
        return;
    }

    for (QDirIterator it(directory_name, directory_flags, iterator_flags); it.hasNext(); ) {
        it.next();
        QString file_path = it.filePath();
        QString relative_path = file_path.right(file_path.size() - directory_prefix);
        if (!substring_find(file_path, relative_path)) {
            emit new_error(relative_path);
        }
        current += it.fileInfo().size();
        emit progress(index, ((double) current * 100 / directory_size));
        if (QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
    }
}

void DirectoryScanner::scan_directories() {
    int index = 0;
    for (auto i: directories) {
        scan_directory(i, index++);
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
    }
    emit finished();
}

std::vector<int> DirectoryScanner::z_function(QString const& str) {
    std::vector<int> zf(str.size(), 0);
    int left = 0;
    int right = 0;
    for (int i = 1; i < str.size(); ++i) {
        zf[i] = std::max(0, std::min(right - i, zf[i - left]));
        while (i + zf[i] < str.size() && str[zf[i]] == str[i + zf[i]]) {
            zf[i]++;
        }
        if (i + zf[i] > right) {
            left = i;
            right = i + zf[i];
        }
    }
    return std::move(zf);
}
