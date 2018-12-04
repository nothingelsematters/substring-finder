#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "utils/parameters.h"

#include <QMainWindow>
#include <QTreeWidget>
#include <QProgressBar>
#include <memory>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


private slots:
    void select_directory();
    void remove_directories_from_list();

    void details_manager();

    void directories_scan();

    void catch_match(QString const& file_name, std::list<std::pair<int, int>> const& coordinates, bool first_match);
    void catch_error(QString const& file_name);
    void set_progress(int row, double progress);
    void result_ready();

signals:
    void clear_details();

private:
    void add_directory(QString const& dir);
    std::map<parameters, bool> get_parameters();

    void notification(const char* content, const char *window_title, int time);

    std::unique_ptr<Ui::MainWindow> ui;
};

#endif // MAINWINDOW_H
