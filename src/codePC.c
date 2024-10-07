#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <windows.h>

#define BUFFER_SIZE 256
#define CMD_READ 0x0
#define CMD_WRITE 0x1

#define RET_FAIL -1
#define RET_NONE 0
#define RET_SUCCESS 1

#define OFFSET_HEADER_1 0
#define OFFSET_HEADER_2 1
#define OFFSET_CMD 2
#define OFFSET_DATALENGTH 3
#define OFFSET_DATA 4
#define OFFSET_TRAILER_1 5
#define OFFSET_TRAILER_2 6

#define HEADER_1 0xab
#define HEADER_2 0xcd
#define TRAILER_1 0xe1
#define TRAILER_2 0xe2

#define LIGHT 1
#define L_A   0
#define L_B   1


#define OLED  2
#define OL_1  0

#define TOUCH 3
#define TOUCH_1 0
#define DO    1

#define TEMPERATURE 4
#define TEMP_1 1

#define HUMIDITY 5
#define HUMI_1 1

#define LCD 6
#define LCD_1 1

#define READ  1
#define START 1
#define PROGRAM 2
#define OFF   0
#define ON    1
#define STATE 2




int flush_buffer(HANDLE port);
HANDLE open_port(const char* device, unsigned long baud_rate, unsigned char bit_size, unsigned char parity);
int compose_packet(unsigned char rw, unsigned char address, unsigned int data_length, unsigned char* data, unsigned char* packet);
int uart_transmit(HANDLE port, unsigned char* writebuf, unsigned int inputLength);
int uart_receive(HANDLE port, unsigned char* rx_buf, unsigned char* data_length, unsigned char* cmd);




HANDLE open_port(const char* device, unsigned long baud_rate, unsigned char bit_size, unsigned char parity) {
    HANDLE port = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (port == INVALID_HANDLE_VALUE)
    {
        return INVALID_HANDLE_VALUE;
    }

    // Flush away any bytes previously read or written.
    BOOL success = FlushFileBuffers(port);
    if (!success)
    {
        printf("Failed to flush serial port");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    // Configure read and write operations to time out after 100 ms.
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutConstant = 3000;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 3000;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    success = SetCommTimeouts(port, &timeouts);
    if (!success)
    {
        printf("Failed to set serial timeouts");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    // Set the baud rate and other options.
    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    dcb.BaudRate = baud_rate;
    dcb.ByteSize = bit_size;
    dcb.Parity = parity;
    dcb.StopBits = ONESTOPBIT;
    success = SetCommState(port, &dcb);
    if (!success)
    {
        printf("Failed to set serial settings");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    // display information
    printf("----------------------------------\n");
    printf("baud rate = %d\n", dcb.BaudRate);
    printf("Parity = %d\n", dcb.Parity);
    printf("Byte Size = %d\n", dcb.ByteSize);
    printf("Stop Bit = %d\n", dcb.StopBits);
    printf("----------------------------------\n");
    return port;
}

int uart_transmit(HANDLE port, unsigned char* writebuf, unsigned int inputLength) {
    int writtenbytes = 0; // Number of bytes successfully written by WriteFile().    
    if (WriteFile(port, writebuf, inputLength, (LPDWORD)&writtenbytes, NULL)) {
        if (writtenbytes == 0) {
            printf("\nWriteFile() timed out\n");
            return RET_FAIL;
        }
    }
    else {
        printf("\nWriteFile() failed\n");
        return RET_FAIL;
    }
    printf("\nTransmited Packet: ");
    for (int i = 0; i < inputLength; i++) {
        printf("\t%x", writebuf[i]);
    }
    return RET_SUCCESS;
}

int uart_receive(HANDLE port, unsigned char* rx_buf, unsigned char* data_length, unsigned char* cmd) {
    int i = 0;
    int state = OFFSET_HEADER_1;
    unsigned char readbuf;
    DWORD nbbytes;

    do {
        if (ReadFile(port, &readbuf, 1, &nbbytes, NULL)) {
            if (nbbytes == 0) {
                printf("\nReadFile() timed out\n");
                return RET_FAIL;
            }
        }
        else {
            printf("\nReadFile() failed\n");
            return RET_FAIL;
        }
        switch (state) {
        case OFFSET_HEADER_1:
            if (readbuf == HEADER_1)
                state = OFFSET_HEADER_2;
            else {
                printf("\n[ERROR]: Wrong header 1");
                return RET_FAIL;
            }
            break;
        case OFFSET_HEADER_2:
            if (readbuf == HEADER_2)
                state = OFFSET_CMD;
            else {
                printf("\n[ERROR]: Wrong header 2");
                return RET_FAIL;
            }
            break;
        case OFFSET_CMD:
            *cmd = readbuf;
            state = OFFSET_DATALENGTH;
            break;
        case OFFSET_DATALENGTH:
            *data_length = readbuf;
            state = OFFSET_DATA;
            break;
        case OFFSET_DATA:
            if (i < *data_length - 1)
                rx_buf[i++] = readbuf;
            else {
                rx_buf[i++] = readbuf;
                state = OFFSET_TRAILER_1;
            }
            break;
        case OFFSET_TRAILER_1:
            if (readbuf == TRAILER_1)
                state = OFFSET_TRAILER_2;
            else {
                printf("\n[ERROR]: Wrong trailer 1");
                return RET_FAIL;
            }
            break;
        case OFFSET_TRAILER_2:
            if (readbuf == TRAILER_2)
                return RET_SUCCESS;
            else {
                printf("\n[ERROR]: Wrong trailer 2");
                return RET_FAIL;
            }
            break;
        default:
            break;
        }
    } while (1);
    return RET_FAIL;
}


int compose_packet(unsigned char address, unsigned int data_length, unsigned char* data, unsigned char* packet) {
    unsigned char frame_length = 6;
    int i = 4;

    packet[0] = HEADER_1;
    packet[1] = HEADER_2;
    packet[2] = address;
    packet[3] = data_length;
    for (i; i < data_length + 4; ++i)
        packet[i] = data[i - 4];
    packet[i] = TRAILER_1;
    packet[i + 1] = TRAILER_2;

    frame_length += data_length;
    return frame_length;
}


int main()
{
    // configuration parameters
    const char* device = "\\\\.\\COM3";
    unsigned long baud_rate = 9600;
    unsigned char bit_size = 8;
    unsigned char parity = 0;
    // code
    int loop = 0;
    unsigned char TX_buf[BUFFER_SIZE], RX_buf[BUFFER_SIZE], TX_packet[BUFFER_SIZE], RX_packet[BUFFER_SIZE];
    int status = RET_FAIL;
    char sel = 0, c;
    char option = 0;
    char option1 = 0;
    int start = START;
    unsigned char TX_buf_len = 0, RX_buf_len = 0;
    unsigned int TX_packet_len = 0;
    unsigned char cmd;
    unsigned int address;
    unsigned int sellight;
    unsigned int listate;
    unsigned int loled;
    unsigned int ooled;
    unsigned int sellcd;
    unsigned int lcdstate;

    // open serial port
    HANDLE port = open_port(device, baud_rate, bit_size, parity);
    if (port == INVALID_HANDLE_VALUE) {
        return -1;
    }
    do {
        switch (start) {
        case START:
            do {
                address = 3;
                TX_buf[0] = TOUCH_1;
                TX_buf[1] = DO;
                TX_buf_len = 2;
                TX_packet_len = compose_packet(address, TX_buf_len, TX_buf, TX_packet);
                status = uart_transmit(port, TX_packet, TX_packet_len);
                printf("\nWaiting for finger...");
                status = uart_receive(port, RX_buf, &RX_buf_len, &cmd);
                if (status = RET_SUCCESS) {
                    int lis = RX_buf[1];
                    if (lis == START) {
                        start = PROGRAM;
                        break;
                    }
                }
            } while (1);
        case PROGRAM:
            do {
                printf("\n(__________Selec Device__________)\n--> Press '1': LIGHT\n--> Press '2': TEMPERATURE\n--> Press '3': HUMIDITY\n--> Press '4': LCD\n--> Press '5': EXIT\n");
                printf("\nSelect device:");
                while ((sel = getchar()) == '\n');
                switch (sel) {
                case '1':
                    do {
                        address = LIGHT;
                        printf("\nPress '1': ON OR OFF\nPress '2':LIGHT STATUS\nPress '3':exit\n");
                        printf("\nSelect option:");
                        while ((option = getchar()) == '\n');
                        switch (option) {
                        case '1':
                            do {
                                printf("\nEnter light id(0:A,1:B): ");
                                scanf("%d", &sellight);
                            } while (sellight > 3);
                            do {
                                printf("\nON or OFF(1,0): ");
                                scanf("%d", &listate);
                            } while (listate > 3);
                            if (sellight == 0) {
                                TX_buf[0] = L_A;
                                printf("\nSelect Light A");
                            }
                            else if (sellight == 1) {
                                TX_buf[0] = L_B;
                                printf("\nSelect Light B");
                            }

                            if (listate == 0) {
                                TX_buf[1] = OFF;
                                printf("\nTurn OFF");
                            }
                            else if (listate == 1) {
                                TX_buf[1] = ON;
                                printf("\nTurn ON");
                            }
                            TX_buf_len = 2;
                            TX_packet_len = compose_packet(address, TX_buf_len, TX_buf, TX_packet);
                            if (status = RET_SUCCESS) {
                                status = uart_transmit(port, TX_packet, TX_packet_len);
                            }
                            break;
                        case '2':
                            do {
                                printf("\nEnter light id(0:A,1:B): ");
                                scanf("%d", &sellight);
                            } while (sellight > 3);
                            do {
                                printf("\nREAD LIGHT STATE(2): ");
                                scanf("%d", &listate);
                            } while (listate > 3);
                            if (sellight == 0) {
                                TX_buf[0] = L_A;
                                printf("\nSelect Light A");
                            }
                            else if (sellight == 1) {
                                TX_buf[0] = L_B;
                                printf("\nSelect Light B");
                            }
                            if (listate == 2) {
                                TX_buf[1] = STATE;
                                printf("\nREAD LIGHT STATE");

                            }
                            TX_buf_len = 2;
                            TX_packet_len = compose_packet(address, TX_buf_len, TX_buf, TX_packet);
                            if (status = RET_SUCCESS) {
                                status = uart_transmit(port, TX_packet, TX_packet_len);
                            }
                            if (status == RET_FAIL)
                                loop = 1;
                            //wait response
                            status = uart_receive(port, RX_buf, &RX_buf_len, &cmd);
                            if (status == RET_FAIL) {
                                loop = 1;
                            }
                            else {
                                printf("\nReceiving data successed! \nReceived data at address %x:\n", cmd);
                                for (int g = 0; g < 2; g++) {
                                    printf("\t%x", RX_buf[g]);
                                }
                            }
                            break;
                        case '3':
                            loop = 2;
                            break;
                        }
                    } while (loop != 2);
                    break;
                case '2':
                    do {
                        address = TEMPERATURE;
                        printf("\nPress '1': READ TEMPERATURE\nPress '2':EXIT\n");
                        printf("\nSelect option:");
                        while ((option1 = getchar()) == '\n');
                        switch (option1) {
                        case '1':
                            do {
                                printf("\nEnter temperature id(1): ");
                                scanf("%d", &loled);
                            } while (loled > 3);
                            do {
                                printf("\nread temperature(1): ");
                                scanf("%d", &ooled);
                            } while (ooled > 3);
                            if (loled == 1) {
                                TX_buf[0] = TEMP_1;
                                printf("\nSelect temperature success");
                            }
                            if (ooled == 1) {
                                TX_buf[1] = READ;
                                printf("\nREAD TEMPERATURE");
                            }
                            TX_buf_len = 2;
                            TX_packet_len = compose_packet(address, TX_buf_len, TX_buf, TX_packet);
                            status = uart_transmit(port, TX_packet, TX_packet_len);
                            status = uart_receive(port, RX_buf, &RX_buf_len, &cmd);
                            if (status == RET_FAIL) {
                                loop = 1;
                            }
                            else {
                                printf("\nReceiving data successed! \nReceived data at address %x:\n", cmd);
                                for (int g = 0; g < 2; g++) {
                                    printf("\t%x", RX_buf[g]);
                                }
                            }
                            break;
                        case '2':
                            loop = 3;
                            break;
                        }
                    } while (loop != 3);
                    break;
                case '3':
                    do {
                        address = HUMIDITY;
                        printf("\nPress '1': READ HUMIDITY\nPress '2':EXIT\n");
                        printf("\nSelect option:");
                        while ((option1 = getchar()) == '\n');
                        switch (option1) {
                        case '1':
                            do {
                                printf("\nEnter HUMIDITY(1): ");
                                scanf("%d", &loled);
                            } while (loled > 3);
                            do {
                                printf("\nread humidity(1): ");
                                scanf("%d", &ooled);
                            } while (ooled > 3);
                            if (loled == 1) {
                                TX_buf[0] = HUMI_1;
                                printf("\nSelect humidity success");
                            }
                            if (ooled == 1) {
                                TX_buf[1] = READ;
                                printf("\nREAD humidity");
                            }
                            TX_buf_len = 2;
                            TX_packet_len = compose_packet(address, TX_buf_len, TX_buf, TX_packet);
                            status = uart_transmit(port, TX_packet, TX_packet_len);
                            status = uart_receive(port, RX_buf, &RX_buf_len, &cmd);
                            if (status == RET_FAIL) {
                                loop = 1;
                            }
                            else {
                                printf("\nReceiving data successed! \nReceived data at address %x:\n", cmd);
                                for (int g = 0; g < 2; g++) {
                                    printf("\t%x", RX_buf[g]);
                                }
                            }
                            break;
                        case '2':
                            loop = 3;
                            break;
                        }
                    } while (loop != 3);
                    break;
                case '4':
                    do {
                        address = LCD;
                        printf("\nPress '1': ON OR OFF\nPress '2':exit\n");
                        printf("\nSelect option:");
                        while ((option = getchar()) == '\n');
                        switch (option) {
                        case '1':
                            do {
                                printf("\nEnter lcd id(1): ");
                                scanf("%d", &sellcd);
                            } while (sellcd > 3);
                            do {
                                printf("\nON or OFF(1,0): ");
                                scanf("%d", &lcdstate);
                            } while (lcdstate > 3);
                            if (sellcd == 1) {
                                TX_buf[0] = LCD_1;
                                printf("\nSelect LCD SUCCESS");
                            }

                            if (lcdstate == 0) {
                                TX_buf[1] = OFF;
                                printf("\nCLEAR LCD");
                            }
                            else if (lcdstate == 1) {
                                TX_buf[1] = ON;
                                printf("\nDISPLAY LCD");
                            }
                            TX_buf_len = 2;
                            TX_packet_len = compose_packet(address, TX_buf_len, TX_buf, TX_packet);
                            if (status = RET_SUCCESS) {
                                status = uart_transmit(port, TX_packet, TX_packet_len);
                            }
                            break;
                        case '2':
                            loop = 1;
                            break;
                        }
                    } while (loop != 1);
                    break;
                case '5':
                    start = START;
                    loop = 1;
                    break;
                }

            } while (loop != 1);
            break;
        }
    } while (1);
    // Close the serial port.
    if (!CloseHandle(port))
    {
        printf("CloseHandle() failed\n");
        return RET_FAIL;
    }
    return RET_NONE;

}