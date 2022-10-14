/********************************************************************************************************
 * @file	uart.c
 *
 * @brief	This is the source file for B80
 *
 * @author	Driver Group
 * @date	2021
 *
 * @par     Copyright (c) 2021, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *
 *******************************************************************************************************/
#include "uart.h"
#include "gpio.h"
#include "compiler.h"

/**
 * @brief     This function is used to look for the prime.if the prime is finded,it will
 * 			  return 1, or return 0.
 * @param[in] the value to judge
 * @return    none
 */
static unsigned char IsPrime(unsigned int n)
{
	unsigned int i = 5;
	if(n <= 3){
		return 1; //although n is prime, but the bwpc must be larger than 2.
	}
	else if((n %2 == 0) || (n % 3 == 0)){
		return 0;
	}
	else{
		for(i=5;i*i<n;i+=6){
			if((n % i == 0)||(n %(i+2))==0){
				return 0;
			}
		}
		return 1;
	}
}

/**
 * @brief  		This funciton serves to calculate the best bwpc(bit width) .i.e reg0x96
 * @param[in] 	baut_rate:The value of the baut rate to set.
 * @param[in] 	tmp_sysclk:The system clock
 *    			algorithm: BaudRate*(div+1)*(bwpc+1)=system clock
 *    			simplify the expression: div*bwpc =  constant(z)
 *    			bwpc range from 3 to 15.so loop and
 *    			get the minimum one decimal point
 * @return 		the position of getting the minimum value
 */
static unsigned char g_bwpc = 0;
static unsigned int  g_uart_div = 0;
static void GetBetterBwpc(unsigned int baut_rate,unsigned int  tmp_sysclk )
{
	unsigned char i = 0, j= 0;
	unsigned int primeInt = 0;
	unsigned char primeDec = 0;
	unsigned int D_intdec[13],D_int[13];
	unsigned char D_dec[13];

	primeInt = tmp_sysclk/baut_rate;
	primeDec = 10*tmp_sysclk/baut_rate - 10*primeInt;
	/************************************************************
	 * calculate the primeInt and check whether primeInt is prime.
	 */
	if(IsPrime(primeInt)){ // primeInt is prime
		primeInt += 1;  //+1 must be not prime. and primeInt must be larger than 2.
	}
	else{
		if(primeDec > 5){ // >5
			primeInt += 1;
			if(IsPrime(primeInt)){
				primeInt -= 1;
			}
		}
	}
	/*******************************************
	 * get the best division value and bit width
	 */
	for(i=3;i<=15;i++){
		D_intdec[i-3] = (10*primeInt)/(i+1);////get the LSB
		D_dec[i-3] = D_intdec[i-3] - 10*(D_intdec[i-3]/10);///get the decimal section
		D_int[i-3] = D_intdec[i-3]/10;///get the integer section
	}

	//find the max and min one decimation point
	unsigned char position_min = 0,position_max = 0;
	unsigned int min = 0xffffffff,max = 0x00;
	for(j=0;j<13;j++){
		if((D_dec[j] <= min)&&(D_int[j] != 0x01)){
			min = D_dec[j];
			position_min = j;
		}
		if(D_dec[j]>=max){
			max = D_dec[j];
			position_max = j;
		}
	}

	if((D_dec[position_min]<5) && (D_dec[position_max]>=5)){
		if(D_dec[position_min]<(10-D_dec[position_max])){
			g_bwpc = position_min + 3;
			g_uart_div = D_int[position_min]-1;
		}
		else{
			g_bwpc = position_max + 3;
			g_uart_div = D_int[position_max];
		}
	}
	else if((D_dec[position_min]<5) && (D_dec[position_max]<5)){
		g_bwpc = position_min + 3;
		g_uart_div = D_int[position_min] - 1;
	}
	else{
		g_bwpc = position_max + 3;
		g_uart_div = D_int[position_max];
	}
}

/**
 * @brief      	This function initializes the UART module.
 * @param[in]  	g_uart_div  -  uart clock divider
 * @param[in]  	g_bwpc      -  bitwidth, should be set to larger than
 *
 *  			sys_clk      baud rate   g_uart_div         g_bwpc
 *
 *	  	  	  	16Mhz        9600          118   			 13
 *           	 	 	 	 19200         118     			 6
 *          	 	 	 	 115200         9       		 13
 *
 *	  	  	  	24Mhz        9600          249       		 9
 *           	 	 	 	 19200		   124               9
 *           	 	 	 	 115200         12    			 15
 *
 *	  	  	  	32Mhz        9600          235       		 13
 *           	 	 	 	 19200		   235               6
 *           	 	 	 	 115200         17    			 13
 *
 *	  	  	  	48Mhz        9600          499       		 9
 *           	 	 	 	 19200		   249               9
 *           	 	 	 	 115200         25    			 15
 *
 * @param[in]  Parity      - selected parity type for UART interface
 * @param[in]  StopBit     - selected length of stop bit for UART interface
 * @return     none
 */
void uart_init(unsigned short g_uart_div, unsigned char g_bwpc, UART_ParityTypeDef Parity, UART_StopBitTypeDef StopBit)
{
    //GetBetterBwpc(BaudRate); //get the best bwpc and uart_div
    reg_uart_ctrl0 = ((reg_uart_ctrl0 & (~FLD_UART_BPWC))| g_bwpc);//set bwpc
    reg_uart_clk_div = (g_uart_div | FLD_UART_CLK_DIV_EN); //set div_clock
    reg_uart_rx_timeout0 = (g_bwpc+1) * 12; //one byte includes 12 bits at most
    reg_uart_rx_timeout1 =((reg_uart_rx_timeout1 & (~FLD_UART_TIMEOUT_MUL))| UART_BW_MUL2);//if over 2*(tmp_bwpc+1),one transaction end.

    //parity config
    if (Parity) {
    	reg_uart_ctrl1  |= FLD_UART_CTRL1_PARITY_EN; //enable parity function
        if (PARITY_EVEN == Parity) {
        	reg_uart_ctrl1  &= (~FLD_UART_CTRL1_PARITY_POLARITY); //enable even parity
        }
        else if (PARITY_ODD == Parity) {
        	reg_uart_ctrl1  |= FLD_UART_CTRL1_PARITY_POLARITY; //enable odd parity
        }
    }
    else {
    	reg_uart_ctrl1  &= (~FLD_UART_CTRL1_PARITY_EN); //disable parity function
    }

    //stop bit config
    reg_uart_ctrl1 = ((reg_uart_ctrl1 & (~FLD_UART_CTRL1_STOP_BIT)) | StopBit);
}
/**
 * @brief      This function initializes the UART module.
 * @param[in]  Baudrate  	- uart baud rate
 * @param[in]  System_clock - clock of system
 * @param[in]  Parity      	- selected parity type for UART interface
 * @param[in]  StopBit     	- selected length of stop bit for UART interface
 * @return     none
 */
void uart_init_baudrate(unsigned int Baudrate,unsigned int System_clock , UART_ParityTypeDef Parity, UART_StopBitTypeDef StopBit)
{
	GetBetterBwpc(Baudrate,System_clock); //get the best bwpc and uart_div
	reg_uart_ctrl0 = ((reg_uart_ctrl0 & (~FLD_UART_BPWC))| g_bwpc); //set bwpc
	reg_uart_clk_div = (g_uart_div | FLD_UART_CLK_DIV_EN); //set div_clock
	reg_uart_rx_timeout0 = (g_bwpc+1) * 12; //one byte includes 12 bits at most

	reg_uart_rx_timeout1 = ((reg_uart_rx_timeout1 & (~UART_BW_MUL2)) | UART_BW_MUL2);//if over 2*(tmp_bwpc+1),one transaction end.
	//parity config
	if (Parity) {
		reg_uart_ctrl1  |= FLD_UART_CTRL1_PARITY_EN; //enable parity function
		if (PARITY_EVEN == Parity) {
			reg_uart_ctrl1  &= (~FLD_UART_CTRL1_PARITY_POLARITY); //enable even parity
		}
		else if (PARITY_ODD == Parity) {
			reg_uart_ctrl1  |= FLD_UART_CTRL1_PARITY_POLARITY; //enable odd parity
		}
	}
	else {
		reg_uart_ctrl1  &= (~FLD_UART_CTRL1_PARITY_EN); //disable parity function
	}

	//stop bit config
	reg_uart_ctrl1 = ((reg_uart_ctrl1 & (~FLD_UART_CTRL1_STOP_BIT)) | StopBit);
}

/**
 * @brief     enable uart DMA mode, in dma mode, rx_dma_en and tx_dma_en must set 1
 *                                  in no dma mode, rx_dma_en and tx_dma_en must set 0
 * @param[in] rx_dma_en
 * @param[in] tx_dma_en
 * @return    none
 */
void uart_dma_enable(unsigned char rx_dma_en, unsigned char tx_dma_en)
{
	if(rx_dma_en){
		/*This function enables auto clr function switch,if be enabled,when there is no data in fifo,
		 the fifo will be automatically cleared and restored to the default state after rx_done.
         in dma mode, need to enable it.*/
		reg_uart_status1 |= FLD_UART_AUTO_RXCLR_EN;
		/*This function enables rx_done function switch,
		 in dma mode,need disable rx_done function.*/
		reg_uart_status1&=~FLD_UART_NDMA_RXDONE_EN;
		/*enable DMA function of rx */
		reg_uart_ctrl0 |= FLD_UART_RX_DMA_EN ;
	}else{
		/*This function enables auto clr function switch,if be enabled,when there is no data in fifo,
		 the fifo will be automatically cleared and restored to the default state after rx_done.
		 * in no_dma, need to disable it, manual clearing is generally used*/
		reg_uart_status1 &= ~FLD_UART_AUTO_RXCLR_EN;
		/*This function enables rx_done function switch,
		 in no_dma mode,need enable rx_done function ,*/
		reg_uart_status1 |= FLD_UART_NDMA_RXDONE_EN;
		/*disable DMA function of rx */
		reg_uart_ctrl0 &= (~FLD_UART_RX_DMA_EN );
	}

	if(tx_dma_en){
		/*enable DMA function of tx */
		reg_uart_ctrl0  |= FLD_UART_TX_DMA_EN;

	}else{
		/*diable DMA function of tx */
		reg_uart_ctrl0	&= (~FLD_UART_TX_DMA_EN);
	}

}

/**
 * @brief     config the irq of uart tx and rx
 * @param[in] rx_irq_en - 1:enable rx irq. 0:disable rx irq
 * @param[in] tx_irq_en - 1:enable tx irq. 0:disable tx irq
 * @return    none
 */
void uart_irq_enable(unsigned char rx_irq_en, unsigned char tx_irq_en)
{
	if(rx_irq_en){
		reg_uart_ctrl0 |= FLD_UART_RX_IRQ_EN ;
	}else{
		reg_uart_ctrl0 &= (~FLD_UART_RX_IRQ_EN );
	}

	if(tx_irq_en){
		reg_uart_ctrl0  |= FLD_UART_TX_IRQ_EN;
	}else{
		reg_uart_ctrl0	&= (~FLD_UART_TX_IRQ_EN);
	}

	if(tx_irq_en||rx_irq_en)
	{
		reg_irq_mask |= FLD_IRQ_UART_EN;
	}
	else
	{
		reg_irq_mask &= ~FLD_IRQ_UART_EN;
	}
}

/**
 * @brief     config the number level setting the irq bit of status register 0x9d
 *            ie 0x9d[3].
 *            If the cnt register value(0x9c[0,3]) larger or equal than the value of 0x99[0,3]
 *            or the cnt register value(0x9c[4,7]) less or equal than the value of 0x99[4,7],
 *            it will set the irq bit of status register 0x9d, ie 0x9d[3]
 * @param[in] rx_level - receive level value. ie 0x99[0,3]
 * @param[in] tx_level - transmit level value.ie 0x99[4,7]
 * @return    none
 */


void uart_ndma_irq_triglevel(unsigned char rx_level, unsigned char tx_level)
{
	reg_uart_ctrl3 = rx_level | (tx_level<<4);
}

/**
 * @brief     get the status of uart irq.
 * @param[in] none
 * @return    0: not uart irq ;
 *            not 0: indicate tx or rx irq
 */

unsigned char uart_ndmairq_get(void)
{
	return  (reg_uart_status0&FLD_UART_IRQ_FLAG );
}
unsigned char uart_TxIndex = 0;

/**
 * @brief     uart send data function with not DMA method.
 *            variable uart_TxIndex,it must cycle the four registers 0x90 0x91 0x92 0x93 for the design of SOC.
 *            so we need variable to remember the index.
 * @param[in] uartData - the data to be send.
 * @return    none
 */
void uart_ndma_send_byte(unsigned char uartData)
{

    while((reg_uart_buf_cnt>>4)>7);

	reg_uart_data_buf(uart_TxIndex) = uartData;

	uart_TxIndex++;
	uart_TxIndex &= 0x03;// cycle the four register 0x90 0x91 0x92 0x93.
}
unsigned char uart_RxIndex = 0;
/**
 * @brief     uart read data function with not DMA method.
 *            variable uart_RxIndex,it must cycle the four registers 0x90 0x91 0x92 0x93 for the design of SOC.
 *            so we need variable to remember the index.
 * @param[in] none.
 * @return    data received.
 */
volatile unsigned char uart_ndma_read_byte(void)
{
	unsigned char rx_data = reg_uart_data_buf(uart_RxIndex);
	uart_RxIndex++;
	uart_RxIndex &= 0x03;// cycle the four register 0x90 0x91 0x92 0x93.
	return rx_data;
}

/**
 * @brief     uart send data function, this  function tell the DMA to get data from the RAM and start the DMA transmission
 * @param[in] Addr - pointer to the buffer containing data need to send
 * @return    none
 * @note      If you want to use uart DMA mode to send data, it is recommended to use this function.
 *            This function just triggers the sending action, you can use interrupt or polling with the FLD_UART_TX_DONE flag to judge whether the sending is complete. 
 *            After the current packet has been sent, this FLD_UART_TX_DONE will be set to 1, and FLD_UART_TX_DONE interrupt can be generated.
 *			  If you use interrupt mode, you need to call uart_clr_tx_done() in the interrupt processing function, uart_clr_tx_done() will set FLD_UART_TX_DONE to 0.
 *            DMA can only send 2047-bytes one time at most.
 */
void uart_send_dma(unsigned char* Addr)
{
	/*
	  In order to prevent the time between the last piece of data and the next piece of data is less than the set timeout time,
	  causing the receiver to treat the next piece of data as the last piece of data.
	*/
	uart_clr_tx_done();
	reg_dma1_addr = (unsigned short)((unsigned int)Addr); //packet data, start address is sendBuff+1
    reg_dma1_size = 0xff;
    reg_dma_chn_en |= FLD_DMA_CHN_UART_TX;
    reg_dma_tx_rdy0	 |= FLD_DMA_CHN_UART_TX;
}

/**
 * @brief     This function is saved for compatibility with other SDK and isn't be used in driver sdk.Because it has the following problems:
 *			  You can't use this function if you open FLD_UART_TX_DONE irq,This function can only be used in polling method.
 *	          There may be a risk of data loss under certain usage conditions.
 *			  It will first check whether the last packet has been sent, if it is checked that the last packet has been sent, 
 *			  it will trigger the sending, otherwise it will not send.
 *		
 * @param[in] Addr - pointer to the buffer containing data need to send
 * @return    1: DMA triggered successfully
 *            0: UART busy : last packet not send over,you can't start to send the current packet data
 *
 * @note      DMA can only send 2047-bytes one time at most.
 *			  
 */
volatile unsigned char uart_dma_send(unsigned char* Addr)
{
	if(reg_uart_status1 & FLD_UART_TX_DONE)
	{
		reg_dma1_addr = (unsigned short)((unsigned int)Addr); //packet data, start address is sendBuff+1
		reg_dma1_size = 0xff;
		reg_dma_chn_en |= FLD_DMA_CHN_UART_TX;
		reg_dma_tx_rdy0	 |= FLD_DMA_CHN_UART_TX;
		return 1;
	}
	return 0;
}

/**
 * @brief     uart send data function, this  function tell the DMA to get data from the RAM and start
 *            the DMA transmission
 * @param[in] byte - single byte data need to send
 * @return    1: send success ;
 *            0: DMA busy
 */
volatile unsigned char uart_send_byte(unsigned char byte)
{
	unsigned int addr;

	unsigned char b[5] = {1, 0,0,0,0};

	addr = (unsigned int)b;

	b[4] = byte;
    if (reg_uart_status1 & FLD_UART_TX_DONE ) {
    	reg_dma1_addr = addr; //packet data, start address is sendBuff+1
    	reg_dma1_addrHi = ((addr>>16)&0xff);
        reg_dma_tx_rdy0	 = FLD_DMA_CHN1;
        return 1;
    }

    return 0;
}

/**
 * @brief     data receive buffer initiate function. DMA would move received uart data to the address space,
 *            uart packet length needs to be no larger than (recBuffLen - 4).
 * @param[in] RecvAddr - pointer to the receiving buffer
 * @param[in] RecvBufLen - length in byte of the receiving buffer
 * @return    none
 */

void uart_recbuff_init(unsigned char *RecvAddr, unsigned short RecvBufLen)
{
    unsigned char bufLen;
    unsigned int addr;

    addr = (unsigned int) RecvAddr;
    bufLen = RecvBufLen / 16;

    reg_dma0_addr = addr; //set receive buffer address
    reg_dma0_addrHi = 0x04;
    reg_dma0_size = bufLen; //set receive buffer size

    reg_dma0_mode = FLD_DMA_WR_MEM;   //set DMA 0 mode to 0x01 for receive
    reg_dma_chn_en |= FLD_DMA_CHN_UART_RX;
}

/**
 * @brief     This function determines whether parity error occurs once a packet arrives.
 * @param[in] none
 * @return    1: parity error ;
 *            0: no parity error
 */
unsigned char uart_is_parity_error(void)
{
    return (reg_uart_status0 & FLD_UART_RX_ERR_FLAG);
}

/**
 * @brief     This function clears parity error status once when it occurs.
 * @param[in] none
 * @return    none
 *
 * Note:
 *(1)DMA mode
 * RX FIFO will also be cleared when parity error flag is cleared .
 *(2)NON-DMA mode
 * When parity error occurs, clear parity error flag after UART receives the data.
 * Cycle the four registers (0x90 0x91 0x92 0x93) from register "0x90" to get data when UART receives the data next time.
 */
void uart_clear_parity_error(void)
{
	reg_uart_status0|= FLD_UART_CLEAR_RX_FLAG; //write 1 to clear
}

/**
 * @brief     UART hardware flow control configuration. Configure RTS pin.
 * @param[in] Enable - enable or disable RTS function.
 * @param[in] Mode - set the mode of RTS(auto or manual).
 * @param[in] Thresh - threshold of trig RTS pin's level toggle(only for auto mode),
 *                     it means the number of bytes that has arrived in Rx buf.
 * @param[in] Invert - whether invert the output of RTS pin(only for auto mode)
 * @param[in] pin   - RTS pin select.
 * @return    none
 */

void uart_set_rts(unsigned char Enable, UART_RTSModeTypeDef Mode, unsigned char Thresh, unsigned char Invert,  GPIO_PinTypeDef pin)
{
    if (Enable)
    {
    	gpio_set_func(pin,UART_RTS );//enable rts pin
        reg_uart_ctrl2 |= FLD_UART_CTRL2_RTS_EN; //enable RTS function
    }
    else
    {
        reg_uart_ctrl2 &= (~FLD_UART_CTRL2_RTS_EN); //disable RTS function
    }

    if (Mode)
    {
    	reg_uart_ctrl2 |= FLD_UART_CTRL2_RTS_MANUAL_EN;
    }
    else {
    	reg_uart_ctrl2 &= (~FLD_UART_CTRL2_RTS_MANUAL_EN);
    }

    if (Invert) {
    	reg_uart_ctrl2 |= FLD_UART_CTRL2_RTS_PARITY;
    }
    else {
    	reg_uart_ctrl2 &= (~FLD_UART_CTRL2_RTS_PARITY);
    }

    //set threshold
    reg_uart_ctrl2 = ((reg_uart_ctrl2 & (~FLD_UART_CTRL2_RTS_TRIG_LVL)) | (Thresh & FLD_UART_CTRL2_RTS_TRIG_LVL));
}

/**
 * @brief     This function sets the RTS pin's level manually
 * @param[in] Polarity - set the output of RTS pin(only for manual mode)
 * @return    none
 */

void uart_set_rts_level(unsigned char Polarity)
{
    if (Polarity) {
    	reg_uart_ctrl2 |= FLD_UART_CTRL2_RTS_MANUAL_VAL;
    }
    else {
    	reg_uart_ctrl2 &= (~FLD_UART_CTRL2_RTS_MANUAL_VAL);
    }
}

/**
 * @brief      UART hardware flow control configuration. Configure CTS pin.
 * @param[in]  Enable - enable or disable CTS function.
 * @param[in]  Select - when CTS's input equals to select, tx will be stopped
 * @param[in]  pin   - CTS pin select.
 * @return     none
 */
void uart_set_cts(unsigned char Enable, unsigned char Select,GPIO_PinTypeDef pin)
{
    if (Enable)
    {
    	 //When the pad is configured with mux input and a pull-up resistor is required, gpio_input_en needs to be placed before gpio_function_dis,
    	//otherwise first set gpio_input_disable and then call the mux function interface,the mux pad will may misread the short low-level timing.confirmed by minghai.20210709.
    	gpio_set_input_en(pin, 1);//enable input
    	gpio_set_func(pin,UART_CTS_I);//enable cts pin
    	reg_uart_ctrl1|= FLD_UART_CTRL1_CTS_EN; //enable CTS function
    }
    else
    {
    	reg_uart_ctrl1  &= (~FLD_UART_CTRL1_CTS_EN); //disable CTS function
    }

    //Select Trigger Voltage
    if (Select)
    {
    	reg_uart_ctrl1  |= FLD_UART_CTRL1_CTS_SELECT;
    }
    else
    {
    	reg_uart_ctrl1  &= (~FLD_UART_CTRL1_CTS_SELECT);
    }
}


/**
 * @brief      This function selects  pin for spi master or slave mode.
 * @param[in]  pin  - the selected pin.
 * @return     none
 */
void uart_set_pin_mux(GPIO_PinTypeDef pin,gpio_func_e function)
{
	if (pin != 0)
	{
		 //When the pad is configured with mux input and a pull-up resistor is required, gpio_input_en needs to be placed before gpio_function_dis,
		 //otherwise first set gpio_input_disable and then call the mux function interface,the mux pad will may misread the short low-level timing.confirmed by minghai.20210709.
		gpio_set_input_en(pin, 1);
		//note: pullup setting must before uart gpio config, cause it will lead to ERR data to uart RX buffer(confirmed by sihui&sunpeng)
		//PM_PIN_PULLUP_1M   PM_PIN_PULLUP_10K
		gpio_setup_up_down_resistor(pin, PM_PIN_PULLUP_10K);  //must, for stability and prevent from current leakage
		gpio_set_func( pin, function);
	}
}


/**
* @brief      This function serves to select pin for UART module.
* @param[in]  tx_pin   - the pin to send data.
* @param[in]  rx_pin   - the pin to receive data.
* @return     none
*/
void uart_gpio_set(GPIO_PinTypeDef tx_pin,GPIO_PinTypeDef rx_pin)
{
	uart_set_pin_mux(tx_pin,(tx_pin != GPIO_PA2) ? UART_TX:1);
	uart_set_pin_mux(rx_pin,(rx_pin != GPIO_PA1) ? UART_RX_I:1);
}
/**
 * @brief   This function enables the irq when UART module receives error data.
 * @param[in] none
 * @return    none
 */
void uart_mask_error_irq_enable(void)
{
   reg_uart_rx_timeout1|= FLD_UART_MASK_ERR_IRQ;
   reg_irq_mask |= FLD_IRQ_UART_EN;
}
/**
 * @brief     This function disables the irq when UART module receives error data.
 * @param[in] none
 * @return    none
 */
void uart_mask_error_irq_dis(void)
{
   reg_uart_rx_timeout1&= ~FLD_UART_MASK_ERR_IRQ;
}

/**
* @brief      This function serves to set rtx pin for UART module.
* @param[in]  rtx_pin  - the rtx pin need to set.
* @return     none
*/
void uart_set_rtx_pin(GPIO_PinTypeDef rtx_pin)
{
	 //When the pad is configured with mux input and a pull-up resistor is required, gpio_input_en needs to be placed before gpio_function_dis,
	//otherwise first set gpio_input_disable and then call the mux function interface,the mux pad will may misread the short low-level timing.confirmed by minghai.20210709.
	gpio_set_input_en(rtx_pin,1);
	gpio_setup_up_down_resistor(rtx_pin, PM_PIN_PULLUP_10K);
 	gpio_set_func(rtx_pin,UART_RTX);
}
/**
 * @brief     This function enables the rx_done irq.
 * @param[in] none
 * @return    none
 */
void uart_rxdone_irq_en(void)
{
	reg_uart_rx_timeout1|= FLD_UART_MASK_RXDONE_IRQ;
	reg_irq_mask |= FLD_IRQ_UART_EN;
}
/**
 * @brief     This function disable the rx_done irq.
 * @param[in] none
 * @return    none
 */
void uart_rxdone_irq_dis(void)
{
	reg_uart_rx_timeout1&= ~FLD_UART_MASK_RXDONE_IRQ;
}

