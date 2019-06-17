#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QThread>

#include <sensor_utils.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    uint8_t Crc8Table[COMMS_CRC8_TABLE_LENGTH] = {0};
    uint16_t sensorId;
    QByteArray memo;
    QByteArray resp;
    QByteArray writeResp;
    QString errString;
    uint16_t errCode;

    sensor_config *lastReadConf;
    sensor_config *lastWrittenConf;

    sensor_data_config *lastReadDataConf;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    bool refreshSensorConfig();

private slots:
    void on_refreshComPorts_clicked();

    void on_connectToCom_clicked();

    void on_writeSensorConfig_clicked();

    void on_loadDataConf_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *port;
    QThread *serialThread;
};

#endif // MAINWINDOW_H
