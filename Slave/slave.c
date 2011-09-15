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

#include <16F873A.h>

#fuses HS, NOWDT, PROTECT, NOLVP, NOBROWNOUT, NOPUT
#use delay(clock=20000000)
#use i2c(SLAVE, SDA=PIN_C4, SCL=PIN_C3, FAST, address = 252, NOFORCE_SW)
#use fast_io(A)
#use fast_io(B)
#use fast_io(C)

#byte OPTION_REG = 0x81

#define _TEMPO_ 250
#define DCTIME 100
#define SERVOTIME 800

const int SLAVE_ADDRESS = 252;
int fState = 0;
unsigned int porta = 0;
unsigned int16 clock = 0;
unsigned int16 speeds[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
unsigned int16 perd = DCTIME;

/*
 * Prototipos
 */
void init(void);
void ssp_interupt(void);


#INT_SSP
void ssp_interupt(void)
{
   byte incoming;
   unsigned int16 valor;

   disable_interrupts(INT_RTCC);
   if (i2c_poll() == FALSE) { //READ
      i2c_write(66);
   } else {                    //WRITE
      incoming = i2c_read();
      
        switch (fState) {
         case 0:
            fState = 1;
            break;
         case 1:
            if(incoming == SLAVE_ADDRESS) {
               fstate = 1;
               break;
            } else {
                porta = incoming;
               
                if (porta == 0xFF) {
                  perd = SERVOTIME;
                  fState = 0;
                  break;
               }
               if (porta == 0xFA) {
                  perd = DCTIME;
                  fState = 0;
                  break;
               }
               fState = 2;
             }
            break;
         case 2:
            if (incoming == SLAVE_ADDRESS) {
               fstate = 1;
               break;
            } else {
               incoming -= 16;
               if (perd == DCTIME)
                  valor = incoming;
               else {
                  if (incoming == 100)
                     valor = SERVOTIME;
                  else if (incoming == 0)
                     valor = 0;
                  else
                     valor = incoming - 101;
               }
               if (porta < 12)
                  speeds[porta] = valor;
               if (porta >= 12) {
                  speeds[0] = valor;
                  speeds[1] = valor;
                  speeds[2] = valor;
                  speeds[3] = valor;
                  speeds[4] = valor;
                  speeds[5] = valor;
                  speeds[6] = valor;
                  speeds[7] = valor;
                  speeds[8] = valor;
                  speeds[9] = valor;
                  speeds[10] = valor;
                  speeds[11] = valor;
               }
               fState = 0;
             }
         break;
      }
   }
   enable_interrupts(INT_RTCC);
}

void init(void)
{
   set_tris_a(0x00);
   set_tris_b(0x00);
   set_tris_c(0b11111000);
   output_low(PIN_C0);
   output_low(PIN_C1);
   output_low(PIN_C2);
   output_float(PIN_C3);
   output_float(PIN_C4);
   enable_interrupts(INT_SSP);
   enable_interrupts(GLOBAL);
   #ASM
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

int main (void)
{
   init();
   
   delay_ms(50);
   
   while (TRUE) {
       clock++;
       if (clock<=speeds[0])
          output_high(PIN_A3);
       else
          output_low(PIN_A3);

       if (clock<=speeds[1])
          output_high(PIN_A2);
       else
          output_low(PIN_A2);

       if (clock<=speeds[2])
          output_high(PIN_A1);
       else
          output_low(PIN_A1);

       if (clock<=speeds[3])
          output_high(PIN_A0);
       else
          output_low(PIN_A0);

       if (clock<=speeds[4])
          output_high(PIN_B0);
       else
          output_low(PIN_B0);

       if (clock<=speeds[5])
          output_high(PIN_B1);
       else
          output_low(PIN_B1);

       if (clock<=speeds[6])
          output_high(PIN_B2);
       else
          output_low(PIN_B2);

       if (clock<=speeds[7])
          output_high(PIN_B3);
       else
          output_low(PIN_B3);

       if (clock<=speeds[8])
          output_high(PIN_B4);
       else
          output_low(PIN_B4);

       if (clock<=speeds[9])
          output_high(PIN_B5);
       else
          output_low(PIN_B5);

       if (clock<=speeds[10])
          output_high(PIN_B6);
       else
          output_low(PIN_B6);

       if (clock<=speeds[11])
          output_high(PIN_B7);
       else
          output_low(PIN_B7);

       if (clock >= perd)
          clock = 0;
   }

    return 0;
}

