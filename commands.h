#ifndef COMMANDS_H
#define COMMANDS_H

#include <QByteArray>
#include <QSerialPort>
#include "sensor_utils.h"

void write_message_to_sensor(QSerialPort *port,
                             QByteArray *msg,
                             QByteArray *response,
                             uint8_t *Crc8Table,
                             uint16_t *error_code,
                             uint8_t msg_id = 0,
                             uint8_t msg_type = 0);

QByteArray genReadMsg(uint8_t *Crc8Table, uint8_t msgId, uint8_t msgSubId,
                      uint16_t destId,
                      uint8_t payloadSize = 3,
                      uint8_t destSubnetId = 0,
                      uint8_t seqNumber = 0);

void parse_gen_conf_read_response(QByteArray resp,
                               sensor_config *s,
                                  QString errString);

QByteArray gen_config_write(uint8_t *Crc8Table,
                            sensor_config *new_config,
                            uint16_t dest_id);

void parse_data_conf_read_response(QByteArray *resp,
                                   sensor_data_config *d,
                                   QString errString);

QByteArray gen_data_conf_write(uint8_t *Crc8Table,
                               sensor_data_config *new_dc,
                               uint16_t dest_id);

void parse_global_push_mode_read_resp(QByteArray *response, QString errS);

QByteArray gen_global_push_mode_write(uint8_t *Crc8Table,
                                      uint8_t globalPushState,
                                      uint16_t dest_id);

void parse_sensor_time_read_resp(QByteArray *response, QString errString,
                                 sensor_datetime *d);

QByteArray gen_sensor_time_write(uint8_t *Crc8Table,
                                 sensor_datetime *d,
                                 uint16_t dest_id);

void parse_approach_info_read_resp(QByteArray *response,
                                   approach *approaches,
                                   QString errString);

QByteArray gen_approach_info_write(uint8_t *Crc8Table, uint8_t numAppr,
                                   approach *aW, uint16_t dest_id);

void parse_classif_read_resp(QByteArray *resp, double *bounds, QString eS);

QByteArray gen_classif_write(uint8_t *Crc8Table, uint16_t *bounds,
                             uint8_t numClasses, uint16_t dest_id);

lane * parse_active_lane_info_read_resp(QByteArray *resp, QString eS);

QByteArray gen_active_lane_info_write(uint8_t *Crc8Table,lane *laneData,
                                      int numActiveLanes, uint16_t destId);

void parse_global_all_uart_push_mode(QByteArray *resp, QString errString);

QByteArray gen_global_all_uart_push_mode_write(uint8_t *Crc8Table,
                                               char pushConfig,
                                               uint16_t destId);

QByteArray gen_speed_bin_conf_write(uint8_t *Crc8Table, uint16_t *bins,
                                    int numBins, uint16_t destId);

QByteArray gen_dir_bin_conf_write(uint8_t *Crc8Table, char dirBinEnabled,
                                  uint16_t destId);

QByteArray gen_offset_sensor_time(uint8_t *Crc8Table,
                                  uint8_t signFlag,
                                  uint16_t offset,
                                  uint16_t destId);

double fixedPtToDouble(uint16_t t);
uint16_t extract16BitFixedPt(QByteArray *arr, int locn);

#endif // COMMANDS_H
