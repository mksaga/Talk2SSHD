#include "sensor_utils.h"

using namespace std;

void SmCommsGenerateCrc8Table(uint8_t *table, int table_size)
{
    // safety measure
    if (table_size != COMMS_CRC8_TABLE_LENGTH) return;

    unsigned char crcValue;
    unsigned char polynomial = 0x1c;
    unsigned int i, j;

    for (i=0; i<COMMS_CRC8_TABLE_LENGTH; i++)
    {
        crcValue = i;
        for (j=0; j<8; j++)
        {
            if (crcValue & 0x80) {
                crcValue = (crcValue << 1);
                crcValue = crcValue ^ polynomial;
            } else {
                crcValue = crcValue << 1;
            }
        }
        table[i] = crcValue & OCTET_MASK;
    }
}

unsigned char SmCommsComputeCrc8(uint8_t *Crc8Table, QByteArray *bufferPtr,
                                 unsigned int bufferLength)
{
    unsigned int i;
    unsigned char crcValue = 0x00;
    unsigned int tableIndex;
    for (i=0; i<bufferLength; i++)
    {
        tableIndex = crcValue ^ (bufferPtr->at(i) & 0xFF);
        crcValue = Crc8Table[tableIndex];
    }
    return crcValue;
}
