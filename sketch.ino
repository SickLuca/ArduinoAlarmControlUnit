#define TRIGGER_PIN 9 // Pin per il Trigger
#define ECHO_PIN 8 // Pin per l'Echo
#define BUTTON_PIN 2 // Pin del pulsante
#define BUZZER_PIN 10 // Pin del pulsante

#define BACKLIGHT_PIN 3  // Pin collegato al pin A del display

//Pin del display
#define RS 12
#define E 11
#define D4 4
#define D5 5
#define D6 6
#define D7 7

//Variabile per gestire inserimento allarme
volatile bool alarmSetted = true; //True = Allarme inserito, False = Allarme disattivato
volatile bool alarmExpired = false; //Flag gestita dal timer di 5 minuti 
volatile uint32_t alarmCounter = 0; // Per il timer di 5 minuti

//Variabili per misurazione distanza
volatile bool measurementComplete = false; // Flag per indicare che la misurazione è completa
volatile bool objectDetected = false;

//Variabili per inserimento codice
volatile uint8_t userCode[5] = {-1, -1, -1, -1, -1}; // Numero a 5 cifre
const uint8_t secretCode[5] = {1, 2, 3, 4, 5}; // Combinazione segreta a 5 cifre
volatile uint8_t posizione = 0; // Indice della cifra corrente

// Variabili per la cifra a rotazione
volatile uint8_t cifra = 0; //La cifra che ruota
volatile uint16_t rotationCounter = 0; // Contatore per mezzo secondo

//Variabili per evitare accensione ripetuta display
volatile bool clearDisplay = false; 
volatile uint32_t entryCounter = 0; // Contatore per i 90s dell'inserimento

//Variabili per debouncing pulsante
volatile bool debounceFlag = true; // Flag di debounce
volatile uint8_t debounceCounter = 0; // Contatore debounce

//Variabili per gestione stato sospensione
volatile uint16_t suspendedCounter = 0; //Timer sospensione

// Per l'implementazione di una FSM senza stati finali
//Variabili per stato di benvenuto
volatile uint16_t welcomeCounter = 0;
volatile bool welcomeExpired = false;


// Definizione degli stati
enum State {
    STANDBY,
    CODE_ENTRY,
    CODE_VALIDATION,
    ALARM,
    WELCOME,
    SUSPENDED
};

//Stato iniziale
State currentState = STANDBY;

void setup() {
  sensorSetup();
  lcd_init();
}

void loop() {
 switch (currentState) {
        case STANDBY:
        lcd_displayOff(); //Display spento
          detection(); //Rileva
            if (objectDetected) {
              currentState = CODE_ENTRY; //Transita nello stato CODE_ENTRY
              timer2Setup(); //Resetta il timer2 che gestisce CODE_ENTRY
              resetInserimento(); //Resetta variabili inserimento (efficace passando da ALARM e WELCOME)
              lcd_displayOn();
              buttonSetup(); //Inizializza il pulsante e il timer di debouncing
            }
            break;

        case CODE_ENTRY:
        updateDisplay(); //Aggiorna il display con le nuove cifre
        detection(); //Rileva
        if(!objectDetected) {
          currentState = SUSPENDED; //Transita nello stato SUSPENDED
        }
            break;
        
        case CODE_VALIDATION:
        currentState = WELCOME; //Parte da WELCOME

        for (uint8_t i = 0; i < 5; i++) {
          if (userCode[i] != secretCode[i]) {
            currentState = ALARM; // Se trova una cifra differente transita in ALARM
            clearDisplay = true; //Bisogna pulire il display
            break;
          }
        
        if (currentState == WELCOME) {
          alarmSetted = false; //Se rimane in Welcome disattiva l'allarme
        }
        lcd_command(0x01); // Pulisce il display
        }
            break;

        case WELCOME:
        lcd_command(0x80); // Posiziona il cursore all'inizio
        printWelcome();
        checkWelcomeExpiration(); // Per l'implementazione di una FSM senza stati finali
            break;
        
        case ALARM:
        if (clearDisplay) {
          lcd_command(0x01); // Pulisce il display
          clearDisplay = false;
        }
        lcd_command(0x80); // Posiziona il cursore all'inizio
        printAlarm();

        PINB |= (1 << PINB2); //Attiva il buzzer

        checkAlarmExpiration();
            break;
        
        case SUSPENDED:
        detection();
        if (objectDetected) {
          currentState = CODE_ENTRY; //Transita in CODE_ENTRY_
          suspendedCounter = 0; //Azzera il contatore di sospensione
        }
            break;
  } 
}

void sensorSetup() {
  // Impostazione dei pin 9 (TRIGGER_PIN) e 8 (ECHO_PIN) come output e input usando i registri
  DDRB |= (1 << DDB1);  // TRIGGER_PIN come output (pin 9)
  DDRB &= ~(1 << DDB0); // ECHO_PIN come input (pin 8)

  // Configuriamo il timer 0 in modalità normale con prescaler 8
  TCCR1A = 0;           // Modalità normale
  TCCR1B = (1 << CS11); // Prescaler 8, ogni tick = 0.5 µs
  TCNT1 = 0;            // Reset del contatore

  // Abilitazione dell'interrupt PCINT su pin 8
  PCICR |= (1 << PCIE0);    // Abilita interrupt per il gruppo PCINT0
  PCMSK0 |= (1 << PCINT0);  // Abilita interrupt sul pin 8 (PCINT0)
}

void detection() {
  if (!measurementComplete) {
    // Genera il segnale di Trigger
    PORTB &= ~(1 << PORTB1);  // Imposta TRIGGER_PIN su LOW
    //delayMicrosecondsTimer1(2);
    PORTB |= (1 << PORTB1);   // Imposta TRIGGER_PIN su HIGH
    //delayMicrosecondsTimer1(10);
    PORTB &= ~(1 << PORTB1);  // Riporta TRIGGER_PIN su LOW

    // Aspetta il completamento della misurazione
    while (!measurementComplete) {
      if (TCNT1 >= 47000) { 
        objectDetected = false;
      }
    }
  }
  // Resetta il flag per la prossima misurazione
  measurementComplete = false;
}

/*
void delayMicrosecondsTimer1(uint16_t us) {
  uint16_t targetTicks = (us * 2); // Con prescaler 8: 1 tick = 0.5 µs
  TCNT1 = 0;                       // Resetta il timer
  while (TCNT1 < targetTicks);     // Attendi fino al raggiungimento dei tick desiderati
}
*/


void lcd_init() {
  DDRD |= (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7); // Imposta D3-D7 come OUTPUT
  DDRB |= (1 << PB4) | (1 << PB3); // Imposta RS e E come OUTPUT
}

// Funzione per inviare un comando al display
void lcd_command(byte cmd) {
  PORTB &= ~(1 << PB4); // Imposta RS (pin 12) a LOW (porta B, pin 4)
  lcd_sendNibble(cmd >> 4); // Invia i primi 4 bit
  lcd_sendNibble(cmd & 0x0F); // Invia i secondi 4 bit
}

// Funzione per scrivere un carattere sul display
void lcd_write(byte data) {
  PORTB |= (1 << PB4);  // Imposta RS (pin 12) a HIGH (porta B, pin 4)
  lcd_sendNibble(data >> 4); // Invia i primi 4 bit
  lcd_sendNibble(data & 0x0F); // Invia i secondi 4 bit
}

// Funzione per inviare i dati al display
void lcd_sendNibble(byte data) {
  // Maschera e invia i 4 bit
  //digitalWrite(D4, (data >> 0) & 1); // D4
  PORTD = (PORTD & ~(1 << PD4)) | (((data >> 0) & 1) << PD4);
  //digitalWrite(D5, (data >> 1) & 1); // D5
  PORTD = (PORTD & ~(1 << PD5)) | (((data >> 1) & 1) << PD5);
  //digitalWrite(D6, (data >> 2) & 1); // D6
  PORTD = (PORTD & ~(1 << PD6)) | (((data >> 2) & 1) << PD6);
  //digitalWrite(D7, (data >> 3) & 1); // D7
  PORTD = (PORTD & ~(1 << PD7)) | (((data >> 3) & 1) << PD7);
  
  // Attiva l'abilitazione (E) per 1 ms per inviare il dato
  PORTB |= (1 << PB3);  // Imposta E (pin 11) a HIGH
  PORTB &= ~(1 << PB3);  // Imposta E (pin 11) a LOW
}

void lcd_displayOn() {
  PORTD |= (1 << PORTD3);  // Imposta il bit 3 di PORTD su HIGH, accendi la retroilluminazione
  // Inizializza il display in modalità 4 bit
  lcd_command(0x02); // Imposta modalità 4 bit
  lcd_command(0x28); // 2 linee, 5x8 font
  lcd_command(0x0C); // Display acceso, cursore spento
  lcd_command(0x06); // Incremento del cursore
  lcd_command(0x01); // Pulisce il display
}

void lcd_displayOff() {
  lcd_command(0x08); // Display off, cursore spento
  PORTD &= ~(1 << PORTD3); // Resetta il bit 3 di PORTD su LOW, spegni la retroilluminazione
}

// Configurazione del timer 2
void timer2Setup() {
  TCCR2A = 0;              // Modalità normale
  TCCR2B = (1 << CS22);    // Prescaler 64
  TCNT2 = 0;               // Reset del timer
  OCR2A = 250;             // Con prescaler 64, 1 tick = 4 µs; 250 * 4 µs = 1 ms
  TIMSK2 = (1 << OCIE2A);  // Abilita l'interrupt su confronto
}

void updateDisplay() {
  lcd_command(0x80); // Posiziona il cursore all'inizio della prima riga
  lcd_write('0' + cifra);  // Scrive la cifra sul display
  lcd_command(0xC0); // Posiziona il cursore all'inizio della seconda riga
  for (uint8_t i = 0; i < 5; i++) {
    lcd_write('0' + userCode[i]); //Stampa il codice inserito carattere per carattere.
  } /* '0' = 48, che sta per 0 nella tabella ASCII. Se per caso userCode[i] è ancora -1, stampa
    48-1=47, che nella tabella ASCII è '/' */
}

void buttonSetup() {
  // Configura il pulsante come input con pull-up
  DDRD &= ~(1 << DDD2); // BUTTON_PIN come input
  PORTD |= (1 << PORTD2); // Pull-up abilitato sul BUTTON_PIN

  // Abilita interrupt esterno sul pulsante
  EICRA |= (1 << ISC01); // Falling edge su INT0
  EIMSK |= (1 << INT0);  // Abilita INT0
}

void printWelcome() {
    lcd_write('W'); // Scrive "Benvenuto"
    lcd_write('e');
    lcd_write('l');
    lcd_write('c');
    lcd_write('o');
    lcd_write('m');
    lcd_write('e');
    lcd_write('!');
}

void printAlarm() {
  lcd_write('A'); // Scrive "Allarme!"
  lcd_write('l');
  lcd_write('a');
  lcd_write('r');
  lcd_write('m');
  lcd_write('!');
}

//Per resettare la cifra in rotazione e il codice inserito dall'utente
void resetInserimento() {
  cifra = 0;
  for (uint8_t i = 0; i < 5; i++) {
    userCode[i] = -1;
  }
}

void checkAlarmExpiration() {
  if  (alarmExpired) {
    alarmExpired = false;
    detection();
    if (!objectDetected) {
      currentState = STANDBY;
      PINB &= ~(1 << PINB2);  // Riporta BUZZER_PIN su LOW
      lcd_command(0x01); // Pulisce il display
      resetInserimento();
    }
  }
}

void checkWelcomeExpiration() { 
  if (welcomeExpired) {  // Per l'implementazione di una FSM senza stati finali
    welcomeExpired = false;
    alarmSetted = true;
    lcd_command(0x01); // Pulisce il display
    detection();                    //
      if (objectDetected) {           //
        currentState = CODE_ENTRY;    //  
      } else currentState = STANDBY;  
  }
}

//---------------------------------------ISR---------------------------------------------------

//Gestione cambiamento di stato del pin ECHO_PIN
ISR(PCINT0_vect) {
  if (PINB & (1 << PINB0)) {
    // Rising edge: ECHO_PIN è in ascolto
    TCNT1 = 0; // Resetta il contatore del timer
  } else {
    // Falling edge: ECHO_PIN ha rilevato il segnale
    measurementComplete = true;  // La misurazione è completa
    if (TCNT1 < 47000) { //simuliamo un timeout a circa 340cm, per emulare un comportamento reale
      objectDetected = true;
    }
  }
}

ISR(TIMER2_COMPA_vect) {
    if (currentState == CODE_ENTRY) {
      rotationCounter++; //Incrementa il timer di rotazione
      entryCounter++; //Incrementa il timer di inserimento

      if (rotationCounter == 500) { // 500ms -> 0,5s
        rotationCounter = 0; //Azzera il timer
        cifra = (cifra + 1) % 10; // Incrementa la cifra (da 0 a 9)
      }
      if (entryCounter == 90000) { // 90s
        entryCounter = 0; //Azzera i timer di stato
        rotationCounter = 0;
        currentState = ALARM; //Transita in ALARM
        clearDisplay = true; //Notifica la necessità di pulire il display
      }

      if (!debounceFlag) { //Se possiamo già premere il tasto, evitiamo di ripetere questo codice
        debounceCounter++; 
        if (debounceCounter == 200) { //200ms tempo medio debounce
        debounceCounter = 0;
        debounceFlag = true;  // Segna che è passato il tempo di debounce
        }
      }
    }

    if (currentState == ALARM) {
    alarmCounter++; //Incrementa il timer di allarme
      if (alarmCounter >= 300000) { //300s = 5 minuti
        alarmExpired = true; //Notifica la scadenza dell'allarme
        alarmCounter = 0; // Resetta il contatore
      }
    }
    
    if (currentState == SUSPENDED) {
    suspendedCounter++; //Incrementa il timer di sospensione
      if (suspendedCounter >= 60000) { //60s
        suspendedCounter = 0; // Resetta il contatore
        currentState = STANDBY; //Transita in STANDBY
      }
    }
  //Per l'implementazione di una FSM senza stati finali
    if (currentState == WELCOME) {
      welcomeCounter++;
      if (welcomeCounter == 10000) { //10s, arbitrario
        welcomeExpired = true;
        welcomeCounter = 0;
        rotationCounter = 0;
        entryCounter = 0;
        cifra = 0;
        for (uint8_t i = 0; i < 5; i++) {
          userCode[i] = 0;
        }
      }

    }
}

// ISR per INT0 (interrupt del pulsante, acquisizione cifra e richiesta di validazione)
ISR(INT0_vect) {
  if (debounceFlag && (currentState == CODE_ENTRY)) {  // Verifica se è passato il tempo di debounce e se può acquisire input
    userCode[posizione] = cifra; // Acquisisce la cifra
      debounceFlag = false;  // Resetta il flag di debounce
      debounceCounter = 0; // Azzera il contatore
    if (posizione == 4) { //Se la posizione è la 4, abbiamo riempito l'inserimento del codice
      currentState = CODE_VALIDATION;
    }
  posizione = (posizione + 1) % 5;  // Passa alla cifra successiva
  }
}
