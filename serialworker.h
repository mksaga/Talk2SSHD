#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>


#include <sensor_utils.h>

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    void setSerialPortPtr(QSerialPort*);
    void writeMsgToSensor(QByteArray *msg, QByteArray *resp,
                          uint8_t *Crc8Table,
                          uint16_t *errorBytes);
    void startRealTimeDataRetrieval(uint8_t reqType, uint8_t laneApprNum,
                                    uint8_t *Crc8Table,
                                    sensor_data_config *sDC,
                                    uint16_t sensorId,
                                    uint16_t dataInterval,
                                    uint8_t numLanes,
                                    uint8_t numApproaches,
                                    uint16_t *errBytes);

private:
    QSerialPort *serialPort;
    uint8_t requestType;
    uint8_t numLanes;
    uint8_t numApprs;
    int numClasses;
    int numSpeedBins;
    int numDirectionBins;

signals:
    void WorkerReady(const QString &result);
    void cmdResponseComplete();
    void iVCCS_ConfigSuccess();
    void iVCCS_CMD_ID(const QString &data);
    void iVCCS_CMD_LISTFILES(const QString &data);
    void iVCCS_CMD_DLFILE(const double &precentage);

public slots:
    void ReceiveData(void);
    void getNewSensorData(QByteArray *msg, uint8_t laneApprNum,
                          realTimeDataNibble *table);
};

#endif // SERIALWORKER_H
