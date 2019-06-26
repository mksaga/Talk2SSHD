#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QFile>
#include <QObject>
#include <QSerialPort>
#include <QTextStream>
#include <QTimer>


#include <sensor_utils.h>

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    ~SerialWorker();
    explicit SerialWorker(QObject *parent = nullptr);
    void setSerialPortPtr(QSerialPort*);
    void setFilePtr(QFile*);
    void setTimerPtr(QTimer *t);
    void startRealTimeDataRetrieval(uint8_t reqType, uint8_t laneApprNum,
                                    uint8_t *Crc8Table,
                                    sensor_data_config *sDC,
                                    uint16_t sensorId,
                                    uint16_t dataInterval,
                                    uint8_t numLanes,
                                    uint8_t numApproaches,
                                    uint16_t *errBytes);
    void stopRealTimeDataRetrieval();
    void writeMsgToSensor(QByteArray *msg, QByteArray *resp,
                          uint8_t *Crc8Table,
                          uint16_t *errorBytes);

private:
    int numClasses;
    int numDirectionBins;
    int numSpeedBins;
    uint8_t laneApprNum;
    uint8_t requestType;
    uint8_t numLanes;
    uint8_t numApprs;
    QByteArray message;
    QFile *dataFile;
    QSerialPort *serialPort;
    QString dataLine;
    QTextStream *retrievedDataStream;
    QTimer *dataTimer;

signals:
    void cmdResponseComplete();
    void fileReadyForRead(QString s);

public slots:
    void getNewSensorData();
};

#endif // SERIALWORKER_H
