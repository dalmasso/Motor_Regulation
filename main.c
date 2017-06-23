/*
 * main.c
 *
 *  Created on: 9 fŽvr. 2016
 *      Author: loicdalmasso
 */
#include "main.h"

float ADCValue_TACHY = 0;		// Tachy value
float ADCValue_CMD = 0;			// CMD (potar) value
uint16_t DAC_Value =0;			// DAC value
float DAC_Buf = 0;				// DAC value after coeff for actual voltage
float Ve[2] = {0,0};			// Ve value ([1]: next  [0]: previous sample)
float Va[2] = {0,0};			// Va value ([1]: next  [0]: previous sample)
float Vs[2] = {0,0};			// Vs value ([1]: next  [0]: previous sample)
uint8_t TIM_top = 0;			// Flag TIMER3


int main(void)
{
	SystemInit();	// Config clock STM
	ADC_Config();	// Config ADC for receive Command, Tachy
	DAC_Config();	// Config DAC for send new Command regulation
	TIM3_Config();	// Config Timer for sample

	while(1)
	{
		// Sample
		while (TIM_top == 1)
		{


			// ADC Tachy
			ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_3Cycles);
			ADC_SoftwareStartConv(ADC1);
			while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) != SET);
			ADC_ClearFlag(ADC1, ADC_FLAG_STRT);
			ADCValue_TACHY = ADC_GetConversionValue(ADC1);

			// Convert NUM value -> Voltage
			ADCValue_TACHY = ADCValue_TACHY *0.000805;
			Vs[1] = ADCValue_TACHY;


			// ADC Consign
			ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 1, ADC_SampleTime_3Cycles);
			ADC_SoftwareStartConv(ADC1);
			while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) != SET);
			ADC_ClearFlag(ADC1, ADC_FLAG_STRT);
			ADCValue_CMD = ADC_GetConversionValue(ADC1);

			// Max ADC to avoid error
			if(ADCValue_CMD>4094)
				ADCValue_CMD = 4094;

			// Rotation Potar
			// Sign - before center (Left) : 0->2046 => ADC(-)
			// Sign + after center (Right)  : 2047 -> 4094 => ADC(+)
			ADCValue_CMD = (ADCValue_CMD - 2047)*2;


			// Convert NUM value -> Voltage
			ADCValue_CMD = ADCValue_CMD * 0.000805;
			Ve[1] = ADCValue_CMD;


			// Sign => 1: sens +  0: sens -
			if(!(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1)))
				Vs[1] = -Vs[1];



			// Mesure temps calcul - start
			GPIO_SetBits(GPIOD,GPIO_Pin_1);

			// TFBF
			//Va[1] = (-0.97)*(Ve[0] - Vs[0]) + 1.18*(Ve[1] - Vs[1]) + Va[0]; // tau : 60ms
			//Va[1] = (-1.167)*(Ve[0] - Vs[0]) + 1.41*(Ve[1] - Vs[1]) + Va[0]; // tau : 47ms 				OK error statique : 0.869
			//Va[1] = (-5.832)*(Ve[0] - Vs[0]) + 7.074*(Ve[1] - Vs[1]) + Va[0]; // amelioration tau : 33 ms
			Va[1] = (-1.944)*(Ve[0] - Vs[0]) + 2.358*(Ve[1] - Vs[1]) + Va[0]; // affinement tau : 32ms

			// Mesure temps calcul - stop
			GPIO_ResetBits(GPIOD,GPIO_Pin_1);


			// DAC saturation
			if(Va[1]>3)
				Va[1] = 3;
			else if(Va[1]<-3)
				Va[1] = -3;

			// Save old samples
			Va[0] = Va[1];
			Ve[0] = Ve[1];
			Vs[0] = Vs[1];


			// DAC: -(PA5)  +(PA4)
			// 0xFFF (4095) = 3V
			if(Va[1]>0)
			{
				DAC_Value = Va[1] * 1365; // Convert Value to actual voltage
				DAC_SetDualChannelData(DAC_Align_12b_R, 0x000,DAC_Value); // DAC positive value
			}
			else
			{
				DAC_Value = (-Va[1] * 1363); // remove sign & Convert Value to actual voltage
				DAC_SetDualChannelData(DAC_Align_12b_R, DAC_Value,0x000); // DAC negative value
			}
			// Send command on DAC
			DAC_DualSoftwareTriggerCmd(ENABLE);

			// Reset flag Timer
			TIM_top = 0;


		}
	}
}





// Initialization ADC
void ADC_Config(void)
{
	GPIO_InitTypeDef		GPIO_InitStructure;
	ADC_InitTypeDef			ADC_InitStructure;
	ADC_CommonInitTypeDef 	ADC_CommonInitStructure;


	// Clocks
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);


	//test temps execution
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOD,&GPIO_InitStructure);



	// GPIO config (ADC_tachy PA2)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA,&GPIO_InitStructure);


	// GPIO config (ADC_consigne PB0)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB,&GPIO_InitStructure);


	// ADC Config
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 3;
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_Cmd(ADC1,ENABLE);

	ADC_SoftwareStartConv(ADC1);
}



// Initialization TIM3
void TIM3_Config(void)
{
	TIM_TimeBaseInitTypeDef		TIM_TimeBaseStructure;
	NVIC_InitTypeDef			NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);

	// Config Timer
	TIM_TimeBaseStructure.TIM_Period = 10000-1; // 10ms	// 8-1; 9us
	TIM_TimeBaseStructure.TIM_Prescaler = 1; // 1MHz
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseStructure);

	// Enable interrupt
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	// Start timer & interrupt
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);
	TIM_SetCounter(TIM3, 0);
	TIM_Cmd(TIM3,ENABLE);
}


// Interrupt Handler TIM3
void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM3,TIM_IT_Update) != RESET)
	{
		TIM_top = 1; // Top timer for sample
		TIM_ClearITPendingBit(TIM3,TIM_IT_Update);
	}
}
/*	//test freq sample
	if(TIM_top == 1)
	{
		GPIO_SetBits(GPIOD,GPIO_Pin_1);
		TIM_top = 0;
	}
	else
	{	GPIO_ResetBits(GPIOD,GPIO_Pin_1);
		TIM_top = 1;
	}

}
*/

// Initialization DAC
void DAC_Config(void)
{
	GPIO_InitTypeDef		GPIO_InitStructure;
	DAC_InitTypeDef			DAC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);

	// PA4
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	// PA5
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	DAC_InitStructure.DAC_Trigger = DAC_Trigger_Software;
	DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
	DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;

	DAC_Init(DAC_Channel_1,&DAC_InitStructure);
	DAC_Init(DAC_Channel_2,&DAC_InitStructure);

	DAC_Cmd(DAC_Channel_1, ENABLE);
	DAC_Cmd(DAC_Channel_2, ENABLE);
}
