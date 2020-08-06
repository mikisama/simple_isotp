# Simple ISO-TP Library

## Sample

```C
#include "isotp.h"

void isotp_rx_cb(uint8_t *data, int len)
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

int main(void)
{
    struct isotp isotp;
    uint32_t can_id;
    uint8_t can_data[8];
    uint8_t can_dlc;

    isotp_init(&isotp, 0x234, raw_can_send, isotp_rx_cb);

    while (1)
    {
        int recv_ok = raw_can_recv(&can_id, can_data, &can_dlc);
        if (recv_ok && can_id == 0x123)
        {
            isotp_rcv(&isotp, can_data, can_dlc);
        }
        isotp_poll(&isotp);
        delay_1ms();
    }
}

```
