#include "tcpworker.h"

#define GATEWAY_IP "166.153.62.218"

TCPWorker::TCPWorker(QObject *parent) : QObject(parent)
{
    (void)parent;
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
        return true;
    }
    return false;
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

void TCPWorker::closeConnection()
{
    sock->close();
    delete dest;
    delete sock;
}
