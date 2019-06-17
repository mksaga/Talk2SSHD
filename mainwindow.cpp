#include "commands.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <stdlib.h>

#include <QByteArray>
#include <QMessageBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QString>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set up necessary data
//    (void)Crc8Table;
//    (void)sensorId;
//    (void)memo;
//    (void)errString;

    // one-time CRC table generation
    SmCommsGenerateCrc8Table(Crc8Table, COMMS_CRC8_TABLE_LENGTH);
    errCode = 0;
    sensorId = 0x0168;
    resp.resize(90);
    writeResp.resize(20);

    lastReadConf = new sensor_config;
    lastWrittenConf = new sensor_config;
    lastReadDataConf = new sensor_data_config;

    // set up serial port (decl. in header file)
    port = new QSerialPort();

    // initialize list of serial port devices
    ui->comPortSelect->clear();
    if (QSerialPortInfo::availablePorts().isEmpty()) {
        ui->comPortSelect->addItem("No ports available");
    } else {
        // iterate over all available ports
        foreach(QSerialPortInfo p, QSerialPortInfo::availablePorts())
        {
            ui->comPortSelect->addItem(QString(p.portName() + " (" +
                                               p.description() + ")"));
        }
    }

    // set sensorID
    QString q = QString("<html><head/><body><p><span style=\" font-size:14pt; font-weight:600; color:#0055ff;\">%1</span></p></body></html>").arg(sensorId);
    ui->sensorId->setText(q);
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

    // sensorConfig pointers
    delete lastReadConf;
    delete lastWrittenConf;
    delete lastReadDataConf;

    delete serialThread;
}


// Updates the list of available serial port devices
void MainWindow::on_refreshComPorts_clicked()
{
    ui->comPortSelect->clear();
    if (QSerialPortInfo::availablePorts().isEmpty()) {
        ui->comPortSelect->addItem("No ports available");
    } else {
        // iterate over all available ports
        foreach(QSerialPortInfo p, QSerialPortInfo::availablePorts())
        {
            ui->comPortSelect->addItem(QString(p.portName() + " (" +
                                               p.description() + ")"));
        }
    }
}

/**
 * @brief MainWindow::refreshSensorConfig: Refreshes sensor configuration immediately after connecting.
 * @return : true if connection succeeded, false otherwise
 */
bool MainWindow::refreshSensorConfig()
{
    memo = genReadMsg(Crc8Table, 0x2A, 0, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode, 0x2A, 0);
    if (resp.at(0) == 'E') {
        // error message
        QMessageBox::critical(this, "Talk2SSHD", "Connection to sensor "
                                                 "failed. Please retry.");
        return false;
    }
    parse_gen_conf_read_response(resp, lastReadConf, errString);
    return true;
}

void MainWindow::on_connectToCom_clicked()
{
    // are we connecting or disconnecting?
    if (ui->connectToCom->text() == "Open Port")
    {
        // disable button
        ui->connectToCom->setEnabled(false);
        // set port name
        port->setPortName(ui->comPortSelect->currentText().left(4));

        // open port
        if (!port->open(QIODevice::ReadWrite))
        {
            // Error msg
            QString pN = port->portName();
            QString es = port->errorString();
            const QString e = QString("Failed to open port %1: %2").arg(pN).arg(es);
            QMessageBox::critical(this, "Talk2SSHD", e);
            // re-enable button
            ui->connectToCom->setEnabled(true);
            return;
        }
        // now that COM port connected, check if sensor is connected
        if (refreshSensorConfig()) {
            // update connection status text
            QString q = "<html><head/><body><p align=\"center\"><span style=\"font-size:9pt; font-weight:600; color:#0d9332;\">CONNECTED </span></p></body></html>";
            ui->comStatus->setText(q);
            // re-enable button, set it to Close
            ui->connectToCom->setEnabled(true);
            ui->connectToCom->setText("Close Port");


            // update config data section
            ui->sensorLocnEntry->setText(lastReadConf->location);
            ui->sensorDescEntry->setText(lastReadConf->description);
            q = lastReadConf->orientation;
            ui->sensorOrientation->setText(q);
            if (lastReadConf->units == 0) {
                ui->unitsMetric->setChecked(true);
                ui->unitsEnglish->setChecked(false);
            } else {
                ui->unitsMetric->setChecked(false);
                ui->unitsEnglish->setChecked(true);
            }
        } else {
            ui->connectToCom->setEnabled(true);
            port->close();
        }
    } else {
        if (port->isOpen())
        {
            port->close();
            QString q = "<html><head/><body><p align=\"center\"><span style=\"font-size:9pt; font-weight:600; color:#ff0000;\">DISCONNECTED </span></p></body></html>";
            ui->comStatus->setText(q);
            ui->connectToCom->setText("Open Port");
        }
    }
}

// DOESN'T WORK, FIX LATER
void MainWindow::on_writeSensorConfig_clicked()
{
    // orientation
    char orientationCh;
    QString orientn = ui->sensorOrientation->text();
    if (orientn.length() > 1 || orientn.length() == 0) {
        QMessageBox::critical(this, "Talk2SSHD", "Orientation is 1 character");
        return;

    } else {
        QByteArray b = orientn.toUtf8();
        char o = b.at(0);
        if ((o != 'N') && (o != 'E') && (o != 'S') && (o != 'W')) {
            QMessageBox::critical(this, "Talk2SSHD",
                                  "Orientation must be N, E, S, or W");
            return;
        } else {
            orientationCh = o;
        }
    }
    lastWrittenConf->orientation = orientationCh;

    // locn
    QString locn = ui->sensorLocnEntry->text();
    if (orientn.length() > 31) {
        QMessageBox::critical(this, "Talk2SSHD", "Location cannot exceed 32 chars");
        return;
    } else {
        lastWrittenConf->location = locn;
    }

    // descrn
    QString desc = ui->sensorDescEntry->text();
    if (desc.length() > 31) {
        QMessageBox::critical(this, "Talk2SSHD", "Description cannot exceed 32 chars");
        return;
    } else {
        lastWrittenConf->description = desc;
    }

    char units;
    if (ui->unitsEnglish->isChecked()) {
        units = 0;
    } else if (ui->unitsMetric->isChecked()){
        units = 1;
    } else {
        QMessageBox::critical(this, "Talk2SSHD", "Please select a unit format");
        return;
    }
    lastWrittenConf->units = units;

    memo = gen_config_write(Crc8Table, lastWrittenConf, sensorId);

    write_message_to_sensor(port, &memo, &writeResp, Crc8Table, &errCode, 0x2A, 1);
    refreshSensorConfig();
}


void MainWindow::on_loadDataConf_clicked()
{
    memo = genReadMsg(Crc8Table, 0x03, 0, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    parse_data_conf_read_response(&resp, lastReadDataConf, errString);
    ui->dataInterval->setValue(lastReadDataConf->data_interval);
    switch(lastReadDataConf->interval_mode) {
        case 0:
            ui->dataConfStorageDisabled->setChecked(true);
            ui->dataConfCircularMode->setChecked(false);
            ui->dataConfFillOnceMode->setChecked(false);
            break;
        case 1:
            ui->dataConfStorageDisabled->setChecked(false);
            ui->dataConfCircularMode->setChecked(true);
            ui->dataConfFillOnceMode->setChecked(false);
            break;
        case 2:
            ui->dataConfStorageDisabled->setChecked(false);
            ui->dataConfCircularMode->setChecked(false);
            ui->dataConfFillOnceMode->setChecked(true);
            break;
    }

    // update EventData config
    ui->dataTypeSelect->setCurrentIndex(1);
    ui->dataPortSelect->setCurrentIndex(1 + lastReadDataConf->e_portnum);

    switch(lastReadDataConf->e_format) {
        case 0: ui->dataFormatSelect->setCurrentIndex(1); break;
        case 1: ui->dataFormatSelect->setCurrentIndex(2); break;
        case 2: ui->dataFormatSelect->setCurrentIndex(3); break;
        case 6: ui->dataFormatSelect->setCurrentIndex(4); break;
        case 7: ui->dataFormatSelect->setCurrentIndex(5); break;
        case 8: ui->dataFormatSelect->setCurrentIndex(6); break;
        case 9: ui->dataFormatSelect->setCurrentIndex(7); break;
    }

    if (lastReadDataConf->e_pushen == 1) {
        ui->dataConfPushEnable->setChecked(true);
    } else {
        ui->dataConfPushEnable->setChecked(false);
    }
    QString q = QString("%1").arg(lastReadDataConf->e_destid);
    ui->dataConfDestID->setText(q);
    q = QString("%1").arg(lastReadDataConf->e_destsubid);
    ui->dataConfDestSubID->setText(q);

    double dec = ((lastReadDataConf->loop_sep) & 0x00ff) / 256.0;
    double intg = ((lastReadDataConf->loop_sep) & 0xff00) >> 8;
    intg += dec;
    q = QString("%1").arg(intg);
    ui->loopSep->setText(q);

    dec = ((lastReadDataConf->loop_size) & 0x00ff) / 256.0;
    intg = ((lastReadDataConf->loop_size) & 0xff00) >> 8;
    intg += dec;
    q = QString("%1").arg(intg);
    ui->loopSize->setText(q);
}
