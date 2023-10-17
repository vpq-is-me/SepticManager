#ifndef BLE_MESSAGING_H_INCLUDED
#define BLE_MESSAGING_H_INCLUDED

#include <stdint.h>
#include <mutex>

#define CREATE_GROUP_ADDR(add) ((uint16_t)0xc000 | add)
#define GROUP_SEPTIC CREATE_GROUP_ADDR(10)
#define GROUP_DOORBELL CREATE_GROUP_ADDR(11)
#define GROUP_WATERPUMP CREATE_GROUP_ADDR(12)

#define CID_ESP 0x02E5
#define ESP_BLE_MESH_MODEL_OP_3(b0, cid)    ((((b0) << 16) | 0xC00000) | (cid))

#define SEPTIC_OP_SET                       ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define SEPTIC_OP_GET                       ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)
#define SEPTIC_OP_ALARM_STATUS              ESP_BLE_MESH_MODEL_OP_3(0x03, CID_ESP)
#define SEPTIC_OP_PEND_ALARM_STATUS         ESP_BLE_MESH_MODEL_OP_3(0x04, CID_ESP)


#define PC2BLE_MAX_LENGTH 8
#define MAX_PC_MSG_LENGTH 256

enum{
    PRX_BLE_MESH_COPY=1,//ble message send through proxy module to  PC
};



typedef struct __attribute__ ((packed)){
    uint16_t src_addr;
    uint16_t dst_addr;
    uint32_t opcode;
    uint16_t length;
    uint8_t msg[];
}ble_msg_copy_t;

typedef struct __attribute__ ((packed)){
    uint16_t length; //length of payload to be sent to PC (raspbery Pi). It include size of 'type' and some of the folLowing struct
    //Used for transmitter and internally while converting to SLIP type message but not transmitted itself//
    union {
        uint8_t raw_arr[MAX_PC_MSG_LENGTH];
        struct {//semi rough struct. Until we became know 'type' actual has to be mapped struct is unknown
            uint8_t type; //type of message to parse properly at reception
            union{
                ble_msg_copy_t msg_copy;
//                struct __attribute__ ((packed)) ble_msg_copy_t_{....
            };
        };
    };
}pc_transm_msg_t;

extern pc_transm_msg_t mesh2pc_msg;
extern pc_transm_msg_t pc2mesh_msg;


void BLE_message_exec(void);
int BLE_send(uint32_t opcode, uint16_t length,uint8_t*msg);
void BLE_Init(void);
void BLE_DeInit(void);
#endif // BLE_MESSAGING_H_INCLUDED
