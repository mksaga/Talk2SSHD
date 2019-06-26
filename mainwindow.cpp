#include "commands.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <stdlib.h>

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QString>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QString q;

    // set up serial port (decl. in header file)
    port = new QSerialPort();
    QTimer *tickTock = new QTimer();

    file = new QFile("RTDATA.txt");
    file->open(QIODevice::WriteOnly);

    serialWorker = new SerialWorker;
    serialWorker->setSerialPortPtr(port);
    serialWorker->setFilePtr(file);

//    serialThread = new QThread;

//    connect(tickTock,       SIGNAL(timeout()),
//            serialWorker,   SLOT(getNewSensorData()));

    serialWorker->setTimerPtr(tickTock);

    // delete worker once it's done
//    connect(serialThread, &QThread::finished, serialWorker, &QObject::deleteLater);

    // once data has finished writing, refresh the view
    connect(serialWorker, &SerialWorker::fileReadyForRead, this, &MainWindow::updateDataView);

    // one-time CRC table generation
    SmCommsGenerateCrc8Table(Crc8Table, COMMS_CRC8_TABLE_LENGTH);
    errCode = 0;
    sensorId = 0x0168;
    resp.resize(90);
    writeResp.resize(20);
    sensorClock = new QTimer(this);
    sensorClock->setTimerType(Qt::PreciseTimer);

    lastReadConf = new sensor_config;
    lastWrittenConf = new sensor_config;
    lastReadDataConf = new sensor_data_config;
    lastReadDateTime = new sensor_datetime;

    // memory allocations assume worst-case of max # approaches and lanes
    appr = new approach[4];

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
    q = QString("<html><head/><body><p><span style=\" font-size:14pt; font-weight:600; color:#0055ff;\">%1</span></p></body></html>").arg(sensorId);
    ui->sensorId->setText(q);

    // Enhances color of real-time data setup button
//    QPushButton *button = ui->writeDataSetup;
//    QPalette pal = button->palette();
//    pal.setColor(QPalette::Button, QColor(Qt::blue));
//    button->setAutoFillBackground(true);
//    button->setPalette(pal);
//    button->update();

    ui->dCDateTimeSelect->setCalendarPopup(true);
    ui->dCDateTimeSelect->setDateTime(QDateTime::currentDateTime());
}

MainWindow::~MainWindow()
{
    if (port->isOpen()) {
        port->close();
    }

//    serialThread->exit();
//    serialThread->wait();

    delete file;
    delete ui;
    delete port;

    // sensorConfig pointers
    delete[] appr;
    delete lastReadConf;
    delete lastReadDataConf;
    delete lastReadDateTime;
    delete lastWrittenConf;

//    delete serialThread;
}

bool isEqual (float f1, float f2)
{
    float epsilon = 0.01;
    if (f1 > f2) {
        return (f1-f2) <= epsilon;
    } else {
        return (f2-f1) <= epsilon;
    }
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
    serialWorker->writeMsgToSensor(&memo, &resp, Crc8Table, &errCode);
    if (resp.at(0) == 'E') {
        // error message
        QMessageBox::critical(this, "Talk2SSHD", "Connection to sensor "
                                                 "failed. Please retry.");
        return false;
    }
    parse_gen_conf_read_response(resp, lastReadConf, errString);
    lastWrittenConf->serial = lastReadConf->serial;
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
                ui->unitsMetric->setChecked(false);
                ui->unitsEnglish->setChecked(true);
            } else {
                ui->unitsMetric->setChecked(true);
                ui->unitsEnglish->setChecked(false);
            }

            refreshDataConfig();
            refreshActiveLanes();
            refreshApproachInfo();
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

    serialWorker->writeMsgToSensor(&memo, &writeResp, Crc8Table, &errCode);
    if (errCode == 0) { QMessageBox::information(this, "T2SSHD", "Success!"); }
    refreshSensorConfig();
}

void MainWindow::refreshDataConfig()
{
    if (!port->isOpen()) {
        QMessageBox::critical(this, "T2SSHD", "Error: not connected to sensor");
        return;
    }
    dataInfoRead = 1;
    memo = genReadMsg(Crc8Table, 0x03, 0, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    parse_data_conf_read_response(&resp, lastReadDataConf, errString);
    ui->dataIntervalEdit->setValue(lastReadDataConf->data_interval);
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

    memo = genReadMsg(Crc8Table, 0x0D, 0, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    if (parse_global_push_mode_read_resp(&resp, errString)) {
        ui->uartLocalPushMode->setChecked(true);
    } else {
        ui->uartLocalPushMode->setChecked(false);
    }
}

void MainWindow::on_loadDataConf_clicked()
{
    refreshDataConfig();
}

/**
 * @brief MainWindow::updateSensorTime : increments the time presented in the GUI
 */
void MainWindow::updateSensorTime()
{
    sensor_datetime *d = lastReadDateTime;
    char minChanged = 0;
    char hrChanged = 0;
    char dayChanged = 0;
    char monChanged = 0;
    char yrChanged = 0;

    if (d->secs == 59) {
        minChanged = 1;
        d->secs = 0;
        d->mins++;
        if (d->mins == 60) {
            hrChanged = 1;
            d->mins = 0;
            d->hrs++;
            if (d->hrs == 24) {
                dayChanged = 1;
                d->hrs = 0;
//                d->day++;
                if (d->mon % 2 == 0) {
                    if (d->mon == 2) {          // February (28/29)
                        if (d->day == 28) {
                            if (d->yr % 4 == 0) {
                                d->day++;
                            } else {
                                d->day = 1;
                                d->mon++;
                                monChanged = 1;
                            }
                        } else if (d->day == 29) {
                            d->day = 1;
                            d->mon = 3;
                        } else {
                            d->day++;
                        }
                    } else if (d->mon < 8) {    // April, June (30)
                        if (d->day == 30) {
                            d->day = 1;
                            d->mon++;
                        } else {
                            d->day++;
                        }
                    } else {                    // Aug, Oct, Dec (31)
                        if (d->day == 31) {
                            d->day = 1;
                            d->mon++;
                            if (d->mon == 13) {
                                d->mon = 1;
                                d->yr++;
                            }
                        } else {
                            d->day++;
                        }
                    }
                } else {
                    if (d->mon < 9) {   // Jan, Mar, May, July (31)
                        if (d->day == 31) {
                            d->day = 1;
                            d->mon++;
                        } else {
                            d->day++;
                        }
                    } else {            // Sep, Nov (30)
                        if (d->day == 30) {
                            d->day = 1;
                            d->mon++;
                        } else {
                            d->day++;
                        }
                    }
                }
            }
        }
        QString q = ui->sensorTime->text();
        // update seconds
        QString app = QString("%1").arg(lastReadDateTime->secs, 2, 10, QChar(48));
        q.replace(89, 2, app);
        // update mins
        if (minChanged) {
            app = QString("%1").arg(lastReadDateTime->mins, 2, 10, QChar(48));
            q.replace(86, 2, app);
        }
        if (hrChanged) {
            app = QString("%1").arg(lastReadDateTime->hrs, 2, 10, QChar(48));
            q.replace(83, 2, app);
        }
        ui->sensorTime->setText(q);
        if (dayChanged || monChanged || yrChanged) {
            QString q2 = ui->sensorDate->text();
            if (dayChanged) {
                app = QString("%1").arg(lastReadDateTime->day, 2, 10, QChar(48));
                q2.replace(86, 2, app);
            }
            if (monChanged) {
                app = QString("%1").arg(lastReadDateTime->mon, 2, 10, QChar(48));
                q2.replace(83, 2, app);
            }
            if (yrChanged) {
                app = QString("%1").arg(lastReadDateTime->yr, 4, 10, QChar(48));
                q2.replace(89, 4, app);
            }
            ui->sensorDate->setText(q2);
        }
    } else {
        // only update seconds
        d->secs++;
        QString q = ui->sensorTime->text();
        QString app = QString("%1").arg(d->secs, 2, 10, QChar(48));
        q.replace(89, 2, app);
        ui->sensorTime->setText(q);
    }
}

/**
 * @brief MainWindow::refreshDateTime : Sends new read_sensor_time message to the sensor
 */
void MainWindow::refreshDateTime()
{
    if (sensorClock->isActive()) {
        return;
    }
    if (!port->isOpen()) {
        QMessageBox::critical(this, "T2SSHD", "Error: not connected to sensor");
        return;
    }
    memo = genReadMsg(Crc8Table, 0x0E, 0, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    parse_sensor_time_read_resp(&resp, errString, lastReadDateTime);


    connect(sensorClock, SIGNAL(timeout()), this, SLOT(updateSensorTime()));
    sensorClock->start(1000);

    sensor_datetime *d = lastReadDateTime;
    QString q = QString("%1:%2:%3 UTC").arg(d->hrs, 2, 10, QChar(48)).arg(d->mins, 2, 10, QChar(48)).arg(d->secs, 2, 10, QChar(48));
    QString f = "<html><head/><body><p align=\"left\"><span style=\" font-size:12pt; font-weight:600;\">" + q +
            "</span></p></body></html>";
    ui->sensorTime->setText(f);
    q = QString("%1/%2/%3").arg(d->mon, 2, 10, QChar(48)).arg(d->day, 2, 10, QChar(48)).arg(d->yr, 2, 10, QChar(48));
    f = "<html><head/><body><p align=\"left\"><span style=\" font-size:12pt; font-weight:600;\">" + q +
            "</span></p></body></html>";
    ui->sensorDate->setText(f);
}

void MainWindow::refreshActiveLanes()
{
    if (!port->isOpen()) {
        QMessageBox::critical(this, "T2SSHD", "Error: not connected to sensor");
        return;
    }
    laneInfoRead = 1;
    memo = genReadMsg(Crc8Table, 0x27, 10, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    parse_active_lane_info_read_resp(&resp, laneArr, &numLanes, errString);
    if (errString.startsWith('E')) return;
    QString q = QString("<html><head/><body><p align=\"right\"><span style=\" font-size:12pt; font-weight:600;\">%1</span></p></body></html>").arg(numLanes);
    ui->numLanesConfigd->setText(q);

    for (int r = 0; r < numLanes; r++) {
        q = QString("Lane %1").arg(r + 1);
        if (laneGridInitialized == 0) {
            laneLabels[r][0] = new QLabel();
            laneLabels[r][1] = new QLabel();
            laneLabels[r][2] = new QLabel();
        }
        laneLabels[r][0]->setText(q);
        char *p = (laneArr + r)->description;
        QString q2 = QString::fromLocal8Bit(p, 8);
        laneLabels[r][1]->setText(q2);
        char c = (laneArr + r)->direction;
        laneLabels[r][2]->setText(QString(c));
        if (laneGridInitialized) {}
        else {
            if (r==0) {
                QLabel *t0 = new QLabel("Lane Number");
                QLabel *t1 = new QLabel("Lane Description");
                QLabel *t2 = new QLabel("Direction (R/L)");
                ui->laneGrid->addWidget(t0, 0, 0);
                ui->laneGrid->addWidget(t1, 0, 1);
                ui->laneGrid->addWidget(t2, 0, 2);
            }
            ui->laneGrid->addWidget(laneLabels[r][0], r+1, 0);
            ui->laneGrid->addWidget(laneLabels[r][1], r+1, 1);
            ui->laneGrid->addWidget(laneLabels[r][2], r+1, 2);
        }
    }
    laneGridInitialized = 1;

    /*
    QGridLayout *layout = ui->gridlayout;
    QLabel* m_boardLbArray[8][8];
    for(int row=0; row<8; row++)
      for(int col=0; col<8; col++)
      {
        m_boardLbArray[row][col] = new QLabel(this);
        m_boardLbArray[row][col]->setText(tr("This is row %1, col %2")
          .arg(row).arg(col));
        layout->addWidget(m_boardLbArray[row][col], row, col);
      }
    */

}

void MainWindow::refreshSpeedBins()
{
    if (!port->isOpen()) {
        QMessageBox::critical(this, "T2SSHD", "Error: not connected to sensor");
        return;
    }
    memo = genReadMsg(Crc8Table, 0x1D, 15, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    parse_speed_bin_conf_read(&resp, &numSpeedBins, speedBins, errString);
    if (errString.startsWith('E')) return;
    QString q = QString("<html><head/><body><p align=\"right\"><span style=\" font-size:12pt; font-weight:600;\">%1</span></p></body></html>").arg(numSpeedBins);
    ui->numSpeedBinsConfigd->setText(q);

    for (int r = 0; r < numSpeedBins; r++) {
        q = QString("Bin %1").arg(r + 1);
        if (speedBinGridInitialized == 0) {
            speedBinLabels[r][0] = new QLabel();
            speedBinLabels[r][1] = new QLabel();
        }
        speedBinLabels[r][0]->setText(q);
        QString q2;
        if (isEqual(*(speedBins + r), 255)) {
             q2 = QString("%1 (all other events)").arg(static_cast<double>(*(speedBins+r)));
        } else {
             q2 = QString("%1").arg(static_cast<double>(*(speedBins + r)));
        }
        speedBinLabels[r][1]->setText(q2);
        speedBinLabels[r][1]->setAlignment(Qt::AlignRight);

        if (speedBinGridInitialized) {}
        else {
            if (r==0) {
                QLabel *t0 = new QLabel("Bin Number");
                QLabel *t1 = new QLabel("Lower Bound Speed");
                t1->setAlignment(Qt::AlignCenter);
                ui->speedBinGrid->addWidget(t0, 0, 0);
                ui->speedBinGrid->addWidget(t1, 0, 1);
            }
            ui->speedBinGrid->addWidget(speedBinLabels[r][0], r+1, 0);
            ui->speedBinGrid->addWidget(speedBinLabels[r][1], r+1, 1);
        }
    }
    speedBinGridInitialized = 1;
}

void MainWindow::on_dataTypeSelect_currentIndexChanged(const QString &arg1)
{
    if (arg1.startsWith("Event")) {
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
    } else if (arg1.startsWith("Interval")) {
        ui->dataTypeSelect->setCurrentIndex(2);
        ui->dataPortSelect->setCurrentIndex(1 + lastReadDataConf->i_portnum);

        switch(lastReadDataConf->i_format) {
            case 0: ui->dataFormatSelect->setCurrentIndex(1); break;
            case 1: ui->dataFormatSelect->setCurrentIndex(2); break;
            case 2: ui->dataFormatSelect->setCurrentIndex(3); break;
            case 6: ui->dataFormatSelect->setCurrentIndex(4); break;
            case 7: ui->dataFormatSelect->setCurrentIndex(5); break;
            case 8: ui->dataFormatSelect->setCurrentIndex(6); break;
            case 9: ui->dataFormatSelect->setCurrentIndex(7); break;
        }

        if (lastReadDataConf->i_pushen == 1) {
            ui->dataConfPushEnable->setChecked(true);
        } else {
            ui->dataConfPushEnable->setChecked(false);
        }
        QString q = QString("%1").arg(lastReadDataConf->i_destid);
        ui->dataConfDestID->setText(q);
        q = QString("%1").arg(lastReadDataConf->i_destsubid);
        ui->dataConfDestSubID->setText(q);
    } else if (arg1.startsWith("Presence")) {
        ui->dataTypeSelect->setCurrentIndex(2);
        ui->dataPortSelect->setCurrentIndex(1 + lastReadDataConf->p_portnum);

        switch(lastReadDataConf->p_format) {
            case 0: ui->dataFormatSelect->setCurrentIndex(1); break;
            case 1: ui->dataFormatSelect->setCurrentIndex(2); break;
            case 2: ui->dataFormatSelect->setCurrentIndex(3); break;
            case 6: ui->dataFormatSelect->setCurrentIndex(4); break;
            case 7: ui->dataFormatSelect->setCurrentIndex(5); break;
            case 8: ui->dataFormatSelect->setCurrentIndex(6); break;
            case 9: ui->dataFormatSelect->setCurrentIndex(7); break;
        }

        if (lastReadDataConf->p_pushen == 1) {
            ui->dataConfPushEnable->setChecked(true);
        } else {
            ui->dataConfPushEnable->setChecked(false);
        }
        QString q = QString("%1").arg(lastReadDataConf->p_destid);
        ui->dataConfDestID->setText(q);
        q = QString("%1").arg(lastReadDataConf->p_destsubid);
        ui->dataConfDestSubID->setText(q);
    }
}

void MainWindow::on_refreshDateTime_clicked()
{
    refreshDateTime();
}

void MainWindow::on_confTabs_tabBarClicked(int index)
{
    if (index == 1) {
        refreshDateTime();
    } else if (index == 4) {
        refreshActiveLanes();
    } else if (index == 5) {
        refreshSpeedBins();
    }
}

void MainWindow::refreshApproachInfo()
{
    if (!port->isOpen()) {
        QMessageBox::critical(this, "T2SSHD", "Error: not connected to sensor");
        return;
    }
    approachInfoRead = 1;
    memo = genReadMsg(Crc8Table, 0x28, 4, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    numApproaches = parse_approach_info_read_resp(&resp, appr, errString);
    QString q = QString("<html><head/><body><p align=\"right\"><span style=\"font-size:12pt; font-weight:600;\">%1</span></p></body></html>").arg(numApproaches);
    ui->numApproachesConfigd->setText(q);
    int i;
    ui->approachSelect->clear();
    for (i=1; i<=numApproaches; i++) {
        q = QString("Approach %1").arg(i);
        ui->approachSelect->addItem(q);
    }
    // set up approach 1
    ui->approachSelect->setCurrentIndex(0);
    uint8_t nL = (*appr).numLanes;
    uint8_t *lanesAppr1 = (*appr).lanesAssigned;
    for (i=0; i<nL; i++) {
        int laneId = *(lanesAppr1 + i);
        switch(laneId) {
            case 0: ui->lane0_2->setChecked(true); break;
            case 1: ui->lane1->setChecked(true); break;
            case 2: ui->lane2->setChecked(true); break;
            case 3: ui->lane3->setChecked(true); break;
            case 4: ui->lane4->setChecked(true); break;
            case 5: ui->lane5->setChecked(true); break;
            case 6: ui->lane6->setChecked(true); break;
            case 7: ui->lane7->setChecked(true); break;
        }
    }
}

void MainWindow::on_readApproachConfBtn_clicked()
{
    refreshApproachInfo();
}

void MainWindow::on_refreshClassConfig_clicked()
{
    if (!port->isOpen()) {
        QMessageBox::critical(this, "T2SSHD", "Error: not connected to sensor");
        return;
    }
    memo = genReadMsg(Crc8Table, 0x13, 0, sensorId);
    write_message_to_sensor(port, &memo, &resp, Crc8Table, &errCode);
    parse_classif_read_resp(&resp, classBounds, &numClasses, errString);
    QString str = QString("<html><head/><body><p><span style=\" font-size:12pt; font-weight:600;\">%1</span></p></body></html>").arg(numClasses);
    ui->numClasses->setText(str);
    QString unitAbbr = "ft";

    int i;
    for (i=0; i<numClasses; i++) {
        QString q = QString("<html><head/><body><p><span style=\" font-size:10pt; font-weight:600;\">Class %1</span></p></body></html>").arg(i+1);
        QString q2= QString("<html><head/><body><p align=\"right\"><span style=\" font-size:10pt; font-weight:600;\">%1%2</span></p></body></html>").arg(*(classBounds + i)).arg(unitAbbr);
        QLabel *l = new QLabel(q);
        QLabel *l2 = new QLabel(q2);
        ui->classGrid->addRow(l, l2);
    }
}

bool MainWindow::validateIntervalDataSetup()
{
    if (!port->isOpen()) {
        QMessageBox::critical(this, "T2SSHD", "Error: not connected to sensor");
        return false;
    }
    QString q;
    bool isNum = false;
    int x;
    if (ui->dCSingleApprRadio->isChecked()) {
        q = ui->dCSingleApprNum->text();
        x = q.toInt(&isNum);
        if (isNum && (x<4 || x == 0xFF)) {
            if (approachInfoRead == 1) {
                if (x <= numApproaches) {
                    // validate time input
                } else {
                    q = QString("Only %1 approaches configured.").arg(numApproaches);
                    QMessageBox::critical(this, "Talk2SSHD", q);
                    return false;
                }
            } else {
                QMessageBox::critical(this, "Talk2SSHD", "Please refresh sensor "
                                                         "approach configuration first.");
                return false;
            }
        } else {
            QMessageBox::critical(this, "Talk2SSHD", "Approach number must be a number 1-4.");
            return false;
        }
    } else if (ui->dCSingleLaneRadio->isChecked()) {
        q = ui->dCSingleLaneNum->text();
        x = q.toInt(&isNum);
        if (isNum && (x<10 || x == 0xFF)) {
            if (laneInfoRead == 1) {
                if (x <= numLanes) {
                    // validate time input
                } else {
                    q = QString("Only %1 lanes configured.").arg(numLanes);
                    QMessageBox::critical(this, "Talk2SSHD", q);
                    return false;
                }
            } else {
                QMessageBox::critical(this, "Talk2SSHD", "Please refresh active "
                                                         "lane information first.");
                return false;
            }
        } else {
            QMessageBox::critical(this, "Talk2SSHD", "Lane number must be a number 1-10.");
            return false;
        }
    } else if (!ui->dCGetAllRadio->isChecked()) {
        QMessageBox::critical(this, "Talk2SSHD", "Choose a data type.");
        return false;
    }
    // validate timestamp
    QDateTime now = QDateTime::currentDateTime();
    QDateTime entry = ui->dCDateTimeSelect->dateTime();

    if (entry <= now) {
        return true;
    } else {
        QMessageBox::critical(this, "Talk2SSHD", "Hold up now! Time entered must be in the past, son.");
        return false;
    }
}

void MainWindow::on_writeDataSetup_clicked()
{
    if (validateIntervalDataSetup()) {
        int reqType = 3;
        // y : the lane/approach number for individual lane or appr
        int y = 4;
        bool isNum = false;

        // reqest type for single lane is 1, single appr is 2, all is 3
        if (ui->dCSingleLaneRadio->isChecked()) {
            reqType = 1;
            y = ui->dCSingleLaneNum->text().toInt(&isNum);
        } else if (ui->dCSingleApprRadio->isChecked()) {
            reqType = 2;
            y = ui->dCSingleApprNum->text().toInt(&isNum);
        }

        QString txt;
        QString tmp;
        if (reqType == 1) {
            if (y==0xFF) {
                tmp = QString("Get All Lanes");
            } else {
                tmp = QString("Get Single Lane (%1)").arg(y+1);
            }
        } else if (reqType == 2) {
            if (y==0xFF) {
                tmp = QString("Get All Approaches");
            } else {
                tmp = QString("Get Single Approach (%1)").arg(y+1);
            }
        }

        QMessageBox b;
        txt.append(tmp);
        tmp = QString("\nData Interval: %1 seconds\n").arg(dataInterval);
        txt.append(tmp);
        b.setText("Double check data retrieval parameters:");
        b.setInformativeText(txt);
        b.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        b.setDefaultButton(QMessageBox::Ok);
        int ret = b.exec();
        if (ret == QMessageBox::Ok) {
            serialWorker->startRealTimeDataRetrieval(reqType, y, Crc8Table,
                                                     lastReadDataConf,
                                                     sensorId, dataInterval,
                                                     numLanes, numApproaches,
                                                     &errCode);
        }
    } else {
        printf("Nope!\n");
    }
}

void MainWindow::on_refreshSensorConfig_clicked()
{
    refreshSensorConfig();
}

void MainWindow::on_dataIntrvlRTD_valueChanged(int arg1)
{
    if (arg1 > 0xFFFF) {
        QMessageBox::critical(this, "T2SSHD", "Error: max data interval is 65535 seconds");
    }
    dataInterval = static_cast<uint16_t>(arg1 & 0x0000FFFF);
    lastReadDataConf->data_interval = dataInterval;
}

void MainWindow::on_stopDataRetrieval_clicked()
{
    serialWorker->stopRealTimeDataRetrieval();
}

void MainWindow::updateDataView(QString dataLine)
{
    ui->dataView->appendPlainText(dataLine);
}
