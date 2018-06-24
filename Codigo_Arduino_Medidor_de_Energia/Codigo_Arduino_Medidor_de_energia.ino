#include <SPI.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>


#include "Arduino.h"
#ifndef LE_TENSAO_CALIBRATION_CONST
#define LE_TENSAO_CALIBRATION_CONST 1126400L
#endif
#if defined(__arm__)
#define ADC_BITS    12
#else
#define ADC_BITS    10
#endif
#define ADC_COUNTS  (1<<ADC_BITS)



class Dados
{
  public:

    void gettensao(unsigned int _inPinV, double _VCAL, double _PHASECAL);
    void getcorrente(unsigned int _inPinI, double _ICAL);

    void settensaoTX(double _VCAL, double _PHASECAL);
    void setcorrenteTX(unsigned int _channel, double _ICAL);

    void calculo_Potencia(unsigned int crossings, unsigned int timeout);
    double calc_corrente_rms(unsigned int NUMBER_OF_SAMPLES);

    long le_tensao();
    //Useful value variables
    double realPower,
           apparentPower,
           powerFactor,
           Vrms,
           Irms;

  private:

    //Ajuste os pinos de entrada da corrente Tensao e corrente
    unsigned int inPinV;
    unsigned int inPinI;
    //Coeficientes de calibração
    //Estes precisam ser definidos para obter resultados precisos
    double VCAL;
    double ICAL;
    double PHASECAL;

    //--------------------------------------------------------------------------------------
    // Declaração de variáveis para o procedimento emon_calc
    //--------------------------------------------------------------------------------------
    int sampleV;                        //sample_ detém o valor de leitura analógico bruto
    int sampleI;

    double lastFilteredV, filteredV;         //Filtered_ é o valor analógico bruto menos o deslocamento DC
    double filteredI;
    double offsetV;                          //Saída de filtro de baixa passagem
    double offsetI;                          //Saída de filtro de baixa passagem

    double phaseShiftedV;                             //Segura a tensao de fase de calibração.

    double sqV, sumV, sqI, sumI, instP, sumP;         //sq = squared, sum = Soma, inst = instantâneo

    int startV;                                       //Tensao instantânea no início da janela de amostra.

    boolean lastVCross, checkVCross;                  //Usado para medir o número de vezes que o limite é cruzado.


};

void Dados::gettensao(unsigned int _inPinV, double _VCAL, double _PHASECAL)
{
  inPinV = _inPinV;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;
  offsetV = ADC_COUNTS >> 1;
}

void Dados::getcorrente(unsigned int _inPinI, double _ICAL)
{
  inPinI = _inPinI;
  ICAL = _ICAL;
  offsetI = ADC_COUNTS >> 1;
}

//--------------------------------------------------------------------------------------
//Define os pinos a serem usados para sensores de tensão e corrente com base no mapa do pino emontx
//--------------------------------------------------------------------------------------
void Dados::settensaoTX(double _VCAL, double _PHASECAL)
{
  inPinV = 2;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;
  offsetV = ADC_COUNTS >> 1;
}

void Dados::setcorrenteTX(unsigned int _channel, double _ICAL)
{
  if (_channel == 1) inPinI = 3;
  if (_channel == 2) inPinI = 0;
  if (_channel == 3) inPinI = 1;
  ICAL = _ICAL;
  offsetI = ADC_COUNTS >> 1;
}

//--------------------------------------------------------------------------------------
// procedimento emon_calc
// Calcula realPower, apparentPower, powerFactor, Vrms, Irms, incremento de kWh
// De uma janela de amostra da rede AC tensao e corrente.
// O tamanho da janela Sample é definido pelo número de meias-ondas ou cruzamentos que escolhemos medir.
//--------------------------------------------------------------------------------------
void Dados::calculo_Potencia(unsigned int crossings, unsigned int timeout)
{
#if defined emonTxV3
  int SupplyTensao = 3300;
#else
  int SupplyTensao = le_tensao();
#endif

  unsigned int crossCount = 0;                             //Usado para medir o número de vezes que o limite é ultrapassado.
  unsigned int numberOfSamples = 0;                        //Isso agora é incrementado

  //-------------------------------------------------------------------------------------------------------------------------
  //1) Espera que a forma de onda esteja próxima da parte 'zero' (mid-scale adc) na curva sen.
  //-------------------------------------------------------------------------------------------------------------------------
  boolean st = false;                                //um indicador para sair do loop while

  unsigned long start = millis();    // millis () - start garante que não fique preso no loop se houver um erro.

  while (st == false)                                //o loop while...
  {
    startV = analogRead(inPinV);                    //usando a forma de onda tensao
    if ((startV < (ADC_COUNTS * 0.55)) && (startV > (ADC_COUNTS * 0.45))) st = true; //verificando se está dentro do alcance
    if ((millis() - start) > timeout) st = true;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) loop de medição principal
  //-------------------------------------------------------------------------------------------------------------------------
  start = millis();

  while ((crossCount < crossings) && ((millis() - start) < timeout))
  {
    numberOfSamples++;                       //Contar o número de vezes em loop.
    lastFilteredV = filteredV;               //Usado para compensação de atraso / fase

    //-----------------------------------------------------------------------------
    // A) Ler em amostras brutas de tensao e corrente
    //-----------------------------------------------------------------------------
    sampleV = analogRead(inPinV);                 //Leia no sinal de tensao bruta
    sampleI = analogRead(inPinI);                 //Leia em sinal de corrente bruta

    //-----------------------------------------------------------------------------
    // B) Aplicando filtros de passa baixa digital para extrair o deslocamento de 2,5 V ou 1,65 V dc,
    // zerando a contagem
    //-----------------------------------------------------------------------------
    offsetV = offsetV + ((sampleV - offsetV) / 1024);
    filteredV = sampleV - offsetV;
    offsetI = offsetI + ((sampleI - offsetI) / 1024);
    filteredI = sampleI - offsetI;

    //-----------------------------------------------------------------------------
    // C) Método da raiz quadrada média
    //-----------------------------------------------------------------------------
    sqV = filteredV * filteredV;                //1) valores quadrados da tensao
    sumV += sqV;                                //2) soma

    //-----------------------------------------------------------------------------
    // D) Root-mean-square method corrente
    //-----------------------------------------------------------------------------
    sqI = filteredI * filteredI;                //1)valores da corrente quadrada
    sumI += sqI;                                //2) soma

    //-----------------------------------------------------------------------------
    // E)Calibração de fase
    //-----------------------------------------------------------------------------
    phaseShiftedV = lastFilteredV + PHASECAL * (filteredV - lastFilteredV);

    //-----------------------------------------------------------------------------
    // F) Instantaneous power calc
    //-----------------------------------------------------------------------------
    instP = phaseShiftedV * filteredI;          //Potência Instantânea
    sumP += instP;                              //Soma

    //-----------------------------------------------------------------------------
   // G) Encontre o número de vezes que a tensao cruzou a tensao inicial
   // a cada 2 cruzamentos teremos amostrados 1 comprimento de onda
    // assim este metodo nos permite amostar um numero inteiro de meio comprimento de onda que aumenta a precisao
    //-----------------------------------------------------------------------------
    lastVCross = checkVCross;
    if (sampleV > startV) checkVCross = true;
    else checkVCross = false;
    if (numberOfSamples == 1) lastVCross = checkVCross;

    if (lastVCross != checkVCross) crossCount++;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Cálculos pós-laços
  //-------------------------------------------------------------------------------------------------------------------------
  // Cálculo da raiz da média da tensao e da corrente quadrada (rms)
  // Coeficientes de calibracao aplicados

  double V_RATIO = VCAL * ((SupplyTensao / 1000.0) / (ADC_COUNTS));
  Vrms = V_RATIO * sqrt(sumV / numberOfSamples);

  double I_RATIO = ICAL * ((SupplyTensao / 1000.0) / (ADC_COUNTS));
  Irms = I_RATIO * sqrt(sumI / numberOfSamples);

  //Valores de potência de cálculo
  realPower = V_RATIO * I_RATIO * sumP / numberOfSamples;
  apparentPower = Vrms * Irms;
  powerFactor = realPower / apparentPower;

  //Redefinir acumuladores
  sumV = 0;
  sumI = 0;
  sumP = 0;
  //--------------------------------------------------------------------------------------
}

//--------------------------------------------------------------------------------------
double Dados::calc_corrente_rms(unsigned int Number_of_Samples)
{

#if defined emonTxV3
  int SupplyTensao = 3300;
#else
  int SupplyTensao = le_tensao();
#endif


  for (unsigned int n = 0; n < Number_of_Samples; n++)
  {
    sampleI = analogRead(inPinI);

    // Filtro passa baixa digital extrai o offset de 2,5 V ou 1,65 V dc,
    // então subtraia isso - o sinal está agora centrado em 0 contagens.
    offsetI = (offsetI + (sampleI - offsetI) / 1024);
    filteredI = sampleI - offsetI;

    // Corrente do método da raiz quadrada média
    // 1) valores da corrente quadrada
    sqI = filteredI * filteredI;
    // 2) sum
    sumI += sqI;
  }

  double I_RATIO = ICAL * ((SupplyTensao / 1000.0) / (ADC_COUNTS));
  Irms = I_RATIO * sqrt(sumI / Number_of_Samples);

  //Redefinir acumuladores
  sumI = 0;
  //--------------------------------------------------------------------------------------

  return Irms;
}
long Dados::le_tensao() {
  long result;

  //not used on emonTx V3 - as Vcc is always 3.3V - eliminates bandgap error and need for calibration http://harizanov.com/2013/09/thoughts-on-avr-adc-accuracy/

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATmega644__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openDados.org/emon/node/2253#comment-11432
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);

#endif


#if defined(__AVR__)
  delay(2);                                        // Aguarde a Vref resolver
  ADCSRA |= _BV(ADSC);                             // Converte
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  result = LE_TENSAO_CALIBRATION_CONST / result;  //1100mV * 1024 ADC passos http://openDados.org/emon/node/1186
  return result;
#elif defined(__arm__)
  return (3300);                                  //Arduino Due
#else
  return (3300);                                  //Acho que outras arquiteturas não suportadas estarão executando um 3.3V!
#endif
}

Dados emon1;

//Pino do sensor SCT
int pino_sct = 1;
void setup()
{
  Serial.begin(9600);
  //Pino, calibracao - Cur Const= Ratio/BurdenR. 1800/62 = 29.
  emon1.getcorrente(pino_sct, 29);
  emon1.gettensao(2, 234.26, 1.7);
}

void loop()
{
   delay(1000);
  char x = Serial.read();
  if(x=='a'){
    Serial.print(emon1.calc_corrente_rms(1480)); 
    Serial.print(",");
    Serial.println(emon1.calc_corrente_rms(1480));
  }
}
