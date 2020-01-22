#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BtnPin    0		// GPIO-Zuordnung
#define LedRot    1
#define LedGrn    2
#define LedBlu    3

#define LowBitMin	0.080	// Timing-Grenzen fpr DCF77-Signal
#define LowBitMax	0.130
#define HighBitMin	0.180
#define HighBitMax	0.230
#define SyncMin		1.800
#define SyncMax		2.200

#define BlockLength 	61	// Sekunden (+ Schaltsekunde)
#define BlockHigh	2	// !!! Minium 2 !!! Datenspeicher für ein Tag (Historik) 
#define GPSArray	1023	// Zeichenpuffer GPS RX
#define MaxTimeDiff	30	// Maximale Abweichtung der Systemzeit 
#define Diff		5	// Maximale Abweichtung zwischen DCF77 und GPS-Zeiten
#define DelaySetTime	300	// Verzögerung der Zeitkorrektur in Sekunden, bis alle Zeiten erfasst sind - Minimum 5 Minuten

// Struktur um Zeitdaten zu transportieren
struct Zeitstempel
{
	int Stunde;
	int Minute;
	int Sekunde;
	int Tag;
	int Wochentag;
	int Monat;
	int Jahr;
	int Status;
	int Schaltsekunde;
	int UmschaltungZeitZone;
	int ZeitZone;
	double Breitengrad;
	double Laengengrad;
	char AusrichtungB;
	char AusrichtungL;
};

// Bit-Decoder
int syncbit     = 0;	// Für LED-Steuerung
int Bitaktuell  = 0;
int Bit	        = 0;
int init	= 0;
int DCFSekunde  = 0;
int runClockcnt = 0;
int runArraycnt = 0;

double Block[BlockHigh][BlockLength];	// Array der Zeitmessung
char Daten[BlockHigh][BlockLength];	// Array der Decoder-Logik

// Debug-Information für Entwicklung
// 0 = Auslieferung - Keine Informationen
// 1 = Zeitmessung direkt (nur DCF77)
// 2 = Protokoll rahmen (DCF77 und GPS)
// 3 = Dekodierte Daten (DCF77 und GPS)
// 4 = Zeitstring (Epche-Time)
int debug = 3;

// Interrupt-Routine - Bei jeder Flanke wird diese Funktion aufgerufen
// In der Funktion wird auf 1 oder 0 geprüft.
// Zeitmessung Impulsstart / Impulsstop = Bitlänge 100ms / 200ms
// Zeitmessung Impulsstart / Impulsstart = Syncrondetektion 2sek= 59te sekunde
void BtnISR(void)
{
	static int Clockcnt;	// Schrittzähler
	static int Arraycnt;	// Historikzähler

	// Bitlänge
	static struct timeval start;
	static struct timeval stop;
	double diff_start;	// Impulslänge
	double diff_stop;
	double diff_bit;

	// Schaltsekunde
	static struct timeval startpause;
	double bit_start;	// Impulslänge
	double bit_stop;
	double diff_pause;
	int n,m;
	
	if(init)	// Einmalige Initialisierung
	{
		Clockcnt = 0;	// Schrittzähler initialisierung
		Arraycnt = 0;	// Historikzähler initialisierung
		gettimeofday(&start,0);
		gettimeofday(&stop,0);
		gettimeofday(&startpause,0);
		init = 0;	// Init aus
		DCFSekunde = 0;
	}

	if(digitalRead(BtnPin)) // Steigende Flanke
	{
		gettimeofday(&start,0);	// Startzeitpunkt - Bitlänge
					// Stopzeitpunkt  - Sync-Erkennung
		
		diff_start = (double)start.tv_sec + ((double)start.tv_usec/1000000.0);
		diff_stop  = (double)startpause.tv_sec + ((double)startpause.tv_usec/1000000.0);
		diff_pause = diff_start - diff_stop;
		if(debug==1)
			printf("--------------Pause %f sek \n",diff_pause);
		startpause.tv_sec = start.tv_sec;
		startpause.tv_usec = start.tv_usec;
		if(DCFSekunde < 60)
			DCFSekunde++;
		// Sync
		if ((diff_pause > SyncMin) & (diff_pause < SyncMax))
		{
			Block[Arraycnt][Clockcnt]=diff_pause;	// Sichern der Werte
			Daten[Arraycnt][Clockcnt]='S';		// Sichern der Logik-Auswertung
			runClockcnt = Clockcnt;			// Übergabe an Decoder
			if(Arraycnt < (BlockHigh - 1))
			{
				runArraycnt = Arraycnt;	// Übergabe an Decoder
				Arraycnt++;
			}
			else
			{
				for(m=1;m < BlockHigh;m++) // Umkopieren der Daten, wenn Array voll
				{
					for(n=0;n <= BlockLength;n++)
					{
						Block[m-1][n]=Block[m][n];	// Nur m muss nach vorne!
						Daten[m-1][n]=Daten[m][n];	// n darf nicht verschoben werden !
					}
				}
				runArraycnt = Arraycnt - 1;	// Nach der Verschiebung muss das Array für den Decoder angepasst werden.
			}
			Clockcnt   = 0;
			DCFSekunde = 0;
			syncbit    = 1;	// Decoderfreigabe - Muss am Ende stehen!
		}
	}
	else	// Fallende Flanke
	{
		gettimeofday(&stop,0);	// Stopzeitpunkt - Bitlänge
		
		bit_start = (double)stop.tv_sec + ((double)stop.tv_usec/1000000.0);
		bit_stop = (double)start.tv_sec + ((double)start.tv_usec/1000000.0);
		diff_bit = bit_start - bit_stop;
		if(debug==1)
			printf("Differenz: %f ms \n",diff_bit*1000.0);
		// Low_bit - 100ms
		if ((diff_bit > LowBitMin) & (diff_bit < LowBitMax))
		{
			Daten[Arraycnt][Clockcnt]='-';	  // Sichern der Logik-Auswertung
			Bit = 0;
		}
		// High_bit - 200ms
		if ((diff_bit > HighBitMin) & (diff_bit < HighBitMax))
		{
			Daten[Arraycnt][Clockcnt]='+';	  // Sichern der Logik-Auswertung
			Bit = 1;
		}
		Block[Arraycnt][Clockcnt]=diff_bit; // Sichern der Zeitwerte
		if(Clockcnt < (BlockLength - 1)) // Begrenzung der Array-länge
			Clockcnt++;
		Bitaktuell = 1;			// Freigabe für LED's
	}
}

// Initialisierung für GPS-Mouse
int gpsmouseinit()
{
	int fd;
	struct termios SerialPortSettings;

	// Seriale-Schnittstelle
	fd=open("/dev/ttyUSB0",O_RDWR | O_NOCTTY | O_NONBLOCK);		// ttyUSB0 wird vom USB-Treiber im System vergeben
	if(fd == -1)
	{
            	printf("\n  Error! in Opening ttyUSB0 ");
            	return(-1);
	}
       	else
       	{
            	printf("\n  ttyUSB0 Opened Successfully ");

		// Lesen der Portparameter
		tcgetattr(fd, &SerialPortSettings);

		// Setze Geschwindigkeit Senden/Empfangen 4800Bd
	 	cfsetispeed(&SerialPortSettings,B4800);
		cfsetospeed(&SerialPortSettings,B4800);

		/* 8N1 Mode */
		SerialPortSettings.c_cflag &= ~PARENB;				// Disables the Parity Enable bit(PARENB),So No Parity
		SerialPortSettings.c_cflag &= ~CSTOPB;				// CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit
		SerialPortSettings.c_cflag &= ~CSIZE;				// Clears the mask for setting the data size
		SerialPortSettings.c_cflag |=  CS8;				// Set the data bits = 8
		SerialPortSettings.c_cflag &= ~CRTSCTS;				// No Hardware flow Control
		SerialPortSettings.c_cflag |= CREAD | CLOCAL;			// Enable receiver,Ignore Modem Control lines
		SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);		// Disable XON/XOFF flow control both i/p and o/p
		SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);	// Non Cannonical mode
		SerialPortSettings.c_oflag &= ~OPOST;				// No Output Processing

		// Schreiben der Parameter
		if((tcsetattr(fd,TCSANOW,&SerialPortSettings)) != 0)
		{
		    printf("\n  ERROR ! Serial-Parameter");
		    close(fd); // Zeiger zurückgeben
		    return(-1);
		}
		else
		{
	            printf("\n  4800 Bd - 8n1 \n");
	            tcflush(fd, TCIFLUSH);  // Löschen von Müll im Speicher
	        }
	}
	return(fd);
}

// GPS-Zeit lesen
int gpsread(int fd,char read_buffer[GPSArray])
{
	static int m;		// Position im Buffer !!! static, weil muss über Funktionsende hinaus erhalten bleiben !!!
	int bytes_read_in  = 0;
	int bytes_read_out = 0;	// Länge der Rückgabe
	char puffer;	// Einzeichen-Puffer

	do
	{
		// Byte für Byte lesen
		bytes_read_in = read(fd,&puffer,1);
		if(bytes_read_in > 0)
		{
			if((puffer >= 0x20) & (m < GPSArray))  // Lesbares Zeichen
			{
				read_buffer[m]=puffer;
				m++;
			}

			if(puffer < 0x20) // Steuerzeichen
			{
				read_buffer[m]=0x00;
				if((read_buffer[0]=='$')&(read_buffer[1]=='G')&(read_buffer[2]=='P')&(read_buffer[3]=='R')&(read_buffer[4]=='M')&(read_buffer[5]=='C'))
					bytes_read_out = m; // Länge des Buffers, wenn gewünschtes Paket gefunden wurde.
				else
					bytes_read_out = 0; // Wenn Müll im Buffer steht, wird eine 0 gesendet. -> Ignorieren
				m = 0;
			}
		}
	}while(bytes_read_in < 1);

	return(bytes_read_out);
}

// Zeichenfolge zerlegen (Zeichen ausschneiden)
int strcut(char StrIn[],char StrOut[],int start,int stop)
{
	int n = 0; // Laufzeitvariablen
	int m = 0;
	
	for(n=start,m=0;n<=stop;n++,m++)
		StrOut[m] = StrIn[n];
	StrOut[m]=0x0;
}

// GPS-Decode $GPRMC
int gpsdecodeGPRMC(char gpszeit[],int length,struct Zeitstempel *data)
{
	int n = 0;	// temporäre Laufvariablen
	int m = 0;
	int pos = 0;	// Aktueller Wert im String
	int len =0;	
	unsigned char checksum = 0;
	char temp1str[12] = {0,0,0,0,0,0,0,0,0,0,0,0};	// Temproräre Zwischenspeicher

	char Zeit[11] = {0,0,0,0,0,0,0,0,0,0,0};	// Datenarray für die Datenübertragung aus dem String
	char Status = 0;
	char Breite[10] = {0,0,0,0,0,0,0,0,0,0};
	char AusrichtB=0;
	char Laenge[11] = {0,0,0,0,0,0,0,0,0,0,0};
	char AusrichtL=0;
	char Speed[7] = {0,0,0,0,0,0,0};
	char Dir[7] = {0,0,0,0,0,0,0};
	char Datum[7] = {0,0,0,0,0,0,0};
	char Check[3] = {0,0,0};
		
	// Berechnung der Checksumme (zwischen $ und *)
	checksum = gpszeit[1]; // Erstes Zeichen
	for (n=2;(n < length);n++)
	{
		if(gpszeit[n]=='*')
			n=length; // Ende der Schleife
		else
			checksum=checksum^gpszeit[n]; // Addiere (Xor) Zeichen dazu
	}
	if(debug==2)
		printf("Checksumme berechnet: %02X \n",checksum);
	
	// Zerlegen des Stringes in einzelne Werte. Mit Komma getrennt, variable Längen.
	// Nur $GPRMC hat die ganzen Daten
	if((gpszeit[0]=='$')&(gpszeit[1]=='G')&(gpszeit[2]=='P')&(gpszeit[3]=='R')&(gpszeit[4]=='M')&(gpszeit[5]=='C'))
	{
		pos=0;
		for(n=6;n<length;n++) // erstes Feld nach Kennung
		{
			if(gpszeit[n] == ',') // Feldtrenner
			{
				pos++;
				m = 0;
			}
			else
			{
				switch(pos)
				{
					case 1: Zeit[m] = gpszeit[n]; 	//Uhrzeit (UTC)
						len = strlen(Zeit);
						break;
			
					case 2: Status = gpszeit[n]; 	//Status
						len = 0;
						break;
						
					case 3: Breite[m] = gpszeit[n];	//Breitengrad
						len = strlen(Breite);
						break;
						
					case 4: AusrichtB = gpszeit[n];	//Ausrichtung N/S
						len = 0;
						break;
						
					case 5: Laenge[m] = gpszeit[n];	//Längengrad
						len = strlen(Laenge);
						break;
						
					case 6: AusrichtL = gpszeit[n];	//Ausrichtung O/W
						len = 0;
						break;
						
					case 7: Speed[m] = gpszeit[n]; 	//Geschwindigkeit (Knoten)
						len = strlen(Speed);
						break;
						
					case 8: Dir[m] = gpszeit[n]; 	//Richtung (Nordweisend)
						len = strlen(Dir);
						break;
						
					case 9: Datum[m] = gpszeit[n]; 	//Datum
						len = strlen(Datum);
						break;
						
					default: break;
				}
				if(m < len)
					m++;
			}
		}
		// Auslesen der Checksumme
		pos=1;
		for(n=6;n<length;n++)
		{
			if(gpszeit[n] == '*') // Feldtrenner
			{
				pos++;
				m = 0;
			}
			else
			{
				switch(pos)
				{
					case 1: ; 	//Nichts machen
						len = 0;
						break;

					case 2: Check[m] = gpszeit[n]; 	//Checksumme
						len = strlen(Check);
						break;

					default: break;	
				}
				if(m < len)
					m++;
			}
		}
		if(debug==2) // Kontrolle
		{
			printf("Debug: Zeit %s\n",Zeit);
			printf("Debug: Status %c\n",Status);
			printf("Debug: Breite %s\n",Breite);
			printf("Debug: AusrichtungB %c\n",AusrichtB);
			printf("Debug: Laenge %s\n",Laenge);
			printf("Debug: AusrichtungL %c\n",AusrichtL);
			printf("Debug: Speed %s\n",Speed);
			printf("Debug: Richtung %s\n",Dir);
			printf("Debug: Datum %s\n",Datum);
			printf("Debug: Checksumme gelesen %s\n",Check);
		}
	}
	// Vergleich der Checksummen
	//	Hexadezimal Zahl = strtol(Check,NULL,16);
	//	Dezimal zahl     = strtol(Check,NULL,10);
	if ((unsigned char)(strtol(Check,NULL,16)) == (unsigned char)(checksum))
	{
		strcut(Zeit,temp1str,0,1);	// Ausschneiden der Stunden
		data->Stunde = atoi(temp1str);	// Integerwert zuweisen
		strcut(Zeit,temp1str,2,3);	// Minuten
		data->Minute = atoi(temp1str);
		strcut(Zeit,temp1str,4,5);	// Sekunden
		data->Sekunde = atoi(temp1str);
		strcut(Datum,temp1str,0,1);	// Tag
		data->Tag = atoi(temp1str);
		strcut(Datum,temp1str,2,3);	// Monat
		data->Monat = atoi(temp1str);
		strcut(Datum,temp1str,4,5);	// Jahr
		data->Jahr = atoi(temp1str);

		if(Status == 'A')		// Fehlercode
			data->Status = 0;	// 0 = Alles ok
		else if(Status == 'V')		// 1 = Daten da, aber evtl. Fehlerhaft
			data->Status = 1;
		else				// -1 = Keine Daten
			data->Status = -1;

		strcut(Breite,temp1str,0,1);	// Breitengrad (Ganze Grad)
		data->Breitengrad = atof(temp1str);
		strcut(Breite,temp1str,2,3);	// Breitengrad (Minuten)
		data->Breitengrad = data->Breitengrad + atof(temp1str)/60;	// Umrechnen der Minuten in Dezimal
		strcut(Breite,temp1str,4,8);	// Breitengrad (Sekunden)
		data->Breitengrad = data->Breitengrad + atof(temp1str)/60;	// Umrechnug der Sekunden in Dezimal
		strcut(Laenge,temp1str,0,2);	// Längengrad (Ganze Grad)
		data->Laengengrad = atof(temp1str);
		strcut(Laenge,temp1str,3,4);	// Längengrad (Minuten)
		data->Laengengrad = data->Laengengrad + atof(temp1str)/60;	// Umrechnen der Minuten in Dezimal
		strcut(Laenge,temp1str,5,9);	// Längengrad (Sekunden)
		data->Laengengrad = data->Laengengrad + atof(temp1str)/60;	// Umrechnen der Sekunden in Dezimal
		data->AusrichtungB = AusrichtB;	// Nord / Süd
		data->AusrichtungL = AusrichtL;	// Ost / West
		data->ZeitZone = 0;

		// Leere Felder - Dummy Werte
		data->Wochentag = -1;
		data->Schaltsekunde = -1;
		data->UmschaltungZeitZone = -1;
		
		if(debug == 2)
			printf("Checksumme OK!\n");
		return(0);
	}
	else
	{
		if(debug == 2)
			printf("Checksum Nicht OK!\n");
		return(1);
	}
}

// Decoderfunktion für das DCF77-Protokoll
int dcf77decode(int localClockcnt,int localArraycnt,struct Zeitstempel *data)
{
	int n=0;	// lokaler Zähler
	int m=0;
	int parityMin = 0;
	int parityStd = 0;
	int parityDat = 0;
	int ParityOK = 0;
	int BCDArray[] = {1,2,4,8,10,20,40,80};

	if(debug==2)	// Anzeige der Debug-Information
	{
		printf("\nClockcnt: %d : Arraycnt %d\n",localClockcnt,localArraycnt); // Anzeige der Schritte
		for(n=0;n<=localClockcnt;n++)		// Array der Daten-Bits
		{
			printf(" Bit %2d : (%c)%f\n",n,Daten[localArraycnt][n],Block[localArraycnt][n]);
		}
	}
	// Checksumme berechnen
	ParityOK = 0;
	if((Daten[localArraycnt][0]=='-') & (Daten[localArraycnt][20]=='+') & (localClockcnt >= 59)) // Rahmenkontrolle
	{
		parityMin=0;
		for(n=21,m=0;n<=28;n++)	// Parity Minute
			if(Daten[localArraycnt][n] == '+')	// Zählen der 1-Symbole
				m++;
		if(m%2 == 0)		// Parity ergänzt auf Grade Anzahl
			parityMin=1;

		parityStd=0;
		for(n=29,m=0;n<=35;n++)	// Parity Stunde
			if(Daten[localArraycnt][n] == '+')	// Zählen der 1-Symbole
				m++;
		if(m%2 == 0)		// Parity ergänzt auf Grade Anzahl
			parityStd=1;

		parityDat=0;
		for(n=36,m=0;n<=58;n++)	// Parity Datum
			if(Daten[localArraycnt][n] == '+')	// Zählen der 1-Symbole
				m++;
		if(m%2 == 0)		// Parity ergänzt auf Grade Anzahl
			parityDat=1;
			
		if((parityMin)&(parityStd)&(parityDat))	// Gesamtauswertung Parity
			ParityOK = 1;
	}
	
	if(ParityOK) // Wenn Parity OK, dann zerlegen der Datenstruktur
	{
		data->Stunde = 0;				// Stunde
		for(n=29,m=0;n<=34;n++,m++)
			if(Daten[localArraycnt][n] == '+')
				data->Stunde += BCDArray[m];
		data->Minute = 0;				// Minute
		for(n=21,m=0;n<=27;n++,m++)
			if(Daten[localArraycnt][n] == '+')
				data->Minute += BCDArray[m];
		data->Sekunde = 0;				//Sekunde - Wird nicht übertragen
		data->Tag = 0;					// Tag
		for(n=36,m=0;n<=41;n++,m++)
			if(Daten[localArraycnt][n] == '+')
				data->Tag += BCDArray[m];
		data->Wochentag = 0;				// Wochentag Sontag = 0
		for(n=42,m=0;n<=44;n++,m++)
			if(Daten[localArraycnt][n] == '+')
				data->Wochentag += BCDArray[m];
		data->Monat = 0;				// Monat
		for(n=45,m=0;n<=49;n++,m++)
			if(Daten[localArraycnt][n] == '+')
				data->Monat += BCDArray[m];
		data->Jahr = 0;					// Jahr
		for(n=50,m=0;n<=57;n++,m++)
			if(Daten[localArraycnt][n] == '+')
				data->Jahr += BCDArray[m];

		if(Daten[localArraycnt][15] == '-')			// Fehlermeldung -> Reserve-System aktiv
			data->Status = 0;
		else
			data->Status = 1; // Störung
			
		if(Daten[localArraycnt][16] == '+')
			data->UmschaltungZeitZone = 1;	// In dieser Stunde Zeitzonenwechsel
		else
			data->UmschaltungZeitZone = 0;

		if((Daten[localArraycnt][17] == '-') & (Daten[localArraycnt][18] == '+'))
			data->ZeitZone = 1; //MEZ
		else if ((Daten[localArraycnt][17] == '+') & (Daten[localArraycnt][18] == '-'))
			data->ZeitZone = 2; //MESZ
		else
			data->Status = -1; // Störung

		if(Daten[localArraycnt][19] == '+')
			data->Schaltsekunde = 1;	// In dieser Stunde Schaltsekunde (61.te Sekunde)
		else
			data->Schaltsekunde = 0;

		// Leere Felder Dummy Werte
		data->Breitengrad = -1.0;
		data->Laengengrad = -1.0;
		data->AusrichtungB = 'Z';
		data->AusrichtungL = 'Z';

		if(debug == 2)
			printf("Checksumme OK!\n");
		return(0);
	}
	else
	{
		if(debug == 2)
			printf("Checksum Nicht OK!\n");
		return(1);
	}
}

// Hauptfunktion
int main(void)
{
	int gpsinitstatus = 0;	// GPS-Mouse aktiv =1
	int gpsstatus = 0;	// GPS gültig =1
	int dcfstatus = 0;	// DCF77 gültig =1
	int update = 0;		// Sekundenschalter für Display-Information
	int n = 0;		// lokaler Zähler
	int m = 0;
	int PRGabbruch = 1;	// Schleife wird beendet, wegen groben Fehler. 0 bei Fehler

	struct tm timeset;	// Zum Erstellen der Epoch-Zeiten für Systemzeitkorrektur
	unsigned long temptime1;
	unsigned long temptime2;
	time_t set_of_time_dcf; // Zeitcodes 
	time_t set_of_time_gps;
	time_t set_of_time;
	struct timeval settime;	// Uhrzeit zum Schreiben ins System
	int timesetreturn;	// Rückmeldung vom Zeitsetz-Mechanismus	
		
	time_t akttime;		// Systemzeit auslesen
	time_t baktime;
	time_t startzeit;	// Systemstart

	struct Zeitstempel DCFData; 	// Struktur der Decoderdaten
	struct Zeitstempel GPSData;

	int fdserial;		// Dateizeiger zur USB-GPS-Mouse
	char gpschararray[GPSArray]; 	// RX_buffer GPS
	int gpsrxcnt = 0;		// Anzahl der RX-Daten
	
// Fehler, wenn keine WiringPi-Lib vorhanden ist
	if(wiringPiSetup() == -1)
	{
		printf("setup wiringPi failed !\n");
		return(1); 
	}

// GPIO-Ports konfigurieren	
	pinMode(BtnPin,INPUT);		// GPIO - Auf Input für DCF-Rx
	pullUpDnControl(BtnPin,PUD_OFF);	// GPIO ohne Pull-Up/-Down

	pinMode(LedRot, OUTPUT);	// LED-Ausgänge
	pinMode(LedGrn, OUTPUT);
	pinMode(LedBlu, OUTPUT);
	digitalWrite(LedRot,0);		// LED-Ausgänge Ausschalten (Sicherheitshalber)
	digitalWrite(LedGrn,0);
	digitalWrite(LedBlu,0);

// Erste Initialierung
	runClockcnt = 0;
	runArraycnt = 0;
	DCFSekunde  = 0;
	
	GPSData.Stunde = 0;
	GPSData.Minute = 0;
	GPSData.Sekunde =0;
	GPSData.Tag = 1;
	GPSData.Wochentag = -1;
	GPSData.Monat = 1;
	GPSData.Jahr = 70;
	GPSData.Status = -1;
	GPSData.Schaltsekunde = -1;
	GPSData.UmschaltungZeitZone = -1;
	GPSData.Breitengrad = -1;
	GPSData.Laengengrad = -1;
	GPSData.AusrichtungB = 'Z';
	GPSData.AusrichtungL = 'Z';
	GPSData.ZeitZone = 0;

	DCFData.Stunde = 0;
	DCFData.Minute = 0;
	DCFData.Sekunde = 0;
	DCFData.Tag = 1;
	DCFData.Wochentag = -1;
	DCFData.Monat = 1;
	DCFData.Jahr = 70;
	DCFData.Status = -1;
	DCFData.Schaltsekunde = -1;
	DCFData.UmschaltungZeitZone = -1;
	DCFData.Breitengrad = -1;
	DCFData.Laengengrad = -1;
	DCFData.AusrichtungB = 'Z';
	DCFData.AusrichtungL = 'Z';
	DCFData.ZeitZone = 0;

	akttime = 0;
	baktime = 0;
	fdserial = -1;
	PRGabbruch = 1;
	init = 1;	// Einmaliger Initialisierunglauf

	// DatenArray löschen
	for(m=0;m < BlockHigh;m++)
		for(n=0;n < BlockLength;n++)
			Block[BlockHigh][BlockLength] = 0x0;	// Array der Zeitmessung
			
	for(m=0;m < BlockHigh;m++)
		for(n=0;n < BlockLength;n++)
			Daten[BlockHigh][BlockLength] = 0x0;	// Array der Decoder-Logik

	for(n=0;n<GPSArray;n++)
		gpschararray[GPSArray] = 0x0; 				// RX_buffer GPS
	
// Installieren der Interrupt-Routine
	if(wiringPiISR(BtnPin, INT_EDGE_BOTH, BtnISR))
	{
		printf("setup ISR failed !\n");
		return(1);
	}

//Serial-Schnittstelle (USB-GPS-Mouse) aktivieren
	fdserial=gpsmouseinit();
	if(fdserial < 0)
	{
		printf("GPS not OK!\n");
		gpsinitstatus=0;
	}
	else
	{
		gpsinitstatus=1;
	}

	time(&akttime);
	time(&startzeit);

//Hauptprogramm
	while(PRGabbruch)
	{
		// Systemzeit - Verzögerung der Anzeige 
		if (baktime != akttime)
		{
			baktime = akttime;
			update = 1;
		}
		else
			update = 0;
		time(&akttime);

// LED Steuerung für DCF77 Empfang 
		if(Bitaktuell)	// Wenn ein Biterkannt wurde
		{
			if(Bit)	// 1
			{
				digitalWrite(LedGrn,0);
				digitalWrite(LedBlu,0);
				digitalWrite(LedRot,1);
				Bit=-1;	// Bit-Zurücksetzen
			}
			else	// 0
			{
				digitalWrite(LedRot,0);
				digitalWrite(LedBlu,0);
				digitalWrite(LedGrn,1);
				Bit=-1;	// Bit-Zurücksetzen
			}
			Bitaktuell=0;	// Biterkennung-Zurücksetzen
		}
		if(syncbit) //Hier ist Sekunde 0 ! (Sekunde 60 im Array)
		{
			digitalWrite(LedBlu,1);
			digitalWrite(LedRot,0);
			digitalWrite(LedGrn,0);
// DCF77 Decodierung Datenpaket
			if(dcf77decode(runClockcnt,runArraycnt,&DCFData)) // Decodieren der Datenpakte
			{
				printf("DCF77 Decoderfehler\n");
				dcfstatus = 0;
			}
			else
				dcfstatus = 1;	// Daten sind gültig
			syncbit=0;				// Bit-Zurücksetzen
		}

// GPSZeit lesen
		if(gpsinitstatus > 0)
		{
			gpsrxcnt = gpsread(fdserial,gpschararray);
			if(gpsrxcnt > 0) // Wenn Daten da sind
			{
				if(debug==2)
					printf("Debug:%s\n",gpschararray);
				if(gpsdecodeGPRMC(gpschararray,gpsrxcnt,&GPSData)) // Decoder
				{
					printf("GPS Decoderfehler\n");
					gpsstatus = 0;
				}
				else
					gpsstatus = 1; // Daten sind gültig
			}
		}
		else
			gpsstatus = 0;

// Debuginformation Zusammenfasseung Zeit-Decoder
		if((debug == 3) & (update))
		{
			if(gpsstatus)
			{
				printf("-----------GPS------------\n");
				printf("Stunde: %d \n",GPSData.Stunde);
				printf("Minute: %d \n",GPSData.Minute);
				printf("Sekunde: %d \n",GPSData.Sekunde);
				printf("Tag: %d \n",GPSData.Tag);
				printf("Wochentag: %d \n",GPSData.Wochentag);
				printf("Monat: %d \n",GPSData.Monat);
				printf("Jahr: %d \n",GPSData.Jahr);
				printf("Status: %d \n",GPSData.Status);
				printf("Schaltsekunde: %d \n",GPSData.Schaltsekunde);
				printf("UmschaltungZeitZone: %d \n",GPSData.UmschaltungZeitZone);
				printf("Breite: %f \n",GPSData.Breitengrad);
				printf("Länge : %f \n",GPSData.Laengengrad);
				printf("RichtungBreite: %c \n",GPSData.AusrichtungB);
				printf("RichtungLänge : %c \n",GPSData.AusrichtungL);
				printf("Zeitzone: %d \n",GPSData.ZeitZone);
			}
			if(dcfstatus)
			{
				printf("-----------DCF------------\n");
				printf("Stunde: %d \n",DCFData.Stunde);
				printf("Minute: %d \n",DCFData.Minute);
				printf("Sekunde gerechnet: %d \n",DCFSekunde);
				printf("Tag: %d \n",DCFData.Tag);
				printf("Wochentag: %d \n",DCFData.Wochentag);
				printf("Monat: %d \n",DCFData.Monat);
				printf("Jahr: %d \n",DCFData.Jahr);
				printf("Status: %d \n",DCFData.Status);
				printf("Schaltsekunde: %d \n",DCFData.Schaltsekunde);
				printf("UmschaltungZeitZone: %d \n",DCFData.UmschaltungZeitZone);
				printf("Breite: %f \n",DCFData.Breitengrad);
				printf("Länge : %f \n",DCFData.Laengengrad);
				printf("RichtungBreite: %c \n",DCFData.AusrichtungB);
				printf("RichtungLänge : %c \n",DCFData.AusrichtungL);
				printf("Zeitzone: %d \n",DCFData.ZeitZone);
			}
			printf("--------------------------\n");
			printf("Zeitanzeige System: %ld -> %s",akttime,ctime(&akttime));
			printf("--------------------------\n");
		}
		
// Setzen der Zeitinformation in das System
		if((update) & (akttime > startzeit + DelaySetTime))
		{
			timeset.tm_sec   = GPSData.Sekunde;
			timeset.tm_min   = GPSData.Minute;
			timeset.tm_hour  = GPSData.Stunde;
			timeset.tm_mday  = GPSData.Tag;
			timeset.tm_mon   = GPSData.Monat-1;
			timeset.tm_year  = (GPSData.Jahr+2000)-1900;
			timeset.tm_isdst = 0;        // Daylightsaving / Sommerzeit nicht relevant
			set_of_time_gps = mktime(&timeset);

			timeset.tm_sec = DCFSekunde;
			timeset.tm_min = DCFData.Minute;
			timeset.tm_hour = DCFData.Stunde;
			timeset.tm_mday = DCFData.Tag;
			timeset.tm_mon = (DCFData.Monat-1);	// Monat ist von 0 - 11 definiert.
			timeset.tm_year = (DCFData.Jahr+2000)-1900;
			timeset.tm_isdst = 0;        // Daylightsaving / Sommerzeit nicht relevant
			set_of_time = mktime(&timeset);
			set_of_time_dcf = set_of_time - (DCFData.ZeitZone * 3600); // Zeitzonen Rückrechnung MEZ/MESZ -> UTC

			if(debug==4)
			{
				printf("Aktuelles System: %ld\n",akttime);
				printf("Empfangen GPS: %ld \n",set_of_time_gps);
				printf("Empfangen DCF: %ld \n",set_of_time_dcf);
			}
			
			if((abs(akttime - set_of_time_dcf) > MaxTimeDiff) | (abs(akttime - set_of_time_gps) > MaxTimeDiff))
			{
				if((gpsstatus) & (dcfstatus))
				{
					if((GPSData.Status == 0) & (DCFData.Status == 0))	// Beide Datensätze sind gültig
					{
						temptime1=set_of_time_dcf;
						temptime2=set_of_time_gps;
						if(abs(temptime1-temptime2) < Diff)		// Wenn Differenz klein, dann DCF-Zeit
						{
							timerclear(&settime);
							settime.tv_sec=set_of_time_dcf;
							timesetreturn = settimeofday(&settime,0);
						}
						else 
						{	
							printf("\n Fehler: Zeitdifferenz zwischen beiden Quellen zu gross");
						}
					}
					else if((GPSData.Status > 0) & (DCFData.Status > 0))	// Wenn DCF und GPS Fehler haben, aber Zeit da ist
					{
						printf("\n Fehler: Beide Quellen melden Einschränkungen!");
						temptime1=set_of_time_dcf;
						temptime2=set_of_time_gps;
						if(abs(temptime1-temptime2) < Diff)		// Wenn Differenz klein, dann DCF-Zeit
						{
							timerclear(&settime);
							settime.tv_sec=set_of_time_dcf;
							timesetreturn = settimeofday(&settime,0);
						}
						else 
						{	
							printf("\n Fehler: Zeitdifferenz zwischen beiden Quellen zu gross");
						}
					}
					else if((GPSData.Status == 0) & (DCFData.Status != 0))	// Nur GPS ist Gültig
					{
						printf("\n Fehler: DCF77 Signal meldet Einschänkungen!");
						timerclear(&settime);
						settime.tv_sec=set_of_time_gps;
						timesetreturn = settimeofday(&settime,0);
					}
					else if((DCFData.Status == 0) & (GPSData.Status != 0))	// Nur DCF ist Gültig
					{
						printf("\n Fehler: GPS Signal meldet Einschänkungen!");
						timerclear(&settime);
						settime.tv_sec=set_of_time_dcf;
						timesetreturn = settimeofday(&settime,0);
					}
					else
						printf("\n Fehler: Keine gültige Uhrzeit empfangbar!");
					
				}
				else if((gpsstatus) & (! dcfstatus))	// Nur GPS verfügbar
					if(GPSData.Status == 0)
					{
						printf("\n Fehler: DCF77 nicht aktiv!");
						timerclear(&settime);
						settime.tv_sec=set_of_time_gps;
						timesetreturn = settimeofday(&settime,0);
					}
				else if((dcfstatus) & (! gpsstatus))	// Nur DCF77 verfügbar
					if(DCFData.Status == 0)
					{
						printf("\n Fehler: GPS nicht aktiv!");
						timerclear(&settime);
						settime.tv_sec=set_of_time_dcf;
						timesetreturn = settimeofday(&settime,0);
					}
				else
					printf("\n Fehler: Keine Uhrzeit empfangbar!");

				if(timesetreturn)
				{
					printf("\n\n Fehler: Systemzeit lässt sich nicht setzen! \n");
					printf("  ----  Root-Rechte ? ---- \n\n");
					PRGabbruch = 0;		// Abbruch des Programmes
				}
				else
				{
					printf("\n Aktuelle Systemzeit %s",ctime(&akttime));	// Anzeige, das Zeit geändert wurde
					time(&akttime);
					baktime = akttime;
					startzeit = akttime - DelaySetTime -1; // Damit die Versögerung nicht noch einmal greift.
					printf(" - neue Systemzeit: %s\n",ctime(&akttime));
				}
			}
		}

		delay(3); // Entlastung der CPU
		
// Aufräumen nach dem Durchlauf
		digitalWrite(LedBlu,0);	// LED-Ausschalten, wegen Impulsanzeige	
		digitalWrite(LedRot,0);
		digitalWrite(LedGrn,0);
	}
// Bei Programmende
	digitalWrite(LedBlu,0);	// LED-Ausschalten
	digitalWrite(LedRot,0);
	digitalWrite(LedGrn,0);

	close(fdserial);
	return -1;
}
