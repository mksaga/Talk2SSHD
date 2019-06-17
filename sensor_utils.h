#ifndef SENSOR_UTILS_H
#define SENSOR_UTILS_H

#define COMMS_CRC8_TABLE_LENGTH 256
#define OCTET_MASK 0x000000ff

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <QByteArray>
#include <QString>

using namespace std;

// is this necessary?
struct z1_header_data {
    uint8_t     dest_subnet_id;
    uint16_t    dest_id;
    uint8_t     src_subnet_id;
    uint16_t    src_id;
};

struct sensor_config {
    uint8_t orientation;
    QString location;
    QString description;
    QString serial;
    uint8_t units;
};

struct sensor_data_config {
    uint16_t data_interval;
    uint8_t interval_mode;

    // see datasheet p14 for explanations of values

    // event data
    uint8_t e_portnum;
    uint8_t e_format;
    uint8_t e_pushen;
    uint8_t e_destsubid;
    uint16_t e_destid;

    // interval data
    uint8_t i_portnum;
    uint8_t i_format;
    uint8_t i_pushen;
    uint8_t i_destsubid;
    uint16_t i_destid;

    // presence data
    uint8_t p_portnum;
    uint8_t p_format;
    uint8_t p_pushen;
    uint8_t p_destsubid;
    uint16_t p_destid;

    //QByteArray data_push_config;
    uint16_t loop_sep;
    uint16_t loop_size;
};

struct sensor_datetime {
    uint8_t day;
    uint8_t mon;
    uint16_t yr;

    uint8_t hrs;
    uint8_t mins;
    uint8_t secs;
    uint16_t ms;
};

typedef struct approach {
    QString description;
    char direction;
    uint8_t numLanes;
    uint8_t *lanesAssigned;
} approach;

struct lane {
    char *description;
    char direction;
    lane() {}
    lane(char *q, char c)
    {
        description = q;
        direction = c;
    }
};

void SmCommsGenerateCrc8Table(uint8_t *crc_table, int length);
unsigned char SmCommsComputeCrc8(uint8_t *crc_table,
                                 QByteArray *bufferPtr,
                                 unsigned int bufferLength);

#endif // SENSOR_UTILS_H
