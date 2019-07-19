#ifndef MYTCP_H
#define MYTCP_H

#include <QDebug>
#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>

class MyTCP : public QObject
{
    Q_OBJECT

public:
    explicit MyTCP(QObject *parent = nullptr);
    void doConnect(QByteArray *b);

signals:
public slots:
private:
    QTcpSocket *sock;
    QHostAddress *dest;
    int timeout;
};

#endif // MYTCP_H
