#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "parameters.h"

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

signals:
    void clear_details();

private:
    void add_directory(QString const& dir);
    std::map<parameters, bool> get_parameters();
    bool substring_find(QString const& file_name, const QString &relative_path, QString const& substring,
                        std::vector<int> zf, bool first_match);

    void notification(const char* content, const char *window_title, int time);
    void file_trouble_message(const char* message, int quantity);

    std::unique_ptr<Ui::MainWindow> ui;
};

#endif // MAINWINDOW_H
