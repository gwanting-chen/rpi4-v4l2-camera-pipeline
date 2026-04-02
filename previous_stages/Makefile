#--------C Langueage-------------
CC = gcc
CFLAGS = -Wall -g

all: $(TARGET1) $(TARGET2) $(TARGET3)

$(TARGET1): main.c
	$(CC) $(CFLAGS) -o $(TARGET1) main.c

$(TARGET2): main_isp.c
	$(CC) $(CFLAGS) -o $(TARGET2) main_isp.c

$(TARGET3): main_isp_dma.c
	$(CC) $(CFLAGS) -o $(TARGET3) main_isp_dma.c

clean:
	rm -f $(TARGET1) $(TARGET2) $(TARGET3)
