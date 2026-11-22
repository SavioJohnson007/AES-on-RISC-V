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

#define ACK_START1 0xCC
#define ACK_START2 0x33

#define ESC_BYTE 0x1B
#define ESC_XOR  0x20
#define MAX_PAYLOAD_SIZE 224

#define PACKET_LOGICAL_MAX (2 + 1 + 2 + MAX_PAYLOAD_SIZE + 2)
#define PACKET_PAD_BYTE 0xFF

#define CPU_FREQ_HZ       50000000UL
#define ACK_TIMEOUT_SEC   3UL
#define ACK_TIMEOUT_CYCLES (CPU_FREQ_HZ * ACK_TIMEOUT_SEC)

#define SEG7_BASE 0x80006000UL
#define SEG7_REG   (*(volatile uint32_t *)SEG7_BASE)

#define MAX_FILE_SIZE (40 * 1024)

/* Packet numbering is continuous across header + ciphertext. */
static uint16_t tx_packet_no = 0;

static uint32_t received_file_size = 0;
static volatile int received_file_ready = 0;

#define BTN1_TIMEOUT_SEC     10UL
#define BTN1_TIMEOUT_CYCLES  (CPU_FREQ_HZ * BTN1_TIMEOUT_SEC)

typedef struct
{
    uint16_t packet_no;
    uint16_t length;
    uint8_t flags;
    uint16_t crc;
} packet_t;

static void uart_out_escaped(uart_t uart, uint8_t b)
{
    if (b == 0x0A || b == 0x0D || b == ESC_BYTE)
    {
        uart_out(uart, ESC_BYTE);
        uart_out(uart, b ^ ESC_XOR);
    }
    else
    {
        uart_out(uart, b);
    }
}

static void seg7_display_decimal(uint32_t value)
{
    uint32_t display = 0;

    for (int i = 0; i < 8; i++)
    {
        display |= (value % 10) << (i * 4);
        value /= 10;
    }

    SEG7_REG = display;
}

typedef struct __attribute__((packed))
{
    uint8_t start1;
    uint8_t start2;
    uint16_t packet_no;
} ack_packet_t;



static char filename[256];

static uint8_t plain_buffer[MAX_FILE_SIZE + AES_BLOCK_SIZE];
static uint8_t cipher_buffer[MAX_FILE_SIZE + AES_BLOCK_SIZE];

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

typedef struct __attribute__((packed))
{
    char magic[4];
    uint8_t type;
    uint16_t filename_len;
    uint64_t file_size;
} host_header_t;

static host_header_t host_hdr;

static void delay(volatile uint32_t count)
{
    while (count--) {
        asm volatile ("nop");
    }
}

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

static int uart_read_byte(uart_t uart, uint8_t *b)
{
    int ch;

    do {
        ch = uart_in(uart);
    } while (ch == UART_EOF);

    *b = (uint8_t)ch;

    return 0;
}

static void uart_read_logical_byte(uart_t uart, uint8_t *out, int *is_terminator)
{
    uint8_t b;
    uart_read_byte(uart, &b);

    if (b == 0x0A)
    {
        *is_terminator = 1;
        return;
    }

    *is_terminator = 0;

    if (b == ESC_BYTE)
    {
        uint8_t b2;
        uart_read_byte(uart, &b2);
        *out = b2 ^ ESC_XOR;
    }
    else
    {
        *out = b;
    }
}

static void send_string(uart_t uart, const char *str)
{
    while (*str)
        uart_out(uart, *str++);
}

static void send_bytes(uart_t uart,
                       const uint8_t *data,
                       uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        uart_out(uart, data[i]);
}

static int receive_bytes(uart_t uart,
                         uint8_t *buffer,
                         uint32_t len)
{
    uint32_t index = 0;

    while (index < len)
    {
        int ch = uart_in(uart);

        if (ch == UART_EOF)
            continue;

        buffer[index++] = (uint8_t)ch;
    }

    return index;
}

void send_ack(uart_t uart, uint16_t packet_no)
{
    uart_out(uart, ACK_START1);
    uart_out(uart, ACK_START2);

    uart_out(uart, packet_no & 0xFF);
    uart_out(uart, packet_no >> 8);
    uart_out(uart, '\n');

    send_string(UART0, "ACK sent for packet ");
    uart_print_u32(UART0, packet_no);
    send_string(UART0, "\r\n");
}


int wait_for_ack(uart_t uart, uint16_t expected_packet)
{
    uint64_t start = get_mcycle();

    uint8_t state = 0;
    uint8_t lo = 0;
    uint8_t hi = 0;

    while ((get_mcycle() - start) < ACK_TIMEOUT_CYCLES)
    {
        int ch = uart_in(uart);

        if (ch == UART_EOF)
            continue;

        uint8_t b = (uint8_t)ch;

        switch (state)
        {
            /*
             * Waiting for first ACK byte: CC
             */
            case 0:
                if (b == ACK_START1)
                    state = 1;
                break;

            /*
             * Got CC, waiting for second ACK byte: 33
             */
            case 1:
                if (b == ACK_START2)
                {
                    state = 2;
                }
                else if (b == ACK_START1)
                {
                    /*
                     * Another CC could be the beginning
                     * of a new ACK.
                     */
                    state = 1;
                }
                else
                {
                    state = 0;
                }
                break;

            /*
             * Packet number low byte
             */
            case 2:
                lo = b;
                state = 3;
                break;

            /*
             * Packet number high byte
             */
            case 3:
                hi = b;

                {
                    uint16_t packet =
                        (uint16_t)lo |
                        ((uint16_t)hi << 8);

                    if (packet == expected_packet)
                    {
                        /*
                         * ACK received.
                         */
                        return 0;
                    }
                }

                /*
                 * Wrong packet number.
                 * Go back to looking for a new ACK.
                 */
                state = 0;
                break;
        }
    }

    /*
     * 1 second elapsed without
     * receiving the correct ACK.
     */
    return 1;
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

int send_packet(uart_t uart,
                uint16_t packet_no,
                uint8_t flags,
                const uint8_t *payload,
                uint16_t payload_len)
{
    uint16_t crc = crc16(payload, payload_len);

    uart_out(uart, PACKET_START1);
    uart_out(uart, PACKET_START2);

    uart_out_escaped(uart, packet_no & 0xff);
    uart_out_escaped(uart, packet_no >> 8);

    uart_out_escaped(uart, flags);

    uart_out_escaped(uart, payload_len & 0xff);
    uart_out_escaped(uart, payload_len >> 8);

    for (uint16_t i = 0; i < payload_len; i++)
        uart_out_escaped(uart, payload[i]);

    uart_out_escaped(uart, crc & 0xff);
    uart_out_escaped(uart, crc >> 8);

    uart_out(uart, '\n');

    send_string(UART0,"TX packet ");
    uart_print_u32(UART0,packet_no);
    send_string(UART0,"\r\n");

    send_string(UART0, "Packet data: ");

    for (uint16_t i = 0; i < payload_len; i++)
    {
        uart_print_hex(UART0, payload[i]);
        uart_out(UART0, ' ');
    }

    send_string(UART0, "\r\n");

    return payload_len;
}



int send_message(uart_t uart,
                 const uint8_t *data,
                 uint32_t len)
{
    uint32_t offset = 0;

    while (offset < len)
    {
        uint16_t chunk;

        if ((len - offset) > MAX_PAYLOAD_SIZE)
            chunk = MAX_PAYLOAD_SIZE;
        else
            chunk = (uint16_t)(len - offset);

        /* Every packet carries exactly 96 payload bytes. */
        uint8_t packet_payload[MAX_PAYLOAD_SIZE];

        for (uint16_t i = 0; i < MAX_PAYLOAD_SIZE; i++)
            packet_payload[i] = PACKET_PAD_BYTE;

        for (uint16_t i = 0; i < chunk; i++)
            packet_payload[i] = data[offset + i];

        uint8_t flags = 0;

        if (offset + chunk == len)
            flags |= FLAG_LAST;

        int retries = 0;
        while(retries < 3){

            send_packet(uart,
                    tx_packet_no,
                    flags,
                    packet_payload,
                    MAX_PAYLOAD_SIZE);

            // delay(200000);
            int ack_result = wait_for_ack(uart, tx_packet_no);
            retries += 1;

            if (ack_result == 0) {
                break;
            }
            else {
                send_string(UART0, "ACK timeout for packet ");
                uart_print_u32(UART0, tx_packet_no);
                send_string(UART0, ", retransmitting...\r\n");
            }
        }

        if(retries == 5){
            send_string(UART0, "Failed to receive ACK for packet ");
            uart_print_u32(UART0, tx_packet_no);
            send_string(UART0, " after 3 attempts. Aborting transfer.\r\n");
            return -1;
        }
        

        // wait_for_ack(uart, tx_packet_no);
        // delay(200000); 

        tx_packet_no++;
        offset += chunk;
    }

    return len;
}

int receive_packet(uart_t uart,
                   packet_t *pkt,
                   uint8_t *payload)
{

    uint8_t cur;
    uart_read_byte(uart, &cur);

    while (1)
    {
        if (cur != PACKET_START1)
        {
            uart_read_byte(uart, &cur);
            continue;
        }

        uint8_t next;
        uart_read_byte(uart, &next);

        if (next == PACKET_START2)
            break;

        cur = next;
    }

    uint8_t logical[PACKET_LOGICAL_MAX];
    uint16_t logical_len = 0;

    while (1)
    {
        uint8_t b;
        int is_term;

        uart_read_logical_byte(uart, &b, &is_term);

        if (is_term)
            break;

        if (logical_len >= PACKET_LOGICAL_MAX)
            return -6; /* malformed / oversized packet */

        logical[logical_len++] = b;
    }

    send_string(UART0, "\r\nRAW PACKET: ");

    for (uint16_t i = 0; i < logical_len; i++)
    {
        uart_print_hex(UART0, logical[i]);
        uart_out(UART0, ' ');
    }

    send_string(UART0, "\r\n");

    if (logical_len < 7)
        return -1;

    uint16_t packet_no  = logical[0] | ((uint16_t)logical[1] << 8);
    uint8_t  flags      = logical[2];
    uint16_t payload_len = logical[3] | ((uint16_t)logical[4] << 8);

    if (payload_len > MAX_PAYLOAD_SIZE)
        return -2;

    if (logical_len != (uint16_t)(5 + payload_len + 2))
    {
        send_string(UART0, "\r\nLength mismatch\r\n");

        send_string(UART0, "Logical=");
        uart_print_u32(UART0, logical_len);

        send_string(UART0, " Expected=");
        uart_print_u32(UART0, 5 + payload_len + 2);

        send_string(UART0, " Payload=");
        uart_print_u32(UART0, payload_len);

        send_string(UART0, "\r\n");

        return -3;
    }

    for (uint16_t i = 0; i < payload_len; i++)
        payload[i] = logical[5 + i];

    uint16_t crc = logical[5 + payload_len] |
                   ((uint16_t)logical[5 + payload_len + 1] << 8);

    uint16_t calc = crc16(payload, payload_len);

    if (calc != crc)
        return -4;

    pkt->packet_no = packet_no;
    pkt->flags = flags;
    pkt->length = payload_len;
    pkt->crc = crc;

    return payload_len;
}

int receive_message(uart_t uart,
                    uint8_t *buffer,
                    uint32_t expected_len)
{
    packet_t pkt;
    uint32_t total = 0;
    uint8_t packet_payload[MAX_PAYLOAD_SIZE];
    uint16_t expected_packet = 0;

    while (total < expected_len)
    {
        int len = receive_packet(uart,
                                 &pkt,
                                 packet_payload);

        /* Do not ACK invalid packets, including CRC failures. */
        if (len < 0){
            send_string(UART0, "Bytes = ");
            uart_print_u32(UART0, len);
            send_string(UART0, "\r\n");
            send_string(UART0, "Packet receive failed...waiting for retransmission\r\n");
            continue;
            // return len;
        }
            

        if (pkt.length != MAX_PAYLOAD_SIZE)
        {
            send_string(UART0, "Invalid packet payload size\r\n");
            continue;
        }

        
        if (pkt.packet_no < expected_packet)
        {
            // Retransmitted packet that was already accepted.
            send_string(UART0, "Duplicate packet received, sending ACK for packet\r\n");
            send_ack(uart, pkt.packet_no);
            continue;
        }

        
        send_string(UART0, "\r\nPACKET RECEIVED\r\n");

        send_string(UART0, "Packet=");
        uart_print_u32(UART0, pkt.packet_no);

        send_string(UART0, " Expected Packet=");
        uart_print_u32(UART0, expected_packet);

        send_string(UART0, " Flags=");
        uart_print_u32(UART0, pkt.flags);

        send_string(UART0, " Length=");
        uart_print_u32(UART0, pkt.length);

        send_string(UART0, "\r\n");

        
        uint32_t remaining = expected_len - total;
        uint32_t copy_len = (remaining < pkt.length) ? remaining : pkt.length;

        for (uint32_t i = 0; i < copy_len; i++)
            buffer[total + i] = packet_payload[i];

        total += copy_len;

        send_ack(uart, pkt.packet_no);

        expected_packet = pkt.packet_no + 1;

        send_string(UART0, "Copied=");
        uart_print_u32(UART0, copy_len);

        send_string(UART0, " Total=");
        uart_print_u32(UART0, total);

        send_string(UART0, "\r\n");
    }

    return total;
}

#define HOST_START1 0xD3
#define HOST_START2 0x91
#define HOST_START3 0x7E
#define HOST_START4 0x42

static void send_host_file(
    const host_header_t *header,
    const uint8_t *data,
    uint32_t len)
{
    /*
     * Everything before this point can be terminal/debug output.
     */

    send_string(UART0, "Sending file to host...\r\n");

    /*
     * Dedicated binary synchronization marker.
     */
    uart_out(UART0, HOST_START1);
    uart_out(UART0, HOST_START2);
    uart_out(UART0, HOST_START3);
    uart_out(UART0, HOST_START4);

    /*
     * Binary header.
     */
    send_bytes(
        UART0,
        (const uint8_t *)header,
        sizeof(host_header_t));

    /*
     * Binary file data.
     */
    send_bytes(
        UART0,
        data,
        len);

    /*
     * Do NOT print anything here.
     *
     * The receiver needs to know that the exact number of
     * bytes from the header have been received.
     */
}

void send_mode(void)
{
    /* Start a new transfer at packet 0. */
    tx_packet_no = 0;

    uint64_t file_size;
    uint16_t filename_len;
    uint32_t padded_size;
    uint32_t cipher_size;

    /*
     * Receive Host Header
     */


    receive_bytes(
        UART0,
        (uint8_t *)&host_hdr,
        sizeof(host_header_t));

    /*
     * Validate header
     */

    if (host_hdr.magic[0] != 'F' ||
        host_hdr.magic[1] != 'P' ||
        host_hdr.magic[2] != 'G' ||
        host_hdr.magic[3] != 'A')
    {
        send_string(UART0, "Invalid header\r\n");
        return;
    }

    file_size = host_hdr.file_size;
    filename_len = host_hdr.filename_len;

    uint64_t original_file_size = file_size;

    if (filename_len >= sizeof(filename))
    {
        send_string(UART0, "Filename too long\r\n");
        return;
    }

    if (file_size > MAX_FILE_SIZE)
    {
        send_string(UART0, "File too large\r\n");
        return;
    }

    /*
     * Receive filename
     */

    receive_bytes(
        UART0,
        (uint8_t *)filename,
        filename_len);

    filename[filename_len] = '\0';

    /*
     * Receive entire file
     */

    send_string(UART0, "Receiving file...\r\n");

    receive_bytes(
        UART0,
        plain_buffer,
        (uint32_t)file_size);

    send_string(UART0, "\r\n===== RECEIVED FROM PC =====\r\n");
    send_string(UART0, "Bytes = ");
    uart_print_u32(UART0, file_size);
    send_string(UART0, "\r\n");

    for (uint32_t i = 0; i < file_size; i++)
    {
        uart_print_hex(UART0, plain_buffer[i]);
        uart_out(UART0, ' ');

        if ((i + 1) % 16 == 0)
            send_string(UART0, "\r\n");
    }

    send_string(UART0, "\r\n");

    send_string(UART0, "Receive complete\r\n");

    /*
     * Pad plaintext
     */

    padded_size = pkcs7_pad(
        plain_buffer,
        (uint32_t)file_size);
    
    uint64_t original_size = host_hdr.file_size;
    /*
     * Encrypt complete file
     */

    uint32_t cycle1 = get_mcycle();

    encrypt_message(
        plain_buffer,
        cipher_buffer,
        padded_size);

    uint32_t cycle2 = get_mcycle();

    // send_string(UART0, "Encryption completed in ");
    // uart_print_u32(UART0, cycle2 - cycle1);
    // send_string(UART0, " cycles\r\n");
    uint32_t encryption_cycles = cycle2 - cycle1;

    /*
    * Calculate encryption bitrate
    *
    * bitrate = (plaintext_bytes * 8 * CPU_frequency) / cycles
    */

    uint64_t encryption_bitrate =
    ((uint64_t)file_size * 8ULL * CPU_FREQ_HZ)
    / encryption_cycles;

    uint64_t encryption_bytes_per_sec =
        ((uint64_t)file_size * CPU_FREQ_HZ)
        / encryption_cycles;

    uint64_t cycles_per_byte =
        (uint64_t)encryption_cycles / file_size;

    // send_string(UART0, "Encryption completed in ");
    // uart_print_u32(UART0, encryption_cycles);
    // send_string(UART0, " cycles\r\n");

    // send_string(UART0, "Encryption bitrate = ");
    // uart_print_u32(UART0, (uint32_t)encryption_bitrate);
    // send_string(UART0, " bps\r\n");

    seg7_display_decimal((uint32_t)encryption_bitrate);

    // send_string(UART0, "Encryption throughput = ");
    // uart_print_u32(UART0, (uint32_t)encryption_bytes_per_sec);
    // send_string(UART0, " bytes/s\r\n");

    // send_string(UART0, "Encryption cycles/byte = ");
    // uart_print_u32(UART0, (uint32_t)cycles_per_byte);
    // send_string(UART0, "\r\n");

    send_string(UART0, "\r\n===== SENDING OVER BLUETOOTH =====\r\n");
    send_string(UART0, "Bytes = ");
    uart_print_u32(UART0, padded_size);
    send_string(UART0, "\r\n");

    for (uint32_t i = 0; i < padded_size; i++)
    {
        uart_print_hex(UART0, cipher_buffer[i]);
        uart_out(UART0, ' ');

        if ((i + 1) % 16 == 0)
            send_string(UART0, "\r\n");
    }

    send_string(UART0, "\r\n");

    cipher_size = padded_size;
    host_hdr.file_size = cipher_size;

    /*
     * Send header over BLE
     */

    send_message(
        UART1,
        (uint8_t *)&host_hdr,
        sizeof(host_header_t));

    /*
     * Send ciphertext over BLE
     */

    send_message(
        UART1,
        cipher_buffer,
        cipher_size);

    send_string(UART0, "Transfer Complete\r\n");
    
    send_string(UART0, "File Size = ");
    uart_print_u32(UART0, (uint64_t)file_size);
    send_string(UART0, " bytes\r\n");

    send_string(UART0, "Encryption completed in ");
    uart_print_u32(UART0, encryption_cycles);
    send_string(UART0, " cycles\r\n");

    send_string(UART0, "Encryption bitrate = ");
    uart_print_u32(UART0, (uint32_t)encryption_bitrate);
    send_string(UART0, " bps\r\n");

    seg7_display_decimal((uint32_t)encryption_bitrate);

    send_string(UART0, "Encryption throughput = ");
    uart_print_u32(UART0, (uint32_t)encryption_bytes_per_sec);
    send_string(UART0, " bytes/s\r\n");

    send_string(UART0, "Encryption cycles/byte = ");
    uart_print_u32(UART0, (uint32_t)cycles_per_byte);
    send_string(UART0, "\r\n");
}

void receive_mode(void)
{
    uint32_t cipher_size;
    int plain_size;

    /*
     * Receive header
     */
    int len = receive_message(
                    UART1,
                    (uint8_t *)&host_hdr,
                    sizeof(host_header_t));

    if (len < 0)
    {
        send_string(UART0, "Header receive failed\r\n");
        return;
    }

    if (host_hdr.magic[0] != 'F' ||
        host_hdr.magic[1] != 'P' ||
        host_hdr.magic[2] != 'G' ||
        host_hdr.magic[3] != 'A')
    {
        send_string(UART0, "Invalid header\r\n");
        return;
    }

    cipher_size = (uint32_t)host_hdr.file_size;

    if (cipher_size > (MAX_FILE_SIZE + AES_BLOCK_SIZE))
    {
        send_string(UART0, "File too large\r\n");
        return;
    }

    send_string(UART0, "Receiving encrypted data...\r\n");

    /*
     * Receive ciphertext
     */
    len = receive_message(
              UART1,
              cipher_buffer,
              cipher_size);

    send_string(UART0, "\r\n===== RECEIVED FROM BLUETOOTH =====\r\n");
    send_string(UART0, "Bytes = ");
    uart_print_u32(UART0, len);
    send_string(UART0, "\r\n");

    if (len > 0)
    {
        for (int i = 0; i < len; i++)
        {
            uart_print_hex(UART0, cipher_buffer[i]);
            uart_out(UART0, ' ');

            if ((i + 1) % 16 == 0)
                send_string(UART0, "\r\n");
        }

        send_string(UART0, "\r\n");
    }

    if (len < 0)
    {
        send_string(UART0, "Cipher receive failed\r\n");
        return;
    }

    send_string(UART0, "\r\n Expected=");
    uart_print_u32(UART0, cipher_size);

    send_string(UART0, " Received=");
    uart_print_u32(UART0, len);

    send_string(UART0, "\r\n");

    if ((uint32_t)len != cipher_size)
    {
        send_string(UART0, "Cipher size mismatch\r\n");
        return;
    }

    /*
     * Decrypt
     */
    uint32_t cycle1 = get_mcycle();

    decrypt_message(
        cipher_buffer,
        plain_buffer,
        cipher_size);

    uint32_t cycle2 = get_mcycle();

    // send_string(UART0, "Decryption completed in ");
    // uart_print_u32(UART0, cycle2 - cycle1);
    // send_string(UART0, " cycles\r\n");

    uint32_t decryption_cycles = cycle2 - cycle1;

    /*
    * Calculate decryption bitrate
    *
    * bitrate = (plaintext_bytes * 8 * CPU_frequency) / cycles
    */

    uint64_t decryption_bitrate =
    ((uint64_t)len * 8ULL * CPU_FREQ_HZ)
    / decryption_cycles;

    uint64_t decryption_bytes_per_sec =
        ((uint64_t)len * CPU_FREQ_HZ)
        / decryption_cycles;

    uint64_t cycles_per_byte =
        (uint64_t)decryption_cycles / len;

    // send_string(UART0, "Decryption completed in ");
    // uart_print_u32(UART0, decryption_cycles);
    // send_string(UART0, " cycles\r\n");

    // send_string(UART0, "Decryption bitrate = ");
    // uart_print_u32(UART0, (uint32_t)decryption_bitrate);
    // send_string(UART0, " bps\r\n");

    seg7_display_decimal((uint32_t)decryption_bitrate);

    // send_string(UART0, "Decryption throughput = ");
    // uart_print_u32(UART0, (uint32_t)decryption_bytes_per_sec);
    // send_string(UART0, " bytes/s\r\n");

    // send_string(UART0, "Decryption cycles/byte = ");
    // uart_print_u32(UART0, (uint32_t)cycles_per_byte);
    // send_string(UART0, "\r\n");


    /*
     * Remove PKCS#7 padding
     */
    plain_size = pkcs7_unpad(
                    plain_buffer,
                    cipher_size);

    
    send_string(UART0, "\r\n===== DECRYPTED DATA =====\r\n");
    send_string(UART0, "Bytes = ");
    uart_print_u32(UART0, plain_size);
    send_string(UART0, "\r\n");

    for (uint32_t i = 0; i < plain_size; i++)
    {
        uart_print_hex(UART0, plain_buffer[i]);
        uart_out(UART0, ' ');

        if ((i + 1) % 16 == 0)
            send_string(UART0, "\r\n");
    }

    send_string(UART0, "\r\n");


    if (plain_size < 0)
    {
        send_string(UART0, "Padding error\r\n");
        return;
    }

    host_hdr.file_size = (uint64_t)plain_size;
    host_hdr.filename_len = 0;

    send_string(UART0, "Cipher Size = ");
    uart_print_u32(UART0, (uint64_t)len);
    send_string(UART0, " bytes\r\n");

    send_string(UART0, "Decryption completed in ");
    uart_print_u32(UART0, decryption_cycles);
    send_string(UART0, " cycles\r\n");

    send_string(UART0, "Decryption bitrate = ");
    uart_print_u32(UART0, (uint32_t)decryption_bitrate);
    send_string(UART0, " bps\r\n");

    seg7_display_decimal((uint32_t)decryption_bitrate);

    send_string(UART0, "Decryption throughput = ");
    uart_print_u32(UART0, (uint32_t)decryption_bytes_per_sec);
    send_string(UART0, " bytes/s\r\n");

    send_string(UART0, "Decryption cycles/byte = ");
    uart_print_u32(UART0, (uint32_t)cycles_per_byte);
    send_string(UART0, "\r\n");
    
    send_host_file(
    &host_hdr,
    plain_buffer,
    (uint32_t)plain_size);

}

int main(void)
{

    send_string(UART0, "Send or Recieve?\r\nBTN3 - Send\r\nBTN2 - Recieve\r\nBTN1 - Activate/Deactivate CLMUL\r\n");
    while (1) {
            
            // if (read_gpio(GPIO_IN_DBNC) & (1 << 4)) {
                
            //     if(use_clmul == 0)
            //         send_string(UART0, "CLMUL is activated\r\n\n");
            //     use_clmul = 1;
            // }
                
            // else {
            //     if(use_clmul == 1)
            //         send_string(UART0, "CLMUL is deactivated\r\n\n");
            //     use_clmul = 0;
            // }

            if(read_gpio(GPIO_IN_DBNC) & (1 << 2)) {

                SEG7_REG = 0x00000000;

                send_string(UART0, "--RECIEVE MODE--\r\n");
                send_string(UART0, "Waiting for message...\r\n");

                // while (uart_in(UART1) != UART_EOF)
                // {
                //     // discard stale bytes
                // }

                receive_mode();
        
            }

            if(read_gpio(GPIO_IN_DBNC) & (1 << 3)) {

                SEG7_REG = 0x00000000;

                send_string(UART0, "--SEND MODE--\r\n");
                send_string(UART0, "Upload message to send:\r\n");

                send_mode();
            
            }
    }
    return 0;
}