#include <stdint.h>
#include "string_utils.h"

#include "../vivado/ibex-demo-system/sw/c/common/demo_system.h"
#include "../vivado/ibex-demo-system/sw/c/common/gpio.h"
#include "../vivado/ibex-demo-system/sw/c/common/uart.h"
#include "aes/aes.h"
#include "aes_wrapper.h"

#define MAX_STR_LEN 256
#define AES_BLOCK_SIZE 16

#define PACKET_START1 0xAA
#define PACKET_START2 0x55
#define FLAG_LAST     0x01

#define MAX_MESSAGE_SIZE 4096
#define MAX_PAYLOAD_SIZE 240

#define TYPE_TEXT   1
#define TYPE_IMAGE  2
#define TYPE_PDF    3
#define TYPE_ZIP    4
#define TYPE_BIN    5

typedef struct
{
    uint32_t file_size;
    uint16_t type;
    uint16_t reserved;
} message_header_t;

static void uart_print_u32(uart_t uart, uint32_t v)
{
    char tmp[12];
    int pos = 0;

    if (v == 0) {
        uart_out(uart, '0');
        return;
    }

    while (v > 0) {
        tmp[pos++] = '0' + (v % 10);
        v /= 10;
    }

    while (pos--)
        uart_out(uart, tmp[pos]);
}

void uart_print_hex(uart_t uart, uint8_t value)
{
    const char hex[] = "0123456789ABCDEF";

    uart_out(uart, hex[(value >> 4) & 0xF]);
    uart_out(uart, hex[value & 0xF]);
}


static void send_string(uart_t uart, const char *str)
{
    while (*str)
        uart_out(uart, *str++);
}

static void send_cipher(uart_t uart, const uint8_t *cipher, int len)
{
    for (int i = 0; i < len; i++) {
        uart_out(uart, cipher[i]);
    }
}

static int receive_cipher(uart_t uart, uint8_t *cipher)
{
    uint16_t len = 0;
    int index = 0;
    int ch;

    /* Receive length (LSB first) */
    do {
        ch = uart_in(uart);
    } while (ch == UART_EOF);
    len = (uint8_t)ch;

    do {
        ch = uart_in(uart);
    } while (ch == UART_EOF);
    len |= ((uint16_t)(uint8_t)ch) << 8;

    /* Receive ciphertext */
    while (index < len) {

        do {
            ch = uart_in(uart);
        } while (ch == UART_EOF);

        cipher[index++] = (uint8_t)ch;
    }

    return len;
}

static int receive_string(uart_t uart, char *buffer, int max_len)
{
    int index = 0;

    while (index < max_len - 1) {

        int ch = uart_in(uart);

        if (ch == UART_EOF)
            continue;
        
        if (ch == '\r' || ch == '\n')
            break;

        buffer[index++] = (char)ch;
        
    }

    buffer[index] = '\0';
    return index;
}

static int pkcs7_pad(uint8_t *buffer, int data_len)
{
    int pad_len = AES_BLOCK_SIZE - (data_len % AES_BLOCK_SIZE);

    /* PKCS#7 always adds a full block if already aligned */
    if (pad_len == 0)
        pad_len = AES_BLOCK_SIZE;

    for (int i = 0; i < pad_len; i++)
        buffer[data_len + i] = (uint8_t)pad_len;

    return data_len + pad_len;
}

static int pkcs7_unpad(uint8_t *buffer, int len)
{
    if (len == 0)
        return -1;

    uint8_t pad_len = buffer[len - 1];

    if (pad_len == 0 || pad_len > AES_BLOCK_SIZE)
        return -1;

    for (int i = 0; i < pad_len; i++) {
        if (buffer[len - 1 - i] != pad_len)
            return -1;
    }

    return len - pad_len;
}
uint16_t crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    return crc;
}


typedef struct
{
    uint16_t packet_no;
    uint16_t total_packets;
    uint16_t length;
    uint8_t flags;
    uint16_t crc;
} packet_t;

int send_packet(uart_t uart,
                uint16_t packet_no,
                uint8_t flags,
                const uint8_t *payload,
                uint16_t payload_len)
{
    uint16_t crc = crc16(payload, payload_len);

    uint16_t total =
            2 +     // packet number
            1 +     // flags
            2 +     // payload length
            payload_len +
            2;      // crc

    uart_out(uart, PACKET_START1);
    uart_out(uart, PACKET_START2);

    uart_out(uart, total & 0xff);
    uart_out(uart, total >> 8);

    uart_out(uart, packet_no & 0xff);
    uart_out(uart, packet_no >> 8);

    uart_out(uart, flags);

    uart_out(uart, payload_len & 0xff);
    uart_out(uart, payload_len >> 8);

    for (uint16_t i=0;i<payload_len;i++)
        uart_out(uart,payload[i]);

    uart_out(uart, crc & 0xff);
    uart_out(uart, crc >> 8);

    uart_out(uart,'\n');

    // send_string(UART0,"TX CRC=");
    // uart_print_hex(UART0,crc & 0xff);
    // uart_print_hex(UART0,crc >> 8);
    // send_string(UART0,"\r\n");

    // send_string(UART0, "Packet total = ");
    // uart_print_u32(UART0, total);
    // send_string(UART0, "\r\n");

    return total;
}

int send_message(uart_t uart,
                 const uint8_t *data,
                 uint32_t len)
{
    static uint16_t packet_no = 0;

    uint32_t offset = 0;

    while (offset < len)
    {
        uint16_t chunk;

        if ((len - offset) > MAX_PAYLOAD_SIZE)
            chunk = MAX_PAYLOAD_SIZE;
        else
            chunk = len - offset;

        uint8_t flags = 0;

        if (offset + chunk == len)
            flags |= FLAG_LAST;

        send_packet(uart,
                    packet_no++,
                    flags,
                    data + offset,
                    chunk);

        offset += chunk;
    }
    

    return len;
}

static int uart_read_byte(uart_t uart, uint8_t *b)
{
    int ch;

    do {
        ch = uart_in(uart);
    } while (ch == UART_EOF);

    *b = (uint8_t)ch;

    uart_print_hex(UART0, *b);
    send_string(UART0, " ");

    return 0;
}


int receive_packet(uart_t uart,
                   packet_t *pkt,
                   uint8_t *payload)
{
    uint8_t b;
    uint16_t total_len;
    uint16_t crc;

    /* Read total packet length */
    // uart_read_byte(uart, &b);

    // total_len = b;

    // uart_read_byte(uart, &b);
    // total_len |= ((uint16_t)b << 8);

    /* Search for packet header */
    while (1)
    {
        uart_read_byte(uart, &b);

        if (b != PACKET_START1)
            continue;

        uint8_t b2;
        uart_read_byte(uart, &b2);

        if (b2 == PACKET_START2)
            break;

        if (b2 == PACKET_START1)
        {
            b = PACKET_START1;
            continue;
        }
    }

    /* Now read packet length */
    uart_read_byte(uart, &b);
    total_len = b;

    uart_read_byte(uart, &b);
    total_len |= ((uint16_t)b << 8);


    /* Sanity check */
    if (total_len < 7)
        return -1;

    /* Packet number */
    uart_read_byte(uart, &b);
    pkt->packet_no = b;

    uart_read_byte(uart, &b);
    pkt->packet_no |= ((uint16_t)b << 8);

    /* Flags */
    uart_read_byte(uart, &pkt->flags);

    /* Payload length */
    uart_read_byte(uart, &b);
    pkt->length = b;

    uart_read_byte(uart, &b);
    pkt->length |= ((uint16_t)b << 8);

    /* Check payload length */
    if (pkt->length > MAX_PAYLOAD_SIZE)
        return -2;

    /* Check total length */
    if (total_len != (uint16_t)(2 + 1 + 2 + pkt->length + 2))
        return -3;

    /* Read payload */
    // for (uint16_t i = 0; i < pkt->length; i++)
    //     uart_read_byte(uart, &payload[i]);
    for (uint16_t i = 0; i < pkt->length; i++)
    {
        if ((i % 16) == 0)
        {
            send_string(UART0, "\r\nIndex=");
            uart_print_u32(UART0, i);
            send_string(UART0, "\r\n");
        }

        uart_read_byte(uart, &payload[i]);
    }

    /* Read CRC */
    uart_read_byte(uart, &b);
    crc = b;

    uart_read_byte(uart, &b);
    crc |= ((uint16_t)b << 8);

    pkt->crc = crc;

    uint16_t calc = crc16(payload, pkt->length);

    // send_string(UART0, "\r\nRX CRC = ");
    // uart_print_hex(UART0, crc >> 8);
    // uart_print_hex(UART0, crc & 0xFF);

    // send_string(UART0, "\r\nCALC   = ");
    // uart_print_hex(UART0, calc >> 8);
    // uart_print_hex(UART0, calc & 0xFF);
    // send_string(UART0, "\r\n");

    /* Verify CRC */
    if (calc != crc)
        return -4;

    // send_string(UART0,"RX CRC=");
    // uart_print_hex(UART0,crc & 0xff);
    // uart_print_hex(UART0,crc >> 8);
    // send_string(UART0,"\r\n");

    // send_string(UART0,"CALC=");
    // uart_print_hex(UART0,calc & 0xff);
    // uart_print_hex(UART0,calc >> 8);
    // send_string(UART0,"\r\n");
        
    uint8_t end;

    uart_read_byte(uart, &end);

    if (end != '\n')
    {
        send_string(UART0, "Delimiter error\r\n");
        return -5;
    }
    return pkt->length;
}

int receive_message(uart_t uart,
                    uint8_t *buffer)
{
    packet_t pkt;

    uint32_t total = 0;
    while (uart_in(UART1) != UART_EOF)
    {
        // discard stale bytes
    }

    while (1)
    {
        int len = receive_packet(uart,
                                 &pkt,
                                 buffer + total);

        if (len < 0)
            return len;

        total += len;

        if (pkt.flags & FLAG_LAST)
            break;
    }

    return total;
}

int main(void)
{

    char send_buffer[MAX_MESSAGE_SIZE + AES_BLOCK_SIZE];
    uint8_t ciphertext[MAX_MESSAGE_SIZE + AES_BLOCK_SIZE];
    uint8_t decrypted[MAX_MESSAGE_SIZE + AES_BLOCK_SIZE];

    send_string(UART0, "Send or Recieve?\r\nBTN3 - Send\r\nBTN2 - Recieve\r\nBTN1 - Activate/Deactivate CLMUL\r\n");
    
    while (1) {

            if (read_gpio(GPIO_IN_DBNC) & (1 << 4)) {
                if(use_clmul == 0)
                    send_string(UART0, "CLMUL is activated\r\n\n");
                use_clmul = 1;
            }
                
            else {
                if(use_clmul == 1)
                    send_string(UART0, "CLMUL is deactivated\r\n\n");
                use_clmul = 0;
            }

            if(read_gpio(GPIO_IN_DBNC) & (1 << 2)) {

                send_string(UART0, "--RECIEVE MODE--\r\n");
                send_string(UART0, "Waiting for message...\r\n");
                
                uint8_t cipher[MAX_MESSAGE_SIZE + AES_BLOCK_SIZE];

                int len = receive_message(UART1, cipher);

                switch(len)
                {
                case -1:
                    send_string(UART0,"Invalid packet\r\n");
                    continue;

                case -2:
                    send_string(UART0,"Payload too large\r\n");
                    continue;

                case -3:
                    send_string(UART0,"Length mismatch\r\n");
                    continue;

                case -4:
                    send_string(UART0,"CRC error\r\n");
                    continue;

                case -5:
                    send_string(UART0,"Delimiter error\r\n");
                    continue;
                }

                send_string(UART0, "Length = ");
                uart_print_u32(UART0, len);
                send_string(UART0, "\r\n");
                
                send_string(UART0, "Received cipher: ");
                for (int i = 0; i < len; i++) {
                    uart_print_u32(UART0, cipher[i]);
                    send_string(UART0, " ");
                }
                send_string(UART0, "\r\n");

                
                uint32_t cycle1 = get_mcycle();
                if (len > 0) {

                    decrypt_message(cipher, decrypted, len);
                    decrypted[len] = '\0';

                }
                
                uint32_t cycle2 = get_mcycle();

                int plain_len = pkcs7_unpad(decrypted, len);

                if (plain_len < 0) {
                    send_string(UART0, "Padding error!\r\n");
                    continue;
                }

                decrypted[plain_len] = '\0';

                send_string(UART0, "Decryption completed in ");
                uart_print_u32(UART0, cycle2 - cycle1);
                send_string(UART0, " cycles\r\n");

                send_string(UART0, "Received message: ");
                send_string(UART0, (char *)decrypted);
                
                send_string(UART0, "\r\n\n");
        
            }

            if(read_gpio(GPIO_IN_DBNC) & (1 << 3)) {

                memset(send_buffer, 0, sizeof(send_buffer));

                send_string(UART0, "--SEND MODE--\r\n");
                send_string(UART0, "Type the message to send:\r\n");
                int len_msg = receive_string(UART0, send_buffer, MAX_MESSAGE_SIZE);
                
                send_string(UART0, "Encrypting... \r\n");
                int padded_len = pkcs7_pad((uint8_t *)send_buffer, len_msg);

                send_string(UART0, "Length = ");
                uart_print_u32(UART0, padded_len);
                send_string(UART0, "\r\n");

                uint32_t cycle1 = get_mcycle();

                encrypt_message((uint8_t *)send_buffer,
                                ciphertext,
                                padded_len);

                uint32_t cycle2 = get_mcycle();

                send_string(UART0, "Encryption completed in ");
                uart_print_u32(UART0, cycle2 - cycle1);
                send_string(UART0, " cycles\r\n");
                
                send_string(UART0, "Sending ");
                uart_print_u32(UART0, padded_len);
                send_string(UART0, " bytes\r\n");

                send_message(UART1, ciphertext, padded_len);
                
                send_string(UART0, "Cipher: ");

                for (int i = 0; i < padded_len; i++) {
                    uart_print_u32(UART0, ciphertext[i]);
                    send_string(UART0, " ");
                }
                send_string(UART0, "\r\n");
                
                send_string(UART0, "Message sent ");
                
                send_string(UART0, "\r\n\n");
            }
            
        
    }

    return 0;
}