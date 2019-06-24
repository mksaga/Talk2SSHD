#include <stdlib.h>

#include "commands.h"
#include "sensor_utils.h"

#include <QTimer>

using namespace std;
const char zero = 0;

/**
 * @brief interpretErrorCode: Returns description for an error code
 * @param errorBytes: 16-bit field containing the error bytes from response
 * @return
 */
QString interpretErrorCode(uint16_t errorBytes)
{
    QByteArray output;

    switch(errorBytes)
    {
        case 0x0000:    output = "No error";
                        break;
        case 0x0001:    output = "Payload size incorrect";
                        break;
        case 0x0002:    output = "Body CRC did not match msg";
                        break;
        case 0x0003:    output = "Write flag set on read-only msg";
                        break;
        case 0x000B:    output = "Config save to non-vol mem failed";
                        break;
        case 0x000F:    output = "Interval req. in data req. doesn't exist in data";
                        break;
        case 0x0010:    output = "Lane req. in data request doesn't exist in data";
                        break;
        case 0x0011:    output = "Interv. data cannot be retrieved, FLASH busy";
                        break;
        case 0x0013:    output = "Attempted to set push config to invalid state";
                        break;
        case 0x0014:    output = "Error setting RTC time";
                        break;
    }

    return output;
}

/**
 * @brief write_message_to_sensor: The big boi. The crown jewel. The printf of this library. A honking function that writes a byte array over serial to the sensor.
 * @param port: pointer to the sensor QSerialPort
 * @param msg: pointer to the message QByteArr
 * @param response: pointer to the response QByteArr to fill
 * @param Crc8Table
 * @param err_bytes
 * @param msg_id: needed to determine how much data to wait for
 * @param msg_type: also needed to gauge amount of data to wait for
 */
void write_message_to_sensor(QSerialPort *port, QByteArray *msg,
                             QByteArray *response, uint8_t *Crc8Table,
                             uint16_t *err_bytes, uint8_t msgId,
                             uint8_t msg_type)
{
    (void)Crc8Table;(void)msgId;(void)msg_type;

    port->clear(QSerialPort::Input);
    // how much data are we waiting for?
    uint16_t dataExpected = 10;
    int waitTimeout = 3000;
    QByteArray head;

    // send message
    port->write(*msg);

    // wait for data to finish writing
    if (port->waitForBytesWritten(waitTimeout)) {
//        printf("written\n");

        // is there *some* new data ready?
        if (port->waitForReadyRead(waitTimeout)) {

            // have we received all the data yet?
            while (port->bytesAvailable() < dataExpected) {
                port->waitForReadyRead(5);
            }

            // let's just use payload size for ALL commands
            head = port->read(10);
            dataExpected = head.at(9);
            dataExpected &= 0x00FF;
//            printf("payload size: %02X\n", dataExpected);

            // dataExpected = body size (not including CRCs)
            while (port->bytesAvailable() < dataExpected + 2) {
                port->waitForReadyRead(5);
            }
//            printf("got what we need\n");
            *response = port->readAll();
            (void)*response->prepend(head);

            // only fill error bytes for result responses
            if (response->at(13) == 2) {
                uint16_t temp1;
                uint16_t temp2;
                temp1 = (response->at(14)) << 8;
                temp2 = (response->at(15)) & 0x00FF;
                *err_bytes = (temp1 | temp2);
            }
        } else {
            *response = "Error: read timed out";
        }
    } else {
        *response = "Error: write timed out";
    }

//    for (int i=0; i<(*response).size(); i++) {
//        printf("%02X ", (*response).at(i));
//    }
//    printf("\n");

//    // for now, only print below for result messages
//    if (response->at(13) == 2) {
//        printf("error code: 0x%04X\n", *err_bytes);
//    }
}

/**
 * @brief genReadMsg: WriteMessageToSensor's partner in crime. Method to construct a read message.
 * @param Crc8Table
 * @param msgId
 * @param msgSubId: for APPROACH_READ, # approaches to read
 * @param destId
 * The next three args have default values of 3, 0, and 0, defined in msg.h
 * @param payloadSize
 * @param destSubnetId
 * @param seqNumber
 * @return
 */
QByteArray genReadMsg(uint8_t *Crc8Table, uint8_t msgId,
                           uint8_t msgSubId,
                           uint16_t destId,
                           uint8_t payloadSize,
                           uint8_t destSubnetId,
                           uint8_t seqNumber)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dst. subnet id (1)
    msg.append(destSubnetId);

    // dst. ID (2)
    msg.append(destId >> 8);
    uint8_t lower8 = destId;
    msg.append(lower8);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(seqNumber);

    // payload size (1)
    msg.append(payloadSize);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // start of body

    // msg ID (1)
    msg.append(msgId);

    // msg sub-ID (1)
    msg.append(msgSubId);

    // msg type (1, read)
    msg.append(zero);

    QByteArray body = msg.right(payloadSize);

    crc = SmCommsComputeCrc8(Crc8Table, &body, payloadSize);
    msg.append(crc);
    return msg;
}


void parse_gen_conf_read_response(QByteArray response,
                                  sensor_config *s,
                                  QString errString)
{
    // error handling
    if (response.at(0) == 'E') {
        s->orientation = 'E';
        s->location = "Error";
        if (response.at(7) == 'r') {
            s->description = "Read timed out";
            errString = "Read timed out";
        } else {
            s->description = "Write timed out";
            errString = "Write timed out";
        }
        s->serial = "wack";
        s->units = '0';

        return;
    }

    if (response.size() < 98) {
        s->location = "Response too short, incomplete";
        return;
    }

    // where does useful body information start?
    int index = 14;

    // extract sensor orientation
    s->orientation = response.at(14);

    // update location in the response array
    index += 2;

    // next 32 or 64 bytes contain location string
    if (response.at(index) == 0) {
        char onChar = 0;
        int i;
        int realZero;
        QString loc;
        for (i=0; i<64; i++) {
            if (onChar) {
                char c = response.at(index);
                loc.append(c);
                onChar = 0;
                index++;
            } else {
                onChar = 1;
                index++;
            }
        }
        realZero = loc.indexOf(zero);
        loc.truncate(realZero);
        s->location = loc;
    } else {
        s->location = response.mid(index, 32);
        index += 32;
    }

    // next 32 bytes contain sensor description
    if (response.at(index) == 0) {
        char onChar = 0;
        int i;
        int realZero;
        QString des;
        for (i=0; i<64; i++) {
            if (onChar) {
                char c = response.at(index);
                des.append(c);
                onChar = 0;
                index++;
            } else {
                onChar = 1;
                index++;
            }
        }
        realZero = des.indexOf(zero);
        des.truncate(realZero);
        s->description = des;
    } else {
        s->description = response.mid(index, 32);
        index += 32;
    }

    // next 16 bytes: serial
    s->serial = response.mid(index, 16);

    // update location
    index += 16;

    // units
    s->units = 0x30 + response.at(index);
}

QByteArray gen_config_write(uint8_t *Crc8Table,
                            sensor_config *new_config,
                            uint16_t dest_id)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dest. subnet ID (1)
    msg.append(zero);

    // dest. ID (xFFFF for broadcast) (2)
    msg.append(dest_id >> 8);
    uint8_t lowerEight = dest_id;
    msg.append(lowerEight);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size (1)
    msg.append(0x56);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table, &msg, 10);
    msg.append(crc);

    // position marking start of body
    int index = 11;

    // MESSAGE BODY

    // msg id (1)
    msg.append(0x2A);

    // msg sub id (1)
    msg.append(zero);

    // r/w (1)
    msg.append(0x01);

    // sensor orientation (2)
    msg.append(new_config->orientation);
    msg.append(new_config->orientation);

    // sensor location string (32)
    int locnLength = (new_config->location).length();
    msg.append(new_config->location);
    for (int i=locnLength; i<32; i++) {
        msg.append(zero);
    }

    // sensor description (32)
    int descrLength = (new_config->description).length();
    msg.append(new_config->description);
    for (int i=descrLength; i<32; i++) {
        msg.append(zero);
    }

    // serial (16)
    msg.append(new_config->serial);
    // probably unnecessary, serial should be 16 bytes
    for (int i=(new_config->serial).length(); i<16; i++) {
        msg.append(zero);
    }

    // units
    msg.append(new_config->units);

    // body CRC (1)

    // snips off the body portion of message
    QByteArray body = msg.mid(index, -1);
    // 86 = number of bytes in body portion of message
    crc = SmCommsComputeCrc8(Crc8Table, &body, 86);
    msg.append(crc);

    return msg;
}

void parse_data_conf_read_response(QByteArray *response,
                                   sensor_data_config *d,
                                   QString errString)
{
    // error handling
    if ((*response).at(0) == 'E') {

        if ((*response).at(7) == 'r') {
            errString = "Read timed out";
        } else {
            errString = "Write timed out";
        }
        return;
    }

    if ((*response).size() < 40) {
        errString = "Response too short, incomplete";
        return;
    }

    // for extracting 16-bit fields
    uint16_t temp1;
    uint16_t temp2;

    // data interval: bytes 14 and 15
    temp1 = (response->at(14)) << 8;
    temp2 = (response->at(15)) & 0x00FF;
    d->data_interval = (temp1 | temp2);

    d->interval_mode = response->at(16);

    // location in response array
    int i = 17;

    // next 18 bytes contain the data push configuration
    d->e_portnum = response->at(i);     i++;
    d->e_format = response->at(i);      i++;
    d->e_pushen = response->at(i);      i++;
    d->e_destsubid = response->at(i);   i++;
    temp1 = (response->at(i)) << 8;     i++;
    temp2 = (response->at(i)) & 0x00FF; i++;
    d->e_destid = (temp1 | temp2);

    d->i_portnum = response->at(i);     i++;
    d->i_format = response->at(i);      i++;
    d->i_pushen = response->at(i);      i++;
    d->i_destsubid = response->at(i);   i++;
    temp1 = (response->at(i)) << 8;     i++;
    temp2 = (response->at(i)) & 0x00FF; i++;
    d->i_destid = (temp1 | temp2);

    d->p_portnum = response->at(i);     i++;
    d->p_format = response->at(i);      i++;
    d->p_pushen = response->at(i);      i++;
    d->p_destsubid = response->at(i);   i++;
    temp1 = (response->at(i)) << 8;     i++;
    temp2 = (response->at(i)) & 0x00FF; i++;
    d->p_destid = (temp1 | temp2);

    temp1 = (response->at(i)) << 8;     i++;
    temp2 = (response->at(i)) & 0x00FF; i++;
    d->loop_sep = (temp1 | temp2);

    temp1 = (response->at(i)) << 8;     i++;
    temp2 = (response->at(i)) & 0x00FF; i++;
    d->loop_size = (temp1 | temp2);
}
/**
 * @brief gen_data_conf_write: Generates a Data Configuration Write message.
 * @param Crc8Table
 * @param new_dc: a sensor_data_config struct that contains all the config details to be written to the sensor
 * @param dest_id: ID of targeted sensor
 * @return
 */
QByteArray gen_data_conf_write(uint8_t *Crc8Table,
                               sensor_data_config *new_dc,
                               uint16_t dest_id)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dest. subnet ID (1)
    msg.append(zero);

    // dest. ID (xFFFF for broadcast) (2)
    msg.append(dest_id >> 8);
    uint8_t lowerEight = dest_id;
    msg.append(lowerEight);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size: 28 (1)
    msg.append(0x1C);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // position marking start of body
    int index = 11;

    // MESSAGE BODY

    // msg id (1)
    msg.append(0x03);

    // msg sub id (1)
    msg.append(zero);

    // r/w (1)
    msg.append(0x01);

    // data interval (2)
    uint16_t dataIntrvl = new_dc->data_interval;

    msg.append(dataIntrvl >> 8);
    msg.append(dataIntrvl & 0x00FF);

    // interval mode (1)
    msg.append(new_dc->interval_mode);

    // Event Data Push Configuration (6)
    msg.append(new_dc->e_portnum);
    msg.append(new_dc->e_format);
    msg.append(new_dc->e_pushen);
    msg.append(new_dc->e_destsubid);
    uint8_t destIdLoE = (new_dc->e_destid) & 0x00FF;
    uint8_t destIdHiE = (new_dc->e_destid) >> 8;
    msg.append(destIdHiE);
    msg.append(destIdLoE);

    // Interval Data Push Configuration (6)
    msg.append(new_dc->i_portnum);
    msg.append(new_dc->i_format);
    msg.append(new_dc->i_pushen);
    msg.append(new_dc->i_destsubid);
    uint8_t destIdLoI = (new_dc->e_destid) & 0x00FF;
    uint8_t destIdHiI = (new_dc->e_destid) >> 8;
    msg.append(destIdHiI);
    msg.append(destIdLoI);

    // Presence Data Push Configuration (6)
    msg.append(new_dc->p_portnum);
    msg.append(new_dc->p_format);
    msg.append(new_dc->p_pushen);
    msg.append(new_dc->p_destsubid);
    uint8_t destIdLoP = (new_dc->e_destid) & 0x00FF;
    uint8_t destIdHiP = (new_dc->e_destid) >> 8;
    msg.append(destIdHiP);
    msg.append(destIdLoP);

    // Loop Separation (2)
    uint8_t loopSepLo = (new_dc->loop_sep) & 0x00FF;
    uint8_t loopSepHi = (new_dc->loop_sep) >> 8;
    msg.append(loopSepHi);
    msg.append(loopSepLo);

    // Loop Size (2)
    uint8_t loopSzLo = (new_dc->loop_size) & 0x00FF;
    uint8_t loopSzHi = (new_dc->loop_size) >> 8;
    msg.append(loopSzHi);
    msg.append(loopSzLo);


    // snips off the body portion of message
    QByteArray body = msg.mid(index, -1);

    //printf("body length: %d\n", body.size());

    // 86 = number of bytes in body portion of message
    crc = SmCommsComputeCrc8(Crc8Table, &body, 28);
    //printf("body CRC OK\n");
    msg.append(crc);

    return msg;
}

uint8_t parse_global_push_mode_read_resp(QByteArray *response,
                                      QString errString)
{
    // error handling
    if ((*response).at(0) == 'E') {

        if ((*response).at(7) == 'r') {
            errString = "Read timed out";
        } else {
            errString = "Write timed out";
        }
        return -1;
    }

    if ((*response).size() < 16) {
        errString = "Response too short, incomplete";
        return -1;
    }
    printf ("Global Push State: ");
    if (response->at(14)) {
        printf("Enabled\n");
    } else {
        printf("Disabled\n");
    }
    return response->at(14);
}
QByteArray gen_global_push_mode_write(uint8_t *Crc8Table,
                                      uint8_t globalPushState,
                                      uint16_t dest_id)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dst. subnet id (1)
    msg.append(zero);

    // dst. ID (2)
    msg.append(dest_id >> 8);
    uint8_t lower8 = dest_id;
    msg.append(lower8);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size: 28 (1)
    msg.append(0x04);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // start of body

    // msg ID (1)
    msg.append(0x0D);

    // msg sub-ID (1, don't care)
    msg.append(zero);

    // msg type (1, write)
    msg.append(0x01);

    // global push state setting
    msg.append(globalPushState);

    // body size is 4 bytes
    QByteArray body = msg.right(4);

    crc = SmCommsComputeCrc8(Crc8Table, &body, 4);
    msg.append(crc);
    return msg;
}

void parse_sensor_time_read_resp(QByteArray *response, QString errString,
                                 sensor_datetime *d)
{
    // error handling
    if ((*response).at(0) == 'E') {

        if ((*response).at(7) == 'r') {
            errString = "Read timed out";
        } else {
            errString = "Write timed out";
        }
        return;
    }

    if ((*response).size() < 23) {
        errString = "Response too short, incomplete";
        return;
    }
    // sensor date starts at byte indexed 14
    uint8_t day;
    uint8_t month;
    uint16_t year;

    // extract year
    uint8_t y1 = response->at(15);
    uint8_t y2 = response->at(16);

    uint8_t tmpHi;
    uint8_t tmpLo;

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
    month = (response->at(17)) >> 5;
    if (y2 & 0x1) {
        // MSB is 1
        month |= 0x8;
    } else {
        month &= 0x7;
    }

    // extract day: lower 5 bits
    day = (response->at(17)) & 0x1F;

//    printf("Year: %u\n", year);
//    printf("Month: %u\n", month);
//    printf("Day: %u\n", day);

    uint8_t hrs;
    uint8_t mins;
    uint8_t secs;
    uint16_t ms;

    tmpLo = (response->at(19) & 0xC0) >> 6;
    tmpHi = (response->at(18) & 0x07) << 2;
    hrs = (tmpHi | tmpLo);

    mins = (response->at(19) & 0x3F);
    secs = (response->at(20) & 0xFC) >> 2;

    // milliseconds
    tmpHi = (response->at(20) & 0x3);
    ms = (tmpHi << 8) | (response->at(21) & 0x00FF);

    // update datetime struct
    d->yr = year;
    d->mon = month;
    d->day = day;
    d->hrs = hrs;
    d->mins = mins;
    d->secs = secs;
    d->ms = ms;

//    printf("Time: %02u:%02u:%02u.%03u\n", hrs, mins, secs, ms);
}

/**
 * @brief gen_sensor_time_write. Note: any year <= 2001 will be set to 2001
 * @param Crc8Table
 * @param d : pointer to a sensor_datetime struct containing the data to be written to the sensor
 * @param dest_id : ID of the target sensor
 * @return
 */
QByteArray gen_sensor_time_write(uint8_t *Crc8Table,
                                 sensor_datetime *d,
                                 uint16_t dest_id)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dest. subnet ID (1)
    msg.append(zero);

    // dest. ID (xFFFF for broadcast) (2)
    msg.append(dest_id >> 8);
    uint8_t lowerEight = dest_id;
    msg.append(lowerEight);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size: 11 (1)
    msg.append(0x0B);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // position marking start of body
    int index = 11;

    // MESSAGE BODY

    // msg id (1)
    msg.append(0x0E);

    // msg sub id (1)
    msg.append(zero);

    // r/w (1)
    msg.append(0x01);

    // DATE (4)

    // 31-24: blank (spares)
    msg.append(zero);

    // 23-16: upper 3 are blank, lower 5 are part of year
    uint8_t tmp;
    tmp = static_cast<uint8_t>(((d->yr) >> 7) & 0x001F);
    msg.append(tmp);

    // 15-8: upper 7 are year, LSB contains upper bit of month
    tmp = static_cast<uint8_t>((d->yr) & 0x007F);
    tmp <<= 1;
    tmp |= ((d->mon) >> 3);
    msg.append(tmp);

    // 7-0: upper 3 are remainder of month, lower 5 contain day
    tmp = ((d->mon) & 0x07) << 5;
    tmp |= (d->day);
    msg.append(tmp);

    // TIME (4)

    // 31-24: upper 5 are spares, lower 3 are hours
    tmp = ((d->hrs) & 0x1C) >> 2;
    msg.append(tmp);

    // 23-16: upper 2 are hours, lower 6 are minutes
    tmp = ((d->hrs) & 0x03) << 6;
    tmp |= d->mins;
    msg.append(tmp);

    // 15-8: upper 6 are seconds, lower 2 are for ms
    tmp = (d->secs) << 2;
    uint8_t tmp2;
    tmp2 = static_cast<uint8_t>(((d->ms) & 0x0300) >> 8);
    msg.append((tmp | tmp2));

    // 7-0: ms
    tmp = static_cast<uint8_t>((d->ms) & 0x00FF);
    msg.append(tmp);

    // snips off the body portion of message
    QByteArray body = msg.mid(index, -1);

    // 11 = number of bytes in body portion of message
    crc = SmCommsComputeCrc8(Crc8Table, &body, 11);
    msg.append(crc);

    return msg;
}

/**
 * @brief gen_approach_info_read: Retrieves information about approaches configured on the sensor. Worst case response size = 96 bytes. Minimum size = 16 bytes.
 * @param Crc8Table
 * @param numApproaches
 * @param dest_id
 * @return
 */

int parse_approach_info_read_resp(QByteArray *response,
                                   approach *approaches,
                                   QString errString)
{
    // error handling
    if ((*response).at(0) == 'E') {

        if ((*response).at(7) == 'r') {
            errString = "Read timed out";
        } else {
            errString = "Write timed out";
        }
        return -1;
    }

    if ((*response).size() < 16) {
        errString = "Response too short, incomplete";
        return -1;
    }

    //int payloadSize = response->at(9);
    int numReturnedApproaches = response->at(12);
    int numApprConfigured = response->at(14);

//    printf("\n");
//    printf("Num Returned Apprs: %u\t", numReturnedApproaches);
//    printf("Num Configured Approaches: %u\n", numApprConfigured);

    int i, j;
    int locn;

//    // to accomodate bizarre string format of approach descriptions
    char onChar = 0;
    char noDataRemaining = 0;

    approach *a;

    // get each of the (useful) approaches
    locn = 15;

    // extract descriptions
    for (j=1; j<=numReturnedApproaches; j++) {
        // don't extract garbage data from unconfigured appr.
        if (j <= numApprConfigured) {
            a = approaches + j - 1;
            noDataRemaining = 0;
            onChar = 0;
            // Sensor returns this information in two different patterns
            // Infer pattern based on whether first descr byte is 0 or data
            char style = response->at(locn);
            if (style != 0) {
                // string appears all together, then zeroes
                for (i=0; (i<16 && (noDataRemaining == 0)); i++) {
                    char c = response->at(locn);
                    if (c == 0) {
                        noDataRemaining = 1;
                        // skip remaining zeroes
                        locn += 15 - i;
                    }
                    a->description.append(c);
                    locn++;
                }
            } else {
                for (i=0; i<16; i++) {
                    if (onChar) {
                        char c = response->at(locn);
                        a->description.append(c);
                        onChar = 0;
                    } else {
                        onChar = 1;
                    }
                    locn++;
                }
            }
        } else {
            locn += 16;
        }
    }

    // extract directions
    for (j=1; j<=numReturnedApproaches; j++) {
        if (j <= numApprConfigured) {
            a = approaches + j - 1;
            a->direction = response->at(locn);
        }
        locn++;
    }
    // extract number of lanes per approach
    for (j=1; j<=numReturnedApproaches; j++) {
        if (j <= numApprConfigured) {
            a = approaches + j - 1;
            a->numLanes = response->at(locn);
        }
        locn++;
    }

    // extract lanes associated with each approach
    for (j=1; j<=numReturnedApproaches; j++) {
        if (j <= numApprConfigured) {
            a = approaches + j - 1;
            int nL = a->numLanes;
            a->lanesAssigned = new uint8_t[2];
            uint8_t *p = a->lanesAssigned;
            for (i=0; i<nL; i++) {
                *(p+i) = response->at(locn);
                locn++;
            }
        }
    }

    // print data associated with each approach

//    printf("\n");
//    for (i=1; i<=numReturnedApproaches; i++) {
//        if (i <= numApprConfigured) {
//            a = approaches + i - 1;
//            const char *descr = (a->description).toLatin1().data();
//            printf("Approach %d: %s\t", i, descr);
//            printf("Direction: %c\t", a->direction);
//            printf("Num Lanes: %u\t", a->numLanes);
//            printf("Lanes assigned: ");
//            uint8_t *p = a->lanesAssigned;
//            for (j=0; j<a->numLanes; j++) {
//                printf("%02u ", *(p+j));
//            }
//            printf("\n");
//        }
//    }
    return numApprConfigured;
}
QByteArray gen_approach_info_write(uint8_t *Crc8Table,
                                   uint8_t numApproaches,
                                   approach *aW,
                                   uint16_t dest_id)
{
    QByteArray msg;

    int i;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dst. subnet id (1)
    msg.append(zero);

    // dst. ID (2)
    msg.append(dest_id >> 8);
    uint8_t lower8 = dest_id;
    msg.append(lower8);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size (1)
    int payloadSize;

    // message ID (1), subID (1), type (1), # approaches (1)
    payloadSize = 4;

    // descriptions (16), R/L chars (1), # lanes (1) for each lane
    payloadSize += 18 * numApproaches;

    // count total number of lanes assigned across all approaches
    int numLaneAssignments = 0;
    for (i=0; i<numApproaches; i++) {
        approach *a = aW + i;
        numLaneAssignments += a->numLanes;
    }

    // each approach has lanes assigned
    payloadSize += numLaneAssignments;

    msg.append(payloadSize);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // start of body

    // msg ID (1)
    msg.append(0x28);

    // TODO: figure this out?? Datasheet says "num approaches returned"
    // msg sub-ID (1)
    msg.append(numApproaches);

    // msg type (1, write)
    msg.append(0x01);

    // number of approaches in data (1)
    msg.append(numApproaches);

    // descriptions of each approach
    for (i=0; i<numApproaches; i++) {
        approach *a = aW + i;
        QString q = a->description;
        int k;
        for (k=0; k<q.size(); k++) {
            QChar c = q.at(k);
            char ch = c.toLatin1();
            msg.append(ch);
        }
        for (k=q.size(); k<16; k++) {
            msg.append(zero);
        }
    }

    // directions of each approach
    for (i=0; i<numApproaches; i++) {
        approach *a = aW + i;
        msg.append(a->direction);
    }

    // number of lanes of each approach
    for (i=0; i<numApproaches; i++) {
        approach *a = aW + i;
        msg.append(a->numLanes);
    }

    // lane assignments for each approach
    for (i=0; i<numApproaches; i++) {
        approach *a = aW + i;
        uint8_t *lanes = a->lanesAssigned;
        uint8_t laneCount = a->numLanes;
        int j = 0;
        for (j=0; j<laneCount; j++) {
            msg.append(*(lanes + j));
        }
    }

    // body is from index 11 onwards
    QByteArray body = msg.mid(11, -1);

    crc = SmCommsComputeCrc8(Crc8Table, &body, payloadSize);
    msg.append(crc);
    return msg;
}


/**
 * @brief extract_16b_fxpt: Helper function. Extracts a uint16_t from the two bytes in QByteArray arr that are at locations i and i+1. Make sure that i+1 < arr.size() before calling.
 * @param arr
 * @param i
 * @return
 */
uint16_t extract16BitFixedPt(QByteArray *arr, int i)
{
    if (i+1 < arr->size()) {
        uint16_t t1 = (arr->at(i)) << 8;
        uint16_t t2 = (arr->at(i+1)) & 0x00FF;
        return (t1 | t2);
    } else {
        return 0xFFFF;
    }
}

/**
 * @brief fixed_pt_to_double: Helper function to convert a uint16_t to a floating point based on appendix A.2
 * @param t
 * @return
 */
float fixedPtToFloat(uint16_t t)
{
    // extract integer part
    uint8_t tI = t >> 8;

    // extract decimal part
    float tD = (t & 0x00FF) / 256.0;

    return (static_cast<float>(tI) + tD);
}

/**
 * @brief parse_classif_read_resp: Parses sensor response to a Classification Configuration Read message.
 * @param response: pointer to byte array of sensor response
 * @param bounds: pointer to array where classifications currently on the sensor will be stored
 * @param errString
 */
void parse_classif_read_resp(QByteArray *response, double *bounds, int *nC, QString eS)
{
    // error handling
    if ((*response).at(0) == 'E') {

        if ((*response).at(7) == 'r') {
            eS = "Read timed out";
        } else {
            eS = "Write timed out";
        }
        return;
    }

    if ((*response).size() < 15) {
        eS = "Response too short, incomplete";
        return;
    }

    int numClasses = response->at(12);
    *nC = numClasses;
    printf("\nNum Classes: %u\n", numClasses);

    int i;
    int locn;

    // where data starts
    locn = 14;
    uint16_t tmp;
    uint8_t tI;
    double tD;
    double d;

    for (i=0; i<numClasses; i++) {
        // parse class as one 16-bit field
        tmp = extract16BitFixedPt(response, locn);

        // extract integer part
        tI = tmp >> 8;

        // extract decimal part
        tD = (tmp & 0x00FF) / 256.0;

        // store bound
        d = static_cast<double>(tI) + tD;
        *(bounds + i) = d;
        locn += 2;
    }

    for (i=0; i<numClasses; i++) {
        printf("Boundary %u: %03.3f\n", i+1, *(bounds + i));
    }
}

/**
 * @brief gen_classif_write: Generates a Classification Configuration message.
 * @param Crc8Table
 * @param bounds: an array of unsigned 16-bit values containing fixed-point representations of bounds.
 * @param numClasses: number of classes being written. Must equal size of bounds array.
 * @param dest_id
 * @return
 */
QByteArray gen_classif_write(uint8_t *Crc8Table,
                             uint16_t *bounds,
                             uint8_t numClasses,
                             uint16_t dest_id)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dst. subnet id (1)
    msg.append(zero);

    // dst. ID (2)
    msg.append(dest_id >> 8);
    uint8_t lower8 = dest_id;
    msg.append(lower8);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size (1)

    // msgID, subID, type
    int payloadSize = 3;

    // bounds
    payloadSize += 2 * numClasses;
    msg.append(payloadSize);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // start of body

    // msg ID (1)
    msg.append(0x13);

    // msg sub-ID (1, # bins to configure)
    msg.append(numClasses);

    // msg type (1, write)
    msg.append(0x01);

    // append classification boundaries
    int i;
    for (i=0; i<numClasses; i++) {
        uint16_t bound = *(bounds + i);
        // split into two bytes
        uint8_t boundLo = bound & 0x00FF;
        uint8_t boundHi = bound >> 8;

        msg.append(boundHi);
        msg.append(boundLo);
    }

    // body size is 4 bytes
    QByteArray body = msg.right(payloadSize);

    crc = SmCommsComputeCrc8(Crc8Table, &body, payloadSize);
    msg.append(crc);
    return msg;
}
/**
 * @brief gen_active_lane_info_read: Generates an Active Lane Information Read message.
 * @param Crc8Table
 * @param dest_id
 * @return
 */


void parse_active_lane_info_read_resp(QByteArray *response, lane *laneArray, int *numConfd, QString eS)
{
    // error handling
    if ((*response).at(0) == 'E') {

        if ((*response).at(7) == 'r') {
            eS = "Read timed out";
        } else {
            eS = "Write timed out";
        }
        return;
    }

    if ((*response).size() < 16) {
        eS = "Response too short, incomplete";
        return;
    }

    int numLanesReturned = response->at(12);
    int locn = 14;
    int numActiveConfiguredLanes = response->at(14);
    *numConfd = numActiveConfiguredLanes;
    locn += 3;

//    printf("Num Lanes Returned: %u\t", numLanesReturned);
//    printf("Num Lanes Config'd: %u\n", numActiveConfiguredLanes);

    // extract each lane's information
    for (int i=1; i<=numLanesReturned; i++) {
        if (i <= numActiveConfiguredLanes) {
            lane l = *(laneArray + i - 1);
            (void)l;    // to shut up compiler warnings

            (laneArray+i-1)->description = new char[8];
            int strLocn = 0;
            if (response->at(locn) == 0) {
                int j;
                char onChar = 0;
                for (j=0; j<16; j++) {
                    if (onChar) {
                        *((laneArray+i-1)->description + strLocn) = response->at(locn);
                        strLocn++;
                        locn++;
                        onChar = 0;
                    } else {
                        onChar=1;
                        locn++;
                    }
                }
            char dir = response->at(locn);
            if (dir) { dir = 'L'; } else { dir = 'R'; }
            (laneArray+i-1)->direction = dir;
            locn++;
            }
        }
    }

//    for (int i=1; i<=numLanesReturned; i++) {
//        if (i <= numActiveConfiguredLanes) {
//            lane l = *(laneArray + i - 1);
//            printf("Lane %u\t", i+1);
//            printf("Desc: %s\t", l.description);
//            printf("Direction: %u\n", l.direction);
//        }
//    }
}

QByteArray gen_active_lane_info_write(uint8_t *Crc8Table,
                                      lane *laneData,
                                      int numActiveLanes,
                                      uint16_t dest_id)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dst. subnet id (1)
    msg.append(zero);

    // dst. ID (2)
    msg.append(dest_id >> 8);
    uint8_t lower8 = dest_id;
    msg.append(lower8);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size: msgID, subID, type, # active lanes (1)
    int payloadSize = 4;

    // each lane as 8-byte description and 1-byte direction
    payloadSize += 9 * numActiveLanes;

    msg.append(payloadSize);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // start of body

    // msg ID (1)
    msg.append(0x27);

    // msg sub-ID (1, don't care)
    msg.append(zero);

    // msg type (1, write)
    msg.append(0x01);

    // number of active lanes (1)
    msg.append(numActiveLanes);

    // append each lane's description & direction
    for (int i=1; i<=numActiveLanes; i++) {
        lane l = *(laneData + i - 1);
        char *des = l.description;
        for (int j=0; j<8; j++) {
            msg.append(*(des + j));
        }
        msg.append(l.direction);
    }

    // snip off body
    QByteArray body = msg.right(payloadSize);

    crc = SmCommsComputeCrc8(Crc8Table, &body, payloadSize);
    msg.append(crc);
    return msg;
}

void parse_global_all_uart_push_mode(QByteArray *resp, QString eS)
{
    // error handling
    if ((*resp).at(0) == 'E') {

        if ((*resp).at(7) == 'r') {
            eS = "Read timed out";
        } else {
            eS = "Write timed out";
        }
        return;
    }

    if ((*resp).size() < 19) {
        eS = "Response too short, incomplete";
        return;
    }
    char rs485GlobalPushState;
    char rs232GlobalPushState;
    char exp0GlobalPushState;
    char exp1GlobalPushState;

    rs485GlobalPushState = resp->at(14);
    rs232GlobalPushState = resp->at(15);
    exp0GlobalPushState = resp->at(16);
    exp1GlobalPushState = resp->at(17);

    printf("RS485 GPS: %u\n", rs485GlobalPushState);
    printf("RS232 GPS: %u\n", rs232GlobalPushState);
    printf("Exp0 GPS: %u\n", exp0GlobalPushState);
    printf("Exp1 GPS: %u\n", exp1GlobalPushState);
}

QByteArray gen_global_all_uart_push_mode_write(uint8_t *Crc8Table,
                                               char pushConfig,
                                               uint16_t destId)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dest. subnet ID (1)
    msg.append(zero);

    // dest. ID (xFFFF for broadcast) (2)
    msg.append(destId >> 8);
    uint8_t lowerEight = destId;
    msg.append(lowerEight);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size (1)
    msg.append(0x7);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table, &msg, 10);
    msg.append(crc);

    // MESSAGE BODY

    // msg id (1)
    msg.append(0x1C);

    // msg sub id (1)
    msg.append(zero);

    // r/w (1)
    msg.append(0x01);

    // pushConfig: 0000 xxxx
    // left to right: RS485, RS232, Exp0, Exp1

    int i;
    char c;
    for (i=3; i>=0; i--) {
        c = (pushConfig >> i) & 0x1;
        msg.append(c);
    }

    // body CRC (1)

    // snips off the body portion of message
    QByteArray body = msg.right(7);
    crc = SmCommsComputeCrc8(Crc8Table, &body, 7);
    msg.append(crc);

    return msg;
}

void parse_speed_bin_conf_read(QByteArray *resp, int *nBins, float *fArr, QString eS)
{
    // error handling
    if ((*resp).at(0) == 'E') {

        if ((*resp).at(7) == 'r') {
            eS = "Read timed out";
        } else {
            eS = "Write timed out";
        }
        return;
    }

    if ((*resp).size() < 15) {
        eS = "Response too short, incomplete";
        return;
    }

    int numBinsDefined = resp->at(12);
    *nBins = numBinsDefined;

    int i;
    int locn = 14;
    for (i=0; i<numBinsDefined; i++) {
        uint16_t threshold = extract16BitFixedPt(resp, locn);
        float f = fixedPtToFloat(threshold);
        *(fArr + i) = f;
        locn += 2;
    }
}

QByteArray gen_speed_bin_conf_write(uint8_t *Crc8Table, uint16_t *bins,
                                    int numBins, uint16_t destId)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dest. subnet ID (1)
    msg.append(zero);

    // dest. ID (xFFFF for broadcast) (2)
    msg.append(destId >> 8);
    uint8_t lowerEight = destId;
    msg.append(lowerEight);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size (1)
    int payloadSize = 3;

    // each bin requires 2 bytes
    payloadSize += (numBins << 1);
    msg.append(payloadSize);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table, &msg, 10);
    msg.append(crc);

    // MESSAGE BODY

    // msg id (1)
    msg.append(0x1D);

    // msg sub id (1)
    msg.append(numBins);

    // r/w (1)
    msg.append(0x01);

    int i;
    for (i=0; i<numBins; i++) {
        uint16_t bin = *(bins + i);
        uint8_t intPart (bin >> 8);
        msg.append(intPart);
        uint8_t decPart = static_cast<uint8_t>(bin);
        msg.append(decPart);
    }

    // body CRC (1)

    // snips off the body portion of message
    QByteArray body = msg.right(payloadSize);
    crc = SmCommsComputeCrc8(Crc8Table, &body, payloadSize);
    msg.append(crc);
    return msg;
}
QByteArray gen_dir_bin_conf_write(uint8_t *Crc8Table,
                                  char dirBinEnabled,
                                  uint16_t destId)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dest. subnet ID (1)
    msg.append(zero);

    // dest. ID (xFFFF for broadcast) (2)
    msg.append(destId >> 8);
    uint8_t lowerEight = destId;
    msg.append(lowerEight);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size (1)
    msg.append(0x4);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table, &msg, 10);
    msg.append(crc);

    // MESSAGE BODY

    // msg id (1)
    msg.append(0x1E);

    // msg sub id (1)
    msg.append(zero);

    // r/w (1)
    msg.append(0x01);

    // bin by direction flag (1)
    msg.append(dirBinEnabled);

    // body CRC (1)

    // snips off the body portion of message
    QByteArray body = msg.right(4);
    crc = SmCommsComputeCrc8(Crc8Table, &body, 4);
    msg.append(crc);
    return msg;
}

/**
 * @brief gen_offset_sensor_time not supported?
 * @param Crc8Table
 * @param signFlag
 * @param offset
 * @param destId
 * @return
 */
QByteArray gen_offset_sensor_time(uint8_t *Crc8Table,
                                  uint8_t signFlag,
                                  uint16_t offset,
                                  uint16_t destId)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dest. subnet ID (1)
    msg.append(zero);

    // dest. ID (xFFFF for broadcast) (2)
    msg.append(destId >> 8);
    uint8_t lowerEight = destId;
    msg.append(lowerEight);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(zero);

    // payload size (1)
    msg.append(0x5);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table, &msg, 10);
    msg.append(crc);

    // MESSAGE BODY

    // msg id (1)
    msg.append(0x1E);

    // msg sub id (1)
    msg.append(signFlag);

    // r/w (1)
    msg.append(0x01);

    // time offset (2)
    uint8_t upper = (offset >> 8) & 0xFF;
    msg.append(upper);
    uint8_t lower = static_cast<uint8_t>(offset);
    msg.append(lower);

    // body CRC (1)

    // snips off the body portion of message
    QByteArray body = msg.right(5);
    crc = SmCommsComputeCrc8(Crc8Table, &body, 5);
    msg.append(crc);
    return msg;
}

QByteArray getVarSizeIntervalDataByTimestamp(uint8_t *Crc8Table,
                                             uint8_t requestType,
                                             uint16_t destId,
                                             uint8_t destSubnetId,
                                             uint8_t seqNumber,
                                             QDateTime dt,
                                             uint8_t singleNum)
{
    QByteArray msg;

    // message version (2)
    msg.append('Z');
    msg.append('1');

    // dst. subnet id (1)
    msg.append(destSubnetId);

    // dst. ID (2)
    msg.append(destId >> 8);
    uint8_t lower8 = destId;
    msg.append(lower8);

    // source subnet ID (1)
    msg.append(zero);

    // source ID (2)
    msg.append(zero);
    msg.append(zero);

    // seq. number (1)
    msg.append(seqNumber);

    // payload size (1)
    msg.append(0x0C);

    // header CRC (1)
    unsigned char crc = SmCommsComputeCrc8(Crc8Table,
                                           &msg, 10);
    msg.append(crc);

    // start of body

    // msg ID (1)
    msg.append(0x74);

    // msg sub-ID (1)
    msg.append(requestType);

    // msg type (1, read)
    msg.append(zero);

    // date (4)
    int yr =  dt.date().year();
    int mo = dt.date().month();
    int day =  dt.date().day();

    uint8_t d = static_cast<uint8_t>((day >> 24) & 0x000000FF);
    uint8_t m = static_cast<uint8_t>((mo  >> 24) & 0x000000FF);
    uint16_t y = static_cast<uint16_t>((yr >> 8) & 0x0000FFFF);

    // 31-24: blank (spares)
    msg.append(zero);

    // 23-16: upper 3 are blank, lower 5 are part of year
    uint8_t tmp;
    tmp = static_cast<uint8_t>((y >> 7) & 0x001F);
    msg.append(tmp);

    // 15-8: upper 7 are year, LSB contains upper bit of month
    tmp = static_cast<uint8_t>((y) & 0x007F);
    tmp <<= 1;
    tmp |= (m >> 3);
    msg.append(tmp);

    // 7-0: upper 3 are remainder of month, lower 5 contain day
    tmp = ((m) & 0x07) << 5;
    tmp |= (d);
    msg.append(tmp);

    // time (4)
    int hrs = dt.time().hour();
    int mins = dt.time().minute();
    int secs = dt.time().second();

    uint8_t h = static_cast<uint8_t>((hrs >> 24) & 0x000000FF);
    uint8_t min = static_cast<uint8_t>((mins >> 24) & 0x000000FF);
    uint8_t sec = static_cast<uint8_t>((secs >> 24) & 0x000000FF);
    uint16_t ms = 0;

    // 31-24: upper 5 are spares, lower 3 are hours
    tmp = ((h) & 0x1C) >> 2;
    msg.append(tmp);

    // 23-16: upper 2 are hours, lower 6 are minutes
    tmp = ((h) & 0x03) << 6;
    tmp |= min;
    msg.append(tmp);

    // 15-8: upper 6 are seconds, lower 2 are for ms
    tmp = (sec) << 2;
    uint8_t tmp2;
    tmp2 = static_cast<uint8_t>(((ms) & 0x0300) >> 8);
    msg.append((tmp | tmp2));

    // 7-0: ms
    tmp = static_cast<uint8_t>(ms & 0x00FF);
    msg.append(tmp);


    // lane/approach number (1)
    msg.append(singleNum);

    QByteArray body = msg.right(0x0C);

    crc = SmCommsComputeCrc8(Crc8Table, &body, 0x0C);
    msg.append(crc);
    return msg;
}

void startRealTimeDataRetrieval(uint8_t reqType, uint8_t laneApprNum,
                                uint8_t *Crc8Table,
                                sensor_data_config *sDC,
                                uint16_t sensorId,
                                uint16_t dataInterval,
                                SerialWorker *sW, uint16_t *errBytes)
{

    QByteArray msg;
    QByteArray resp(128, '*');
    QDateTime dt;

    uint8_t seqNumber = 0;

    *errBytes = 0;

    // first, update data configuration
    msg = gen_data_conf_write(Crc8Table, sDC, sensorId);
    sW->writeMsgToSensor(&msg, &resp, Crc8Table, errBytes);

    if (*errBytes == 0) {
        dt = QDateTime::currentDateTimeUtc();
        printf("Data interval: %u\n", dataInterval);
        printf("Start DA RETreival!\n");
    }
}
