#include "lpc17xx.h"
#include <stdio.h>
#include "math.h"

#define C0 0x3F
#define C1 0x30
#define C2 0x5B
#define C3 0x4F
#define C4 0x66
#define C5 0x6D
#define C6 0x7D
#define C7 0x07 // (0b111_1000)
#define C8 0x7F // (0b000_0000)
#define C9 0x6F // (0b001_1000)
#define CA 0x77 // (0b0001000)
#define CB 0x7F // (0b0000011)
#define CC 0x39 // (0b1000110)
#define CD 0x21 // (0b0100001)
#define CE 0x79 // (0b0000110)
#define CF 0x71 // (0b0001110)
#define CH 0x76 // (0b0001001)
#define CO 0x3F // (0b1000000)
#define CL 0x38 // (0b1000111)
#define CR 0x2F // (0b0101111)
#define CG 0x3D // (0b0000010)
#define CN 0x2D // (0b0101011)
#define Cr 0x50
#define C__ 0x5F // (0b1011111)
#define Ce 0x7B
#define Cn 0x54
#define Cline 0x40
#define SystemFrequency 100000000
#define Num_Muestras 50
#define Pi 3.14159
#define DAC_BIAS	0x00010000  // Settling time a valor 2,5us
#define Switch_texto_de_aviso (1<<0)
#define Switch_horassegundos (1<<1)
#define led_visualiza_reloj (1<<3) 
#define led_visualiza_Alarma_1 (1<<4) 
#define led_visualiza_Alarma_2 (1<<5) 
#define led_visualiza_Temporizador_1 (1<<6) 
#define led_visualiza_Temporizador_2 (1<<7) 
#define Switch_alarm1_OnOff (1<<8)
#define Switch_alarm2_OnOff (1<<9)
#define Pulsaador_Programacion (1<<10)
#define Pulsaador_Incrementar (1<<11)
#define Pulsaador_Decrementar (1<<12)
#define Switch_cambio_visualizacion (1<<13) 
#define Generacion_Senales_de_aviso (1<<26)

//Array Principal
uint16_t visual[4]={0,0,0,0}; //La función de este array es almacenar los datos para visualizar en el display
//Array funciones 
uint32_t FuncionSin[Num_Muestras];
uint32_t Zero[Num_Muestras];
uint32_t FuncionTri[Num_Muestras];
uint32_t FuncionCuadrado[Num_Muestras];

const uint8_t segment[10]={C0,C1,C2,C3,C4,C5,C6,C7,C8,C9}; //Valores del 0-9
const uint8_t HOLA[4]={CH,C0,CL,CA}; //Array para visualizar HOLA

//Variables sueltas empleadas a lo largo del programa
uint8_t i,a=0,seleccion=0,aux1,aux2, cont=0, a1=3, retardo_interrupcion=5, led_alm1=0, led_alm2=0, temp_alm1=0, temp_alm2=0; //Variables utilizadas para los IFs y los FOR
uint32_t frecuencia;//Variable de la frecuencia de muestreo de las funciones
uint32_t cg[4]={0,0,0,0}; //Array para sumar y restar en las alarmas
//Variables Horas/Min+Array horas
uint32_t hora[4]={0,0,0,0};
//Variables Segundos+Array Segundos
uint32_t segundos[4]={0,0,0,0};
//Variables Alarma 1 
uint32_t alarm1[4]={0,0,0,0};
//Variables Alarma 1 
uint32_t alarm2[4]={0,0,0,0};
//Variables Alarma 1 
uint32_t temp1[4]={0,0,0,0};
//Variables Alarma 1 
uint32_t temp2[4]={0,0,0,0};
//Variables para las ISRs
uint8_t flagext0=0; //Variable Configuracion
uint8_t flagext1=0; //Variable pulso incremento
uint8_t flagext2=0; //Variable pulso decremento
uint8_t flagext3=0; //Variable para modo de programa

void delay_1ms(uint32_t ms){
   	uint32_t i;
   	for(i=0;i<ms*14283;i++);
}

void Funciones(){ //Con esta funcion determino las funciones de inicio. 
	for(i=0;i<Num_Muestras;i++){
		FuncionSin[i]=512+512*sin(2*Pi*i/Num_Muestras); 
	}
	for(i=0;i<Num_Muestras;i++){
		if(i<25){ //Parte del triángulo ascendente
			FuncionTri[i]=(2046*i)/(Num_Muestras);
		}else{ //Parte del triángulo descendente
			FuncionTri[i]=(2046*(i-(a*2)))/(Num_Muestras);
			a++;
			if(a>=25){
			 a=0;
			}
		}
	}
	for(i=0;i<Num_Muestras;i++){
		if(i<25){
		FuncionCuadrado[i]=2046*25;
		}else{
		FuncionCuadrado[i]=0;
		}
	}
	for(i=0;i<Num_Muestras;i++){
		Zero[i]=0;
	}
}
void configGPIO(void){ //Función para configurar todos los pines
	LPC_PINCON->PINSEL0=0;//Configuramos los pines P0.[0..15] como GPIO
	LPC_PINCON->PINSEL3=0;//Configuramos los pines P1.[16..31] como GPIO
	LPC_PINCON->PINSEL4=0; //Configuramos los pines P2.[0..15] como GPIO
	LPC_GPIO0->FIODIR=0x000000F0; //Pines P0.[4..7] como salidas
	LPC_GPIO1->FIODIR=0x0FF00000; //Pines P1.[20..27] como salidas
	LPC_GPIO0->FIODIR &=~ Switch_alarm1_OnOff; //Pin P0.8 definido como entrada, sw_tipo_funcion "0"
	LPC_GPIO0->FIODIR &=~ Switch_alarm2_OnOff; //Pin P0.9 definido como entrada, sw_tipo_funcion "1"
	LPC_GPIO2->FIODIR &=~ Switch_texto_de_aviso; //Pines P2.0, P2.1, P2.10, P2.11, P2.12 y P2.13 configurados como entradas 
	LPC_GPIO2->FIODIR &=~ Switch_cambio_visualizacion;
	LPC_GPIO2->FIODIR &=~ Switch_horassegundos;
	LPC_GPIO2->FIODIR &=~ Pulsaador_Programacion;
	LPC_GPIO2->FIODIR &=~ Pulsaador_Incrementar;
	LPC_GPIO2->FIODIR &=~ Pulsaador_Decrementar;
	LPC_GPIO2->FIODIR |= Generacion_Senales_de_aviso; //Activo el pinde la generacion de señal P1.26 como salida
	LPC_GPIO2->FIODIR |= led_visualiza_Alarma_1|led_visualiza_Alarma_2|led_visualiza_reloj|led_visualiza_Temporizador_1|led_visualiza_Temporizador_2; //Pines P2.[3..7] configurados como salidas 
	LPC_GPIO0->FIOPIN = 0xF0; //Inicializo todos los displays como apagados
	LPC_GPIO2->FIOPIN |= led_visualiza_Alarma_1|led_visualiza_Alarma_2|led_visualiza_Temporizador_1|led_visualiza_Temporizador_2; //Apago los LEDs de los P2.[3..7] de inicio
	LPC_GPIO2->FIOPIN&=~led_visualiza_reloj; //Enciendo de inicio el LED del reloj, dado que va a ser el primer modo activo
}
void ConfigDAC(void){
  // Configuramos el pin p0.26 como salida DAC 
	// En PINSEL1 bits 20 a 0 y bit 21 a 1
  LPC_PINCON->PINSEL1&=~(0x1<<20);
	LPC_PINCON->PINSEL1|=(0x1<<21);
	LPC_PINCON->PINMODE1&=~((3<<20));
	LPC_PINCON->PINMODE1|=(2<<20);		//Pin 0.26 funcionando sin pull-up/down	
  return;
}
void Configura_SysTick(void){
	SysTick->LOAD=9999999; //Configuramos para que se introduzca interrupcion cada 0,1s
	SysTick->VAL=0; //Valor inicial=0
	SysTick->CTRL|=0x7;//Activamos el reloj y el tickin
}
void ConfiguraTimer0(void){//Configuración del Timer0 para generar los retardos de 10ms
  LPC_SC->PCLKSEL0|=(1<<3); //Configuro el CCLK/2 a 50MHz-> 20ns
  LPC_TIM0->MR0=500000-1;//Configuro la interrupción por match del Timer0 de inicio. Hace la interrupcion cada 10ms
	LPC_TIM0->PR=0;//Cada vez que el Prescaler alcance este valor se incrementa en uno el contador del Timer0
	LPC_TIM0->MCR=(1<<0)|(1<<1);//Habilito la interrupción por match y el reseteo
	LPC_TIM0->TCR |= 1<<1;//Resetear Timer0 
	LPC_TIM0->TCR &=~(1<<1); // Volvemos a poner a 0 para eliminar el Reset de la línea anterior
	NVIC_EnableIRQ(TIMER0_IRQn); // Habilitar interrupción Timer0
	LPC_TIM0->TCR |= 1 << 0; // Arrancar el Timer0
}
void ConfiguraTimer1(void){//Configuración del Timer1 para generar las frecuencias de las funciones
  LPC_SC->PCLKSEL0|=(1<<3); //Configuro el CCLK/2 a 50MHz-> 20ns
  LPC_TIM1->MR0=(1000000/frecuencia)-1;//Configuro la interrupción por match del Timer1 que depende del valor de la frecuencia
	LPC_TIM1->PR=0; //Cada vez que el Prescaler alcance este valor se incrementa en uno el contador del Timer1
	LPC_TIM1->MCR=0x3; //Habilito la interrupción por match del MR0 y reseteo por match del MR0
	LPC_TIM1->TCR |= 1<<1;//Resetear Timer1
	LPC_TIM1->TCR &=~(1<<1); // Volvemos a poner a 0 para eliminar el Reset de la línea anterior
	NVIC_EnableIRQ(TIMER1_IRQn); // Habilitar interrupción Timer1
	LPC_TIM1->TCR |= 1 << 0; // Arrancar el Timer 
}
void ConfiguraTimer2(void){//Configuración del Timer1 para generar las frecuencias de las funciones
	LPC_SC->PCONP|= (1<<22); //Conecto el timer 2 a alimentacion
  LPC_SC->PCLKSEL0|=(1<<3); //Configuro el CCLK/2 a 50MHz-> 20ns
	LPC_TIM2->MR0=25000000-1;//Configuro la interrupcion del match 0 del Timer 2 cada segundo
	LPC_TIM2->PR=0; //Cada vez que el Prescaler alcance este valor se incrementa en uno el contador del Timer2
	LPC_TIM2->MCR=0x3; //Habilito la interrupción por match del MR0 y reseteo por match del MR0
	LPC_TIM2->TCR |= 1<<1;//Resetear Timer2
	LPC_TIM2->TCR &=~(1<<1); // Volvemos a poner a 0 para eliminar el Reset de la línea anterior
	NVIC_EnableIRQ(TIMER2_IRQn); // Habilitar interrupción Timer2
	LPC_TIM2->TCR |= 1 << 0; // Arrancar el Timer2 
}
void ConfiguracionEXT(void){
	LPC_PINCON->PINSEL4|=(0x01<<20); //Configuro P2.10 como entrada de interrupcion EXT0
	LPC_PINCON->PINSEL4|=(0x01<<22); //Configuro P2.11 como entrada de interrupcion EXT1
	LPC_PINCON->PINSEL4|=(0x01<<24); //Configuro P2.12 como entrada de interrupcion EXT2
	LPC_PINCON->PINSEL4|=(0x01<<26); //Configuro P2.12 como entrada de interrupcion EXT3
	LPC_SC->EXTMODE|=(0<<0); //EXT0 activa por sensibilidad de nivel, en este caso nivel de subida (Debido a que mi placa no tiene los pulsadores KEY1, KEY2 voy a utilizar pulsadores en el circuito)
	LPC_SC->EXTPOLAR|=(0<<0); //EXT0 activa a nivel bajo
	LPC_SC->EXTMODE|=(0<<1); //EXT1 activa por sensibilidad de nivel (Debido a que mi placa no tiene los pulsadores KEY1, KEY2 voy a utilizar pulsadores en el circuito)
	LPC_SC->EXTPOLAR|=(0<<1); //EXT1 activa a nivel bajo
	LPC_SC->EXTMODE|=(0<<2); //EXT2 activa por sensibilidad de nivel (Debido a que mi placa no tiene los pulsadores KEY1, KEY2 voy a utilizar pulsadores en el circuito)
	LPC_SC->EXTPOLAR|=(0<<2); //EXT2 activa a nivel bajo
	LPC_SC->EXTMODE|=(0<<3); //EXT3 activa por sensibilidad de nivel (Debido a que mi placa no tiene los pulsadores KEY1, KEY2 voy a utilizar pulsadores en el circuito)
	LPC_SC->EXTPOLAR|=(0<<3); //EXT3 activa a nivel bajo
	//Borro los flags de inicio de EINT1-3
	LPC_SC->EXTINT|=(0xF);
	//Hacemos un Clear de las peticiones de interrupcion
	NVIC_ClearPendingIRQ(EINT0_IRQn);
	NVIC_ClearPendingIRQ(EINT1_IRQn);
	NVIC_ClearPendingIRQ(EINT2_IRQn);
	NVIC_ClearPendingIRQ(EINT3_IRQn);
	//Habilitación de las interrupciones
  NVIC_EnableIRQ(EINT0_IRQn);	// Habilitar interrupción EINT0
  NVIC_EnableIRQ(EINT1_IRQn);	// Habilitar interrupción EINT1
	NVIC_EnableIRQ(EINT2_IRQn);	// Habilitar interrupción EINT2
	NVIC_EnableIRQ(EINT3_IRQn);	// Habilitar interrupción EINT3
}
void SysTick_Handler(void){
	static uint8_t tempos1=0;
	static uint8_t seg=0;
	static uint8_t tempos2=0;
	tempos1++;
	tempos2++;
	seg++;
	segundos[1]=seg;
if((hora[0]==alarm1[0])&&(hora[1]==alarm1[1])&&(hora[2]==alarm1[2])&&(hora[3]==alarm1[3])&&(led_alm1==1)&&(temp_alm1==0)){//Si la hora coincide con la alarma 1 programada y la alarma 2 esta activa el sistema emitira la señal
		a1=1;
		frecuencia=180;
		ConfiguraTimer1();
	}
	if((hora[0]==alarm2[0])&&(hora[1]==alarm2[1])&&(hora[2]==alarm2[2])&&(hora[3]==alarm2[3])&&(led_alm2==1)&&(temp_alm2==0)){//Si la hora coincide con la alarma 2 programada y la alarma 2 esta activa el sistema emitira la señal
		a1=2;
	frecuencia=10000;
	ConfiguraTimer1();
	}
	if((hora[3]>2)&&(hora[2]>=0)){
		hora[2]=0;
		hora[3]=0;
	}
	if((hora[3]==2)&&(hora[2]==4)){
		hora[2]=0;
		hora[3]=0;
	}
	if(segundos[1]>9){
		seg=0;
		segundos[1]=0;
		segundos[2]++;
		if(segundos[2]>9){
			segundos[2]=0;
			segundos[3]++;
			if(segundos[3]>5){
				segundos[3]=0;
				hora[0]++;
				if(temp_alm1==1){
					temp_alm1=0;
					}
					if(temp_alm2==1){
					temp_alm2=0;
					}
				if(hora[0]>9){
					hora[0]=0;
					hora[1]++;
					if(hora[1]>5){
						hora[1]=0;
						hora[2]++;
						if(hora[2]>9){
							hora[2]=0;
							hora[3]++;
						}
					}
				}
			}
		}
	}
	  if((temp1[0]!=0)||(temp1[1]!=0)||(temp1[2]!=0)||(temp1[3]!=0)){ //Si cualquiera de los valores del temporizador 1 es distinto de 0 significa que se ha programado el temporizador 1
			if(tempos1>=10){
				tempos1=0;
				temp1[0]--;
				if(temp1[0]+1<=0){
						temp1[0]=9;
						temp1[1]--;
					if(temp1[1]+1<=0){
						temp1[1]=5;
						temp1[2]--;
					if(temp1[2]+1<=0){
							temp1[2]=9;
							temp1[3]--;
						}
					}
				}
			}
			if((temp1[3]<=0)&&(temp1[2]<=0)&&(temp1[1]<=0)&&(temp1[0]<=0)){ //si todos los valores del array del temporizador es 0 significa que el temporador ya ha realizado la cuenta completa y por lo tanto debe emitir la señar senoidal
				temp1[3]=0;
				temp1[2]=0;
				temp1[1]=0;
				temp1[0]=0;
				a1=0;
				frecuencia=2000;
				ConfiguraTimer1();
				}
		}
		if((temp2[0]!=0)||(temp2[1]!=0)||(temp2[2]!=0)||(temp2[3]!=0)){
			if(tempos2>=10){
				tempos2=0;
				temp2[0]--;
			if(temp2[0]+1<=0){
				temp2[0]=9;
				temp2[1]--;
				if(temp2[1]+1<=0){
					temp2[1]=5;
					temp2[2]--;
					if(temp2[2]+1<=0){
						temp2[2]=9;
						temp2[3]--;
					}
				}
			}
		}
		if((temp2[3]==0)&&(temp2[2]==0)&&(temp2[1]==0)&&(temp2[0]==0)){
			temp2[3]=0;
			temp2[2]=0;
			temp2[1]=0;
			temp2[0]=0;
			a1=0;
			frecuencia=4000;
			ConfiguraTimer1();
		}
	}
}
void DAC_Handler(void){
	cont++;
	switch(a1){
			case 0://Selecciono el tipo de funcion, si es 00 función seno
				LPC_DAC->DACR=(FuncionSin[cont]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
			case 1://Selecciono el tipo de funcion, si es 01 función triángulo
				LPC_DAC->DACR=(FuncionTri[cont]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
			case 2://Selecciono el tipo de funcion, si es 10 función cuadrado
				LPC_DAC->DACR=(FuncionCuadrado[cont]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
			case 3: //Selecciono el tipo de funcion, si es 11 función 0V
				LPC_DAC->DACR=(Zero[i]<<6)|(0<<16); //Copiamos el valor digital del voltaje en el registro, también controlo el retardo de cambio de voltaje
				if(cont>=50){ //Si la variable conteo llega a 50 se inicializa a 0 para que comience otra vez
					cont=0;
				}
				break;
		}
}
void TIMER0_IRQHandler(void){
	static uint8_t display=0;
	if((LPC_TIM0->IR & 0x01)==0x01){ //Interrupcion por match del MR0
		LPC_TIM0->IR |= 1<<0; //Borro Flag de interrupcion por MR0
		LPC_GPIO0->FIOPIN =0xF0; //Apago los displays 
		LPC_GPIO1->FIOPIN &=~(1<<27); //Desactivar el punto
		if((LPC_GPIO0->FIOPIN & Switch_alarm1_OnOff)==0){//Si la Alarma 1 esta activada
			LPC_GPIO2->FIOPIN&=~ led_visualiza_Alarma_1; //Enciendo el LED de la Alarma 1	
			led_alm1=1;
		}else{
			LPC_GPIO2->FIOPIN|=led_visualiza_Alarma_1;
			led_alm1=0;
		}		
		if((LPC_GPIO0->FIOPIN & Switch_alarm2_OnOff)==0){ //Si la Alarma 2 esta activada
			LPC_GPIO2->FIOPIN&=~ led_visualiza_Alarma_2; //Enciendo el LED de la Alarma 1	
			led_alm2=1;
		}else{
			LPC_GPIO2->FIOPIN|=led_visualiza_Alarma_2;
			led_alm2=0;
		}
		if((LPC_GPIO2->FIOPIN & Switch_texto_de_aviso)==0){
		//Visualizar HOLA
			visual[0]=HOLA[3];
			visual[1]=HOLA[2];
			visual[2]=HOLA[1];
			visual[3]=HOLA[0];		
		}else{
			if(flagext3==0){
				if((LPC_GPIO2->FIOPIN & Switch_horassegundos)==0){
					visual[0]=segment[hora[0]];			
					visual[1]=segment[hora[1]];			
					visual[2]=segment[hora[2]];			
					visual[3]=segment[hora[3]];
				}else{				
					visual[0]=segment[segundos[0]];			
					visual[1]=segment[segundos[1]];		
					visual[2]=segment[segundos[2]];			
					visual[3]=segment[segundos[3]];	
				}
			}
			if(flagext3==1){		
				visual[0]=segment[alarm1[0]];		
				visual[1]=segment[alarm1[1]];	
				visual[2]=segment[alarm1[2]];	
				visual[3]=segment[alarm1[3]];	
			}	
			if(flagext3==2){		
				visual[0]=segment[alarm2[0]];
				visual[1]=segment[alarm2[1]];
				visual[2]=segment[alarm2[2]];
				visual[3]=segment[alarm2[3]];
			}
			if(flagext3==3){			
				visual[0]=segment[temp1[0]];			
				visual[1]=segment[temp1[1]];			
				visual[2]=segment[temp1[2]];			
				visual[3]=segment[temp1[3]];		
			}		
			if(flagext3==4){			
				visual[0]=segment[temp2[0]];			
				visual[1]=segment[temp2[1]];			
				visual[2]=segment[temp2[2]];			
				visual[3]=segment[temp2[3]];	
			}
		}
	if(flagext0==1){//Si activo la interrupion para la programacion
		seleccion++;//Esta variable se encarga de enceder el display correcto
		if(seleccion>4){ //Si estabamos en el display 4 antes de pulsar el boton me aseguro que en la siguiente pulsacion se eliga el display 1
		seleccion=0; //Pongo a 0 la seleccion del display, es decir, seleccion el primer display
		}
		flagext0=0;
	}
	if((seleccion!=0)){//Si estamos en el modo programación
		if(flagext3==0){
			if((LPC_GPIO2->FIOPIN & Switch_horassegundos)==0){
				hora[0]=cg[0];
				hora[1]=cg[1];
				hora[2]=cg[2];
				hora[3]=cg[3];
				if(hora[2]==4&&hora[3]>=2){
					hora[3]=0;
					hora[2]=(10*cg[3]+cg[2])-4;
				}
			}else{				
				segundos[0]=cg[0];
				segundos[1]=cg[1];
				segundos[2]=cg[2];
				segundos[3]=cg[3];
			}
		}
		if(flagext3==1){
			alarm1[0]=cg[0];
			alarm1[1]=cg[1];
			alarm1[2]=cg[2];
			alarm1[3]=cg[3];
		}
		if(flagext3==2){
			alarm2[0]=cg[0];
			alarm2[1]=cg[1];
			alarm2[2]=cg[2];
			alarm2[3]=cg[3];
		}
		if(flagext3==3){
			temp1[0]=cg[0];
			temp1[1]=cg[1];
			temp1[2]=cg[2];
			temp1[3]=cg[3];
		}
		if(flagext3==4){
			temp2[0]=cg[0];
			temp2[1]=cg[1];
			temp2[2]=cg[2];
			temp2[3]=cg[3];
		}
			visual[0]=segment[cg[0]];
			visual[1]=segment[cg[1]];
			visual[2]=segment[cg[2]];
			visual[3]=segment[cg[3]];
		if(seleccion==1){
			LPC_GPIO1->FIOPIN = (visual[3]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<7); //Enciendo el displays 
		}
		else if(seleccion==2){
			LPC_GPIO1->FIOPIN = (visual[2]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<6); //Enciendo el displays 
		}
		else if(seleccion==3){
			LPC_GPIO1->FIOPIN = (visual[1]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<5); //Enciendo el displays 
		}
		else if(seleccion==4){
			LPC_GPIO1->FIOPIN =(visual[0]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<4); //Enciendo el displays 
		}
	}else{
		if(display==0){
			LPC_GPIO1->FIOPIN = (visual[0]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<4); //Enciendo el displays 
			display=1;
		}
		else if(display==1){
			LPC_GPIO1->FIOPIN=(visual[1]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<5); //Enciendo el displays 
			display=2;
		}
		else if(display==2){
			LPC_GPIO1->FIOPIN=(visual[2]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<6); //Enciendo el displays 
			display=3;
		}
		else if(display==3){
			LPC_GPIO1->FIOPIN=(visual[3]<<20); //Activar para representar el valor que se representa en el display 
			LPC_GPIO0->FIOPIN &=~(1<<7); //Enciendo el displays 
			display=0;
		}
	}
}
}
void TIMER1_IRQHandler(void){ //Se encarga del DAC, segun se modifica la frecuencia se modifica el Match
	if((LPC_TIM1->IR & 0x01) == 0x01){ //Si se produce el Match del Timer0
		LPC_TIM1->IR|=(1<<0); //Borrar flag de interrupción MR0
		DAC_Handler();
	}
}	
void TIMER2_IRQHandler(void){ //Se encarga del DAC, segun se modifica la frecuencia se modifica el Match
	if((LPC_TIM2->IR & 0x01) == 0x01){ //Si se produce el Match del Timer0
		LPC_TIM2->IR|=(1<<0); //Borrar flag de interrupción MR0
		if(a1==1){
			a++;
			if(a>=10){
				a=0;
				a1=3;
				temp_alm1=1;
			}			
		}
		if(a1==2){
			a++;
			if(a>=10){
				a=0;
				a1=3;
				temp_alm2=1;
			}			
		}
		if(a1==0){
			a++;
			if(a>=5){
				a=0;
				a1=3;
			}
		}
	}
}
void EINT0_IRQHandler(void){ //Interrupción para la selección de los DISPLAYS
	delay_1ms(250);
	LPC_SC->EXTINT |= (0<<1);   //Borrar el flag de la EINT1 --> EXTINT.0
	flagext0=1;
}
void EINT1_IRQHandler(void){ //Interrupción incrementar el valor
	delay_1ms(250);
	LPC_SC->EXTINT |= (0<<2);   //Borrar el flag de la EINT1 --> EXTINT.1
	//Si activo el puls_mas incremento en uno el valor que se muestra en el display
	flagext1=1;
	if(flagext1==1){ //Si se pulsa sumar
			if(seleccion==4){
				if(cg[0]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[0]++;
				}else{
					cg[0]=0;
				}
			}
			if(seleccion==3){
				if(cg[1]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[1]++;
				}else{
					cg[1]=0;
				}
			}
			if(seleccion==2){
				if(cg[2]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[2]++;
				}else{
					cg[2]=0;
				}
			}
			if(seleccion==1){
				if(cg[3]<9){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[3]++;
				}else{
					cg[3]=0;
				}
			}
		}
			flagext1=0;//Salgo del if para finalizar la ISR
}
void EINT2_IRQHandler(void){ //Interrupción decrementar el valor
	delay_1ms(250);
	LPC_SC->EXTINT |= (0<<3);   //Borrar el flag de la EINT2 --> EXTINT.2
	//Si activo el puls_menos decremento en uno el valor que se muestra en el display
	flagext2=1;
	if(flagext2==1){
			if(seleccion==4){
				if(cg[0]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[0]--;
				}else{
					cg[0]=9;
				}
			}
			if(seleccion==3){
				if(cg[1]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[1]--;
				}else{
					cg[1]=9;
				}
			}
			if(seleccion==2){
				if(cg[2]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[2]--;
				}else{
					cg[2]=9;
				}	
			}
			if(seleccion==1){
				if(cg[3]>0){//Si el valor previo a pulsar el boton es 9, me aseguro que si pulso el boton el siguiente número sea 0
					cg[3]--;
				}else{
					cg[3]=9;
				}
			}
			flagext2=0;//Salgo del if para finalizar la ISR
	}
}
void EINT3_IRQHandler(void){ //Interrupción por flanco ascendente
	delay_1ms(250);
	LPC_SC->EXTINT |= (0<<4);   //Borrar el flag de la EINT3 --> EXTINT.3
	flagext3++; //Incremento en 1 el valor cada vez que se produce una interrupción si pulso el switch de cambio entre programas
	if(flagext3>4){
		flagext3=0;
	}
	if(flagext3==0){
			LPC_GPIO2->FIOPIN|=led_visualiza_Temporizador_1|led_visualiza_Temporizador_2;//Me aseguro que apago los Leds de los otras funciones estan apagados 
			LPC_GPIO2->FIOPIN&=~ led_visualiza_reloj; //Enciendo el LED del reloj	
			
		}
		if(flagext3==1){
			LPC_GPIO2->FIOPIN|=led_visualiza_reloj|led_visualiza_Temporizador_1|led_visualiza_Temporizador_2;//Me aseguro que apago los Leds de los tipos de funcion	
		}
		if(flagext3==2){
			LPC_GPIO2->FIOPIN|=led_visualiza_reloj|led_visualiza_Temporizador_1|led_visualiza_Temporizador_2;//Me aseguro que apago los Leds de los tipos de funcion
		}
		if(flagext3==3){
			LPC_GPIO2->FIOPIN|=led_visualiza_reloj|led_visualiza_Temporizador_2;//Me aseguro que apago los Leds de los tipos de funcion
			LPC_GPIO2->FIOPIN&=~ led_visualiza_Temporizador_1; //Enciendo el LED del Temporizador 1
		}
		if(flagext3==4){
			LPC_GPIO2->FIOPIN|=led_visualiza_reloj|led_visualiza_Temporizador_1;//Me aseguro que apago los Leds de los tipos de funcion
			LPC_GPIO2->FIOPIN&=~ led_visualiza_Temporizador_2; //Enciendo el LED del Temporizador 2
		}
}
int main(){
  configGPIO(); //Llamada a la función configuración GPIO
	ConfigDAC(); //Llamada a la configuracion del DAC
	Configura_SysTick(); //Llamada a la funcion configuracion del SysTick
	ConfiguraTimer0(); //Llamada a la función configuración Timer0
	ConfiguraTimer1();//Llamada a la función configuración Timer1
	ConfiguraTimer2();//Llamada a la función configuración Timer2	
	ConfiguracionEXT(); //Llamada a la funcion configuracion y habilitacion de las interrupciones
	Funciones();
	while(1);//Bucle principal indefinido
}
