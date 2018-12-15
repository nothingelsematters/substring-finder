#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils/directoryscanner.h"
#include "utils/trigrammanager.h"

#include <QCommonStyle>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QCheckBox>
#include <QDirIterator>
#include <QDir>
#include <QProgressBar>
#include <QFileInfo>
#include <QLabel>
#include <QtCore/QThread>
#include <QMovie>
#include <QDebug>

#include <map>
#include <vector>
#include <list>
#include <string>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow) {
    ui->setupUi(this);
    QWidget::setWindowTitle("Substring Finder");
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), qApp->desktop()->availableGeometry()));

    ui->detailsList->setHidden(true);
    ui->cancelButton->setHidden(true);
    ui->scanButton->setDisabled(true);

    QCommonStyle style;
    ui->actionAdd_Directory->setIcon(style.standardIcon(QCommonStyle::SP_DialogOpenButton));
    ui->actionRemove_Directories_From_List->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));
    ui->actionExit->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));

    connect(ui->actionAdd_Directory, &QAction::triggered, this, &MainWindow::select_directory);
    connect(ui->actionRemove_Directories_From_List, &QAction::triggered,
            this, &MainWindow::remove_directories_from_list);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    connect(ui->recursiveCheckbox, &QCheckBox::toggled, this, &MainWindow::normalize_directories);
    connect(ui->scanButton, &QAbstractButton::clicked, this, &MainWindow::directories_scan);
    connect(ui->prepareButton, &QPushButton::clicked, this, &MainWindow::preparations);
    connect(ui->recursiveCheckbox, &QCheckBox::stateChanged, this, &MainWindow::handle_scan_button);
    connect(ui->hiddenCheckbox, &QCheckBox::stateChanged, this, &MainWindow::handle_scan_button);
    connect(ui->preprocessCheckBox, &QCheckBox::stateChanged, this, &MainWindow::handle_scan_button);
    connect(ui->preprocessCheckBox, &QCheckBox::toggled, ui->prepareButton, &QPushButton::setVisible);
    connect(ui->preprocessCheckBox, &QCheckBox::toggled, ui->scanButton, &QPushButton::setDisabled);

    connect(ui->inputString, &QLineEdit::returnPressed, ui->scanButton, &QPushButton::click);

    qRegisterMetaType<std::list<int>>("coordinates");
    qRegisterMetaType<std::map<QString, std::map<QString,
            std::set<std::list<QChar>>>>*>("trigrams");
}

MainWindow::~MainWindow() {
    delete preprocessing;
}

std::map<parameters, bool> MainWindow::get_parameters() {
    std::map<parameters, bool> result;
    result[parameters::Hidden] = ui->hiddenCheckbox->checkState();
    result[parameters::Recursive] = ui->recursiveCheckbox->checkState();
    result[parameters::FirstMatch] = ui->firstMatchCheckbox->checkState();
    result[parameters::Preprocess] = ui->preprocessCheckBox->checkState();

    return std::move(result);
}

void MainWindow::notification(const char* content,
                              const char* window_title = "Notification",
                              int time = 4000) {
    QMessageBox* msgbox = new QMessageBox(QMessageBox::Information,
        QString(window_title), QString(content),
        QMessageBox::StandardButtons(QMessageBox::Ok), this, Qt::WindowType::Popup);
    msgbox->open();

    QTimer* timer = new QTimer();
    connect(timer, &QTimer::timeout, msgbox, &QMessageBox::close);
    connect(timer, &QTimer::timeout, timer, &QTimer::stop);
    connect(timer, &QTimer::timeout, timer, &QTimer::deleteLater);
    timer->start(time);
}

void MainWindow::details_manager() {
    QPushButton* sender = static_cast<QPushButton*>(QObject::sender());
    if (sender->text() == "hide details") {
        ui->detailsList->setHidden(true);
        sender->setText("show details");
    } else {
        ui->detailsList->setHidden(false);
        sender->setText("hide details");
    }
}

QString MainWindow::get_directory_name(int row) {
    return static_cast<QProgressBar*>(ui->directoriesTable->cellWidget(row, 0))->format();
}

void MainWindow::select_directory() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory for Scanning",
        QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir != nullptr) {
        add_directory(dir);
    }
}

void MainWindow::add_directory(QString const& dir) {
    bool recursive = get_parameters()[parameters::Recursive];
    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        QString cur_dir = get_directory_name(i);
        if (cur_dir == dir || (recursive && dir.indexOf(cur_dir + '/') == 0)) {
            notification("This directory is already on the list");
            return;
        }

        if (recursive && cur_dir.indexOf(dir + '/') == 0) {
            while (i < ui->directoriesTable->rowCount()) {
                if (get_directory_name(i).indexOf(dir) == 0) {
                    remove_directory(i);
                } else {
                    ++i;
                }
            }
            notification("Subdirectories were replaced with the directory chosen\n"
                         "If you want to scan exactly these directories, you can "
                         "turn off the \"Recursive\" option");
            break;
        }
    }

    ui->directoriesTable->insertRow(ui->directoriesTable->rowCount());
    QTableWidgetItem* item = new QTableWidgetItem();
    ui->directoriesTable->setItem(ui->directoriesTable->rowCount() - 1, 0, item);
    QProgressBar* pb = new QProgressBar();
    pb->setFormat(dir);
    pb->setValue(0);
    ui->directoriesTable->setCellWidget(ui->directoriesTable->rowCount() - 1, 0, pb);

    directories_to_preprocess.insert(dir);
    if (get_parameters()[parameters::Preprocess]) {
        ui->scanButton->setDisabled(true);
    }
}

void MainWindow::remove_directory(int row) {
    QString name = get_directory_name(row);
    ui->directoriesTable->removeRow(row);
    directories_to_preprocess.erase(name);
    if (preprocessing != nullptr) {
        (*preprocessing).erase(name);
    }
}

void MainWindow::remove_directories_from_list() {
    for (auto i: ui->directoriesTable->selectedItems()) {
        remove_directory(i->row());
    }
}

void MainWindow::normalize_directories(bool checkbox_state) {
    if (!checkbox_state) {
        return;
    }

    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        QString dir_name = get_directory_name(i);
        for (int j = i + 1; j < ui->directoriesTable->rowCount(); ++j) {
            QString second_dir_name = get_directory_name(j);

            if (dir_name.indexOf(second_dir_name + '/') == 0) {
                static_cast<QProgressBar*>(ui->directoriesTable->cellWidget(i--, 0))->
                        setFormat(second_dir_name);
                remove_directory(j);
                break;
            } else if (second_dir_name.indexOf(dir_name + '/') == 0) {
                remove_directory(j--);
            }
        }
    }
}

std::pair<DirectoryScanner*, QThread*> MainWindow::new_dir_scanner() {
    ui->detailsList->clear();
    ui->detailsList->setHidden(true);
    emit clear_details();

    ui->scanButton->setDisabled(true);
    ui->cancelButton->setHidden(false);
    ui->prepareButton->setDisabled(true);
    ui->actionRemove_Directories_From_List->setDisabled(true);
    ui->actionAdd_Directory->setDisabled(true);

    QThread* worker_thread = new QThread();
    DirectoryScanner* dir_scanner = new DirectoryScanner(get_parameters(), preprocessing);
    dir_scanner->moveToThread(worker_thread);

    connect(dir_scanner, &DirectoryScanner::new_match, this, &MainWindow::catch_match);
    connect(dir_scanner, &DirectoryScanner::new_error, this, &MainWindow::catch_error);
    connect(dir_scanner, &DirectoryScanner::finished, worker_thread, &QThread::quit);
    connect(dir_scanner, &DirectoryScanner::finished, this, &MainWindow::finished_process);
    connect(dir_scanner, &DirectoryScanner::finished, dir_scanner, &DirectoryScanner::deleteLater);
    connect(dir_scanner, &DirectoryScanner::progress, this, &MainWindow::set_progress);
    connect(worker_thread, &QThread::finished, worker_thread, &QThread::deleteLater);
    connect(ui->cancelButton, &QPushButton::clicked, worker_thread, &QThread::requestInterruption);

    return {dir_scanner, worker_thread};
}

void MainWindow::handle_scan_button() {
    if (get_parameters()[parameters::Preprocess]) {
        ui->scanButton->setDisabled(true);
        for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
            directories_to_preprocess.insert(get_directory_name(i));
        }
        if (preprocessing != nullptr) {
            delete preprocessing;
            preprocessing = nullptr;
        }
    }
}

void MainWindow::finished_process() {
    ui->prepareButton->setDisabled(false);
    ui->actionRemove_Directories_From_List->setDisabled(false);
    ui->actionAdd_Directory->setDisabled(false);
    ui->cancelButton->setHidden(true);
    ui->directoriesTable->setStyleSheet("");
    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        static_cast<QProgressBar*>(ui->directoriesTable->cellWidget(i, 0))->setValue(0);
    }
}

void MainWindow::preparations() {
    if (ui->directoriesTable->rowCount() == 0) {
        notification("Please choose directories to scan");
        return;
    }
    if (directories_to_preprocess.size() == 0) {
        notification("There's nothing to preprocess");
        return;
    }
    if (preprocessing == nullptr) {
        preprocessing = new std::map<QString, std::map<QString, std::set<int64_t>>>();
    }
    ui->detailsList->clear();
    ui->detailsList->setHidden(true);
    emit clear_details();

    ui->scanButton->setDisabled(true);
    ui->cancelButton->setHidden(false);
    ui->prepareButton->setDisabled(true);
    ui->actionRemove_Directories_From_List->setDisabled(true);
    ui->actionAdd_Directory->setDisabled(true);
    ui->directoriesTable->setStyleSheet("QProgressBar::chunk { background-color: rgba(0, 0, 255, 100) }");

    QThread* thread = new QThread();
    TrigramManager* tm = new TrigramManager(directories_to_preprocess, get_parameters());
    tm->moveToThread(thread);

    connect(thread, &QThread::started, tm, &TrigramManager::manage_trigrams);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(tm, &TrigramManager::result, this, &MainWindow::prepared);
    connect(tm, &TrigramManager::finished, tm, &TrigramManager::deleteLater);
    connect(tm, &TrigramManager::finished, thread, &QThread::quit);
    connect(tm, &TrigramManager::finished, this, &MainWindow::finished_process);
    connect(tm, &TrigramManager::throw_progress, this, &MainWindow::set_progress);
    connect(tm, &TrigramManager::throw_error, this, &MainWindow::catch_error);
    connect(ui->cancelButton, &QPushButton::clicked, tm, &TrigramManager::canceled);
    thread->start();
}

void MainWindow::prepared(std::map<QString, std::map<QString,
                          std::set<int64_t>>>* result) {
    ui->scanButton->setDisabled(false);
    preprocessing = result;
    directories_to_preprocess.clear();
}

void MainWindow::directories_scan() {
    ui->stringsList->clear();
    QString input_string = ui->inputString->text();
    if (input_string.size() == 0) {
        notification("Please write a string to search for");
        return;
    }

    if (ui->directoriesTable->rowCount() == 0) {
        notification("Please choose directories to scan");
        return;
    }
    auto [dir_scanner, worker_thread] = new_dir_scanner();
    dir_scanner->add_scan_properties(input_string);

    if (!get_parameters()[parameters::Preprocess] || input_string.size() < 3) {
        std::list<QString> directories;
        for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
            QString directory_name = get_directory_name(i);
            directories.push_back(directory_name);
        }
        dir_scanner->add_directories(std::move(directories));
    }

    connect(worker_thread, &QThread::started, dir_scanner, &DirectoryScanner::scan_directories);
    connect(dir_scanner, &DirectoryScanner::finished, this, &MainWindow::result_ready);
    worker_thread->start();
}

void MainWindow::result_ready() {
    ui->scanButton->setDisabled(false);
    int count = ui->stringsList->invisibleRootItem()->childCount();
    if (count == 0) {
        notification("No matches found");
    } else {
        notification((std::to_string(count) + " matches found").c_str());
    }
}

void MainWindow::catch_match(QString const& file_name, std::list<int> const& coordinates, bool first_match) {
    QTreeWidgetItem* parent = new QTreeWidgetItem(ui->stringsList);
    parent->setText(0, file_name + ": " + QString::number(coordinates.size()));
    ui->stringsList->insertTopLevelItem(0, parent);
    if (!first_match) {
        for (auto i: coordinates) {
            QTreeWidgetItem* item = new QTreeWidgetItem(parent);
            item->setText(0, QString::number(i));
        }
    }
}

void MainWindow::catch_error(QString const& file_name) {
    if (ui->detailsList->count() == 0) {
        QPushButton* button = new QPushButton("show details");
        ui->mainGrid->addWidget(button);
        connect(button, &QPushButton::clicked, this, &MainWindow::details_manager);
        connect(this, &MainWindow::clear_details, button, &QPushButton::deleteLater);
        connect(this, &MainWindow::clear_details, ui->statusBar, &QStatusBar::clearMessage);
    }
    ui->detailsList->addItem(new QListWidgetItem(file_name));
    ui->statusBar->showMessage(QString("Troubled reading: ") +
                                QString(QString::number(ui->detailsList->count())) + " file(s) troubled reading");
}

void MainWindow::set_progress(QString const& directory, double progress) {
    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        if (get_directory_name(i) == directory) {
            static_cast<QProgressBar*>(ui->directoriesTable->cellWidget(i, 0))->setValue(progress);
                return;
        }
    }
}
