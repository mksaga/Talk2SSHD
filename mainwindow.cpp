#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <stdlib.h>

#include <QByteArray>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QString>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set up necessary data
    uint8_t Crc8Table;
    uint16_t sensorId;
    QByteArray message;
    QString errorString;

    // set up serial port (decl. in header file)
    port = new QSerialPort();
}

MainWindow::~MainWindow()
{
    if (port->isOpen()) {
        port->close();
    }
    serialThread->exit();
    serialThread->wait();

    delete ui;
    delete port;
    delete serialThread;
}


// Updates the list of available serial port devices
void MainWindow::on_refreshComPorts_clicked()
{
    ui->comPortSelect->clear();
    // iterate over all available ports
    foreach(QSerialPortInfo p, QSerialPortInfo::availablePorts())
    {
        ui->comPortSelect->addItem(QString(p.portName() + " (" +
                                           p.description() + ")"));
    }
}
