#include <stdio.h>
#include <string.h>
#include "tusb.h"
#include "pdnd/pdnd.h"
#include "pdnd/pdnd_display.h"
#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#define SM_RX 0
#define SM_TX 1

char *get_stop_bits(cdc_line_coding_t *lc) {
    switch(lc->stop_bits) {
        case 0:
            return "1";
        case 1:
            return "1.5";
        case 2:
            return "2";
        default:
            return "Unknown";
    }
    return "Impossible";
}

char *get_parity(cdc_line_coding_t *lc) {
    switch(lc->parity) {
        case 0:
            return "None";
        case 1:
            return "Odd";
        case 2:
            return "Even";
        case 3:
            return "Mark";
        case 4:
            return "Space";
        default:
            return "Unknown";
    }
    return "Impossible";
}

void init_uart_rx(int baud) {
    PIO pio = pio1;
    uint sm = 0;
    uint offset = pio_add_program(pio, &uart_rx_program);
    uart_rx_program_init(pio, SM_RX, offset, pdnd_in_pin(0), baud);

    offset = pio_add_program(pio, &uart_tx_program);
    uart_tx_program_init(pio, SM_TX, offset, pdnd_out_pin(1), baud);
    pdnd_enable_buffers(1);
}

void clear_pio() {
    pdnd_enable_buffers(0);
    pio_clear_instruction_memory (pio1);
}

int main() {
    stdio_init_all();
    stdio_set_translate_crlf(&stdio_usb, false);
    pdnd_initialize();
    pdnd_enable_buffers(0);
    pdnd_display_initialize();

    bool usb_connected = false;

    int baud_rate = 0;
    cdc_line_coding_t old_coding;
    memset(&old_coding, 0, sizeof(old_coding));

    while(1) {
        if(tud_cdc_n_connected(0)) {
            cdc_line_coding_t coding;
            tud_cdc_n_get_line_coding (0, &coding);
            bool display = false;
            if(usb_connected == false) {
                // We just got a USB connection, so initialize UART.
                usb_connected = true;
                init_uart_rx(coding.bit_rate);
                baud_rate = coding.bit_rate;
                display = true;
            } else {
                if(coding.bit_rate != baud_rate) {
                    clear_pio();
                    init_uart_rx(coding.bit_rate);
                    baud_rate = coding.bit_rate;
                    display = true;
                }
                // TODO: This somehow doesn't work. Doesn't matter
                // cause we only support adjusting baud rate anyway.
                if(memcmp(&coding, &old_coding, sizeof(cdc_line_coding_t) != 0)) {
                    display = true;
                    old_coding = coding;
                }
            }

            if(display) {
                cls(false);
                pprintf("UART Bridge\nBaud: %7d\nParity: %s\nStop: %s Bits: %d", coding.bit_rate, get_parity(&coding), get_stop_bits(&coding), coding.data_bits);
                display = false;
            }

            // TODO: Should not use stdio for this stuff :)
            int c = getchar_timeout_us(1);
            if(c != PICO_ERROR_TIMEOUT) {
                uart_tx_program_putc(pio1, SM_TX, c);
            }

            if(uart_rx_program_data_available(pio1, SM_RX)) {
                putchar(uart_rx_program_getc(pio1, SM_RX));
            }

        } else {
            if(usb_connected) {
                clear_pio();
            }
            cls(false);
            usb_connected = false;
            pprintf("UART Bridge\nNo connection.");
        }
    }

    return 0;
}
