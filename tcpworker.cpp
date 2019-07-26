#include "tcpworker.h"

#define GATEWAY_IP "166.153.62.218"

TCPWorker::TCPWorker(QObject *parent) : QObject(parent)
{
    (void)parent;
    socketConnected = false;
    dataRetrievalClicked = false;
}

void TCPWorker::setTimerPtr(QTimer *t)
{
    dataTimer = t;
}

// Should I implement the setDest and setPort methods?

bool TCPWorker::startConnection(QString addr, int p)
{
    sock = new QTcpSocket(this);
    dest = new QHostAddress(addr);
    port = p;
    timeout = 2000;

    sock->connectToHost(*dest, port);

    if (sock->waitForConnected(timeout)) {
        socketConnected = true;
        return true;
    }
    return false;
}

bool TCPWorker::getConnectionStatus()
{
    return socketConnected;
}

void TCPWorker::writeToSensor(QByteArray *msg, QByteArray *resp,
                              uint16_t *errBytes, qint64 len)
{
    QByteArray head;
    uint16_t bytesExpected = 10;

    // 1: write on the socket
    sock->write(*msg, len);

    if (sock->waitForBytesWritten(timeout)) {
        qDebug() << "Message written on socket.";

        if (sock->waitForReadyRead(timeout)) {
            qDebug() << "Data ready to receive";

            while (sock->bytesAvailable() < bytesExpected) {
                sock->waitForReadyRead(5);
            }

            // only read first 10 bytes of sensor response
            head = sock->read(10);
            bytesExpected = head.at(9);
            bytesExpected &= 0x00FF;

            while (sock->bytesAvailable() < bytesExpected) {
                sock->waitForReadyRead(5);
            }

            // we read 10 bytes of header, plus 2 bytes for checksum
            resp->resize(13 + bytesExpected);
            *resp = sock->readAll();
            resp->prepend(head);

            // fill error bytes if result response (Msg Type 2)
            if (resp->at(13) == 2) {
                uint16_t t1, t2;
                t1 = (resp->at(14)) << 8;
                t2 = (resp->at(15)) & 0x00FF;
                *errBytes = (t1 | t2);
            }
            qDebug() << "Data reception complete.";
        } else {
            qDebug() << "Read timed out.";
        }
    } else {
        qDebug() << "Write timed out.";
    }
}

void TCPWorker::setFilePtr(QFile * f)
{
    dataFile = f;
    retrievedDataStream = new QTextStream(f);
}

void TCPWorker::startRealTimeDataRetrieval(uint8_t reqType, uint8_t lAN,
                                           uint8_t *Crc8Table,
                                           sensor_data_config *sDC,
                                           uint16_t sensorId,
                                           uint16_t dataInterval, uint8_t nL, uint8_t nA,
                                           uint16_t *errBytes)
{
    QByteArray resp(128, '*');
    QDateTime dt;

    laneApprNum = lAN;

    *errBytes = 0;

    // first, update data configuration
    message = gen_data_conf_write(Crc8Table, sDC, sensorId);
    int len = message.size();
    writeToSensor(&message, &resp, errBytes, len);

    // next, check for bins
    // length (classification)
    message = genReadMsg(Crc8Table, 0x13, 0, sensorId);
    len = message.size();
    writeToSensor(&message, &resp, errBytes, len);
    if (resp.size() > 10) {
        // compute based on payload size
        numClasses = (resp.at(9) - 3) / 2;
    }

    // speed bins
    message = genReadMsg(Crc8Table, 0x1D, 0, sensorId);
    len = message.size();
    writeToSensor(&message, &resp, errBytes, len);
    if (resp.size() > 13) {
        numSpeedBins = (resp.at(9) - 3) / 2;
        numSpeedBins = resp.at(12);
    }

    if (*errBytes == 0) {
        dt = QDateTime::currentDateTimeUtc();
        printf("Data interval: %u\n", dataInterval);
        printf("Start DA RETreival!\n");
        requestType = reqType;
        numLanes = nL;
        numApprs = nA;

        if (dataFile->exists()) {
            printf("File exists.\n");
            if (dataFile->isOpen()) {
                printf("File is already open.\n");
            } else if (dataFile->open(QIODevice::WriteOnly)) {
                printf("File opened.\n");
            } else {
                printf("Couldn't open file.\n");
                return;
            }
        } else {
            printf("File does not exist.\n");
        }

        // set up header row of the data file
        QString headerLine;
        QString spacer = "    ";
        QString t;

        (*retrievedDataStream) << qSetFieldWidth(25) << center << "Datetime";
        t = QString("%1").arg("Datetime" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << qSetFieldWidth(5) << "Interval Duration" << spacer;
        t = QString("%1").arg("Interval Duration" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << "Total # Lanes/Apprs" << spacer;
        t = QString("%1").arg("Total # Lanes/Apprs" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << "Avg Speed" << spacer;
        t = QString("%1").arg("Avg Speed" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << "Volume" << spacer;
        t = QString("%1").arg("Volume" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << "Avg Occupancy" << spacer;
        t = QString("%1").arg("Avg Occupancy" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << "85th Pctle Speed" << spacer;
        t = QString("%1").arg("85th Pctle Speed" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << "Headway (ms)" << spacer;
        t = QString("%1").arg("Headway (ms)" + spacer);
        headerLine.append(t);

        (*retrievedDataStream) << "Gap (ms)" << spacer;
        t = QString("%1").arg("Gap (ms)" + spacer);
        headerLine.append(t);

        int i;
        for (i=0; i<numClasses && i<10; i++) {
            (*retrievedDataStream) << qSetFieldWidth(11) << "Length Bin "
                                   << qSetFieldWidth(2) << i;
            t = QString("%1").arg("Length Bin " + QString('1' + i));
            t.append(spacer);
            headerLine.append(t);
        }
        (*retrievedDataStream) << spacer;
        for (i=0; i<numSpeedBins; i++) {
            (*retrievedDataStream) << "Speed Bin " << i;
            t = QString("%1").arg("Speed Bin " + QString('1' + i));
            t.append(spacer);
            headerLine.append(t);
        }
        (*retrievedDataStream) << "\n";
        headerLine.append("\n");


        message = getVarSizeIntervalDataByTimestamp(Crc8Table, reqType, sensorId,
                                                0, 0, dt, laneApprNum);

        if (!dataRetrievalClicked) {
            connect(dataTimer, &QTimer::timeout, this, &TCPWorker::getNewSensorData,
                Qt::DirectConnection);
            emit fileReadyForRead(headerLine);
            dataRetrievalClicked = true;
        }
        dataTimer->start(dataInterval * 1000);
    }
}

void TCPWorker::getNewSensorData()
{
    uint8_t seqNumber = 0;
    (void)seqNumber;
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

    int packetNumber;
    (void)packetNumber;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t tmpHi;
    uint8_t tmpLo;
    uint8_t hrs;
    uint8_t mins;
    uint8_t secs;
    uint16_t ms;
    (void)ms;
    uint16_t intervalDuration;
    double avgSpeed;
    uint32_t volume;
    double avgOccupancy;
    double eightyFifthPctlSpeed;
    uint32_t headway;
    uint32_t gap;

    QString newDataLine;

    sock->write(message);
    while (!sock->waitForBytesWritten(waitTimeout));

    for (i=0; ( (i<loopLimit) && (!intervalNotPresent) ); i++) {

            // is *some* data ready?
            if (sock->waitForReadyRead(waitTimeout)) {

                // have we received everything we're looking for?
                while (sock->bytesAvailable() < dataExpected) {
                    sock->waitForReadyRead(5);
                }
                head = sock->read(10);
                dataExpected = head.at(9);
                dataExpected &= 0x00FF;
                while (sock->bytesAvailable() < dataExpected + 2) {
                    sock->waitForReadyRead();
                }
                resp = sock->read(dataExpected+2);
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
            if (errorCode == 0x000F) {
                intervalNotPresent = true;
                i = loopLimit;
                newDataLine = "No New Data";
                emit fileReadyForRead(newDataLine);
                break;
            } else {

            // extract date and time

            if (resp.size() < 23) { return; }

            // sensor date starts at byte indexed 14

            int indexNumPos = 14;
            int locn = indexNumPos + 3;


            // lane/approach number: byte 17
            packetNumber = resp.at(locn);
            locn++;

            // extract year
            uint8_t y1 = resp.at(locn); locn++;
            uint8_t y2 = resp.at(locn); locn++;


            // fill up lower 8 bits
            if (y1 & 0x1) {
                // bit @ index 7 should be 1
                tmpLo = (y2 >> 1) | 0x80;
            } else {
                // bit @ index 7 should be 0
                tmpLo = (y2 >> 1) & 0x7F;
            }
            // upper 8 bits: index 1-5 from upper byte
            tmpHi = (y1 & 0x1E) >> 1;
            year = (tmpHi << 8) | ((tmpLo & 0x00FF));

            // get lower 3 bits of month
            month = (resp.at(locn)) >> 5; locn++;
            if (y2 & 0x1) {
                // MSB is 1
                month |= 0x8;
            } else {
                month &= 0x7;
            }

            // extract day: lower 5 bits
            day = (resp.at(locn)) & 0x1F; locn++;

            tmpLo = (resp.at(19) & 0xC0) >> 6;
            tmpHi = (resp.at(18) & 0x07) << 2;
            hrs = (tmpHi | tmpLo);

            mins = (resp.at(19) & 0x3F);
            secs = (resp.at(20) & 0xFC) >> 2;

            // milliseconds
            tmpHi = (resp.at(20) & 0x3);
            ms = (tmpHi << 8) | (resp.at(21) & 0x00FF);

            (*retrievedDataStream).setPadChar('0');
            (*retrievedDataStream) << qSetFieldWidth(2) << month << "/" << day << "/" <<
                                      qSetFieldWidth(4) << year << " " <<
                                      qSetFieldWidth(2) << hrs << ":" << mins <<
                                      ":" << secs << " ";
            QChar z = QChar(48);
            QString q = QString("%1/%2/%3 %4:%5:%6").arg(month, 2, 10, z).arg(day, 2, 10, z).arg(year, 4).arg(hrs, 2, 10, z).arg(mins, 2, 10, z).arg(secs, 2, 10, z);
            newDataLine.append(q);

            tmpHi = resp.at(locn); locn++;
            tmpLo = resp.at(locn); locn++;

            intervalDuration = (static_cast<uint16_t>((tmpHi << 8) & 0xFF00));
            intervalDuration |= ( (static_cast<uint16_t>(tmpLo)) & 0x00FF);

            (*retrievedDataStream) << qSetFieldWidth(6) << intervalDuration;
            q = QString("%1").arg(intervalDuration, 6);
            newDataLine.append(q);

            // num lanes & approaches configured, respectively
            (*retrievedDataStream)  << qSetFieldWidth(2) << resp.at(locn);
            q = QString("%1").arg(resp.at(locn), 2);
            newDataLine.append(q);
            locn++;
            (*retrievedDataStream) << resp.at(locn);
            q = QString("%1").arg(resp.at(locn), 2);
            newDataLine.append(q);
            locn++;

            // avg speed
            avgSpeed = doubleFrom24BitFixedPt(&resp, locn);
            (*retrievedDataStream) << qSetFieldWidth(6) << avgSpeed;
            q = QString("%1").arg(avgSpeed, 6);
            newDataLine.append(q);
            locn += 3;

            // volume (3 bytes)
            volume = static_cast<uint32_t>( (resp.at(locn) << 24) & 0xFF0000);
            locn++;
            volume |= static_cast<uint32_t>( (resp.at(locn) << 16) & 0x00FF00);
            locn++;
            volume |= static_cast<uint32_t>( (resp.at(locn) ) & 0x0000FF);
            locn++;

            (*retrievedDataStream) << qSetFieldWidth(6) << volume;
            q = QString("%1").arg(volume, 6);
            newDataLine.append(q);

            avgOccupancy = static_cast<double>(fixedPtToFloat(extract16BitFixedPt(&resp, locn)));
            locn += 2;
            (*retrievedDataStream) << qSetFieldWidth(5) << avgOccupancy;
            q = QString("%1").arg(avgOccupancy, 5);
            newDataLine.append(q);

            eightyFifthPctlSpeed = doubleFrom24BitFixedPt(&resp, locn);
            (*retrievedDataStream) << qSetFieldWidth(4) << eightyFifthPctlSpeed;
            q = QString("%1").arg(eightyFifthPctlSpeed, 4);
            newDataLine.append(q);
            locn += 3;

            // headway (3 bytes)
            headway = static_cast<uint32_t>( (resp.at(locn) << 24) & 0xFF0000);
            locn++;
            headway |= static_cast<uint32_t>( (resp.at(locn) << 16) & 0x00FF00);
            locn++;
            headway |= static_cast<uint32_t>( (resp.at(locn) ) & 0x0000FF);
            locn++;

            (*retrievedDataStream) << headway;
            q = QString("%1").arg(headway);
            newDataLine.append(q);

            // gap (3 bytes)
            gap = static_cast<uint32_t>( (resp.at(locn) << 24) & 0xFF0000);
            locn++;
            gap |= static_cast<uint32_t>( (resp.at(locn) << 16) & 0x00FF00);
            locn++;
            gap |= static_cast<uint32_t>( (resp.at(locn) ) & 0x0000FF);
            locn++;

            (*retrievedDataStream) << gap;
            q = QString("%1").arg(gap);
            newDataLine.append(q);

            if (locn == dataExpected + 10) {
                return;
            } else {
                while (locn < dataExpected) {
                    char binType = resp.at(locn);
                    (void)binType;
                    char numBins;
                    locn++;
                    numBins = resp.at(locn);
                    for (int j=0; j<numBins; j++) {
                        uint32_t count;
                        count = static_cast<uint32_t>( (resp.at(locn) << 24) & 0xFF0000);
                        locn++;
                        count |= static_cast<uint32_t>( (resp.at(locn) << 16) & 0x00FF00);
                        locn++;
                        count |= static_cast<uint32_t>( (resp.at(locn) ) & 0x0000FF);
                        locn++;

                        (*retrievedDataStream) << count;
                        q = QString("%1").arg(count);
                        newDataLine.append(q);
                    }
                }
            }


            (*retrievedDataStream) << "\n\n";
            newDataLine.append("\n\n");
            emit fileReadyForRead(newDataLine);
        }
    }
}

void TCPWorker::stopRealTimeDataRetrieval()
{
    dataFile->close();
    if (dataTimer->isActive()) {
        dataTimer->stop();
    }
}

void TCPWorker::closeConnection()
{
    sock->close();
    delete dest;
    delete sock;
}
