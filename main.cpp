#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <EEPROMex.h>
#include <avr/pgmspace.h>
#include <OneWire.h>
#include <MemoryFree.h>
#include <OneWireHub.h>
#include <DS2401.h>

#define NUM_ROWS 4
#define OFFSET_X 10
#define OFFSET_Y 10
#define FONT_SIZE 1
#define FONT_HEIGHT 8
#define FONT_WIDTH 6
#define LINE_WIDTH 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define RESET_PIN 4

#define DEBUG 1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, RESET_PIN);

/*
 * Some words about keys: I intend to use this device only as an
 * iButton emulator for now, but later might add Metakom and
 * Cyfral keys. iButton key uses 48 bits of information,
 * Metakom - 28, Cyfral - 16, so dword should be plenty for a
 * key. But we also need to keep key type (1 byte) and key name 
 * (let's say 32 characters 1 byte each), so memory diagram looks
 * like this:
 * 
 * Key {
 *      byte type;
 *      char name[32];
 *      byte key[8];
 * }
 * 
 * All of these keys are stored in EEPROM and the first two bytes
 * of it are used for the total number of keys.
 */
#define KEY_PIN A3

#define KEY_NAME_OFFSET 1
#define KEY_NAME_LEN 32
#define KEY_TYPE_OFFSET 0
#define KEY_OFFSET 33
#define KEY_TABLE_OFFSET 2
#define KEY_SIZE 41

struct Key {
    uint64_t cur_key;
    int key_index;
    byte key_type;
};

OneWire ibutton(KEY_PIN);

byte read_key(uint64_t *key);
byte copy_key(uint64_t new_key, Adafruit_SSD1306 *display = NULL);
void emulate_key(uint64_t key);
void save_key(bool original_title = false);
void delete_key(int index);
Key get_key_by_index(int index);
int get_key_offset(int index);
void update_key_by_index(Key key);

void writeByte(byte data, int pin);

byte read_ds1990(uint64_t *key);
byte copy_ds1990(uint64_t new_key, Adafruit_SSD1306 *display = NULL);
void emulate_ds1990(uint64_t key);

const int read_functions[] PROGMEM = {
    (const int)read_ds1990,
};

const int emulate_functions[] PROGMEM = {
    (const int)emulate_ds1990,
};

const int copy_functions[] PROGMEM = {
    (const int)copy_ds1990,
};

void top_button ();
void middle_button ();
void bottom_button ();
void check_buttons ();
bool check_button(int index);
void draw(int offset);

void switch_screen(int offset);
void redraw();

void list_screen_top_button_pressed(int offset);
void list_screen_middle_button_pressed(int offset);
void read_screen_menu_middle_button_pressed(int offset);
void list_screen_bottom_button_pressed(int offset);
void list_screen_draw(int offset);
void key_list_top_button_pressed(int offset);
void key_list_middle_button_pressed(int offset);
void key_menu_middle_button_pressed(int offset); // TODO
void key_list_bottom_button_pressed(int offset);
void key_list_draw(int offset);

void display_screen_top_button_pressed(int offset);
void read_screen_middle_button_pressed(int offset);
void emulate_screen_middle_button_pressed(int offset);
void copy_screen_middle_button_pressed(int offset);
void display_screen_bottom_button_pressed(int offset);
void display_screen_draw(int offset);
void display_key_screen_draw(int offset);

#define SCREEN_TOP_BUTTON_OFFSET 0
#define SCREEN_MIDDLE_BUTTON_OFFSET 1
#define SCREEN_BOTTOM_BUTTON_OFFSET 2
#define SCREEN_DRAW_FUNC_OFFSET 3

#define LIST_SCREEN_N_OFFSET 4
#define LIST_SCREEN_STRINGS_OFFSET 5
#define KEY_LIST_MAIN_OFFSET 4
#define KEY_LIST_N_OFFSET 5
#define KEY_LIST_STRINGS_OFFSET 6

#define DISPLAY_SCREEN_NAME_OFFSET 4
#define DISPLAY_SCREEN_SPECIFIER_OFFSET 5
#define DISPLAY_SCREEN_N_OFFSET 6
#define DISPLAY_SCREEN_OPTIONS_OFFSET 7

#define DISPLAY_SCREEN_NAME_Y_OFFSET 7
/*
 * So... Arduino Pro Mini has 2Kb of SRAM, which is too little for 
 * proper screen managment system, considering 1Kb gets used by 
 * screen buffer. In order to overcome that, screen layout stays 
 * in Flash memory using PROGMEM keyword
 * 
 * Objects can be represented like this:
 * ListScreen {
 *      // offset points to position in array of strings,
 *      // corresponding to the screen, keeping the functions
 *      void (*on_top_button_pressed)(int offset);
 *      void (*on_middle_button_pressed)(int offset);
 *      void (*on_bottom_button_pressed)(int offset);
 *      void (*draw)(int offset);
 * 
 *      int n_childen;
 *      char *names[];
 *      int offsets[];
 * }
 * 
 * KeyList {
 *      // offset points to position in array of strings,
 *      // corresponding to the screen, keeping the functions
 *      void (*on_top_button_pressed)(int offset);
 *      void (*on_middle_button_pressed)(int offset);
 *      void (*on_bottom_button_pressed)(int offset);
 *      void (*draw)(int offset);
 * 
 *      int main_offset;
 *      int n_childen;
 *      char *names[];
 *      int offsets[];
 * }
 * 
 * DisplayScreen {
 *      void (*on_top_button_pressed)(int offset);
 *      void (*on_middle_button_pressed)(int offset);
 *      void (*on_bottom_button_pressed)(int offset);
 *      void (*draw)(int offset);
 * 
 *      char *name;
 *      char *specifier;
 *      int n_options;
 *      char *names[];
 * }
 */

const char str_blank[] PROGMEM = "";
const char str0[] PROGMEM = "READ KEY";
const char str1[] PROGMEM = "EMULATE KEY";
const char str2[] PROGMEM = "COPY KEY";
const char str3[] PROGMEM = "CHOOSE KEY";
const char str4[] PROGMEM = "BRUTE FORCE";
const char str5[] PROGMEM = "BACK";
const char str6[] PROGMEM = "TYPE: ";
const char str7[] PROGMEM = "DS1990";
const char str8[] PROGMEM = "READING...";
const char str9[] PROGMEM = "SUCCESSFULLY READ";
const char str10[] PROGMEM = "SAVE";
const char str11[] PROGMEM = "EMULATE";
const char str12[] PROGMEM = "COPY";
const char str13[] PROGMEM = "CANCEL";
const char str14[] PROGMEM = "EMULATING...";
const char str15[] PROGMEM = "COPYING...";
const char str16[] PROGMEM = "Copying successful";
const char str17[] PROGMEM = "Copying failure";
const char str18[] PROGMEM = "READ BUT NOT IBUTTON";
const char str19[] PROGMEM = "READ BUT WRONG CRC";
const char str20[] PROGMEM = "New key ";
const char str21[] PROGMEM = "DELETE";

const char *const string_arr[] PROGMEM = {str0, str1, str2, str3, str4, str5, str6, str7, str8,
    str9, str10, str11, str12, str13, str14, str15, str16, str17, str18, str19, str20, str21};

#define BUFFER_LEN 64

char buffer[BUFFER_LEN];
bool new_data = false;
void check_serial();
void process_serial();

enum Offset {
    MAIN_MENU = 0,
    READ_SCREEN = 10,
    READ_SUCCESSFUL_MENU = 18,
    KEY_MENU = 31,
    EMULATE_SCREEN = 44,
    COPY_SCREEN = 51,
    NULL_SCREEN
};

const int screens[] PROGMEM = {
    //Main key menu
    (const int)key_list_top_button_pressed,
    (const int)key_list_middle_button_pressed,
    (const int)key_list_bottom_button_pressed,
    (const int)key_list_draw,
    KEY_MENU,
    2,
    (const int)str0,
    (const int)str_blank,
    READ_SCREEN,
    NULL_SCREEN,

    //Read screen
    (const int)display_screen_top_button_pressed,
    (const int)read_screen_middle_button_pressed,
    (const int)display_screen_bottom_button_pressed,
    (const int)display_screen_draw,
    (const int)str0,
    (const int)str6,
    1,
    (const int)str7,

    //Read screen menu
    (const int)list_screen_top_button_pressed,
    (const int)read_screen_menu_middle_button_pressed,
    (const int)list_screen_bottom_button_pressed,
    (const int)list_screen_draw,
    4,
    (const int)str10,
    (const int)str11,
    (const int)str12,
    (const int)str13,
    MAIN_MENU,
    EMULATE_SCREEN,
    COPY_SCREEN,
    MAIN_MENU,

    //Key menu
    (const int)list_screen_top_button_pressed,
    (const int)key_menu_middle_button_pressed,
    (const int)list_screen_bottom_button_pressed,
    (const int)list_screen_draw,
    4,
    (const int)str11,
    (const int)str12,
    (const int)str21,
    (const int)str5,
    EMULATE_SCREEN,
    COPY_SCREEN,
    MAIN_MENU,
    MAIN_MENU,

    //Emulate screen
    (const int)display_screen_top_button_pressed,
    (const int)emulate_screen_middle_button_pressed,
    (const int)display_screen_bottom_button_pressed,
    (const int)display_key_screen_draw,
    (const int)str1,
    (const int)str_blank,
    0,

    //Copy screen
    (const int)display_screen_top_button_pressed,
    (const int)copy_screen_middle_button_pressed,
    (const int)display_screen_bottom_button_pressed,
    (const int)display_key_screen_draw,
    (const int)str2,
    (const int)str_blank,
    0,
};

int prev_screen = 0;
int cur_screen = 0;

int cur_child = 0;

Key global_key = {0, -1, 0};

#define TOP_BUTTON_PIN 2
#define MIDDLE_BUTTON_PIN 3
#define BOTTOM_BUTTON_PIN 4
#define TOP_BUTTON_INDEX 0
#define MIDDLE_BUTTON_INDEX 1
#define BOTTOM_BUTTON_INDEX 2

struct Button {
    byte pin : 4;
    bool executed : 1;
    bool pressed : 1;
    void (*func)(void);
};

struct Button buttons[3] = {
    { TOP_BUTTON_PIN, false, false, top_button },
    { MIDDLE_BUTTON_PIN, false, false, middle_button },
    { BOTTOM_BUTTON_PIN, false, false, bottom_button }
};

void setup() {
    Serial.begin(9600);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));

        for(;;);
    }

    display.setRotation(2);

    pinMode(TOP_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MIDDLE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BOTTOM_BUTTON_PIN, INPUT_PULLUP);

    switch_screen(MAIN_MENU);
    reinterpret_cast<decltype(draw)*>(pgm_read_word_near(&screens[MAIN_MENU + SCREEN_DRAW_FUNC_OFFSET]))(MAIN_MENU);
}

void loop() {
    check_buttons();
    check_serial();
    process_serial();
}

void check_serial() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '[';
    char endMarker = ']';
    char rc;
 
    while (Serial.available() > 0 && !new_data) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                buffer[ndx] = rc;
                ndx++;
                if (ndx >= BUFFER_LEN) {
                    ndx = BUFFER_LEN - 1;
                }
            }
            else {
                buffer[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                new_data = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void process_serial() {
    if (new_data) {
        Serial.print("Received: ");
        Serial.println(buffer);

        if (buffer[0] == 'K') {
            int key_num = atoi(buffer + 2);
            EEPROM.updateInt(0, key_num);
        } else if (buffer[0] == 'D') {
            int deleted = atoi(buffer + 2);
            delete_key(deleted);
        } else if (buffer[0] == 'W') {
            byte i = 2, j = 0;
            for (; j < KEY_NAME_LEN && buffer[i] != ' '; i++, j++) {
                buffer[j] = buffer[i];
            }
            if (j < KEY_NAME_LEN)
                buffer[j] = '\0';

            char *cur_pointer = buffer + i + 1;
            global_key.key_type = strtol(cur_pointer, &cur_pointer, 10);

            global_key.cur_key = 0;
            for (j = 0; j < 7; j++) {
                ((uint8_t*)&global_key.cur_key)[j] = (byte)strtol(cur_pointer, &cur_pointer,  16);
            }

            ((uint8_t*)&global_key.cur_key)[7] = ibutton.crc8((uint8_t*)&global_key.cur_key, 7);

            Serial.print(F("Received "));
            for (j = 0; j < 8; j++) {
                if (((uint8_t*)&global_key.cur_key)[j] / 16 == 0)
                    Serial.print(0);
                
                Serial.print(((uint8_t*)&global_key.cur_key)[j], HEX);
                Serial.print(' ');
            }
            Serial.println();

            save_key(true);
        } else if (buffer[0] == 'L') {
            int cur_n_keys = EEPROM.readInt(0);
            Serial.print(F("Number of keys - "));
            Serial.println(cur_n_keys);

            for (int i = 0; i < cur_n_keys; i++) {
                int cur_key_start = get_key_offset(i);

                Serial.print(i);
                Serial.print(' ');

                for (byte j = 0; j < KEY_NAME_LEN && EEPROM.readByte(cur_key_start + KEY_NAME_OFFSET + j) != '\0'; j++) {
                    Serial.print((char)EEPROM.readByte(cur_key_start + KEY_NAME_OFFSET + j));
                }

                Serial.print(' ');
                Serial.print(EEPROM.readByte(cur_key_start + KEY_TYPE_OFFSET));
                Serial.print(' ');

                for (byte j = 0; j < 8; j++) {
                    if (EEPROM.readByte(cur_key_start + KEY_OFFSET + j) / 16 == 0)
                        Serial.print(0);
                    
                    Serial.print(EEPROM.readByte(cur_key_start + KEY_OFFSET + j), HEX);
                    Serial.print(' ');
                }

                Serial.println();
            }
        }

        new_data = false;
    }
}

#pragma region BUTTONS

void check_buttons () {
    for (byte i = 0; i < 3; i++) {
        buttons[i].pressed = !digitalRead(buttons[i].pin);
    }
    
    delay(25);

    for (byte i = 0; i < 3; i++) {
        if (buttons[i].pressed && !digitalRead(buttons[i].pin)) {
            if (!buttons[i].executed) {
                buttons[i].executed = true;
                buttons[i].func();
            }
        } else {
            buttons[i].pressed = false;
            buttons[i].executed = false;
        }
    }
}

bool check_button(int index) {
    buttons[index].pressed = !digitalRead(buttons[index].pin);

    delay(25);

    if (buttons[index].pressed && !digitalRead(buttons[index].pin)) {
        if (!buttons[index].executed) {
            return true;
        }
    } else {
        buttons[index].pressed = false;
        buttons[index].executed = false;
    }

    return false;
}

void top_button () {
    reinterpret_cast<decltype(draw)*>(pgm_read_word_near(&screens[cur_screen + SCREEN_TOP_BUTTON_OFFSET]))(cur_screen);
}

void middle_button () {
    reinterpret_cast<decltype(draw)*>(pgm_read_word_near(&screens[cur_screen + SCREEN_MIDDLE_BUTTON_OFFSET]))(cur_screen);
}

void bottom_button () {
    #if DEBUG
    Serial.println(F("bottom_button"));
    #endif

    reinterpret_cast<decltype(draw)*>(pgm_read_word_near(&screens[cur_screen + SCREEN_BOTTOM_BUTTON_OFFSET]))(cur_screen);
}

#pragma endregion

void switch_screen(int offset) {
    prev_screen = cur_screen;
    cur_screen = offset;
    cur_child = 0;
}

void redraw() {
    reinterpret_cast<decltype(draw)*>(pgm_read_word(&screens[cur_screen + SCREEN_DRAW_FUNC_OFFSET]))(cur_screen);
}

#pragma region LIST_SCREEN

void list_screen_top_button_pressed(int offset) {
    #if DEBUG
    Serial.println(F("list_screen_top_button_pressed"));
    #endif

    byte n_children = (int)pgm_read_word_near(&screens[offset + LIST_SCREEN_N_OFFSET]);
    cur_child = (cur_child + n_children - 1) % n_children;

    redraw();
}

void list_screen_middle_button_pressed(int offset) {
    #if DEBUG
    Serial.println(F("list_screen_middle_button_pressed"));
    #endif

    byte n_children = (int)pgm_read_word_near(&screens[offset + LIST_SCREEN_N_OFFSET]);
    int arr_start = offset + LIST_SCREEN_STRINGS_OFFSET + n_children;
    int new_offset = (int)pgm_read_word_near(&screens[arr_start + cur_child]);

    switch_screen(new_offset);
    redraw();
}

void read_screen_menu_middle_button_pressed(int offset) {
    #if DEBUG
    Serial.println(F("read_screen_menu_middle_button_pressed"));
    #endif
    byte n_children = (int)pgm_read_word_near(&screens[offset + LIST_SCREEN_N_OFFSET]);
    int arr_start = offset + LIST_SCREEN_STRINGS_OFFSET + n_children;
    int new_offset = (int)pgm_read_word_near(&screens[arr_start + cur_child]);

    #if DEBUG
    Serial.print(F("Current child "));
    Serial.println(cur_child);
    #endif

    if (cur_child == 0)
        save_key();
        
    switch_screen(new_offset);
    prev_screen = MAIN_MENU;
    redraw();

    if (new_offset == EMULATE_SCREEN) {
        reinterpret_cast<decltype(draw)*>(pgm_read_word_near(&screens[new_offset + SCREEN_MIDDLE_BUTTON_OFFSET]))(new_offset);
    } else if (new_offset == COPY_SCREEN) {
        reinterpret_cast<decltype(draw)*>(pgm_read_word_near(&screens[new_offset + SCREEN_MIDDLE_BUTTON_OFFSET]))(new_offset);
    }
}

void list_screen_bottom_button_pressed(int offset) {
    #if DEBUG
    Serial.println(F("list_screen_bottom_button_pressed"));
    #endif

    byte n_children = (int)pgm_read_word_near(&screens[offset + LIST_SCREEN_N_OFFSET]);
    cur_child = (cur_child + 1) % n_children;

    redraw();
}

void list_screen_draw(int offset) {
    byte width  = SCREEN_WIDTH - OFFSET_X * 2,
         height = (SCREEN_HEIGHT - OFFSET_Y * 2) / NUM_ROWS,
         text_y_offset = (height - FONT_HEIGHT) / 2 + 1;
    
    byte n_children = (int)pgm_read_word_near(&screens[offset + LIST_SCREEN_N_OFFSET]);
    int arr_start = offset + LIST_SCREEN_STRINGS_OFFSET;

    display.clearDisplay();
    display.setTextSize(FONT_SIZE);

    for (byte start = (cur_child / NUM_ROWS) * NUM_ROWS, i = 0;
            (i < NUM_ROWS) && (start < n_children); i++, start++) {
        strcpy_P(buffer, (char *)pgm_read_word_near(&screens[arr_start + start]));
        display.fillRect(OFFSET_X, OFFSET_Y + height * i, 
                            width, height, start == cur_child);
        display.setCursor(OFFSET_X + 1, OFFSET_Y + height * i + text_y_offset);
        display.setTextColor(start != cur_child);
        display.println(buffer);

        if ((int)pgm_read_word_near(&screens[offset + LIST_SCREEN_STRINGS_OFFSET + n_children + start]) == NULL_SCREEN) {
            display.fillRect(OFFSET_X + 1, OFFSET_Y + height * i + (height - LINE_WIDTH) / 2, display.width() - (OFFSET_X + 1) * 2, LINE_WIDTH, start != cur_child);
        }
    }

    display.display();
}

void key_list_top_button_pressed(int offset) {
    byte n_children = (int)pgm_read_word_near(&screens[offset + KEY_LIST_N_OFFSET]);
    byte n_keys = EEPROM.readInt(0);
    cur_child = (cur_child + n_children + n_keys - 1) % (n_children + n_keys);

    if ((int)pgm_read_word_near(&screens[offset + KEY_LIST_STRINGS_OFFSET + n_children + cur_child]) == NULL_SCREEN)
        cur_child = (cur_child + n_children + n_keys - 1) % (n_children + n_keys);
    
    redraw();
}

void key_list_middle_button_pressed(int offset) {
    byte n_children = (int)pgm_read_word_near(&screens[offset + KEY_LIST_N_OFFSET]);

    if (cur_child < n_children) {
        int arr_start = offset + KEY_LIST_STRINGS_OFFSET + n_children;
        int new_offset = (int)pgm_read_word_near(&screens[arr_start + cur_child]);

        if (new_offset != NULL_SCREEN) {
            switch_screen(new_offset);
            redraw();
        }    
    } else {
        global_key = get_key_by_index(cur_child - n_children);
        
        int new_offset = (int)pgm_read_word(&screens[offset + KEY_LIST_MAIN_OFFSET]);
        switch_screen(new_offset);
        redraw();
    }
}

void key_menu_middle_button_pressed(int offset) {
    byte n_children = (int)pgm_read_word_near(&screens[offset + LIST_SCREEN_N_OFFSET]);
    int arr_start = offset + LIST_SCREEN_STRINGS_OFFSET + n_children;
    int new_offset = (int)pgm_read_word_near(&screens[arr_start + cur_child]);

    if (cur_child == 2) {
        delete_key(global_key.key_index);
    }

    switch_screen(new_offset);
    redraw();
}

void key_list_bottom_button_pressed(int offset) {
    byte n_children = (int)pgm_read_word_near(&screens[offset + KEY_LIST_N_OFFSET]);
    byte n_keys = EEPROM.readInt(0);
    cur_child = (cur_child + 1) % (n_children + n_keys);

    if ((int)pgm_read_word_near(&screens[offset + KEY_LIST_STRINGS_OFFSET + n_children + cur_child]) == NULL_SCREEN)
        cur_child = (cur_child + 1) % (n_children + n_keys);

    redraw();
}

void key_list_draw(int offset) {
    display.clearDisplay();

    byte width  = SCREEN_WIDTH - OFFSET_X * 2,
         height = (SCREEN_HEIGHT - OFFSET_Y * 2) / NUM_ROWS,
         text_y_offset = (height - FONT_HEIGHT) / 2 + 1;
    
    byte n_children = (int)pgm_read_word_near(&screens[offset + KEY_LIST_N_OFFSET]);
    byte n_keys = EEPROM.readInt(0);
    byte start = 0, i = 0;
    
    for (start = (cur_child / NUM_ROWS) * NUM_ROWS, i = 0;
            (i < NUM_ROWS) && (start < n_children); i++, start++) {
        strcpy_P(buffer, (char *)pgm_read_word(&screens[offset + KEY_LIST_STRINGS_OFFSET + start]));

        display.fillRect(OFFSET_X, OFFSET_Y + height * i, 
                            width, height, start == cur_child);
        display.setCursor(OFFSET_X + 1, OFFSET_Y + height * i + text_y_offset);
        display.setTextColor(start != cur_child);
        display.println(buffer);

        if ((int)pgm_read_word_near(&screens[offset + KEY_LIST_STRINGS_OFFSET + n_children + start]) == NULL_SCREEN) {
            display.fillRect(OFFSET_X + 1, OFFSET_Y + height * i + (height - LINE_WIDTH) / 2, display.width() - (OFFSET_X + 1) * 2, LINE_WIDTH, start != cur_child);
        }
    }

    for (byte j = 0; (start < n_keys + n_children) && (i < NUM_ROWS); start++, i++, j++) {
        int key_start = get_key_offset(start - n_children);

        byte k = 0;
        for (;k < KEY_NAME_LEN && EEPROM.readByte(key_start + KEY_NAME_OFFSET + k) != '\0'; k++)
            buffer[k] = EEPROM.readByte(key_start + KEY_NAME_OFFSET + k);
        
        if (k < BUFFER_LEN)
            buffer[k] = '\0';
        
        display.fillRect(OFFSET_X, OFFSET_Y + height * i, 
                            width, height, start == cur_child);
        display.setCursor(OFFSET_X + 1, OFFSET_Y + height * i + text_y_offset);
        display.setTextColor(start != cur_child);
        display.println(buffer);
    }

    display.display();
}

#pragma endregion


#pragma region DISPLAY_SCREEN

void display_screen_top_button_pressed(int offset) {
    #if DEBUG
    Serial.println(F("display_screen_top_button_pressed"));
    #endif

    byte n_options = (int) pgm_read_word_near(&screens[offset + DISPLAY_SCREEN_N_OFFSET]);

    if (n_options)
        cur_child = (cur_child + 1) % n_options;

    redraw();
}

void read_screen_middle_button_pressed(int offset) {
    #if DEBUG
    Serial.println(F("read_screen_middle_button_pressed"));
    #endif

    global_key.key_type = cur_child;
    global_key.key_index = -1;

    display.fillRect(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, FONT_HEIGHT * FONT_SIZE, BLACK);

    strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[8]));
    byte msg_len = strlen(buffer);
    
    display.setTextSize(FONT_SIZE);
    display.setTextColor(WHITE);
    display.setCursor((SCREEN_WIDTH - msg_len * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2);
    display.println(buffer);
    display.display();

    byte exit_code = 1;

    while (exit_code == 1) {
        if (check_button(MIDDLE_BUTTON_INDEX)) {
            buttons[MIDDLE_BUTTON_INDEX].executed = true;
            redraw();
            return;
        }

        if (check_button(BOTTOM_BUTTON_INDEX)) {
            buttons[BOTTOM_BUTTON_INDEX].executed = true;
            switch_screen(prev_screen);
            redraw();
            return;
        }
        
        exit_code = read_key(&global_key.cur_key);
        delay(25);
    }
    
    display.fillRect(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, FONT_HEIGHT * FONT_SIZE, BLACK);

    switch (exit_code) {
        case 0:
            strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[9]));
            display.setCursor((SCREEN_WIDTH - 16 * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2 + FONT_SIZE * FONT_HEIGHT);
            for (byte i = 0; i < 8; i++) {
                if (((uint8_t *)&global_key.cur_key)[i] / 16 == 0)
                    display.print(0);
                display.print(((uint8_t *)&global_key.cur_key)[i], HEX);
            }
            break;
        case 2:
            strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[18]));
            break;
        case 3:
            strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[19]));
            break;
    }

    msg_len = strlen(buffer);
    display.setCursor((SCREEN_WIDTH - msg_len * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2);
    display.println(buffer);
    display.display();
    delay(2000);

    if (exit_code == 0)
        switch_screen(READ_SUCCESSFUL_MENU);
    else
        switch_screen(prev_screen);
    
    redraw();
}

void emulate_screen_middle_button_pressed(int offset) {
    int n_keys = EEPROM.readInt(0);

    display.setTextSize(FONT_SIZE);
    display.setTextColor(WHITE);

    strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[14]));
    byte msg_len = strlen(buffer);
    
    display.setCursor((SCREEN_WIDTH - msg_len * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2 + 2 * FONT_SIZE * FONT_HEIGHT);
    display.println(buffer);
    display.display();

    for (;; (global_key.key_index != -1) && (global_key.key_index = (global_key.key_index + 1) % n_keys)) {
        if (global_key.key_index != -1)
            global_key = get_key_by_index(global_key.key_index);
        
        display.fillRect((SCREEN_WIDTH - 16 * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2 + FONT_SIZE * FONT_HEIGHT, 16 * FONT_WIDTH * FONT_SIZE, FONT_HEIGHT * FONT_SIZE, BLACK);
        display.setCursor((SCREEN_WIDTH - 16 * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2 + FONT_SIZE * FONT_HEIGHT);

        for (byte i = 0; i < 8; i++) {
            if (((uint8_t *)&global_key.cur_key)[i] / 16 == 0)
                display.print(0);
            display.print(((uint8_t *)&global_key.cur_key)[i], HEX);
        }

        display.display();

        emulate_key(global_key.cur_key);

        if (check_button(MIDDLE_BUTTON_INDEX)) {
            buttons[MIDDLE_BUTTON_INDEX].executed = true;
            break;
        }

        if (check_button(BOTTOM_BUTTON_INDEX)) {
            buttons[BOTTOM_BUTTON_INDEX].executed = true;
            switch_screen(prev_screen);
            break;
        }

        if (check_button(TOP_BUTTON_INDEX)) {
            buttons[TOP_BUTTON_INDEX].executed = true;
            if (global_key.key_index == -1)
                break;
        }
    }
    
    redraw();
}

void copy_screen_middle_button_pressed(int offset) {
    display.setTextSize(FONT_SIZE);
    display.setTextColor(WHITE);

    strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[15]));
    byte msg_len = strlen(buffer);
    
    display.setCursor((SCREEN_WIDTH - msg_len * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2 + 2 * FONT_SIZE * FONT_HEIGHT);
    display.println(buffer);
    display.display();

    byte exit_code = 1;

    while (exit_code == 1) {
        for (byte i = 0; i < 40; i++) {
            if (check_button(MIDDLE_BUTTON_INDEX)) {
                buttons[MIDDLE_BUTTON_INDEX].executed = true;
                redraw();
                return;
            }

            if (check_button(BOTTOM_BUTTON_INDEX)) {
                buttons[BOTTOM_BUTTON_INDEX].executed = true;
                switch_screen(prev_screen);
                redraw();
                return;
            }

            delay(25);
        }
        
        exit_code = copy_key(global_key.cur_key, &display);
    }
    
    display.fillRect(0, SCREEN_HEIGHT / 2 + 2 * FONT_SIZE * FONT_HEIGHT, SCREEN_WIDTH, FONT_HEIGHT * FONT_SIZE, BLACK);

    if (exit_code == 2) {
        strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[17]));
    } else {
        strcpy_P(buffer, (char *)pgm_read_word_near(&string_arr[16]));
    }

    msg_len = strlen(buffer);
    display.setCursor((SCREEN_WIDTH - msg_len * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2 + 2 * FONT_SIZE * FONT_HEIGHT);
    display.println(buffer);
    display.display();
    delay(2000);

    switch_screen(prev_screen);
    redraw();
}

void display_screen_bottom_button_pressed(int offset) {
    #if DEBUG
    Serial.println(F("display_screen_bottom_button_pressed"));
    #endif

    cur_child = 0;
    switch_screen (prev_screen);
    redraw();
}

void display_key_screen_draw(int offset) {
    #if DEBUG
    Serial.println(F("display_screen_draw"));
    #endif

    strcpy_P(buffer, (char *)pgm_read_word_near(&screens[offset + DISPLAY_SCREEN_NAME_OFFSET]));
    byte name_len = strlen(buffer);
    
    display.clearDisplay();

    display.setTextSize(FONT_SIZE);
    display.setTextColor(WHITE);
    display.setCursor((SCREEN_WIDTH - name_len * FONT_SIZE * FONT_WIDTH + 1) / 2, OFFSET_Y + DISPLAY_SCREEN_NAME_Y_OFFSET);
    display.println(buffer);

    display.setCursor((SCREEN_WIDTH - 16 * FONT_SIZE * FONT_WIDTH) / 2, SCREEN_HEIGHT / 2 + FONT_SIZE * FONT_HEIGHT);
    for (byte i = 0; i < 8; i++) {
        if (((uint8_t *)&global_key.cur_key)[i] / 16 == 0)
            display.print(0);
        display.print(((uint8_t *)&global_key.cur_key)[i], HEX);
    }

    display.display();
}

void display_screen_draw(int offset) {
    #if DEBUG
    Serial.println(F("display_screen_draw"));
    #endif

    strcpy_P(buffer, (char *)pgm_read_word_near(&screens[offset + DISPLAY_SCREEN_NAME_OFFSET]));
    byte name_len = strlen(buffer);
    
    display.clearDisplay();

    display.setTextSize(FONT_SIZE);
    display.setTextColor(WHITE);
    display.setCursor((SCREEN_WIDTH - name_len * FONT_SIZE * FONT_WIDTH + 1) / 2, OFFSET_Y + DISPLAY_SCREEN_NAME_Y_OFFSET);
    display.println(buffer);

    byte n_options = (byte)pgm_read_word_near(&screens[offset + DISPLAY_SCREEN_N_OFFSET]);

    if (n_options != 0) {    
        strcpy_P(buffer, (char *)pgm_read_word_near(&screens[offset + DISPLAY_SCREEN_OPTIONS_OFFSET + cur_child]));
        byte option_len = strlen(buffer);
        strcpy_P(buffer, (char *)pgm_read_word_near(&screens[offset + DISPLAY_SCREEN_SPECIFIER_OFFSET]));
        name_len = strlen(buffer);
        display.setCursor((SCREEN_WIDTH - (name_len + option_len) * FONT_SIZE * FONT_WIDTH + 1) / 2, 
                            SCREEN_HEIGHT / 2 + 1);
        display.println(buffer);
        
        strcpy_P(buffer, (char *)pgm_read_word_near(&screens[offset + DISPLAY_SCREEN_OPTIONS_OFFSET + cur_child]));
        display.setCursor((SCREEN_WIDTH - (name_len + option_len) * FONT_SIZE * FONT_WIDTH + 1) / 2 + name_len * FONT_WIDTH * FONT_SIZE, 
                            SCREEN_HEIGHT / 2 + 1);
        display.println(buffer);
    }

    display.display();
}

#pragma endregion


#pragma region KEYS

byte read_key(uint64_t *key) {
    return reinterpret_cast<decltype(read_key)*>(pgm_read_word_near(&read_functions[global_key.key_type]))(key);
}

byte copy_key(uint64_t new_key, Adafruit_SSD1306 *display) {
    return reinterpret_cast<decltype(copy_key)*>(pgm_read_word_near(&copy_functions[global_key.key_type]))(new_key, display);
}

void emulate_key(uint64_t key) {
    reinterpret_cast<decltype(emulate_key)*>(pgm_read_word_near(&emulate_functions[global_key.key_type]))(key);
}

void save_key(bool original_title) {
    int cur_n_keys = EEPROM.readInt(0);

    int i = 0;

    for (;i < cur_n_keys && !(EEPROM.readByte(KEY_TABLE_OFFSET + i * KEY_SIZE + KEY_TYPE_OFFSET) >> 7); i++);

    if (i != cur_n_keys && (EEPROM.readByte(KEY_TABLE_OFFSET + i * KEY_SIZE + KEY_TYPE_OFFSET) & ~(1 << 7)) != 1) {
        byte num_free = EEPROM.readByte(KEY_TABLE_OFFSET + i * KEY_SIZE + KEY_TYPE_OFFSET) & ~(1 << 7);
        EEPROM.updateByte(KEY_TABLE_OFFSET + (i + 1) * KEY_SIZE + KEY_TYPE_OFFSET, (1 << 7) | (num_free - 1));
        EEPROM.updateByte(KEY_TABLE_OFFSET + (i + num_free - 1) * KEY_SIZE + KEY_TYPE_OFFSET, (1 << 7) | (num_free - 1));
    }

    global_key.key_index = i;
    cur_n_keys++;

    int key_start = KEY_TABLE_OFFSET + global_key.key_index * KEY_SIZE;

    EEPROM.updateInt(0, cur_n_keys);
    update_key_by_index(global_key);

    if (!original_title) {
        strcpy_P(buffer, (char *)pgm_read_word(&string_arr[20]));

        if (cur_n_keys >= 10) {
            buffer[8] = '0' + cur_n_keys / 10;
            buffer[9] = '0' + cur_n_keys % 10;
            buffer[10] = '\0';
        } else {
            buffer[8] = '0' + cur_n_keys % 10;
            buffer[9] = '\0';
        }
    }

    for (byte i = 0; i < KEY_NAME_LEN; i++) {
        EEPROM.updateByte(key_start + KEY_NAME_OFFSET + i, buffer[i]);
    }
}

void delete_key(int index) {
    int offset = get_key_offset(index);
    int n_keys = EEPROM.readInt(0);
    int last_offset = get_key_offset(n_keys - 1);
    EEPROM.updateInt(0, n_keys - 1);

    EEPROM.updateByte(offset + KEY_TYPE_OFFSET, 1 | (1 << 7));

    if ((offset != last_offset) && (EEPROM.readByte(offset + KEY_SIZE + KEY_TYPE_OFFSET) >> 7) &&
        (offset != KEY_TABLE_OFFSET) && (EEPROM.readByte(offset - KEY_SIZE + KEY_TYPE_OFFSET) >> 7)) {
            byte num_free_1 = EEPROM.readByte(offset - KEY_SIZE + KEY_TYPE_OFFSET) & ~(1 << 7),
                 num_free_2 = EEPROM.readByte(offset + KEY_SIZE + KEY_TYPE_OFFSET) & ~(1 << 7);
            
            EEPROM.updateByte(offset - num_free_1 * KEY_SIZE + KEY_TYPE_OFFSET, (num_free_1 + num_free_2 + 1) | (1 << 7));
            EEPROM.updateByte(offset + num_free_2 * KEY_SIZE + KEY_TYPE_OFFSET, (num_free_1 + num_free_2 + 1) | (1 << 7));
    } else {
        if ((offset != last_offset) && (EEPROM.readByte(offset + KEY_SIZE + KEY_TYPE_OFFSET) >> 7)) {
            byte num_free = EEPROM.readByte(offset + KEY_SIZE + KEY_TYPE_OFFSET) & ~(1 << 7);
            EEPROM.updateByte(offset + KEY_TYPE_OFFSET, (num_free + 1) | (1 << 7));
            EEPROM.updateByte(offset + num_free * KEY_SIZE + KEY_TYPE_OFFSET, (num_free + 1) | (1 << 7));
        } else if ((offset != KEY_TABLE_OFFSET) && (EEPROM.readByte(offset - KEY_SIZE + KEY_TYPE_OFFSET) >> 7)) {
            byte num_free = EEPROM.readByte(offset - KEY_SIZE + KEY_TYPE_OFFSET) & ~(1 << 7);
            EEPROM.updateByte(offset + KEY_TYPE_OFFSET, (num_free + 1) | (1 << 7));
            EEPROM.updateByte(offset - num_free * KEY_SIZE + KEY_TYPE_OFFSET, (num_free + 1) | (1 << 7));
        }
    }
}

Key get_key_by_index(int index) {
    int offset = get_key_offset(index);

    Key key = (struct Key){0, index, EEPROM.readByte(offset + KEY_TYPE_OFFSET)};
    key.cur_key = EEPROM.readLong(offset + KEY_OFFSET + 4);
    key.cur_key <<= 32;
    key.cur_key |= EEPROM.readLong(offset + KEY_OFFSET);

    return key;
}

int get_key_offset(int index) {
    int real_index = 0, i = 0;

    if (EEPROM.readByte(KEY_TABLE_OFFSET + KEY_TYPE_OFFSET) >> 7) {
        byte key_offset = EEPROM.readByte(KEY_TABLE_OFFSET + KEY_TYPE_OFFSET) & ~(1 << 7);
        real_index += key_offset;
        // Serial.println(F("la"));
    }

    for (;i < index; i++) {
        if (EEPROM.readByte(KEY_TABLE_OFFSET + real_index * KEY_SIZE + KEY_SIZE + KEY_TYPE_OFFSET) >> 7) {
            byte key_offset = EEPROM.readByte(KEY_TABLE_OFFSET + real_index * KEY_SIZE + KEY_SIZE + KEY_TYPE_OFFSET) & ~(1 << 7);
            real_index += key_offset;
        } else {
            real_index++;
        }
    }

    return KEY_TABLE_OFFSET + real_index * KEY_SIZE;
}

void update_key_by_index(Key key) {
    int cur_key_start = KEY_TABLE_OFFSET + key.key_index * KEY_SIZE;

    EEPROM.updateByte(cur_key_start + KEY_TYPE_OFFSET, key.key_type);
    EEPROM.updateLong(cur_key_start + KEY_OFFSET, (uint32_t)key.cur_key);
    EEPROM.updateLong(cur_key_start + KEY_OFFSET + 4, (uint32_t)(key.cur_key >> 32));
}

byte read_ds1990(uint64_t *key) {
    #if DEBUG
    Serial.println(F("Reading key..."));
    #endif

    if(!ibutton.reset()) {
        #if DEBUG
        Serial.println(F("No available devices!"));
        #endif

        return 1;
    }

    ibutton.write(0x33);
    delay(1);

    ibutton.read_bytes((uint8_t *)key, 8);

    #if DEBUG
    Serial.print(F("Read key "));
    for (byte i = 0; i < 8; i++) {
        if (((uint8_t *)key)[i] / 16 == 0)
            Serial.print(0);
        Serial.print(((uint8_t *)key)[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
    #endif

    if (((uint8_t *)key)[0] != 0x01) {
        #if DEBUG
        Serial.println(F("Device is not iButton!"));
        Serial.println();
        #endif

        return 2;
    }
    

    if (ibutton.crc8((uint8_t *)key, 7) != ((uint8_t *)key)[7]) {
        #if DEBUG
        Serial.println((uint32_t)(*key >> 32), HEX);
        Serial.print((uint32_t)*key, HEX);

        Serial.print(F("Incorrect CRC!\nCorrect CRC:"));
        Serial.println(ibutton.crc8((uint8_t *)key, 7), HEX);
        #endif

        return 3;
    }

    ibutton.reset_search();
    return 0;
}

byte copy_ds1990(uint64_t new_key, Adafruit_SSD1306 *display) {
    if(!ibutton.reset()) {
        #if DEBUG
        Serial.println(F("No available devices!"));
        #endif

        return 1;
    }

    ibutton.write(0x33);
    delay(1);

    uint64_t prev_key;
    ibutton.read_bytes((uint8_t *)&prev_key, 8);

    #if DEBUG
    for (byte i = 0; i < 8; i++) {
        if (((uint8_t *)&prev_key)[i] / 16 == 0)
            Serial.print(0);
        Serial.print(((uint8_t *)&prev_key)[i], HEX);
        Serial.print(' ');
    }
    Serial.println();

    if (((uint8_t *)&prev_key)[0] != 0x01) {
        Serial.println(F("Device signature is not iButton!"));
    }
    
    if (ibutton.crc8((uint8_t *)&prev_key, 7) != ((uint8_t *)&prev_key)[7]) {
        Serial.print(F("Incorrect CRC! Correct CRC:"));
        Serial.println(ibutton.crc8((uint8_t *)&prev_key, 7), HEX);
    }
    
    
    Serial.print(F("Writing iButton ID: "));
    for (byte i = 0; i < 8; i++) {
        if (((uint8_t *)&new_key)[i] / 16 == 0)
            Serial.print(0);
        Serial.print(((uint8_t *)&new_key)[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
    #endif

    if (display != NULL) {
        display->fillRect(display->width() / 2 - FONT_SIZE * FONT_WIDTH * 4, display->height() / 2, FONT_SIZE * FONT_WIDTH * 8, FONT_SIZE * FONT_HEIGHT, BLACK);
        display->display();
        display->setTextColor(WHITE);
        display->setTextSize(FONT_SIZE);
        display->setCursor(display->width() / 2 - FONT_SIZE * FONT_WIDTH * 4, display->height() / 2);
    }
    
    ibutton.skip();
    ibutton.reset();
    ibutton.write(0xD1);
    digitalWrite(KEY_PIN, LOW); pinMode(KEY_PIN, OUTPUT); delayMicroseconds(60);
    pinMode(KEY_PIN, INPUT); digitalWrite(KEY_PIN, HIGH); delay(10);
                        
    ibutton.skip();
    ibutton.reset();
    ibutton.write(0xD5);
    
    for (byte i = 0; i < 7; i++){
        writeByte(((uint8_t *)&new_key)[i], KEY_PIN);

        #if DEBUG
        Serial.print(F("*"));
        #endif

        if (display != NULL) {
            display->print(F("*"));
            display->display();
        }
    }
    
    writeByte(ibutton.crc8((uint8_t *)&new_key, 7), KEY_PIN);

    #if DEBUG
    Serial.println(F("*"));
    #endif

    if (display != NULL) {
        display->print(F("*"));
        display->display();
    }
    
    ibutton.reset();
    ibutton.write(0xD1);
    digitalWrite(KEY_PIN, LOW); pinMode(KEY_PIN, OUTPUT); delayMicroseconds(10);
    pinMode(KEY_PIN, INPUT); digitalWrite(KEY_PIN, HIGH); delay(10);

    ibutton.skip();
    ibutton.reset();
    ibutton.write(0x33);

    uint64_t read_key;
    ibutton.read_bytes((uint8_t*)&read_key, 8);
    if (read_key != new_key)
        return 2;

    #if DEBUG
    Serial.print(F("ID after write:"));

    for (byte i = 0; i < 8; i++) {
        if (((uint8_t *)&read_key)[i] / 16 == 0)
            Serial.print(0);
        Serial.print(((uint8_t *)&read_key)[i], HEX);
        Serial.print(' ');
    }
    #endif

    return 0;
}

void writeByte(byte data, int pin){
    for(byte data_bit = 0; data_bit < 8; data_bit++){
        if (data & 1){
            digitalWrite(pin, LOW);
            pinMode(pin, OUTPUT);
            delayMicroseconds(60);
            pinMode(pin, INPUT);
            digitalWrite(pin, HIGH);
            delay(10);
        } else {
            digitalWrite(pin, LOW);
            pinMode(pin, OUTPUT);
            pinMode(pin, INPUT);
            digitalWrite(pin, HIGH);
            delay(10);
        }
    
    data = data >> 1;
  }
}

void emulate_ds1990(uint64_t key) {
    auto hub = OneWireHub(KEY_PIN);
    auto ds1990 = DS2401(DS2401::family_code, ((uint8_t*)&key)[1], ((uint8_t*)&key)[2], ((uint8_t*)&key)[3],
                                ((uint8_t*)&key)[4], ((uint8_t*)&key)[5], ((uint8_t*)&key)[6]);
    hub.attach(ds1990);
    
    while (1) {
        if (check_button(MIDDLE_BUTTON_INDEX) || check_button(BOTTOM_BUTTON_INDEX) || check_button(TOP_BUTTON_INDEX)) {
            return;
        }

        hub.poll();
    }
}

#pragma endregion