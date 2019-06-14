#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QThread>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_refreshComPorts_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *port;
    QThread *serialThread;
};

#endif // MAINWINDOW_H
