#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QSerialPort>
#include <QTableWidgetItem>
#include <QThread>
#include <QTimer>

#include "tcpworker.h"
#include "sensor_utils.h"
#include "serialworker.h"

namespace Ui {
class MainWindow;
}

// To define whether sensor is connected via serial or IP
enum CXN_MODE { SERIAL, IP };

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
    int numLanes;

    double classBounds[8] = {0};
    lane laneArr[10];

    char laneGridInitialized = 0;
    QLabel* laneLabels[10][3];
    QLabel* speedBinLabels[15][2];

    float speedBins[15];
    char speedBinGridInitialized = 0;

    int numClasses;
    int numSpeedBins;
    int numDirectionBins;

    uint16_t dataInterval;

    char approachInfoRead = 0;
    char dataInfoRead = 0;
    char laneInfoRead = 0;

    sensor_config *lastReadConf;
    sensor_config *lastWrittenConf;
    sensor_data_config *lastReadDataConf;
    sensor_datetime *lastReadDateTime;
    approach *appr;

    bool sensorConnected;
    CXN_MODE connectionMode;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void refreshApproachInfo();
    void refreshActiveLanes();
    void refreshDataConfig();
    void refreshDateTime();
    bool refreshSensorConfig();
    void refreshSpeedBins();
    bool validateIntervalDataSetup();

    QFile *file;
    QSerialPort *port;
    QThread *serialThread;
    SerialWorker *serialWorker;
    TCPWorker *tcpWorker;
    Ui::MainWindow *ui;

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

    void updateDataView(QString dataLine);

    void on_writeDataSetup_clicked();
    void on_refreshSensorConfig_clicked();
    void on_dataIntrvlRTD_valueChanged(int arg1);
    void on_stopDataRetrieval_clicked();
    void on_connectViaCom_clicked();
    void on_connectViaIp_clicked();
};

#endif // MAINWINDOW_H
