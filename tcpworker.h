#ifndef TCPWORKER_H
#define TCPWORKER_H

#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include "commands.h"
#include "sensor_utils.h"

class TCPWorker : public QObject
{
    Q_OBJECT
public:
    explicit TCPWorker(QObject *parent = nullptr);
    void closeConnection();
    bool getConnectionStatus();
    void setDest(QString addr);
    void setFilePtr(QFile*);
    void setPort(int port);
    void setTimerPtr(QTimer*);
    bool startConnection(QString addr, int port);
    void startRealTimeDataRetrieval(uint8_t reqType, uint8_t lAN,
                                    uint8_t *Crc8Table,
                                    sensor_data_config *sDC,
                                    uint16_t sensorId,
                                    uint16_t dataInterval, uint8_t nL, uint8_t nA,
                                    uint16_t *errBytes);
    void stopRealTimeDataRetrieval();
    void writeToSensor(QByteArray *msg, QByteArray *resp,
                       uint16_t *errBytes, qint64 len);

private:
    QTcpSocket *sock;
    QHostAddress *dest;
    quint16 port;
    bool socketConnected;
    bool dataRetrievalClicked;

    int timeout;

    int numClasses;
    int numDirectionBins;
    int numSpeedBins;
    uint8_t laneApprNum;
    uint8_t requestType;
    uint8_t numLanes;
    uint8_t numApprs;
    QByteArray message;
    QFile *dataFile;
    QString dataLine;
    QTextStream *retrievedDataStream;
    QTimer *dataTimer;

public slots:
    void getNewSensorData();

signals:
    void fileReadyForRead(QString s);
};

#endif // TCPWORKER_H
