// Works with the Atmel AT28C64B

/*
    Commands are always at least 4 bytes
    [CMD][Address][Operand0]

    Where CMD = 1 byte
    Address = 2 bytes (uint16_t)
    Operand0 = 1 byte

    Some commands may read additional data.

    A CMD_ACK will be sent after the first 4 bytes is received.

    There will always be a second result sent after the CMD_ACK which should be CMD_SUCCESS
*/
#define CMD_WRITE 1
#define CMD_WRITE_PAGE 2
#define CMD_READ 3
#define CMD_ACK 199
#define CMD_SUCCESS 0

#define PAGE_MASK 0xFFC0
#define PAGE_LENGTH 64

#define ERROR_INVALID_LENGTH 255
#define ERROR_INVALID_PAGE 254
#define ERROR_INVALID_COMMAND 253
#define ERROR_NO_DATA_SENT 252
#define ERROR_UNKNOWN 251

#define SHIFT_SERIAL 10
#define SHIFT_LATCH 16
#define SHIFT_CLOCK 14
#define WRITE_ENABLE 15

#define IO_0 2
#define IO_1 3
#define IO_2 4
#define IO_3 5
#define IO_4 6
#define IO_5 7
#define IO_6 8
#define IO_7 9

#define DATA_POLL_PIN IO_7

#define OUTPUT_ENABLE 0x8000

uint8_t currentMode = INPUT;

void setAddress(uint16_t address, bool outputEnabled)
{
    // OE enabled low
    address = address | (outputEnabled ? 0 : OUTPUT_ENABLE);
    shiftOut(SHIFT_SERIAL, SHIFT_CLOCK, MSBFIRST, address >> 8);
    shiftOut(SHIFT_SERIAL, SHIFT_CLOCK, MSBFIRST, address);

    digitalWrite(SHIFT_LATCH, LOW);
    digitalWrite(SHIFT_LATCH, HIGH);
    digitalWrite(SHIFT_LATCH, LOW);
}

byte eepromRead(uint16_t address)
{
    setAddress(address, true);

    setDataRead();

    byte data = 0;
    for (int pin = IO_7; pin >= IO_0; pin--)
    {
        data = (data << 1) + digitalRead(pin);
    }

    return data;
}

void pollDataPin(uint8_t pin, uint8_t val)
{
    uint8_t found = val == HIGH ? LOW : HIGH;

    pinMode(pin, INPUT);

    while (found != val)
    {
        found = digitalRead(pin);
    }

    pinMode(pin, currentMode);
}

void waitForWriteComplete(uint8_t address, uint8_t data)
{
    // Set the address with output enabled
    setAddress(address, true);
    // poll for the compliment of most significant bit
    pollDataPin(DATA_POLL_PIN, data & 0x80 == 0 ? HIGH : LOW);
}

void eepromWrite(uint16_t address, uint8_t data, bool waitForComplete)
{
    setAddress(address, false);

    setDataWrite();

    for (int pin = IO_0; pin <= IO_7; pin++)
    {
        digitalWrite(pin, data & 1);
        data >>= 1;
    }

    // Pulse eeprom write enable low
    digitalWrite(WRITE_ENABLE, HIGH);
    delayMicroseconds(1);
    digitalWrite(WRITE_ENABLE, LOW);
    delayMicroseconds(1);
    digitalWrite(WRITE_ENABLE, HIGH);

    if (!waitForComplete)
        return;

    waitForWriteComplete(address, data);
}

int8_t eepromWritePage(uint16_t startAddress, uint8_t length, uint8_t data[PAGE_LENGTH])
{
    if (length < 1 || length > PAGE_LENGTH)
        return ERROR_INVALID_LENGTH;

    uint16_t page = startAddress & PAGE_MASK;
    uint16_t endAddress = startAddress + (length - 1);

    // ensure all bytes are in the same page
    if ((endAddress & PAGE_MASK) != page)
        return ERROR_INVALID_PAGE;

    uint16_t currentAddress = startAddress;
    for (int i = 0; i < length; i++, currentAddress++)
    {
        eepromWrite(currentAddress, data[i], i == (length - 1));
    }

    return CMD_SUCCESS;
}

void setDataPinMode(uint8_t mode)
{
    if (currentMode == mode)
        return;

    for (int pin = IO_0; pin <= IO_7; pin++)
    {
        pinMode(pin, mode);
    }
    currentMode = mode;
}

void setDataRead()
{
    setDataPinMode(INPUT);
}

void setDataWrite()
{
    setDataPinMode(OUTPUT);
}

void setup()
{
    pinMode(SHIFT_SERIAL, OUTPUT);
    pinMode(SHIFT_CLOCK, OUTPUT);
    pinMode(SHIFT_LATCH, OUTPUT);
    digitalWrite(WRITE_ENABLE, HIGH);
    pinMode(WRITE_ENABLE, OUTPUT);

    Serial.begin(115200);
    while (!Serial)
    {
    }

    Serial.setTimeout(1000);
}

void loop()
{
    if (Serial.available() == 0)
        return;

    uint8_t buf[PAGE_LENGTH];
    size_t readResult;
    uint8_t cmd;
    uint16_t address;
    uint8_t data;
    uint8_t length;
    uint8_t operand0;
    uint8_t response = ERROR_UNKNOWN;

    readResult = Serial.readBytes(buf, 4);
    if (readResult == 0)
    {
        Serial.write(ERROR_NO_DATA_SENT);
        return;
    }

    cmd = buf[0];
    address = ((uint16_t)buf[1]) << 8;
    address += buf[2];
    operand0 = buf[3];

    Serial.write(CMD_ACK);
    Serial.flush();

    switch (cmd)
    {
    case CMD_WRITE:
        eepromWrite(address, operand0, true);
        response = CMD_SUCCESS;
        break;
    case CMD_WRITE_PAGE:
        length = operand0;
        readResult = Serial.readBytes(buf, length);
        if (readResult == 0)
        {
            response = ERROR_NO_DATA_SENT;
            break;
        }

        response = eepromWritePage(address, length, buf);
        break;
    case CMD_READ:
        length = operand0;
        
        for (uint16_t i = address; i < address + length; i++)
        {
            Serial.write(eepromRead(i));
        }

        response = CMD_SUCCESS;
        break;
    default:
        response = ERROR_INVALID_COMMAND;
        break;
    }
    Serial.write(response);
}