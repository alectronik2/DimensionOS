export module arch.ps2;

import arch.io;
import arch.cpu;
import arch.idt;
import types;

// ==================== PS/2 Keyboard Port Definitions ====================

#define PS2_DATA_PORT    0x60  // Read/Write data
#define PS2_STATUS_PORT  0x64  // Read status
#define PS2_COMMAND_PORT 0x64  // Write commands

// Status Register Bits
#define PS2_STATUS_OUTPUT_FULL  0x01  // Output buffer full (data available)
#define PS2_STATUS_INPUT_FULL   0x02  // Input buffer full (don't write)
#define PS2_STATUS_SYSTEM       0x04  // System flag
#define PS2_STATUS_COMMAND      0x08  // Command/Data (0 = data, 1 = command)
#define PS2_STATUS_TIMEOUT      0x40  // Timeout error
#define PS2_STATUS_PARITY       0x80  // Parity error

// PS/2 Controller Commands
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_DISABLE_PORT2   0xA7
#define PS2_CMD_ENABLE_PORT2    0xA8
#define PS2_CMD_TEST_PORT2      0xA9
#define PS2_CMD_TEST_CONTROLLER 0xAA
#define PS2_CMD_TEST_PORT1      0xAB
#define PS2_CMD_DISABLE_PORT1   0xAD
#define PS2_CMD_ENABLE_PORT1    0xAE
#define PS2_CMD_READ_OUTPUT     0xD0
#define PS2_CMD_WRITE_OUTPUT    0xD1

// Keyboard Commands
#define KB_CMD_SET_LEDS         0xED
#define KB_CMD_ECHO             0xEE
#define KB_CMD_SET_SCANCODE     0xF0
#define KB_CMD_IDENTIFY         0xF2
#define KB_CMD_SET_RATE         0xF3
#define KB_CMD_ENABLE           0xF4
#define KB_CMD_DISABLE          0xF5
#define KB_CMD_SET_DEFAULT      0xF6
#define KB_CMD_RESEND           0xFE
#define KB_CMD_RESET            0xFF

// Keyboard Responses
#define KB_RESPONSE_ACK         0xFA
#define KB_RESPONSE_RESEND      0xFE
#define KB_RESPONSE_ERROR       0xFC
#define KB_RESPONSE_TEST_PASS   0xAA

// ==================== Key State Tracking ====================

#define KEY_BUFFER_SIZE 256

typedef struct {
    uint8_t buffer[KEY_BUFFER_SIZE];
    uint16_t read_index;
    uint16_t write_index;
    uint16_t count;
} key_buffer_t;

typedef struct {
    bool shift_left;
    bool shift_right;
    bool ctrl_left;
    bool ctrl_right;
    bool alt_left;
    bool alt_right;
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
    bool extended;  // E0 prefix received
} keyboard_state_t;

static key_buffer_t key_buffer = {0};
static keyboard_state_t kb_state = {0};

// ==================== Scancode Tables ====================

// Scancode Set 1 (default) - Main keys
static const char scancode_to_ascii_unshifted[128] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    '7',  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3',  '0',  '.',  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0
};

static const char scancode_to_ascii_shifted[128] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    '7',  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3',  '0',  '.',  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0
};

// Special key codes for non-ASCII keys
enum special_keys {
    KEY_F1 = 0x3B,
    KEY_F2 = 0x3C,
    KEY_F3 = 0x3D,
    KEY_F4 = 0x3E,
    KEY_F5 = 0x3F,
    KEY_F6 = 0x40,
    KEY_F7 = 0x41,
    KEY_F8 = 0x42,
    KEY_F9 = 0x43,
    KEY_F10 = 0x44,
    KEY_F11 = 0x57,
    KEY_F12 = 0x58,
    KEY_ESC = 0x01,
    KEY_LCTRL = 0x1D,
    KEY_LSHIFT = 0x2A,
    KEY_RSHIFT = 0x36,
    KEY_LALT = 0x38,
    KEY_CAPS = 0x3A,
    KEY_NUM = 0x45,
    KEY_SCROLL = 0x46,
    KEY_HOME = 0x47,
    KEY_UP = 0x48,
    KEY_PGUP = 0x49,
    KEY_LEFT = 0x4B,
    KEY_RIGHT = 0x4D,
    KEY_END = 0x4F,
    KEY_DOWN = 0x50,
    KEY_PGDN = 0x51,
    KEY_INSERT = 0x52,
    KEY_DELETE = 0x53
};

// ==================== Low-level I/O Functions ====================

static inline void 
io_wait(void) {
    // Port 0x80 is used for POST codes and is safe to use for delays
    arch::outb(0x80, 0);
}

// ==================== PS/2 Controller Functions ====================

// Wait for input buffer to be ready (empty)
static bool 
ps2_wait_input(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if( !(arch::inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) ) {
            return true;
        }
        io_wait();
    }
    return false;
}

// Wait for output buffer to have data
static bool 
ps2_wait_output() {
    uint32_t timeout = 100000;
    while( timeout-- ) {
        if( arch::inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL ) {
            return true;
        }
        io_wait();
    }
    return false;
}

// Send command to PS/2 controller
static bool
ps2_send_command( uint8_t command ) {
    if( !ps2_wait_input() ) 
        return false;
    arch::outb(PS2_COMMAND_PORT, command);
    return true;
}

// Send data to keyboard
static bool 
keyboard_send_command( uint8_t command ) {
    if (!ps2_wait_input()) 
        return false;
    arch::outb(PS2_DATA_PORT, command);
    
    // Wait for ACK
    if (!ps2_wait_output()) return false;
    return arch::inb(PS2_DATA_PORT) == KB_RESPONSE_ACK;
}

// Read data from keyboard
static uint8_t 
keyboard_read_data(void) {
    ps2_wait_output();
    return arch::inb(PS2_DATA_PORT);
}

// ==================== LED Control ====================

static void keyboard_update_leds(void) {
    uint8_t led_status = 0;
    
    if (kb_state.scroll_lock) led_status |= 0x01;
    if (kb_state.num_lock)    led_status |= 0x02;
    if (kb_state.caps_lock)   led_status |= 0x04;
    
    keyboard_send_command(KB_CMD_SET_LEDS);
    ps2_wait_input();
    arch::outb(PS2_DATA_PORT, led_status);
}

void
ps2_irq_handler( arch::interrupt_context *ctx ) {

}

export namespace arch {
    bool
    init_ps2() {
        // Disable devices during initialization
        ps2_send_command(PS2_CMD_DISABLE_PORT1);
        ps2_send_command(PS2_CMD_DISABLE_PORT2);

         // Flush output buffer
        while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
            inb(PS2_DATA_PORT);
            io_wait();
        }
        
        // Set controller configuration
        ps2_send_command(PS2_CMD_READ_CONFIG);
        ps2_wait_output();
        uint8_t config = inb(PS2_DATA_PORT);
        
        // Clear bits 0, 1, and 6 (enable IRQ1, disable IRQ12, disable translation)
        config &= ~((1 << 0) | (1 << 1) | (1 << 6));
        config |= (1 << 0);  // Enable IRQ1 for keyboard
        
        ps2_send_command(PS2_CMD_WRITE_CONFIG);
        ps2_wait_input();
        outb(PS2_DATA_PORT, config);
        
        // Controller self-test
        ps2_send_command(PS2_CMD_TEST_CONTROLLER);
        ps2_wait_output();
        if (inb(PS2_DATA_PORT) != 0x55) {
            return false;  // Controller test failed
        }
        
        // Test keyboard port
        ps2_send_command(PS2_CMD_TEST_PORT1);
        ps2_wait_output();
        if (inb(PS2_DATA_PORT) != 0x00) {
            return false;  // Port test failed
        }
        
        // Enable keyboard port
        ps2_send_command(PS2_CMD_ENABLE_PORT1);
        
        // Reset keyboard
        if (!keyboard_send_command(KB_CMD_RESET)) {
            return false;
        }
        
        // Wait for self-test result
        ps2_wait_output();
        if (inb(PS2_DATA_PORT) != KB_RESPONSE_TEST_PASS) {
            return false;
        }
        
        // Set scancode set 1 (most compatible)
        keyboard_send_command(KB_CMD_SET_SCANCODE);
        ps2_wait_input();
        outb(PS2_DATA_PORT, 1);
        
        // Enable keyboard scanning
        keyboard_send_command(KB_CMD_ENABLE);
        
        // Set default LED state
        keyboard_update_leds();
        
        // Clear any pending data
        while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
            inb(PS2_DATA_PORT);
        }
        
        register_irq_handler( 0x21, ps2_irq_handler );

        return true;
    }
}
