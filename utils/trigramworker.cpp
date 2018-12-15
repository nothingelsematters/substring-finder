#include "trigramworker.h"

#include <QTextStream>
#include <QThread>
#include <QDir>
#include <QFile>

TrigramWorker::TrigramWorker(QObject *parent) : QObject(parent) {}

TrigramWorker::~TrigramWorker() {}

void TrigramWorker::process_files() {
    for (auto i: files) {
        process_file(i);
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
        emit throw_progress(i.first);
    }
    emit files_processed(&trigrams);
}

void TrigramWorker::process_file(std::pair<QString, QString> const& file_directory) {
    auto [directory_name, file_name] = file_directory;
    QFile file(file_name);
    if (!file.open(QFile::ReadOnly)) {
        emit throw_error(file_name.right(file_name.size() - directory_name.size() +
                                         QDir(directory_name).dirName().size()));
        return;
    }
    const int BUFFER_SIZE = 1 << 18;
    const size_t MAXIMUM = 1 << 18;

    QTextStream stream(&file);
    QString buffer = stream.read(BUFFER_SIZE);
    auto data = buffer.data();
    if (buffer.size() < 3) {
        return;
    }

    std::set<int64_t>* cur_set = &trigrams[directory_name][file_name];
    cur_set->clear();
    int64_t trigram = (((int64_t) data[0].unicode()) << 16) + (((int64_t) data[1].unicode()) << 32);

    while (buffer.size() >= 3) {
        for (int i = 2; i < buffer.size() && cur_set->size() < MAXIMUM; ++i) {
            trigram = (trigram >> 16) + (((int64_t) data[i].unicode()) << 32);
            cur_set->insert(trigram);

            if (QThread::currentThread()->isInterruptionRequested()) {
                break;
            }
        }
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
        if (cur_set->size() == MAXIMUM) {
            trigrams[directory_name].erase(file_name);
            break;
        }
        buffer = stream.read(BUFFER_SIZE);
        data = buffer.data();
    }
    file.close();

    return;
}
