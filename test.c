#include <stdio.h>
#include "isotp.h"

void raw_can_send(uint32_t can_id, uint8_t *can_data, uint8_t can_dlc)
{
    printf("can send\n");
    for (int i = 0; i < can_dlc; i++)
    {
        printf("%02x ", can_data[i]);
    }
    printf("\n");
}

void isotp_rx_cb(uint8_t *data, int len)
{
    printf("rx complete\n");
    for (int i = 0; i < len; i++)
    {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

int main(void)
{
    struct isotp isotp;
    isotp_init(&isotp, 0x234, raw_can_send, isotp_rx_cb);

    //// rx test
    uint8_t sim_can_data[][8] = {
        {0x07, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
        {0x10, 0x10, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        {0x21, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d},
        {0x22, 0x0e, 0x0f, 0x10, 0x00, 0x00, 0x00, 0x00},
    };

    // rx single frame
    isotp_rcv(&isotp, sim_can_data[0], 8);
    isotp_poll(&isotp);

    // rx multi frame
    isotp_rcv(&isotp, sim_can_data[1], 8);
    isotp_poll(&isotp);
    isotp_rcv(&isotp, sim_can_data[2], 8);
    isotp_poll(&isotp);
    isotp_rcv(&isotp, sim_can_data[3], 8);
    isotp_poll(&isotp);

    //// tx test

    // tx single frame
    uint8_t tx_data_0[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    isotp_send(&isotp, tx_data_0, sizeof(tx_data_0));
    isotp_poll(&isotp);

    // tx multi frame
    uint8_t tx_data_1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    uint8_t sim_fc[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    isotp_send(&isotp, tx_data_1, sizeof(tx_data_1));
    isotp_rcv(&isotp, sim_fc, 8);
    isotp_poll(&isotp);
    isotp_poll(&isotp);
    isotp_poll(&isotp);

    return 0;
}
