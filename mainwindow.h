#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "utils/parameters.h"
#include "utils/directoryscanner.h"

#include <QMainWindow>
#include <QTreeWidget>
#include <QProgressBar>
#include <memory>
#include <set>
#include <list>


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();


private slots:
    void select_directory();
    void remove_directories_from_list();
    void normalize_directories(bool checkbox_state);

    void details_manager();
    void handle_scan_button();

    void finished_process();
    void preparations();
    void prepared(std::map<QString, std::map<QString,
                  std::set<int64_t>>>* result);
    void directories_scan();
    void result_ready();

    void catch_match(QString const& file_name, std::list<int> const& coordinates, bool first_match);
    void catch_error(QString const& file_name);
    void set_progress(QString const& directory, double progress);

signals:
    void clear_details();

private:
    QString get_directory_name(int row);
    void add_directory(QString const& dir);
    void remove_directory(int row);

    std::map<parameters, bool> get_parameters();
    std::pair<DirectoryScanner*, QThread*> new_dir_scanner();

    void notification(const char* content, const char* window_title, int time);
    std::map<QString, std::map<QString, std::set<int64_t>>>* preprocessing = nullptr;
    std::set<QString> directories_to_preprocess;

    Ui::MainWindow* ui;
};

#endif // MAINWINDOW_H
