/*
 *   Copyright (C) 2016 by Jim McLaughlin KI6ZUM
 *   Copyright (C) 2016 by Andy Uribe CA6JAU
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "Config.h"
#include "Globals.h"

#include "SerialPort.h"

#if defined(STM32F4XX) || defined(STM32F4)

volatile uint32_t intcount1, intcount3;

#define TX_SERIAL_FIFO_SIZE 256U
#define RX_SERIAL_FIFO_SIZE 256U

volatile uint8_t  TXSerialfifo1[TX_SERIAL_FIFO_SIZE];
volatile uint8_t  RXSerialfifo1[RX_SERIAL_FIFO_SIZE];
volatile uint16_t TXSerialfifohead1, TXSerialfifotail1;
volatile uint16_t RXSerialfifohead1, RXSerialfifotail1;

volatile uint8_t  TXSerialfifo3[TX_SERIAL_FIFO_SIZE];
volatile uint8_t  RXSerialfifo3[RX_SERIAL_FIFO_SIZE];
volatile uint16_t TXSerialfifohead3, TXSerialfifotail3;
volatile uint16_t RXSerialfifohead3, RXSerialfifotail3;

extern "C" {
  void USART1_IRQHandler();
  void USART3_IRQHandler();
}

/* ************* USART1 ***************** */

// Init queues
void TXSerialfifoinit1()
{
	TXSerialfifohead1 = 0U;
	TXSerialfifotail1 = 0U;
}

void RXSerialfifoinit1()
{
	RXSerialfifohead1 = 0U;
	RXSerialfifotail1 = 0U;
}

// How full is queue
// TODO decide if how full or how empty is preferred info to return
uint16_t TXSerialfifolevel1()
{
  uint32_t tail = TXSerialfifotail1;
  uint32_t head = TXSerialfifohead1;

  if (tail > head)
    return TX_SERIAL_FIFO_SIZE + head - tail;
  else
    return head - tail;
}

uint16_t RXSerialfifolevel1()
{
  uint32_t tail = RXSerialfifotail1;
  uint32_t head = RXSerialfifohead1;

  if (tail > head)
    return RX_SERIAL_FIFO_SIZE + head - tail;
  else
    return head - tail;
}

// Flushes the transmit shift register
// warning: this call is blocking
void TXSerialFlush1()
{
  // wait until the TXE shows the shift register is empty
  while (USART_GetITStatus(USART1, USART_FLAG_TXE))
    ;
}

uint8_t TXSerialfifoput1(uint8_t next)
{
  if (TXSerialfifolevel1() < TX_SERIAL_FIFO_SIZE) {
    TXSerialfifo1[TXSerialfifohead1] = next;

    TXSerialfifohead1++;
    if (TXSerialfifohead1 >= TX_SERIAL_FIFO_SIZE)
      TXSerialfifohead1 = 0U;

    // make sure transmit interrupts are enabled as long as there is data to send
    USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
    return 1U;
  } else {
    return 0U; // signal an overflow occurred by returning a zero count
  }
}

void USART1_IRQHandler()
{
  uint8_t c;

  if (USART_GetITStatus(USART1, USART_IT_RXNE)) {
    c = (uint8_t) USART_ReceiveData(USART1);

    if (RXSerialfifolevel1() < RX_SERIAL_FIFO_SIZE) {
      RXSerialfifo1[RXSerialfifohead1] = c;

      RXSerialfifohead1++;
      if (RXSerialfifohead1 >= RX_SERIAL_FIFO_SIZE)
        RXSerialfifohead1 = 0U;
    } else {
      // TODO - do something if rx fifo is full?
    }

    USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    intcount1++;
  }

  if (USART_GetITStatus(USART1, USART_IT_TXE)) {
    c = 0U;

    if (TXSerialfifohead1 != TXSerialfifotail1) { // if the fifo is not empty
      c = TXSerialfifo1[TXSerialfifotail1];

      TXSerialfifotail1++;
      if (TXSerialfifotail1 >= TX_SERIAL_FIFO_SIZE)
        TXSerialfifotail1 = 0U;

      USART_SendData(USART1, c);
    } else { // if there's no more data to transmit then turn off TX interrupts
      USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
    }

    USART_ClearITPendingBit(USART1, USART_IT_TXE);
  }
}

void InitUSART1(int speed)
{
	// USART1 - TXD PA9  - RXD PA10 - pins on mmdvm pi board
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	// USART IRQ init
	NVIC_InitStructure.NVIC_IRQChannel    = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
	NVIC_Init(&NVIC_InitStructure);

	// Configure USART as alternate function
	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10; //  Tx | Rx
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// Configure USART baud rate
	USART_StructInit(&USART_InitStructure);
	USART_InitStructure.USART_BaudRate   = speed;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits   = USART_StopBits_1;
	USART_InitStructure.USART_Parity     = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode       = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_Cmd(USART1, ENABLE);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	// initialize the fifos
	TXSerialfifoinit1();
	RXSerialfifoinit1();
}

uint8_t AvailUSART1(void)
{
	if (RXSerialfifolevel1() > 0U)
		return 1U;
	else
		return 0U;
}

uint8_t ReadUSART1(void)
{
	uint8_t data_c = RXSerialfifo1[RXSerialfifotail1];

  RXSerialfifotail1++;
  if (RXSerialfifotail1 >= RX_SERIAL_FIFO_SIZE)
    RXSerialfifotail1 = 0U;

	return data_c;
}

void WriteUSART1(const uint8_t* data, uint16_t length)
{
	for (uint16_t i = 0U; i < length; i++)
		TXSerialfifoput1(data[i]);
		
	USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
}

/* ************* USART3 ***************** */

// Init queues
void TXSerialfifoinit3()
{
	TXSerialfifohead3 = 0U;
	TXSerialfifotail3 = 0U;
}

void RXSerialfifoinit3()
{
	RXSerialfifohead3 = 0U;
	RXSerialfifotail3 = 0U;
}

// How full is queue
// TODO decide if how full or how empty is preferred info to return
uint16_t TXSerialfifolevel3()
{
  uint32_t tail = TXSerialfifotail3;
  uint32_t head = TXSerialfifohead3;

  if (tail > head)
    return TX_SERIAL_FIFO_SIZE + head - tail;
  else
    return head - tail;
}

uint16_t RXSerialfifolevel3()
{
  uint32_t tail = RXSerialfifotail3;
  uint32_t head = RXSerialfifohead3;

  if (tail > head)
    return RX_SERIAL_FIFO_SIZE + head - tail;
  else
    return head - tail;
}

// Flushes the transmit shift register
// warning: this call is blocking
void TXSerialFlush3()
{
  // wait until the TXE shows the shift register is empty
  while (USART_GetITStatus(USART3, USART_FLAG_TXE))
    ;
}

uint8_t TXSerialfifoput3(uint8_t next)
{
  if (TXSerialfifolevel3() < TX_SERIAL_FIFO_SIZE) {
    TXSerialfifo3[TXSerialfifohead3] = next;

    TXSerialfifohead3++;
    if (TXSerialfifohead3 >= TX_SERIAL_FIFO_SIZE)
      TXSerialfifohead3 = 0U;

    // make sure transmit interrupts are enabled as long as there is data to send
    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
    return 1U;
  } else {
    return 0U; // signal an overflow occurred by returning a zero count
  }
}

void USART3_IRQHandler()
{
  uint8_t c;

  if (USART_GetITStatus(USART3, USART_IT_RXNE)) {
    c = (uint8_t) USART_ReceiveData(USART3);

    if (RXSerialfifolevel3() < RX_SERIAL_FIFO_SIZE) {
      RXSerialfifo3[RXSerialfifohead3] = c;

      RXSerialfifohead3++;
      if (RXSerialfifohead3 >= RX_SERIAL_FIFO_SIZE)
        RXSerialfifohead3 = 0U;
    } else {
      // TODO - do something if rx fifo is full?
    }

    USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    intcount3++;
  }

  if (USART_GetITStatus(USART3, USART_IT_TXE)) {
    c = 0U;

    if (TXSerialfifohead3 != TXSerialfifotail3) { // if the fifo is not empty
      c = TXSerialfifo3[TXSerialfifotail3];

      TXSerialfifotail3++;
      if (TXSerialfifotail3 >= TX_SERIAL_FIFO_SIZE)
        TXSerialfifotail3 = 0U;

      USART_SendData(USART3, c);
    } else { // if there's no more data to transmit then turn off TX interrupts
      USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
    }

    USART_ClearITPendingBit(USART3, USART_IT_TXE);
  }
}

void InitUSART3(int speed)
{
	// USART3 - TXD PB10 - RXD PB11 - pins when jumpered from FTDI board to F4Discovery board
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3);

	// USART IRQ init
	NVIC_InitStructure.NVIC_IRQChannel    = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
	NVIC_Init(&NVIC_InitStructure);

	// Configure USART as alternate function
	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_11; //  Tx | Rx
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// Configure USART baud rate
	USART_StructInit(&USART_InitStructure);
	USART_InitStructure.USART_BaudRate   = speed;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits   = USART_StopBits_1;
	USART_InitStructure.USART_Parity     = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode       = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);

	USART_Cmd(USART3, ENABLE);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

	// initialize the fifos
	TXSerialfifoinit3();
	RXSerialfifoinit3();
}

uint8_t AvailUSART3(void)
{
	if (RXSerialfifolevel3() > 0U)
		return 1U;
	else
		return 0U;
}

uint8_t ReadUSART3(void)
{
	uint8_t data_c = RXSerialfifo3[RXSerialfifotail3];

  RXSerialfifotail3++;
  if (RXSerialfifotail3 >= RX_SERIAL_FIFO_SIZE)
    RXSerialfifotail3 = 0U;

	return data_c;
}

void WriteUSART3(const uint8_t* data, uint16_t length)
{
	for (uint16_t i = 0U; i < length; i++)
		TXSerialfifoput3(data[i]);
		
	USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
}

/////////////////////////////////////////////////////////////////

void CSerialPort::beginInt(uint8_t n, int speed)
{
  switch (n) {
    case 1U:
		  InitUSART3(speed);
      break;
    case 3U:
		  InitUSART1(speed);
      break;
    default:
      break;
  }   
}

int CSerialPort::availableInt(uint8_t n)
{ 
    switch (n) {
      case 1U: 
    	  return AvailUSART3();
      case 3U: 
        return AvailUSART1();
      default:
        return false;
  	}
}

uint8_t CSerialPort::readInt(uint8_t n)
{   
	switch (n) {
  	case 1U:
	  	return ReadUSART3();
	  case 3U:
		  return ReadUSART1();
	  default:
      return 0U;
	}
}

void CSerialPort::writeInt(uint8_t n, const uint8_t* data, uint16_t length, bool flush)
{
  switch (n) {
    case 1U:
    	WriteUSART3(data, length);
      if (flush)
        TXSerialFlush3();
      break;
    case 3U:
      WriteUSART1(data, length);
      if (flush)
        TXSerialFlush1();
       break;
    default:
       break;
  }
}

#endif

