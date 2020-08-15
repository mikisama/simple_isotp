# Simple ISO-TP Library

## Sample

```C
#include "isotp.h"

void isotp_rx_cb(uint8_t *data, int len, enum isotp_tatype tatype)
{
    /* handle received isotp data, usually UDS */
}

void raw_can_send(uint32_t can_id, uint8_t *can_data, uint8_t can_dlc)
{
    /* send can frame */
}

int raw_can_recv(uint32_t *can_id, uint8_t *can_data, uint8_t *can_dlc)
{
    /* receive can frame */
}

void delay_1ms(void)
{
    /* delay_1ms */
}

#define ISOTP_TX_ID 0x111
#define ISOTP_RX_PHY_ID 0x222
#define ISOTP_RX_FUN_ID 0x333

int main(void)
{
    struct isotp isotp;
    uint32_t can_id;
    uint8_t can_data[8];
    uint8_t can_dlc;

    isotp_init(&isotp, ISOTP_TX_ID, raw_can_send, isotp_rx_cb);

    while (1)
    {
        int recv_ok = raw_can_recv(&can_id, can_data, &can_dlc);
        if (recv_ok)
        {
            switch (can_id)
            {
            case ISOTP_RX_PHY_ID:
                isotp_rcv(&isotp, can_data, can_dlc, ISOTP_TATYPE_PHYSICAL);
                break;
            case ISOTP_RX_FUN_ID:
                isotp_rcv(&isotp, can_data, can_dlc, ISOTP_TATYPE_FUNCTIONAL);
                break;
            }
        }
        isotp_poll(&isotp);
        delay_1ms();
    }
}

```
