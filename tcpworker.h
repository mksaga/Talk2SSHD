#ifndef TCPWORKER_H
#define TCPWORKER_H

#include <QDebug>
#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>

class TCPWorker : public QObject
{
    Q_OBJECT
public:
    explicit TCPWorker(QObject *parent = nullptr);
    bool startConnection();
    void closeConnection();
    void writeToSensor(QByteArray *msg, QByteArray *resp,
                       uint16_t *errBytes, qint64 len);

signals:

public slots:
private:
    QTcpSocket *sock;
    QHostAddress *dest;
    quint16 port;
    int timeout;
};

#endif // TCPWORKER_H
