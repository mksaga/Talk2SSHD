#include "serialworker.h"
#include <commands.h>

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QTimer>

SerialWorker::SerialWorker(QObject *parent) : QObject(parent)
{

}

void SerialWorker::setSerialPortPtr(QSerialPort *serialPtr)
{
    serialPort = serialPtr;
}

void SerialWorker::writeMsgToSensor(QByteArray *msg,
                                    QByteArray *response,
                                    uint8_t *Crc8Table,
                                    uint16_t *err_bytes)
{
    (void)Crc8Table;
    serialPort->clear(QSerialPort::Input);

    // how much data are we waiting for?
    uint16_t dataExpected = 10;
    int waitTimeout = 3000;
    QByteArray head;

    // send message
    serialPort->write(*msg);

    // wait for data to finish writing
    if (serialPort->waitForBytesWritten(waitTimeout)) {
        printf("wrote %02X\t", msg->at(11));

        // is there *some* new data ready?
        if (serialPort->waitForReadyRead(waitTimeout)) {

            // have we received all the data yet?
            while (serialPort->bytesAvailable() < dataExpected) {
                serialPort->waitForReadyRead(5);
            }

            // let's just use payload size for ALL commands
            head = serialPort->read(10);
            dataExpected = head.at(9);
            dataExpected &= 0x00FF;
            printf("payload size: %02X\n", dataExpected);

            // dataExpected = body size (not including CRCs)
            while (serialPort->bytesAvailable() < dataExpected + 2) {
                serialPort->waitForReadyRead(5);
            }
            *response = serialPort->readAll();
            (void)*response->prepend(head);

            // only fill error bytes for result responses
            if (response->at(13) == 2) {
                uint16_t temp1;
                uint16_t temp2;
                temp1 = (response->at(14)) << 8;
                temp2 = (response->at(15)) & 0x00FF;
                *err_bytes = (temp1 | temp2);
            }
            emit cmdResponseComplete();
        } else {
            *response = "Error: read timed out";
        }
    } else {
        *response = "Error: write timed out";
    }
}

void SerialWorker::startRealTimeDataRetrieval(uint8_t reqType, uint8_t laneApprNum,
                                uint8_t *Crc8Table,
                                sensor_data_config *sDC,
                                uint16_t sensorId,
                                uint16_t dataInterval, uint8_t nL, uint8_t nA,
                                uint16_t *errBytes)
{
    QByteArray msg;
    QByteArray resp(128, '*');
    QDateTime dt;

    *errBytes = 0;

    // first, update data configuration
    msg = gen_data_conf_write(Crc8Table, sDC, sensorId);
    writeMsgToSensor(&msg, &resp, Crc8Table, errBytes);

    // next, check for length, speed, and direction bins

    // length (classification)
    msg = genReadMsg(Crc8Table, 0x13, 0, sensorId);
    writeMsgToSensor(&msg, &resp, Crc8Table, errBytes);
    if (resp.size() > 10) { numClasses = (resp.at(9) - 3) / 2; }

    // speed bins
    msg = genReadMsg(Crc8Table, 0x1D, 0, sensorId);
    writeMsgToSensor(&msg, &resp, Crc8Table, errBytes);
    if (resp.size() > 13) {
        numSpeedBins = (resp.at(9) - 3) / 2;
        numSpeedBins = resp.at(12);
    }

    // direction bins
    // to be completed (awaiting Wavetronix response)
    numDirectionBins = 0;

    if (*errBytes == 0) {
        dt = QDateTime::currentDateTimeUtc();
        printf("Data interval: %u\n", dataInterval);
        printf("Start DA RETreival!\n");
        requestType = reqType;
        numLanes = nL;
        numApprs = nA;
        const int x = nL+nA;
        realTimeDataNibble realTimeDataVals[x];

        msg = getVarSizeIntervalDataByTimestamp(Crc8Table, reqType, sensorId,
                                                0, 0, dt, laneApprNum);
        QTimer *pollNewData = new QTimer(this);
        connect(pollNewData, SIGNAL(timeout()),
                this, SLOT(getNewSensorData(&msg, laneApprNum, realTimeDataVals)));
        pollNewData->start(dataInterval * 1000);

    }
}

void SerialWorker::getNewSensorData(QByteArray *msg, uint8_t laneApprNum,
                                    realTimeDataNibble *dataTable)
{
    uint8_t seqNumber = 0;
    uint16_t errorCode = 0x0000;
    QByteArray resp(128, '*');
    QDateTime dt;

    errorCode = 0;

    int loopLimit = 0;
    if ( (requestType == 1) || (requestType == 2) ) {
        if (laneApprNum == 0xFF) {
            requestType == 1 ? loopLimit = numLanes : loopLimit = numApprs;
        } else {
            loopLimit = 1;
        }
    } else {
        // total number of lanes AND approaches
        loopLimit = numLanes + numApprs;
    }

    uint16_t dataExpected = 10;
    int waitTimeout = 3000;
    QByteArray head;
    bool intervalNotPresent = false;
    int i;

    int totalBinCount = numClasses + numSpeedBins + numDirectionBins;
    (void)totalBinCount;

    serialPort->clear(QSerialPort::Input);
    serialPort->write(*msg);
    while (!serialPort->waitForBytesWritten(waitTimeout));

    for (i=0; ( (i<loopLimit) && (!intervalNotPresent) ); i++) {

            // is *some* data ready?
            if (serialPort->waitForReadyRead(waitTimeout)) {

                // have we received everything we're looking for?
                while (serialPort->bytesAvailable() < dataExpected) {
                    serialPort->waitForReadyRead(5);
                }
                head = serialPort->read(10);
                dataExpected = head.at(9);
                dataExpected &= 0x00FF;
                while (serialPort->bytesAvailable() < dataExpected + 2) {
                    serialPort->waitForReadyRead();
                }
                resp = serialPort->read(dataExpected+2);
                (void)resp.prepend(head);

                if (resp.size() >= 13) {
                    // either this is a result response,
                    // or payload size = 5
                    if (resp.at(13) == 2 || resp.at(9) == 5) {
                        uint16_t temp1;
                        uint16_t temp2;
                        temp1 = (resp.at(14)) << 8;
                        temp2 = (resp.at(15)) & 0x00FF;
                        errorCode = (temp1 | temp2);
                    }
                }
            }
            if (errorCode == 0x000F) { intervalNotPresent = true; break; }

            // process response array
            realTimeDataNibble *dN = dataTable + i;

            // extract date and time



        }
}
