#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/sync.h"
//#include "printfcolor.h"

//////////////////////////////////////////////////////////////////////////////
// I/O PINS
#define BT1_PIN 16 // 19
#define BT2_PIN 17 // 18
#define BT3_PIN 18 // 16
#define BT4_PIN 19 // 17

#define UART_ID uart1
#define UART_BAUD 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// MENU PARAM
#define NUM_GAMES 3
//////////////////////////////////////////////////////////////////////////////

enum Screen {
    MAIN_MENU,
    SELECTOR,

    PONG_MENU,
    PONG_GAME,

    TETRIS_MENU,
    TETRIS_GAME,

    FOOSBALL_LOBBY,
    FOOSBALL_MENU,
    FOOSBALL_GAME
};

// UART PARAM
#define UART_HELLO_INTERVAL 250
#define UART_TIMEOUT 1500

bool peerConnected = false;
bool peerReady = false;

uint32_t lastUARTSend = 0;
uint32_t lastUARTReceive = 0;

typedef enum {
    UART_HOST_REQUEST = 0xA1,
    UART_JOIN_ACK     = 0xA2,
    UART_START_GAME   = 0xA3
} UARTMessage;

typedef enum {
    ROLE_NONE,
    ROLE_HOST,
    ROLE_CLIENT
} MultiplayerRole;

MultiplayerRole role = ROLE_NONE;

// Constants
const char *gameList[NUM_GAMES] = {
    "Pong",
    "Tetris",
    "Foosball"
};

// Global Var
void printMenu();
void menu_select_up();
void menu_select_down();

enum Screen screen = MAIN_MENU;

// Menu Selector
volatile int lastPressed = 0;
int menuCurrentSelect = 0;
int menuLength = 5;
const char** menuList;

// PONG
void pong_init()
{
    screen = PONG_MENU;
    printf("You are playing Pong\n");
}

// TETRIS
void tetris_init()
{
    screen = TETRIS_MENU;
    printf("You are playing Tetris\n");
}

// FOOSBALL
void foosball_init()
{
    screen = FOOSBALL_LOBBY;
    role = ROLE_HOST;

    printf("Connect 2P controller to start playing Foosball\n");
}

void foosball_start()
{
    if(role != ROLE_HOST || screen != FOOSBALL_MENU)
        return;

    uart_putc_raw(UART_ID, UART_START_GAME);
    screen = FOOSBALL_GAME;
}

void foosball_game_init()
{
    printf("Foosball game started!\n");

    if(role == ROLE_HOST)
        printf("You are Player 1 (Host)\n");
    else
        printf("You are Player 2 (Client)\n");
}

void foosball_game_update()
{
    // TODO
}

// PUSH BUTTONS
void pb_init()
{
    uint32_t BTMask =  (1u << BT1_PIN) | (1u << BT2_PIN) | (1u << BT3_PIN) | (1u << BT4_PIN);
    gpio_init_mask(BTMask);

    gpio_set_dir(BT1_PIN, false);
    gpio_set_dir(BT2_PIN, false);
    gpio_set_dir(BT3_PIN, false);
    gpio_set_dir(BT4_PIN, false);
}

void pb_isr()
{
    if(gpio_get_irq_event_mask(BT1_PIN) & 8u)
    {
        gpio_acknowledge_irq(BT1_PIN, 8u);
        lastPressed = 1;
    }

    if(gpio_get_irq_event_mask(BT2_PIN) & 8u)
    {
        gpio_acknowledge_irq(BT2_PIN, 8u);
        lastPressed = 2;
    }

    if(gpio_get_irq_event_mask(BT3_PIN) & 8u)
    {
        gpio_acknowledge_irq(BT3_PIN, 8u);
        lastPressed = 3;
    }

    if(gpio_get_irq_event_mask(BT4_PIN) & 8u)
    {
        gpio_acknowledge_irq(BT4_PIN, 8u);
        lastPressed = 4;
    }
}

void pb_irq_init()
{
    // Add handler functions
    uint32_t BTMask =  (1u << BT1_PIN) | (1u << BT2_PIN) | (1u << BT3_PIN) | (1u << BT4_PIN);
    gpio_add_raw_irq_handler_masked(BTMask, pb_isr);
    
    // Enable GPIO IRQs (on core 0)
    gpio_set_irq_enabled(BT1_PIN, 8u, true);
    gpio_set_irq_enabled(BT2_PIN, 8u, true);
    gpio_set_irq_enabled(BT3_PIN, 8u, true);
    gpio_set_irq_enabled(BT4_PIN, 8u, true);

    // Enable BANK0 IRQ interrupt (21 For safe)
    nvic_hw->icpr[0] = 1u << 21;
    nvic_hw->iser[0] = 1u << 21;
}

// UART
void uart_process_message(uint8_t msg)
{
    switch(msg)
    {
        case UART_HOST_REQUEST:
            // Accept automatically while in the game selector
            if((screen == SELECTOR || screen == MAIN_MENU)
               && role == ROLE_NONE)
            {
                role = ROLE_CLIENT;
                screen = FOOSBALL_LOBBY;

                uart_putc_raw(UART_ID, UART_JOIN_ACK);
                printf("Joined Foosball as Player 2!\n");
            }
            else if(screen == FOOSBALL_LOBBY &&
                    role == ROLE_CLIENT)
            {
                // Resend acknowledgement if host is still waiting
                uart_putc_raw(UART_ID, UART_JOIN_ACK);
            }
            break;

        case UART_JOIN_ACK:
            if(screen == FOOSBALL_LOBBY &&
               role == ROLE_HOST)
            {
                screen = FOOSBALL_MENU;
                printf("Player 2 connected!\n");
                printf("Press BT4 to start Foosball.\n");
            }
            break;

        case UART_START_GAME:
            if(screen == FOOSBALL_LOBBY &&
               role == ROLE_CLIENT)
            {
                screen = FOOSBALL_GAME;
            }
            break;
    }
}

void uart_service()
{
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Host periodically requests a connection
    if(screen == FOOSBALL_LOBBY && role == ROLE_HOST)
    {
        if(now - lastUARTSend >= UART_HELLO_INTERVAL)
        {
            uart_putc_raw(UART_ID, UART_HOST_REQUEST);
            lastUARTSend = now;
        }
    }

    // Handle incoming UART messages
    while(uart_is_readable(UART_ID))
    {
        uint8_t msg = uart_getc(UART_ID);
        uart_process_message(msg);
    }
}

void game_uart_init()
{
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_init(UART_ID, UART_BAUD);

    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);
}

// UTILITIES
void menu_select_down()
{
    if(screen != SELECTOR) return;
    if(menuCurrentSelect >= menuLength) return;
    menuCurrentSelect++;
}

void menu_select_up()
{
    if(screen != SELECTOR) return;
    if(menuCurrentSelect <= 1) return;
    menuCurrentSelect--;
}

void printMenu()
{
    int i;

    for(i = 0; i < menuLength; i++)
    {
        if(i+1 == menuCurrentSelect){
            printf("%d. [%s] <--\n", i+1, menuList[i]);
            continue;
        }
        printf("%d. %s\n", i+1, menuList[i]);
    }
}

void selectorMenu(const char **list, int listLength)
{
    screen = SELECTOR;

    menuList = list;
    menuLength = listLength;
    menuCurrentSelect = 1;

    printMenu();
}

void selector_update(int button)
{
    if(button == 1)
    {
        menu_select_up();
        printMenu();
    }
    else if(button == 2)
    {
        menu_select_down();
        printMenu();
    }
    else if(button == 4)
    {
        switch(menuCurrentSelect)
        {
            case 1:
                pong_init();
                break;

            case 2:
                tetris_init();
                break;

            case 3:
                foosball_init();
                break;
        }
    }
}

int main()
{
    stdio_init_all();
    pb_init();
    pb_irq_init();
    game_uart_init();

    printf("Welcome to ");
    sleep_ms(800);
    printf("HaithamPlayer420!\n");
    sleep_ms(800);
    printf("Please select the game you wish to play today.\n");

    // Initialize game selector
    selectorMenu(gameList, NUM_GAMES);

    enum Screen previousScreen = screen;

    while(true)
    {
        uart_service();

        // Screen transitions
        if(screen != previousScreen)
        {
            previousScreen = screen;

            switch(screen)
            {
                case FOOSBALL_GAME:
                    foosball_game_init();
                    break;

                default:
                    break;
            }
        }

        // Retrieve button event
        uint32_t irq_state = save_and_disable_interrupts();

        int button = lastPressed;
        lastPressed = 0;

        restore_interrupts(irq_state);

        // Run the current screen
        switch(screen)
        {
            case SELECTOR:
                if(button > 0)
                    selector_update(button);
                break;

            case PONG_MENU:
                // TODO Pong menu
                break;

            case PONG_GAME:
                // TODO Pong update
                break;

            case TETRIS_MENU:
                // TODO Tetris menu
                break;

            case TETRIS_GAME:
                // TODO Tetris update
                break;

            case FOOSBALL_LOBBY:
                // Host waits for JOIN_ACK
                // Client waits for START_GAME
                break;

            case FOOSBALL_MENU:
                // Only host can start
                if(button == 4)
                    foosball_start();
                break;

            case FOOSBALL_GAME:
                foosball_game_update();
                break;

            default:
                break;
        }

        tight_loop_contents();
    }
}