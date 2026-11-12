#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

//////////////////////////////////////////////////////////////////////////////
// No autotest, but you might want to set this anyway.
const char* username = "jsuantho";

// When testing basic TX/RX
// #define STEP2
// When connecting UART to printf(), fgets()
// #define STEP3
// When testing UART IRQ for buffering
// #define STEP4
// When testing PCS
#define STEP5

//////////////////////////////////////////////////////////////////////////////

void init_uart() {
    // fill in
    // Set GPIO 0 and 1 as UART TX and RX
    hw_write_masked(&pads_bank0_hw->io[0],
                   PADS_BANK0_GPIO0_IE_BITS,
                   PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS
    );
    hw_write_masked(&pads_bank0_hw->io[1],
                   PADS_BANK0_GPIO0_IE_BITS,
                   PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS
    );
    io_bank0_hw->io[0].ctrl = GPIO_FUNC_UART << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
    io_bank0_hw->io[1].ctrl = GPIO_FUNC_UART << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
    hw_clear_bits(&pads_bank0_hw->io[0], PADS_BANK0_GPIO0_ISO_BITS);
    hw_clear_bits(&pads_bank0_hw->io[1], PADS_BANK0_GPIO0_ISO_BITS);

    // Set Baud rate to 115200
    uart0_hw->ibrd = 81u;
    uart0_hw->fbrd = 24u;

    // Set configs
    // 8 data bits: 11 on bit 6:5
    // No Parity: 0 on bit 1 
    // 1 Stop bit: 0 on bit 3
    hw_write_masked(
        &uart0_hw->lcr_h,
        (3u << 5),
        (3u << 5) | (1u << 3) | (1u << 1)
    );

    // Enable UART, TX, and RX
    uart0_hw->cr = 1u | (1u << 8) | (1u << 9);
}

#ifdef STEP2
int main() {
    init_uart();
    for (;;) {
        char buf[2];
        uart_read_blocking(uart0, (uint8_t*)buf, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0'; // Ensure null-termination
        uart_puts(uart0, "You said: ");
        uart_puts(uart0, buf);
        uart_puts(uart0, "\n");
    }
}
#endif

//////////////////////////////////////////////////////////////////////////////

#ifdef STEP3

// 3.3
int _read(__unused int handle, char *buffer, int length) {
    // Your code here to read from the UART and fill the buffer.
    // DO NOT USE THE STDIO_* FUNCTIONS FROM ABOVE.  Only UART ones.

    // The argument "handle" is unused.  This is meant for use with 
    // files, which are not very different from text streams.  However, 
    // we read from the UART, not the file specified by the handle.

    // handle is irrelevant since these functions will only ever be called 
    // by the correct functions.  No need for an if statement.

    // Instructions: Given the buffer and a specific length to read, read 1 
    // character at a time from the UART until the buffer is 
    // filled or the length is reached. 
    
    int i = 0;
    while(i < length){
        // Loop until ready to read from uart (RX is not empty)
        while(uart0_hw->fr & UART_UARTFR_RXFE_BITS);
        // Take one character and put in buffer[i]
        *(buffer + i)  = uart0_hw->dr & 0xFF;
        if(*(buffer + i) == 8){ // is this a backspace
            putchar(8u); // print it
            putchar(32u); // Followed by a space
            putchar(8u); // Followed by another backspace
        }
        else
        {
            putchar(*(buffer + i));
            i++;
        }
    }

    return i;
}

int _write(__unused int handle, char *buffer, int length) {
    // Your code here to write to the UART from the buffer.
    // DO NOT USE THE STDIO_* FUNCTIONS FROM ABOVE.  Only UART ones.

    // The argument "handle" is unused.  This is meant for use with 
    // files, which are not very different from text streams.  However, 
    // we write to the UART, not the file specified by the handle.

    // handle is irrelevant since these functions will only ever be called 
    // by the correct functions.  No need for an if statement.

    // Instructions: Given the buffer and a specific length to write, write 1
    // character at a time to the UART until the length is reached. 
    int i = 0;
    while (i < length) {
        // Loop until ready to write to uart (TX is not full)
        while(uart0_hw->fr & UART_UARTFR_TXFF_BITS);
        // Take one character and put in uart
        uart0_hw->dr = *(buffer + i);
        i++;
    }

    return i;
}

int main()
{
    init_uart();

    // insert any setbuf lines below this line...
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    char name[8];
    int age = 0;
    for(;;) {
        printf("Enter your name and age: ");
        scanf("%s %d", name, &age);
        printf("Hello, %s! You are %d years old.\n", name, age);
        sleep_ms(100);  // in case the output loops and is too fast
    }
}
#endif

//////////////////////////////////////////////////////////////////////////////

#ifdef STEP4

#define BUFSIZE 32
char serbuf[BUFSIZE];
int seridx = 0;
int newline_seen = 0;

// add this here so that compiler does not complain about implicit function
void uart_rx_handler();

void init_uart_irq() {
    // fill in.
    // Disable the internal FIFO (bit 4)
    hw_write_masked(
        &uart0_hw->lcr_h,
        0u,
        (1u << 4)
    );

    // Enable the necessary bits to mask the receive interrupt 
    hw_write_masked(
        &uart0_hw->imsc,
        1u << 4,
        1u << 4
    );  

    // Set up uart_rx_handler to be the exclusive handler for the overall UART interrupt
    irq_set_exclusive_handler(33, uart_rx_handler);

    // Enable TIMER interrupt (33)
    nvic_hw->icpr[1] = 1u << 1;
    nvic_hw->iser[1] = 1u << 1;
}

void uart_rx_handler() {
    // fill in.
    // Acknowledge Interrupt
    uart0_hw->icr = 1u << 4;
    
    if(seridx >= BUFSIZE) return;

    char c = uart0_hw->dr & 0xFF;

    if(c == 0x0A) newline_seen = 1;
    
    if(c == 8u && seridx > 0)
    {
        putchar(8u);
        putchar(32u);
        putchar(8u);

        seridx--;
        serbuf[seridx] = '\0';
    }
    else
    {
        putchar(c);
        serbuf[seridx] = c;
        seridx++;
    }
}

int _read(__unused int handle, char *buffer, int length) {
    // fill in.
    while(!newline_seen) sleep_ms(5);
    newline_seen = 0;
    int cnt = seridx > length ? length : seridx;
    memcpy(buffer, serbuf, cnt);
    seridx = 0;

    return cnt;
}

int _write(__unused int handle, char *buffer, int length) {
    // fill in.
    int i = 0;
    while (i < length) {
        // Loop until ready to write to uart (TX is not full)
        while(uart0_hw->fr & UART_UARTFR_TXFF_BITS);
        // Take one character and put in uart
        uart0_hw->dr = *(buffer + i);
        i++;
    }

    return i;
}

int main() {
    // fill in.
    init_uart();
    init_uart_irq();

    setbuf(stdout, NULL); // Disable buffering for stdout

    char name[8];
    int age = 0;
    for(;;) {
        printf("Enter your name and age: ");
        scanf("%s %d", name, &age);
        // THIS IS IMPORTANT.
        fflush(stdin);
        printf("Hello, %s! You are %d years old.\r\n", name, age);
        sleep_ms(100);  // in case the output loops and is too fast
    }
}

#endif

//////////////////////////////////////////////////////////////////////////////

#ifdef STEP5

// Copy global variables, init_uart_irq, uart_rx_handler, _read, and _write from STEP4.

#define BUFSIZE 32
char serbuf[BUFSIZE];
int seridx = 0;
int newline_seen = 0;

// add this here so that compiler does not complain about implicit function
void uart_rx_handler();

void init_uart_irq() {
    // fill in.
    // Disable the internal FIFO (bit 4)
    hw_write_masked(
        &uart0_hw->lcr_h,
        0u,
        (1u << 4)
    );

    // Enable the necessary bits to mask the receive interrupt 
    hw_write_masked(
        &uart0_hw->imsc,
        1u << 4,
        1u << 4
    );  

    // Set up uart_rx_handler to be the exclusive handler for the overall UART interrupt
    irq_set_exclusive_handler(33, uart_rx_handler);

    // Enable TIMER interrupt (33)
    nvic_hw->icpr[1] = 1u << 1;
    nvic_hw->iser[1] = 1u << 1;
}

void uart_rx_handler() {
    // fill in.
    // Acknowledge Interrupt
    uart0_hw->icr = 1u << 4;
    
    if(seridx >= BUFSIZE) return;

    char c = uart0_hw->dr & 0xFF;

    if(c == 0x0A) newline_seen = 1;
    
    if(c == 8u && seridx > 0)
    {
        putchar(8u);
        putchar(32u);
        putchar(8u);

        seridx--;
        serbuf[seridx] = '\0';
    }
    else
    {
        putchar(c);
        serbuf[seridx] = c;
        seridx++;
    }
}

int _read(__unused int handle, char *buffer, int length) {
    // fill in.
    while(!newline_seen) sleep_ms(5);
    newline_seen = 0;
    int cnt = seridx > length ? length : seridx;
    memcpy(buffer, serbuf, cnt);
    seridx = 0;

    return cnt;
}

int _write(__unused int handle, char *buffer, int length) {
    // fill in.
    int i = 0;
    while (i < length) {
        // Loop until ready to write to uart (TX is not full)
        while(uart0_hw->fr & UART_UARTFR_TXFF_BITS);
        // Take one character and put in uart
        uart0_hw->dr = *(buffer + i);
        i++;
    }

    return i;
}

void cmd_gpio(int argc, char **argv) {
    // This is the main command handler for the "gpio" command.
    // It will call either cmd_gpio_out or cmd_gpio_set based on the arguments.
    
    // Ensure that argc is at least 2, otherwise print an example use case and return.
    if (argc < 2){
        printf("Usage: \ngpio out <pin_number>\n gpio set <pin_number> <0/1>\n");
        return;
    }
    // If the second argument is "out":
    //      Ensure that argc is exactly 3, otherwise print an example use case and return.
    //      Convert the third argument to an integer pin number using atoi.
    //      Check if the pin number is valid (0-47), otherwise print an error and return.
    //      Set the pin to output using gpio_init and gpio_set_dir.
    //      Print a success message.
    if(!strcmp(argv[1], "out")){
        if (argc != 3)
        {
            printf("Usage: gpio out <pin_number>\n");
            return;
        }

        int gpio = atoi(argv[2]);
        if(gpio < 0 || gpio > 47)
        {
            printf("Invalid pin number: %d. Must be between 0 and 47.\n", gpio);
            return;
        }
        
        gpio_init(gpio);
        gpio_set_dir(gpio, 1);

        printf("Initialized pin %d as output.\n", gpio);
    }

    // If the second argument is "set":
    //      Ensure that argc is exactly 4, otherwise print an example use case and return.
    //      Convert the third argument to an integer pin number using atoi.
    //      Check if the pin number is valid (0-47), otherwise print an error and return.
    //      Check if the pin has been initialized as a GPIO output, if not, return.
    //      Convert the fourth argument to an integer value (0 or 1) using atoi.
    //      Check if the value is valid (0 or 1), otherwise print an error and return.
    //      Set the pin to the specified value using gpio_put.
    //      Print a success message.
    
    else if(!strcmp(argv[1], "set")){
        if (argc != 4)
        {
            printf("Usage: gpio set <pin_number> <0/1>\n");
            return;
        }

        int gpio = atoi(argv[2]);
        if(gpio < 0 || gpio > 47)
        {
            printf("Invalid pin number: %d. Must be between 0 and 47.\n", gpio);
            return;
        }
        if(gpio_get_function(gpio) != GPIO_FUNC_SIO || !gpio_is_dir_out(gpio))
        {
            printf("Pin %d is not initialized as an output.\n", gpio);
            return;
        }

        int val = atoi(argv[3]);
        if(val != 0 && val != 1)
        {
            printf("Invalid value: %d. Must be 0 or 1.\n", val);
            return;
        }

        gpio_put(gpio, val);

        printf("Set pin %d to %d.\n", gpio, val);
    }

    // Else, print an unknown command error.
    else
    {
        printf("Unknown command: %s", argv[0]);
    }

}

int main() {
    // See lab for instructions.
    init_uart();
    init_uart_irq();

    setbuf(stdout, NULL);

    printf("%s's Peripheral Command Shell (PCS)\n", username);
    printf("Enter a command below.\n\n");

    int argc;
    char *argv[10];
    char input[100];

    for(;;)
    {
        argc = 0;

        printf("\r\n> ");
        fgets(input, sizeof(input), stdin);
        fflush(stdin);
        input[strcspn(input, "\n")] = '\0';
        
        char * inptr = strtok(input, " ");
        while(inptr != NULL && argc < 10) 
        {
            argv[argc] = inptr;
            inptr = strtok(NULL, " ");
            argc++;
            
        }
        cmd_gpio(argc, argv);
    }
}

#endif

//////////////////////////////////////////////////////////////////////////////
