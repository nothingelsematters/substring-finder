#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils/filereader.h"

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

#include <map>
#include <vector>
#include <list>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow) {
    ui->setupUi(this);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), qApp->desktop()->availableGeometry()));

    ui->directoriesTable->horizontalHeader()->setStretchLastSection(true);
    ui->detailsList->setHidden(true);

    QCommonStyle style;
    ui->actionAdd_Directory->setIcon(style.standardIcon(QCommonStyle::SP_DialogOpenButton));
    ui->actionRemove_Directories_From_List->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));
    ui->actionExit->setIcon(style.standardIcon(QCommonStyle::SP_DialogCloseButton));

    connect(ui->actionAdd_Directory, &QAction::triggered, this, &MainWindow::select_directory);
    connect(ui->actionRemove_Directories_From_List, &QAction::triggered,
            this, &MainWindow::remove_directories_from_list);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    connect(ui->scanButton, &QAbstractButton::clicked, this, &MainWindow::directories_scan);
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

void MainWindow::file_trouble_message(const char* message, int quantity) {
    ui->statusBar->showMessage(QString("Troubled ") + message + ": " +
        QString(QString::number(quantity)) + " file(s) troubled " + message);
    QPushButton* button = new QPushButton("show details");
    ui->mainGrid->addWidget(button);
    connect(button, &QPushButton::clicked, this, &MainWindow::details_manager);
    connect(this, &MainWindow::clear_details, button, &QPushButton::deleteLater);
    connect(this, &MainWindow::clear_details, ui->statusBar, &QStatusBar::clearMessage);
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
                if (static_cast<QProgressBar*>(ui->
                        directoriesTable->cellWidget(i, 0))->format().indexOf(dir) == 0) {
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

std::vector<int> z_function(QString const& str) {
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

bool MainWindow::substring_find(QString const& file_name, QString const& relative_path,
                                QString const& substring, std::vector<int> zf, bool first_match) {
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
            ui->stringsList->addItem(new QListWidgetItem(relative_path + ": " +
                                                         QString::number(line_number) + " " +
                                                         QString::number(i - last_line)));
            if (first_match) {
                return true;
            }
        }

        if (file_reader[i] == QChar('\n')) {
            ++line_number;
            last_line = i;
        }
    }
    return true;
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
    auto params = get_parameters();

    QFlags<QDirIterator::IteratorFlag> it_flags = params[parameters::Recursive] ?
        QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QFlags<QDir::Filter> dir_flags = {QDir::NoDotAndDotDot, QDir::Files};
    if (params[parameters::Hidden]) {
        dir_flags |= QDir::Hidden;
    }
    std::vector<int> zf = z_function(input_string);

    for (int i = 0; i < ui->directoriesTable->rowCount(); ++i) {
        QProgressBar* pb = (QProgressBar*) ui->directoriesTable->cellWidget(i, 0);
        QString directory_name = pb->format();
        size_t directory_prefix = directory_name.size() - QDir(directory_name).dirName().size();
        size_t directory_size = 0;
        size_t current = 0;

        for (QDirIterator it(directory_name, dir_flags, it_flags); it.hasNext(); ) {
            it.next();
            directory_size += it.fileInfo().size();
        }

        if (directory_size == 0) {
            pb->setValue(100);
            continue;
        }

        for (QDirIterator it(directory_name, dir_flags, it_flags); it.hasNext(); ) {
            it.next();
            QString file_path = it.filePath();
            QString relative_path = file_path.right(file_path.size() - directory_prefix);
            if (!substring_find(file_path, relative_path, input_string, zf,
                                params[parameters::FirstMatch])) {
                ui->detailsList->addItem(new QListWidgetItem(relative_path));
            }
            current += it.fileInfo().size();
            pb->setValue((double) (current * 100 / directory_size));
        }
    }

    if (ui->stringsList->count() == 0) {
        notification("No match found");
    }

    if (ui->detailsList->count() > 0) {
        file_trouble_message("reading", ui->detailsList->count());
    }
}
