#include <stdint.h>

#include "../vivado/ibex-demo-system/sw/c/common/demo_system.h"
#include "../vivado/ibex-demo-system/sw/c/common/gpio.h"
#include "../vivado/ibex-demo-system/sw/c/common/uart.h"

#define MAX_STR_LEN 256

typedef enum {
    RECEIVE_MODE,
    SEND_MODE
} state_t;
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

static void reverse_string(char *str, int len)
{
    int i = 0;
    int j = len - 1;

    while (i < j) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}

static void send_string(uart_t uart, const char *str)
{
    while (*str)
        uart_out(uart, *str++);
}

static void receive_mode(char *buffer, int *index, state_t *state)
{
    // Highest priority
    if (read_gpio(GPIO_IN_DBNC) & (1 << 3)) {

        send_string(UART0, "\r\nEntering SEND mode\r\n");

        *index = 0;
        *state = SEND_MODE;
        return;
    }

    int ch = uart_in(UART1);

    if (ch == UART_EOF)
        return;

    if (ch == '\r' || ch == '\n') {

        buffer[*index] = '\0';

        send_string(UART0, "Decrypting...\r\n");

        uint32_t start = get_mcycle();

        reverse_string(buffer, *index);

        uint32_t end = get_mcycle();

        send_string(UART0, "Received: ");
        send_string(UART0, buffer);
        send_string(UART0, "\r\n");

        send_string(UART0, "Cycles: ");
        uart_print_u32(UART0, end - start);
        send_string(UART0, "\r\n");

        *index = 0;
    }
    else if (*index < MAX_STR_LEN - 1) {

        buffer[(*index)++] = (char)ch;
    }
}

static void send_mode(char *buffer, int *index, state_t *state)
{
    static int prompt_printed = 0;

    if (!prompt_printed) {

        send_string(UART0, "Type message:\r\n");

        prompt_printed = 1;
    }

    int ch = uart_in(UART0);

    if (ch == UART_EOF)
        return;

    if (ch == '\r' || ch == '\n') {

        buffer[*index] = '\0';

        send_string(UART0, "\r\nEncrypting...\r\n");

        uint32_t start = get_mcycle();

        reverse_string(buffer, *index);

        uint32_t end = get_mcycle();

        send_string(UART1, buffer);
        uart_out(UART1, '\n');

        send_string(UART0, "Sent: ");
        send_string(UART0, buffer);
        send_string(UART0, "\r\n");

        send_string(UART0, "Cycles: ");
        uart_print_u32(UART0, end - start);
        send_string(UART0, "\r\n");

        *index = 0;

        prompt_printed = 0;

        *state = RECEIVE_MODE;

        send_string(UART0, "Returning to RECEIVE mode\r\n");

        return;
    }

    if (*index < MAX_STR_LEN - 1) {

        buffer[(*index)++] = (char)ch;

        // Optional local echo
        uart_out(UART0, ch);
    }
}

int main(void){
    state_t state = RECEIVE_MODE;

    char send_buffer[MAX_STR_LEN];
    char receive_buffer[MAX_STR_LEN];

    int send_index = 0;
    int receive_index = 0;

    while (1) {

        switch (state) {

        case RECEIVE_MODE:
            receive_mode(receive_buffer, &receive_index, &state);
            break;

        case SEND_MODE:
            send_mode(send_buffer, &send_index, &state);
            break;
        }
    }
}

