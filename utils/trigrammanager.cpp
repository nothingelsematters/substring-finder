#include "trigrammanager.h"

#include <QThread>

TrigramManager::TrigramManager(QObject *parent) : QObject(parent) {}

TrigramManager::~TrigramManager() {}

TrigramManager::TrigramManager(std::set<QString> const& directories, std::map<parameters, bool> const& params)
    : params(params),
      directories(directories) {

    iterator_flags = params.at(parameters::Recursive) ?
        QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    directory_flags = {QDir::NoDotAndDotDot, QDir::Files};
    if (params.at(parameters::Hidden)) {
        directory_flags |= QDir::Hidden;
    }
    trigrams = new std::map<QString, std::map<QString, std::set<int64_t>>>;
}

void TrigramManager::manage_trigrams() {
    for (auto directory_name: directories) {
        sizes[directory_name] = 0;
        scan_progress[directory_name] = 0;
        for (QDirIterator it(directory_name, directory_flags, iterator_flags); it.hasNext(); ) {
            it.next();
            files.emplace_back(it.fileInfo().size(), std::make_pair(directory_name, it.filePath()));
            sizes[directory_name]++;
            if (QThread::currentThread()->isInterruptionRequested()) {
                return;
            }
        }
        if (sizes[directory_name] == 0) {
            emit throw_progress(directory_name, 100);
        }
    }

    size_t thread_quantity = std::min((size_t) 6, files.size());

    for (size_t i = 0; i < thread_quantity; ++i) {
        worker.push_back(make_worker(i, thread_quantity));
    }
}

TrigramWorker* TrigramManager::make_worker(size_t index, size_t quantity) {
    TrigramWorker* new_worker = new TrigramWorker();
    QThread* thread = new QThread();
    for (size_t i = index; i < files.size(); i += quantity) {
        new_worker->files.push_back(files[i].second);
    }
    new_worker->moveToThread(thread);

    connect(this, &TrigramManager::result, new_worker, &TrigramWorker::deleteLater);
    connect(this, &TrigramManager::result, thread, &QThread::quit);
    connect(this, &TrigramManager::cancel, thread, &QThread::requestInterruption);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(thread, &QThread::started, new_worker, &TrigramWorker::process_files);
    connect(new_worker, &TrigramWorker::throw_progress, this, &TrigramManager::progress);
    connect(new_worker, &TrigramWorker::files_processed, this, &TrigramManager::ready);
    connect(new_worker, &TrigramWorker::throw_error, this, &TrigramManager::catch_error);
    thread->start();

    return new_worker;
}

void TrigramManager::canceled() {
    emit cancel();
    emit finished();
}

void TrigramManager::catch_error(QString const& file_name) {
    emit throw_error(file_name);
}

void TrigramManager::ready(std::map<QString, std::map<QString, std::set<int64_t>>>* res) {
    for (auto i: directories) {
       (*trigrams)[i].insert((*res)[i].begin(), (*res)[i].end());
    }
    if (++workers_ready == worker.size()) {
        emit result(trigrams);
        emit finished();
    }
}

void TrigramManager::progress(QString const& directory) {
    emit throw_progress(directory, (double) ++scan_progress[directory] * 100 / sizes[directory]);
}
