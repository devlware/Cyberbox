/******************************************************************************
*
* Copyright (C) 2011 by IMBRAX <opensource@imbrax.com.br>
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
******************************************************************************/

#include <16F877A.h>
#device ADC=10
#fuses HS,NOWDT,PROTECT,NOLVP,NOBROWNOUT,PUT
#use delay(clock=20000000)
#use rs232(baud=115200, xmit=PIN_C6, rcv=PIN_C7, ERRORS, PARITY=N)

#USE fast_io(B)
#USE fast_io(E)
#USE fast_io(C)
#USE fast_io(D)

#define MATATEMPO 200 //usecs
#define MATAPEQ 3

#include <string.h>
#include <stdlib.h>

#use i2c(master,sda=PIN_C4, scl=PIN_C3, FAST, NOFORCE_SW)
#byte OPTION_REG = 0x81

const int SLAVE_ADDRESS = 252;

unsigned int16 anal_cont = 0;
unsigned int16 entrada = 0x0;
unsigned int16 anal_var = 1500;
unsigned int16 anal_value;
unsigned int16 a_max[8] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};
unsigned int16 a_min[8] = {1023,1023,1023,1023,1023,1023,1023,1023};
unsigned int16 w_dig = 0x0;
unsigned int   w_anal = 0;
unsigned int1  servo_dc = 0; //1-SERVO 0-DC


/*
 * Prototipos
 */
void erro(byte num);
void leitura(unsigned int cmd);
void timer2_isr(void);
void timer1_isr(void);
void init(void);
void showLeds(void);
void z_vel(unsigned int tmp, unsigned int16 param);
void ad_test(int tmp, unsigned int param);
void set_anal(int tmp, int1 val);
void set_dig(unsigned int tmp, int1 val);
unsigned int qual_slave_io(unsigned int cmd);
int executar(unsigned int tmp, unsigned int param1, char c);

/*
 *
 */
void erro(byte num)
{
   printf("%c",160+num);
}

/*
 *
 */
#INT_TIMER2
void timer2_isr(void)
{
   unsigned int i;

   disable_interrupts(INT_TIMER1);
   set_timer2(5); //50

   if (anal_cont == anal_var) {
      for (i = 0; i < 8; i++) {
         set_adc_channel(i);
         read_adc();
         delay_us(12);
         anal_value = read_adc();

         if (anal_value > a_max[i])
            a_max[i] = anal_value;
         if (anal_value < a_min[i])
            a_min[i] = anal_value;
         if (bit_test(w_anal, i))
            printf("%c%c%c", 192 + i, anal_value >> 8, anal_value);
      }
      anal_cont = 0;
   }
   anal_cont++;
   enable_interrupts(INT_TIMER1);
}

/*
 *
 */
#INT_TIMER1
void timer1_isr(void)
{
   unsigned int16 dado = 0;
   unsigned int i;

   disable_interrupts(INT_TIMER2);
   set_timer1(65400);
   dado = input_d();
   dado = dado << 8;
   dado += input_b();

   if (entrada != dado) {
      for (i = 0; i <= 15; i++) {
         if ((bit_test(entrada,i) != bit_test(dado,i)) && bit_test(w_dig,i))
            printf("%c%c", i, bit_test(dado, i));
      }
      entrada = dado;
   }
   enable_interrupts(INT_TIMER2);
}

/*
 *
 */
void z_vel(unsigned int tmp, unsigned int16 param)
{
   unsigned int change = 0, retorno = 0, tries = 0;
   unsigned int1 ack = 1; //0-ACK, 1-NOACK

   if (param == 0)
         retorno = 32+tmp; //RESET
   if (param == 100)
         retorno = 16+tmp; //SET
   if (param < 100 && param > 0) {
      if(servo_dc) {
         servo_dc = 0;
         change = 0xFA;
      }
      retorno = 144+tmp; ///PWM - DC
   }
   else if (param > 100) {// <<---------------<<
      if (!servo_dc) {
         servo_dc = 1;
         change = 0xFF;
      }
      retorno = 144+tmp; //PWM - SERVO
   }
   if (change) {
      while (tries < 5) {
         tries++;
         i2c_start();
         delay_us(MATAPEQ);
         ack = i2c_write(SLA);
         if (ack) {
            i2c_stop();
            continue;
         }
         delay_us(MATATEMPO);
         ack = i2c_write(change);
         if (ack) {
            delay_us(MATAPEQ);
            i2c_stop();
            continue;
         }
         break;
      }
      delay_us(MATAPEQ);
      i2c_stop();
      change = 0;
   }
   if (tries >= 5)
      erro(5);
   delay_us(MATATEMPO);
   tries = 0;
   param += 16;

   while (tries < 5) {
      tries++;
      i2c_start();
      delay_us(MATAPEQ);
      ack = i2c_write(SLAVE_ADDRESS);
      if (ack) {
         delay_us(MATAPEQ);
         i2c_stop();
         continue;
      }

      delay_us(MATATEMPO);
      ack = i2c_write(tmp);
      if (ack) {
         delay_us(MATAPEQ);
         i2c_stop();
         continue;
      }
      delay_us(MATATEMPO);
      ack = i2c_write(param);
      if (ack) {
         delay_us(MATAPEQ);
         i2c_stop();
         continue;
      }
      break;
   }
   delay_us(MATAPEQ);
   i2c_stop();

   if (tries >= 5)
      erro(3);
   else
      printf("%c",retorno); //means OK;
   tries = 0;
}

/*
 *
 */
void leitura(unsigned int cmd)
{
    printf("%c%c", cmd, bit_test(entrada, cmd));
}

/*
 *
 */
void ad_test(int tmp, unsigned int param)
{
    unsigned int16 temp = 0;

    switch (param) {
        case 0:
            set_adc_channel(tmp);
            read_adc();
            delay_us(12);
            temp = read_adc();
            printf("%c%c%c", 0b11000000 + tmp, temp >> 8, temp);
            break;
        case 1:
            set_adc_channel(tmp);
            read_adc();
            delay_us(12);
            temp = read_adc();
            printf("%c%c%c", 0b11010000 + tmp, a_min[tmp] >> 8, a_min[tmp]);
            a_min[tmp] = temp;
            break;
        case 2:
            set_adc_channel(tmp);
            read_adc();
            delay_us(12);
            temp = read_adc();
            printf("%c%c%c", 0b11100000 + tmp, a_max[tmp] >> 8, a_max[tmp]);
            a_max[tmp] = temp;
            break;
    }
}

/*
 *
 */
void set_anal(int tmp, int1 val)
{
   if (val)
      bit_set(w_anal,tmp);
   else
      bit_clear(w_anal,tmp);
}

/*
 *
 */
void set_dig(unsigned int tmp, int1 val)
{
   if (val)
      bit_set(w_dig,tmp);
   else
      bit_clear(w_dig,tmp);
}

/*
 *
 */
int executar(unsigned int tmp, unsigned int param1, char c)
{
    switch (c) {
        case 'n':
            if(tmp > 7)
                erro(1);
            else {
                if (param1) {
                    set_anal(tmp,1);
                    printf("%c", tmp + 80);
                } else {
                    set_anal(tmp,0);
                    printf("%c", tmp + 96);
                }
            }
            break;
        case 'k':
            if (tmp > 15)
                erro(1);
            else {
                if(param1) {
                    set_dig(tmp,1);
                    printf("%c", tmp + 48);
                } else {
                    set_dig(tmp, 0);
                    printf("%c", tmp + 64);
                }
            }
            break;
        case 'v':
            if(param1 > 201)
               erro(1);
            disable_interrupts(INT_TIMER1);
            disable_interrupts(INT_TIMER2);
            z_vel(tmp, param1);
            enable_interrupts(INT_TIMER1);
            enable_interrupts(INT_TIMER2);
         break;
         case 'f':
            printf("%c%c%c", 112, entrada >> 8, entrada);
         break;
         case 's':
               disable_interrupts(INT_TIMER1);
               disable_interrupts(INT_TIMER2);
               z_vel(tmp, 100);
               enable_interrupts(INT_TIMER1);
               enable_interrupts(INT_TIMER2);
            break;
         case 'r':
               disable_interrupts(INT_TIMER1);
               disable_interrupts(INT_TIMER2);
               z_vel(tmp, 0);
               enable_interrupts(INT_TIMER1);
               enable_interrupts(INT_TIMER2);
            break;
         case 'a':
            if(tmp > 7)
               erro(1);
            else
               ad_test(tmp,param1);
            break;
         case 'l':
            if(tmp > 15)
               erro(1);
            else
               leitura(tmp);
            break;
         default:
            erro(1);
            break;
      }
      return 0;
}

/*
 *
 */
void init(void)
{
   set_tris_b(0xFF);
   set_tris_d(0xFF);
   set_tris_c(0b10011110);
   output_float(PIN_C4);
   output_float(PIN_C3);
   anal_cont = 0;
   setup_adc_ports(A_ANALOG);
   setup_adc(ADC_CLOCK_DIV_32);
   set_adc_channel(0);
   set_timer1(65000);
   setup_timer_1(T1_INTERNAL | T1_DIV_BY_1);
   set_timer2(0);
   setup_timer_2(T2_DIV_BY_16, 0, 16);
   enable_interrupts(INT_TIMER1);
   enable_interrupts(INT_TIMER2);
   delay_ms(10);
   enable_interrupts(GLOBAL);
   #ASM
      bsf 0x98,2

      bsf 0x9C,0
      bsf 0x9C,1
      bsf 0x9C,2
      bcf 0x9C,3
      bcf 0x9C,4
      bcf 0x9C,5
      bcf 0x9C,6
      bcf 0x9C,7

      bcf 0x9D,0
      bcf 0x9D,1
      bcf 0x9D,2
      bcf 0x9D,3
      bcf 0x9D,4
      bcf 0x9D,5
      bcf 0x9D,6
      bcf 0x9D,7
   #ENDASM
}

/*
 *
 */
void showLeds(void)
{
    int i = 0;
        
    for (; i <= 12; i++) {
        disable_interrupts(INT_TIMER1);
        disable_interrupts(INT_TIMER2);
        z_vel(i, 100);
        enable_interrupts(INT_TIMER1);
        enable_interrupts(INT_TIMER2);
        delay_ms(50);
    }

    for (i = 0; i <= 12; i++) {
        disable_interrupts(INT_TIMER1);
        disable_interrupts(INT_TIMER2);
        z_vel(i, 0);
        enable_interrupts(INT_TIMER1);
        enable_interrupts(INT_TIMER2);
        delay_ms(50);
    }
}

/*
 *
 */
int main(void)
{
   unsigned int carac;
   unsigned int tmp = 0, porta = 0;
   unsigned int16 param1 = 0;
   char com;

   init();
   
   showLeds();
   delay_ms(1000);
   
   for (;;) {
      param1 = 0xFFFF;
      carac = getc();
      if (bit_test(RS232_ERRORS,0)) {
         erro(6);
         continue;
      }
      tmp = carac & 0b11110000;
      tmp = tmp>>4;
      porta = (carac & 0b00001111);

      switch (tmp) {
         case 0:
            com = 'l';
            break;
         case 1:
            com = 's';
            break;
         case 2:
            com = 'r';
            break;
         case 12:
            param1 = 0x00;
            com = 'a';
            break;
         case 13:
            param1 = 0x01;
            com = 'a';
            break;
         case 14:
            param1 = 0x02;
            com = 'a';
            break;
         case 3:
            com = 'k';
            param1 = 0x01;
            break;
         case 4:
            com = 'k';
            param1 = 0x00;
            break;
         case 5:
            com = 'n';
            param1 = 0x01;
            break;
         case 6:
            com = 'n';
            param1 = 0x00;
            break;
         case 7:
            com = 'f';
            break;
         case 8:           //TIMER
            carac = getc();
            if (bit_test(RS232_ERRORS,0)) {
               erro(6);
               continue;
            }
            anal_var = porta << 8;
            anal_cont = anal_var = ((anal_var | carac) * 16) + 10;
            printf("%c", 128);
            com = 'X';
            break;
         case 9:
            com = 'v';
            param1 = 0x00;
            param1 = getc();
            if(bit_test(RS232_ERRORS, 0)) {
               erro(6);
               continue;
            }
            break;
         case 15:
            printf("%c", 240);
            com = 'X';
            break;
      }
      if (com != 'X')
         executar(porta, param1, com);
   }

    return 0;
}

