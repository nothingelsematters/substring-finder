#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils/directoryscanner.h"

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

#include <map>
#include <vector>
#include <list>
#include <string>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow) {
    ui->setupUi(this);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), qApp->desktop()->availableGeometry()));

    ui->detailsList->setHidden(true);
    ui->cancelButton->setHidden(true);

    QCommonStyle style;
    ui->actionAdd_Directory->setIcon(style.standardIcon(QCommonStyle::SP_DialogOpenButton));
    ui->actionRemove_Directories_From_List->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));
    ui->actionExit->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));

    connect(ui->actionAdd_Directory, &QAction::triggered, this, &MainWindow::select_directory);
    connect(ui->actionRemove_Directories_From_List, &QAction::triggered,
            this, &MainWindow::remove_directories_from_list);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    connect(ui->scanButton, &QAbstractButton::clicked, this, &MainWindow::directories_scan);
    connect(ui->inputString, &QLineEdit::returnPressed, this, &MainWindow::directories_scan);
}

MainWindow::~MainWindow() {}

std::map<parameters, bool> MainWindow::get_parameters() {
    std::map<parameters, bool> result;
    result[parameters::Hidden] = ui->hiddenCheckbox->checkState();
    result[parameters::Recursive] = ui->recursiveCheckbox->checkState();
    result[parameters::FirstMatch] = ui->firstMatchCheckbox->checkState();

    return std::move(result);
}

void MainWindow::notification(const char* content,
                              const char* window_title = "Notification",
                              int time = 3000) {
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

void MainWindow::result_ready() {
    ui->scanButton->setDisabled(false);
    connect(ui->inputString, &QLineEdit::returnPressed, this, &MainWindow::directories_scan);
    ui->cancelButton->setHidden(true);

    int count = ui->stringsList->invisibleRootItem()->childCount();
    if (count == 0) {
        notification("No matches found");
    } else {
        notification((std::to_string(count) + " matches found").c_str());
    }
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

void MainWindow::select_directory() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory for Scanning",
        QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir != nullptr) {
        add_directory(dir);
    }
}

void MainWindow::add_directory(QString const& dir) {
    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        QString cur_dir = static_cast<QProgressBar*>(ui->directoriesTable->
                                                     cellWidget(i, 0))->format();
        if (cur_dir == dir || dir.indexOf(cur_dir + '/') == 0) {
            notification("This directory is already on the list");
            return;
        }

        if (cur_dir.indexOf(dir) == 0) {
            while (i < ui->directoriesTable->rowCount()) {
                if (static_cast<QProgressBar*>(ui->directoriesTable->
                       cellWidget(i, 0))->format().indexOf(dir) == 0) {
                    ui->directoriesTable->removeRow(i);
                } else {
                    ++i;
                }
            }
            notification("Subdirectories were replaced with the directory chosen");
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
}

void MainWindow::remove_directories_from_list() {
    auto directory_list = ui->directoriesTable->selectedItems();
    for (auto i: directory_list) {
        ui->directoriesTable->removeRow(i->row());
    }
}

void MainWindow::directories_scan() {
    ui->stringsList->clear();
    ui->detailsList->clear();
    ui->detailsList->setHidden(true);
    emit clear_details();

    if (ui->directoriesTable->rowCount() == 0) {
        notification("Please choose directories to scan");
        return;
    }
    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        static_cast<QProgressBar*>(ui->directoriesTable->cellWidget(i, 0))->setValue(0);
    }
    QString input_string = ui->inputString->text();
    if (input_string.size() == 0) {
        notification("Please write a string to search for");
        return;
    }

    ui->scanButton->setDisabled(true);
    ui->cancelButton->setHidden(false);
    disconnect(ui->inputString, &QLineEdit::returnPressed, this, &MainWindow::directories_scan);
    std::list<QString> directories;

    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        QString directory_name = static_cast<QProgressBar*>(ui->directoriesTable->cellWidget(i, 0))->format();
        directories.push_back(directory_name);
    }
    DirectoryScanner* dir_scanner = new DirectoryScanner(directories,
        input_string, get_parameters());
    QThread* worker_thread = new QThread();
    dir_scanner->moveToThread(worker_thread);
    connect(dir_scanner, &DirectoryScanner::new_match, this, &MainWindow::catch_match);
    connect(dir_scanner, &DirectoryScanner::new_error, this, &MainWindow::catch_error);
    connect(dir_scanner, &DirectoryScanner::progress, this, &MainWindow::set_progress);
    connect(dir_scanner, &DirectoryScanner::finished, worker_thread, &QThread::quit);
    connect(dir_scanner, &DirectoryScanner::finished, dir_scanner, &DirectoryScanner::deleteLater);
    connect(dir_scanner, &DirectoryScanner::finished, this, &MainWindow::result_ready);
    connect(worker_thread, &QThread::finished, worker_thread, &QThread::deleteLater);
    connect(worker_thread, &QThread::started, dir_scanner, &DirectoryScanner::scan_directories);
    connect(ui->cancelButton, &QPushButton::clicked, worker_thread, &QThread::requestInterruption);
    worker_thread->start();
}

void MainWindow::catch_match(QString const& file_name, std::list<std::pair<int, int>> const& coordinates, bool first_match) {
    QTreeWidgetItem* parent = new QTreeWidgetItem(ui->stringsList);
    parent->setText(0, file_name);
    ui->stringsList->insertTopLevelItem(0, parent);
    if (!first_match) {
        for (auto i: coordinates) {
            QTreeWidgetItem* item = new QTreeWidgetItem(parent);
            item->setText(0, QString::number(i.first) + ": " + QString::number(i.second));
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

void MainWindow::set_progress(int row, double progress) {
    static_cast<QProgressBar*>(ui->directoriesTable->cellWidget(row, 0))->setValue(progress);
}
