/********************************************************************************************************
 * @file	random.c
 *
 * @brief	This is the source file for B80
 *
 * @author	Driver Group
 * @date	2021
 *
 * @par		Copyright (c) 2021, Telink Semiconductor (Shanghai) Co., Ltd.
 *			All rights reserved.
 *
 *          The information contained herein is confidential property of Telink
 *          Semiconductor (Shanghai) Co., Ltd. and is available under the terms
 *          of Commercial License Agreement between Telink Semiconductor (Shanghai)
 *          Co., Ltd. and the licensee or the terms described here-in. This heading
 *          MUST NOT be removed from this file.
 *
 *          Licensee shall not delete, modify or alter (or permit any third party to delete, modify, or
 *          alter) any information contained herein in whole or in part except as expressly authorized
 *          by Telink semiconductor (shanghai) Co., Ltd. Otherwise, licensee shall be solely responsible
 *          for any claim to the extent arising out of or relating to such deletion(s), modification(s)
 *          or alteration(s).
 *
 *          Licensees are granted free, non-transferable use of the information in this
 *          file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided.
 *
 *******************************************************************************************************/
#include "adc.h"
#include "analog.h"
#include "dfifo.h"
#include "clock.h"
#include "gpio.h"
#include "timer.h"
#include "random.h"
_attribute_data_retention_	unsigned int rnd_m_w = 0;
_attribute_data_retention_	unsigned int rnd_m_z = 0;

typedef union
{
	unsigned int rng32;

	struct {
		unsigned int bit0:1;
		unsigned int bit1:1;
		unsigned int bit2:1;
		unsigned int bit3:1;
		unsigned int bit4:1;
		unsigned int bit5:1;
		unsigned int bit6:1;
		unsigned int bit7:1;
		unsigned int bit8:1;
		unsigned int bit9:1;
		unsigned int bit10:1;
		unsigned int bit11:1;
		unsigned int bit12:1;
		unsigned int bit13:1;
		unsigned int bit14:1;
		unsigned int bit15:1;
		unsigned int bit16:1;

	}rng_bits;

}ADC_RNG_ValDef;


volatile static ADC_RNG_ValDef rng = {0};


static unsigned short rng_made(void)
{

	rng.rng_bits.bit16 = rng.rng_bits.bit16 ^ rng.rng_bits.bit15 ^ rng.rng_bits.bit13 ^ rng.rng_bits.bit4 ^ rng.rng_bits.bit0;
	if(rng.rng_bits.bit16)
	{
		rng.rng32 = (rng.rng32<<1)+ 1;
	}
	else
	{
		rng.rng32 = (rng.rng32<<1);
	}

	return ((unsigned short)rng.rng32);
}


/**
 * @brief This function serves to set adc sampling and get results.
 * @param[in]  none.
 * @return the result of sampling.
 */
unsigned short adc_rng_result(void)
{
	volatile signed int adc_dat_buf[16];
	volatile unsigned short rng_result;

	unsigned char i;
	//unsigned int j;

	//dfifo setting will lose in suspend/deep, so we need config it every time
	adc_config_misc_channel_buf((unsigned short *)adc_dat_buf,16);  //size: ADC_SAMPLE_NUM*4
	dfifo_enable_dfifo2();

	unsigned int t0 = clock_time();

	while(!clock_time_exceed(t0, 25));  //wait at least 2 sample cycle(f = 96K, T = 10.4us)

	for(i=0;i<16;i++)
	{
		while((!adc_dat_buf[i])&&(!clock_time_exceed(t0,25)));  //wait for new adc sample data,

		t0 = clock_time();

		rng.rng32 &= 0x0000ffff;
		if(adc_dat_buf[i] & BIT(0))
		{
			rng.rng_bits.bit16 = 1;
		}

		rng_result = rng_made();

	}
	dfifo_disable_dfifo2();
	return rng_result;

}

/**
 * @brief This function is used for ADC configuration of ADC supply voltage sampling.
 * @return none
 */
void rng_init(void)
{
	//set Vbat divider select,
	adc_set_vref_vbat_divider(ADC_VBAT_DIVIDER_OFF);

	//set channel mode and channel
	adc_set_ain_chn_misc(VBAT, GND);

	//set Analog input pre-scaling and
	adc_set_ain_pre_scaler(ADC_PRESCALER_1F8);//  ADC_PRESCALER_1F8
}

void random_generator_init(void)
{
	rng.rng32 = 0x0000ffff;
	//ADC model init
	adc_init();
	rng_init();
	//After setting the ADC parameters, turn on the ADC power supply control bit
	adc_power_on_sar_adc(1);
	rnd_m_w = adc_rng_result()<<16 | adc_rng_result();
	rnd_m_z = adc_rng_result()<<16 | adc_rng_result();
	adc_power_on_sar_adc(0);
}


/**
 * @brief     This function performs to get one random number
 * @param[in] none.
 * @return    the value of one random number.
 */
unsigned int rand(void)  //16M clock, code in flash 23us, code in sram 4us
{
	rnd_m_w = 18000 * (rnd_m_w & 0xffff) + (rnd_m_w >> 16);
	rnd_m_z = 36969 * (rnd_m_z & 0xffff) + (rnd_m_z >> 16);
	unsigned int result = (rnd_m_z << 16) + rnd_m_w;

	return (unsigned int)( result ^ clock_time() );
}





















