#if !defined(ISOTP_H)
#define ISOTP_H

#include <stdint.h>
#include "isotp_config.h"

enum isotp_tatype
{
    ISOTP_TATYPE_PHYSICAL = 0,
    ISOTP_TATYPE_FUNCTIONAL,
};

struct fc_opts
{
    uint8_t bs;
    uint8_t stmin;
};

struct tpcon
{
    int idx;
    int len;
    int state;
    uint8_t bs;
    uint8_t sn;
    uint8_t buf[CONFIG_ISOTP_MAX_MSG_LENGTH];
};

typedef void (*isotp_rx_callback_t)(uint8_t *data, int len, enum isotp_tatype tatype);
typedef void (*isotp_can_tx_func_t)(uint32_t can_id, uint8_t *can_data, uint8_t can_dlc);

struct isotp
{
    struct tpcon rx, tx;
    struct fc_opts rxfc, txfc;
    uint8_t tx_wft;
    enum isotp_tatype tatype;
    uint32_t rxtimer, txtimer;
    uint32_t tx_id;
    isotp_can_tx_func_t tx_func;
    isotp_rx_callback_t rx_cb;
};

/**
 * @brief init the ISO-TP instance
 *
 * @param isotp pointer to instance
 * @param tx_id transmit CAN ID
 * @param tx_func raw can send function
 * @param rx_cb callback function when receive is done
 */
void isotp_init(struct isotp *isotp, uint32_t tx_id, isotp_can_tx_func_t tx_func, isotp_rx_callback_t rx_cb);

/**
 * @brief send data through isotp
 *
 * @param isotp pointer to instance
 * @param data the data to be sent. (Up to CONFIG_ISOTP_MAX_MSG_LENGTH bytes).
 * @param len length of the data to send
 */
void isotp_send(struct isotp *isotp, uint8_t *data, int len);

/**
 * @brief receive incoming CAN message
 *
 * @param isotp pointer to instance
 * @param can_data the data of can frame
 * @param can_dlc the dlc of can frame
 * @param tatype target address type
 */
void isotp_rcv(struct isotp *isotp, uint8_t *can_data, uint8_t can_dlc, enum isotp_tatype tatype);

/**
 * @brief polling function, call this function every 1ms
 *
 * @param isotp pointer to instance
 */
void isotp_poll(struct isotp *isotp);

#endif // ISOTP_H
