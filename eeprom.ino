// Works with the Atmel AT28C64B

#define CMD_WRITE

#define PAGE_MASK 0xFFC0
#define SUCCESS 0
#define ERROR_INVALID_LENGTH -1
#define ERROR_INVALID_PAGE -2

#define SHIFT_SERIAL 10
#define SHIFT_LATCH  16
#define SHIFT_CLOCK  14
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

    if (!waitForComplete) return;

    waitForWriteComplete(address, data);
}

int8_t eepromWritePage(uint16_t startAddress, uint8_t length, uint8_t data[64])
{
    if (length < 1 || length > 64) return ERROR_INVALID_LENGTH;

    uint16_t page = startAddress & PAGE_MASK;
    uint16_t endAddress = startAddress + (length - 1);

    // ensure all bytes are in the same page
    if ((endAddress & PAGE_MASK) != page) return ERROR_INVALID_PAGE;

    uint16_t currentAddress = startAddress;
    for (int i = 0; i < length; i++, currentAddress++)
    {
        eepromWrite(currentAddress, data[i], i == (length - 1));
    }


    return SUCCESS;
}

void setDataPinMode(uint8_t mode)
{
    if (currentMode == mode) return;

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

    const uint16_t startAddr = 0;
    const uint16_t length = 64;
    const uint16_t endAddr = startAddr + length;

    char str[1024];

    uint8_t value[length];

    for (int i = 0; i < length; i++)
    {
        value[i] = i;
    }

    int8_t result = eepromWritePage(startAddr, length, value);

    if (result != SUCCESS)
    {
        sprintf(str, "Bad Result: %d", result);
        Serial.println(str);
        return;
    }


    uint8_t valueRead[8];

    for (int currentAddress = startAddr; currentAddress < endAddr; )
    {
        for (int j = 0; j < 8; j++, currentAddress++)
        {
            valueRead[j] = eepromRead(currentAddress);
        }

        sprintf(str, "%02X %02X %02X %02X %02X %02X %02X %02X", valueRead[0], valueRead[1], valueRead[2], valueRead[3], valueRead[4], valueRead[5], valueRead[6], valueRead[7]);

        Serial.println(str);
    }
}

void loop()
{
    // put your main code here, to run repeatedly:
}