/*
	Generalized SPI driver for the Freescale MKW01.

	Programmed by William Harrington, Theo Hill
*/
#include "spi.h"
#include <assert.h>

static const struct pin_assign PCS0 [] = {
	{.module=&SPI0, .pin={&PORTE, 16}, .alt=2},
	{.module=&SPI0, .pin={&PORTD, 0},  .alt=2},
	{.module=&SPI1, .pin={&PORTD, 4},  .alt=2},
	{}
};

static const struct pin_assign SCK [] = {
	{.module=&SPI0, .pin={&PORTE, 17}, .alt=2},
	{.module=&SPI0, .pin={&PORTC, 5},  .alt=2},
	{.module=&SPI1, .pin={&PORTD, 5},  .alt=2},
	{.module=&SPI1, .pin={&PORTE, 2},  .alt=2},
	{}
};

static const struct pin_assign MISO [] = {
	{.module=&SPI0, .pin={&PORTE, 18}, .alt=5},
	{.module=&SPI0, .pin={&PORTE, 19}, .alt=2},
	{.module=&SPI0, .pin={&PORTC, 7},  .alt=2},
	{.module=&SPI0, .pin={&PORTC, 6},  .alt=5},
	{.module=&SPI1, .pin={&PORTB, 17}, .alt=2},
	{.module=&SPI1, .pin={&PORTE, 3},  .alt=2},
	{.module=&SPI1, .pin={&PORTE, 1},  .alt=5},
	{.module=&SPI1, .pin={&PORTD, 7},  .alt=2},
	{.module=&SPI1, .pin={&PORTD, 6},  .alt=5},
	{}
};

static const struct pin_assign MOSI [] = {
	{.module=&SPI0, .pin={&PORTE, 18}, .alt=2},
	{.module=&SPI0, .pin={&PORTE, 19}, .alt=5},
	{.module=&SPI0, .pin={&PORTC, 7},  .alt=5},
	{.module=&SPI0, .pin={&PORTC, 6},  .alt=2},
	{.module=&SPI1, .pin={&PORTB, 17}, .alt=5},
	{.module=&SPI1, .pin={&PORTE, 3},  .alt=5},
	{.module=&SPI1, .pin={&PORTE, 1},  .alt=2},
	{.module=&SPI1, .pin={&PORTD, 7},  .alt=5},
	{.module=&SPI1, .pin={&PORTD, 6},  .alt=2},
	{}
};

#define ENABLE_IN_MASTER (5 << 4)
#define SS_OE (1 << 1)
#define MASTER_MODE_FAULT_EN (1 << 4)


void initialize_trans_spi(volatile struct spi * SPI){
	/* configuration for SPI0, see Chapter 8.1 */
	struct spi_config config = {
		/* Serial Clock */
		.SCK = {.port=&PORTC, .pin=5,},

		/* Slave Select */
		.SS = {.port=&PORTD, .pin=0,},

		/* Master out slave in */
		.MOSI = {.port=&PORTC, .pin=6,},

		/* Master in slave out */
		.MISO = {.port=&PORTC, .pin=7,},

		/* Polarity */
		.CPOL = 0,

		/* Phase */
		.CPHA = 0,

		.SPIMODE = 0,
	};

	/* Select desired pin functionality */
	set_pin_alt(SCK,  SPI, &config.SCK);
	/* We need to control the slave select manually with PTD0 set as GPIO to 
	perform SPI operations longer than one byte*/
	PORTD.PCR[0] |= 0x100; 	//Enable PTD0 as a GPIO
	GPIOD.PSOR = 0x1;		//Set the output signal to high
	GPIOD.PDDR |= 0x1;		//Set PTD0 data direction to output
	set_pin_alt(MOSI, SPI, &config.MOSI);
	set_pin_alt(MISO, SPI, &config.MISO);

	SPI->C1 = ENABLE_IN_MASTER | (config.CPOL << 3) | (config.CPHA << 2);
	SPI->C2 = 0x0;

	/* 1MHz baud rate */
	SPI->BR = 0x22;
}

void spi_read_16(volatile struct spi * SPI, size_t len, uint16_t * buffer) {
	/* prevent calls when in 8-bit SPI mode */
 	assert(SPI->C2 & (1 << 6));

	uint16_t dummy[len];
	spi_transaction_16(SPI, len, dummy, buffer);
}

void spi_read_8(volatile struct spi * SPI, size_t len, uint8_t * buffer) {
	/* prevent calls when in 16-bit SPI mode */
	assert(!(SPI->C2 & (1 << 6)));

	uint8_t dummy[len];
	spi_transaction_8(SPI, len, dummy, buffer);
}

void spi_write_16(volatile struct spi * SPI, size_t len, uint16_t * buffer) {
	/* prevent calls when in 8-bit SPI mode */
	assert(SPI->C2 & (1 << 6));

	uint16_t dummy[len];
	spi_transaction_16(SPI, len, buffer, dummy);
}

void spi_write_8(volatile struct spi * SPI, size_t len, uint8_t * buffer) {
	/* prevent calls when in 16-bit SPI mode */
	assert(!(SPI->C2 & (1 << 6)));

	uint8_t dummy[len];
	spi_transaction_8(SPI, len, buffer, dummy);
}

void spi_transaction_16(volatile struct spi * SPI, size_t len, uint16_t * send, uint16_t * recv){
 	if(!len) return;

	/* iterate through number of bytes for transaction */
	for(unsigned int i = 0; i < len; ++i){

		/* poll the transmit buffer empty flag */
		while(!(SPI->S & (1 << 5)));

		/* extract the MSB and LSB */
		uint8_t MSB = (send[i] >> 8) & 0xFF;
		uint8_t LSB = (send[i]) & 0xFF;

		/* send em by writing to the data registers */
		SPI->DH = MSB;
		SPI->DL = LSB;

		/* poll the read buffer full flag */
		while(!(SPI->S & (1 << 7)));

		/* grab data, concatenate into buffer */
		recv[i] = (SPI->DH << 8) | SPI->DL;
	}
}

void spi_transaction_8(volatile struct spi * SPI, size_t len, uint8_t * send, uint8_t * recv){
 	if(!len) return;

	/* iterate through number of bytes for transaction */
 	GPIOD.PCOR = 0x1;	//Assert low to start the transaction
	for(unsigned int i = 0; i < len; ++i){

		/* Poll the transmit buffer empty flag */
		while(!(SPI->S & (1 << 5)));

		/* Send byte by writing to the data registers */
		SPI->DL = send[i];

		/* Make sure the byte was sent*/
		while(!(SPI->S & (1 << 5)));

		/* poll the read buffer full flag to make sure the data was sent before reading*/
		while(!(SPI->S & (1 << 7)));

		/* grab data into buffer.... The first byte will be garbage */
		recv[i] = SPI->DL;
	}
	
	GPIOD.PSOR = 0x1;	//Assert high to start the transaction
}