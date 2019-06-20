#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QSerialPort>
#include <QTableWidgetItem>
//#include <QThread>
#include <QTimer>

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
    QTimer *sensorClock;
    int numApproaches;
    int numClasses;
    int numLanes;
    double classBounds[8] = {0};
    lane laneArr[10];

    char laneGridInitialized = 0;
    QLabel* laneLabels[10][3];

    sensor_config *lastReadConf;
    sensor_config *lastWrittenConf;
    sensor_data_config *lastReadDataConf;
    sensor_datetime *lastReadDateTime;
    approach *appr;
    QTableWidgetItem twi[16];

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void refreshActiveLanes();
    void refreshDateTime();
    bool refreshSensorConfig();


private slots:
    void on_refreshComPorts_clicked();

    void on_connectToCom_clicked();

    void on_writeSensorConfig_clicked();

    void on_loadDataConf_clicked();

    void on_dataTypeSelect_currentIndexChanged(const QString &arg1);

    void on_refreshDateTime_clicked();

    void updateSensorTime();

    void on_confTabs_tabBarClicked(int index);

    void on_readApproachConfBtn_clicked();

    void on_refreshClassConfig_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *port;
    QThread *serialThread;
};

#endif // MAINWINDOW_H
