#include "mytcp.h"

#define GATEWAY_IP "166.153.62.218"
#define GATEWAY_PORT 10001

MyTCP::MyTCP(QObject *parent)
{
    (void)parent;
}

void MyTCP::doConnect(QByteArray *b)
{
    sock = new QTcpSocket(this);
    dest = new QHostAddress(GATEWAY_IP);
    timeout = 2000;

    sock->connectToHost(*dest, GATEWAY_PORT);

    if (sock->waitForConnected(timeout)) {
        qDebug() << "connected!";
        sock->write(*b, 15);

        if (sock->waitForBytesWritten(timeout)) {
            qDebug() << "bytes written. Aye aye.";
            if (sock->waitForReadyRead(timeout)) {

                qDebug() << "Got data to read. Arr.";
                qDebug() << "reading:" << sock->bytesAvailable();
                QByteArray receivedData = sock->readAll();
                int recvSize = receivedData.size();

                unsigned short i = 0;
                char msgStarted = 0;
                char lastChar = 0;

                while (msgStarted == 0) {
                    if (i >= recvSize) {
                        qDebug() << "A mortifying error. Abort ship.";
                        return;
                    }
                    char tempCh = receivedData.at(i);
                    if (lastChar == 0x5A) {
                        if (tempCh == 0x31) {
                            msgStarted = 1;
                        } else {
                            lastChar = tempCh;
                        }
                    } else {
                        lastChar = tempCh;
                        i++;
                    }
                }

                if (i > recvSize) { qDebug() << "Something horribly wrong."; return; }
                QByteArray cleanedData = receivedData.right(recvSize-i+1);

                for (i=0; i<cleanedData.size(); i++) {
                    printf("%02X ", cleanedData.at(i));
                }
                printf("\n");

            } else {
                qDebug() << "no data to read. Nyet nyet.";
            }
        } else {
            qDebug() << "couldn't write. Nyet nyet.";
        }

        qDebug() << "closing socket";
        sock->close();
    } else {
        qDebug() << "yeah, you're not connected";
    }

    delete dest;
    delete sock;
}
