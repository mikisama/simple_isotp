#include <string.h>
#include "isotp.h"

/* N_PCI type values in bits 7-4 of N_PCI bytes */
#define N_PCI_SF 0x00 /* single frame */
#define N_PCI_FF 0x10 /* first frame */
#define N_PCI_CF 0x20 /* consecutive frame */
#define N_PCI_FC 0x30 /* flow control */

#define N_PCI_SZ 1      /* size of the PCI byte #1 */
#define SF_PCI_SZ 1     /* size of SingleFrame PCI including 4 bit SF_DL */
#define FF_PCI_SZ 2     /* size of FirstFrame PCI including 12 bit FF_DL */
#define FC_CONTENT_SZ 3 /* flow control content size in byte (FS/BS/STmin) */

/* Flow Status given in FC frame */
#define ISOTP_FC_CTS 0   /* clear to send */
#define ISOTP_FC_WT 1    /* wait */
#define ISOTP_FC_OVFLW 2 /* overflow */

enum
{
    ISOTP_IDLE = 0,
    ISOTP_WAIT_FIRST_FC,
    ISOTP_WAIT_FC,
    ISOTP_WAIT_DATA,
    ISOTP_SENDING
};

static void isotp_send_fc(struct isotp *isotp, uint8_t flowstatus)
{
    uint8_t can_data[8] = {0};
    can_data[0] = N_PCI_FC | flowstatus;
    can_data[1] = isotp->rxfc.bs;
    can_data[2] = isotp->rxfc.stmin;
    isotp->tx_func(isotp->tx_id, can_data, 8);
    isotp->rx.bs = 0;
    isotp->rxtimer = CONFIG_ISOTP_CR_TIMEOUT;
}

static void isotp_rcv_fc(struct isotp *isotp, uint8_t *can_data, uint8_t can_dlc)
{
    if (isotp->tx.state == ISOTP_WAIT_FC ||
        isotp->tx.state == ISOTP_WAIT_FIRST_FC)
    {
        isotp->txtimer = 0;
        if (can_dlc >= FC_CONTENT_SZ)
        {
            if (isotp->tx.state == ISOTP_WAIT_FIRST_FC)
            {
                isotp->txfc.bs = can_data[1];
                isotp->txfc.stmin = can_data[2];
                if ((isotp->txfc.stmin > 0x7F) &&
                    ((isotp->txfc.stmin < 0xF1) || (isotp->txfc.stmin > 0xF9)))
                {
                    isotp->txfc.stmin = 0x7F;
                }
                if (0xF1 <= isotp->txfc.stmin &&
                    isotp->txfc.stmin <= 0xF9)
                {
                    /**
                     * 100 us - 900 us
                     * we use 1ms resolution timer...
                     */
                    isotp->txfc.stmin = 1;
                }
                isotp->tx.state = ISOTP_WAIT_FC;
            }
            switch (can_data[0] & 0x0F)
            {
            case ISOTP_FC_CTS:
                isotp->tx.bs = 0;
                isotp->tx_wft = 0;
                isotp->tx.state = ISOTP_SENDING;
                isotp->txtimer = isotp->txfc.stmin;
                break;
            case ISOTP_FC_WT:
                if (isotp->tx_wft++ >= CONFIG_ISOTP_WFTMAX)
                {
                    isotp->tx.state = ISOTP_IDLE;
                }
                else
                {
                    /* starting waiting for next FC */
                    isotp->txtimer = CONFIG_ISOTP_BS_TIMEOUT;
                }
                break;
            case ISOTP_FC_OVFLW:
                /* overflow in receiver side */
            default:
                isotp->tx.state = ISOTP_IDLE;
                break;
            }
        }
        else
        {
            isotp->tx.state = ISOTP_IDLE;
        }
    }
}

static void isotp_rcv_sf(struct isotp *isotp, uint8_t *can_data, uint8_t can_dlc)
{
    isotp->rxtimer = 0;
    isotp->rx.state = ISOTP_IDLE;
    isotp->rx.len = (can_data[0] & 0x0F);
    if (0 < isotp->rx.len && isotp->rx.len <= can_dlc - SF_PCI_SZ)
    {
        isotp->rx.idx = 0;
        for (int i = SF_PCI_SZ; i < isotp->rx.len + SF_PCI_SZ; i++)
        {
            isotp->rx.buf[isotp->rx.idx++] = can_data[i];
        }
        isotp->rx_cb(isotp->rx.buf, isotp->rx.len, isotp->tatype);
    }
}

static void isotp_rcv_ff(struct isotp *isotp, uint8_t *can_data, uint8_t can_dlc)
{
    isotp->rxtimer = 0;
    isotp->rx.state = ISOTP_IDLE;
    isotp->rx.len = (can_data[0] & 0x0F) << 8;
    isotp->rx.len += can_data[1];
    if (isotp->rx.len >= 8)
    {
        if (isotp->rx.len <= CONFIG_ISOTP_MAX_MSG_LENGTH)
        {
            isotp->rx.idx = 0;
            for (int i = FF_PCI_SZ; i < can_dlc; i++)
            {
                isotp->rx.buf[isotp->rx.idx++] = can_data[i];
            }
            isotp->rx.sn = 1;
            isotp->rx.state = ISOTP_WAIT_DATA;
            isotp_send_fc(isotp, ISOTP_FC_CTS);
        }
        else
        {
            isotp_send_fc(isotp, ISOTP_FC_OVFLW);
        }
    }
}

static void isotp_rcv_cf(struct isotp *isotp, uint8_t *can_data, uint8_t can_dlc)
{
    if (isotp->rx.state == ISOTP_WAIT_DATA)
    {
        isotp->rxtimer = 0;
        if ((can_data[0] & 0x0F) == isotp->rx.sn)
        {
            isotp->rx.sn++;
            if (isotp->rx.sn > 15)
            {
                isotp->rx.sn = 0;
            }
            for (int i = N_PCI_SZ; i < can_dlc && isotp->rx.idx < isotp->rx.len; i++)
            {
                isotp->rx.buf[isotp->rx.idx++] = can_data[i];
            }
            if (isotp->rx.idx < isotp->rx.len)
            {
                if (!isotp->rxfc.bs || ++isotp->rx.bs < isotp->rxfc.bs)
                {
                    isotp->rxtimer = CONFIG_ISOTP_CR_TIMEOUT;
                }
                else
                {
                    isotp_send_fc(isotp, ISOTP_FC_CTS);
                }
            }
            else
            {
                isotp->rx_cb(isotp->rx.buf, isotp->rx.len, isotp->tatype);
                isotp->rx.state = ISOTP_IDLE;
            }
        }
        else
        {
            isotp->rx.state = ISOTP_IDLE;
        }
    }
}

static void isotp_send_sf(struct isotp *isotp)
{
    uint8_t can_data[8] = {0};
    can_data[0] = N_PCI_SF | isotp->tx.len;
    for (int i = 0; i < isotp->tx.len; i++)
    {
        can_data[SF_PCI_SZ + i] = isotp->tx.buf[isotp->tx.idx++];
    }
    isotp->tx_func(isotp->tx_id, can_data, 8);
    isotp->tx.state = ISOTP_IDLE;
}

static void isotp_send_ff(struct isotp *isotp)
{
    uint8_t can_data[8] = {0};
    can_data[0] = N_PCI_FF | ((isotp->tx.len >> 8) & 0x0F);
    can_data[1] = isotp->tx.len & 0xFF;
    for (int i = 0; i < 6; i++)
    {
        can_data[FF_PCI_SZ + i] = isotp->tx.buf[isotp->tx.idx++];
    }
    isotp->tx_func(isotp->tx_id, can_data, 8);
    isotp->tx.sn = 1;
    isotp->tx.state = ISOTP_WAIT_FIRST_FC;
    isotp->txtimer = CONFIG_ISOTP_BS_TIMEOUT;
}

static void isotp_send_cf(struct isotp *isotp)
{
    uint8_t remain = (isotp->tx.len - isotp->tx.idx);
    uint8_t num = (remain < 7) ? remain : 7;
    uint8_t can_data[8] = {0};
    can_data[0] = N_PCI_CF | isotp->tx.sn++;
    if (isotp->tx.sn > 15)
    {
        isotp->tx.sn = 0;
    }
    isotp->tx.bs++;
    for (int i = 0; i < num; i++)
    {
        can_data[N_PCI_SZ + i] = isotp->tx.buf[isotp->tx.idx++];
    }
    isotp->tx_func(isotp->tx_id, can_data, 8);
    if (isotp->tx.idx < isotp->tx.len)
    {
        if (isotp->txfc.bs && isotp->tx.bs >= isotp->txfc.bs)
        {
            isotp->tx.state = ISOTP_WAIT_FC;
            isotp->txtimer = CONFIG_ISOTP_BS_TIMEOUT;
        }
        else
        {
            isotp->txtimer = isotp->txfc.stmin;
        }
    }
    else
    {
        isotp->tx.state = ISOTP_IDLE;
    }
}

void isotp_init(struct isotp *isotp, uint32_t tx_id, isotp_can_tx_func_t tx_func, isotp_rx_callback_t rx_cb)
{
    isotp->rx.state = ISOTP_IDLE;
    isotp->tx.state = ISOTP_IDLE;
    isotp->rxfc.bs = CONFIG_ISOTP_BS;
    isotp->rxfc.stmin = CONFIG_ISOTP_STMIN;
    isotp->rxtimer = 0;
    isotp->txtimer = 0;
    isotp->tx_id = tx_id;
    isotp->tx_func = tx_func;
    isotp->rx_cb = rx_cb;
}

void isotp_send(struct isotp *isotp, uint8_t *data, int len)
{
    if (isotp->tx.state == ISOTP_IDLE && len <= CONFIG_ISOTP_MAX_MSG_LENGTH)
    {
        memcpy(isotp->tx.buf, data, len);
        isotp->tx.state = ISOTP_SENDING;
        isotp->tx.len = len;
        isotp->tx.idx = 0;
        if (len < 8)
        {
            isotp_send_sf(isotp);
        }
        else
        {
            isotp_send_ff(isotp);
        }
    }
}

void isotp_rcv(struct isotp *isotp, uint8_t *can_data, uint8_t can_dlc, enum isotp_tatype tatype)
{
    uint8_t n_pci_type = can_data[0] & 0xF0;
    /* we support half-duplex communication */
    if ((n_pci_type == N_PCI_FC && isotp->rx.state == ISOTP_IDLE) ||
        (n_pci_type != N_PCI_FC && isotp->tx.state == ISOTP_IDLE))
    {
        /**
         * physical addressing (1 to 1 communication) shall be supported for all types of network layer messages
         * functional addressing (1 to n communication) shall only be supported for singleframe transmission
         */
        if (tatype == ISOTP_TATYPE_PHYSICAL || n_pci_type == N_PCI_SF)
        {
            isotp->tatype = tatype;
            switch (n_pci_type)
            {
            case N_PCI_FC:
                isotp_rcv_fc(isotp, can_data, can_dlc);
                break;
            case N_PCI_SF:
                isotp_rcv_sf(isotp, can_data, can_dlc);
                break;
            case N_PCI_FF:
                isotp_rcv_ff(isotp, can_data, can_dlc);
                break;
            case N_PCI_CF:
                isotp_rcv_cf(isotp, can_data, can_dlc);
                break;
            }
        }
    }
}

void isotp_poll(struct isotp *isotp)
{
    switch (isotp->tx.state)
    {
    case ISOTP_WAIT_FC:
    case ISOTP_WAIT_FIRST_FC:
        if (isotp->txtimer > 0)
        {
            isotp->txtimer--;
            if (isotp->txtimer == 0)
            {
                /* Bs timeout */
                isotp->tx.state = ISOTP_IDLE;
            }
        }
        break;
    case ISOTP_SENDING:
        if (isotp->txfc.stmin)
        {
            if (isotp->txtimer > 0)
            {
                isotp->txtimer--;
                if (isotp->txtimer == 0)
                {
                    /* STmin timeout */
                    isotp_send_cf(isotp);
                }
            }
        }
        else
        {
            /* STmin off */
            isotp_send_cf(isotp);
        }
        break;
    }

    switch (isotp->rx.state)
    {
    case ISOTP_WAIT_DATA:
        if (isotp->rxtimer > 0)
        {
            isotp->rxtimer--;
            if (isotp->rxtimer == 0)
            {
                /* Cr timeout */
                isotp->rx.state = ISOTP_IDLE;
            }
        }
        break;
    }
}
