#define BtnPin    0		// GPIO-Zuordnung

#define LowBitMin	0.080	// Timing-Grenzen fpr DCF77-Signal
#define LowBitMax	0.130
#define HighBitMin	0.180
#define HighBitMax	0.230
#define SyncMin		1.800
#define SyncMax		2.200

#define BlockLength 	61	// Sekunden (+ Schaltsekunde)
#define BlockHigh	2	// !!! Minium 2 !!! Datenspeicher für ein Tag (Historik) 

// Bit-Decoder
int syncbit;
int Bitaktuell;
int Bit;
int dcfInterruptInit;
int DCFSekunde;
int runClockcnt;
int runArraycnt;

double Block[BlockHigh][BlockLength];	// Array der Zeitmessung
char Daten[BlockHigh][BlockLength];	// Array der Decoder-Logik

//#############################################################################################################  
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
	
	if(dcfInterruptInit)	// Einmalige Initialisierung
	{
		Clockcnt = 0;	// Schrittzähler initialisierung
		Arraycnt = 0;	// Historikzähler initialisierung
		gettimeofday(&start,0);
		gettimeofday(&stop,0);
		gettimeofday(&startpause,0);
		DCFSekunde = 0;
		dcfInterruptInit = 0;	// Init aus
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

//#############################################################################################################  
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

//#############################################################################################################  
int dcf77_init(struct Zeitstempel DCFData)
{
	int m,n;			// Laufvariablen

	// GPIO-Ports konfigurieren	
	pinMode(BtnPin,INPUT);		// GPIO - Auf Input für DCF-Rx
	pullUpDnControl(BtnPin,PUD_OFF);// GPIO ohne Pull-Up/-Down

	pinMode(LedRot, OUTPUT);	// LED-Ausgänge
	pinMode(LedGrn, OUTPUT);
	pinMode(LedBlu, OUTPUT);
	digitalWrite(LedRot,0);		// LED-Ausgänge Ausschalten (Sicherheitshalber)
	digitalWrite(LedGrn,0);
	digitalWrite(LedBlu,0);

	// DatenArray löschen
	for(m=0;m < BlockHigh;m++)
		for(n=0;n < BlockLength;n++)
			Block[BlockHigh][BlockLength] = 0x0;	// Array der Zeitmessung
			
	for(m=0;m < BlockHigh;m++)
		for(n=0;n < BlockLength;n++)
			Daten[BlockHigh][BlockLength] = 0x0;	// Array der Decoder-Logik

	syncbit = 0;
	Bitaktuell = 0;
	Bit = 0;
	dcfInterruptInit = 1;
        runClockcnt = 0;
        runArraycnt = 0;
        DCFSekunde  = 0;

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
	
// Installieren der Interrupt-Routine
	if(wiringPiISR(BtnPin, INT_EDGE_BOTH, BtnISR))
	{
		printf("setup ISR failed !\n");
		return(-1);
	}
	else
		return(1);
}

//#############################################################################################################  
int dcf77_run(struct Zeitstempel DCFData)
{
	int dcfreturn = 0;
	
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
				dcfreturn = -1;
			}
			else
				dcfreturn = 1;	// Daten sind gültig
			syncbit=0;	
		}
		return(dcfreturn);
}

//#############################################################################################################  
// Debuginformation Zusammenfasseung Zeit-Decoder
int dcf77_debug(struct Zeitstempel DCFData)
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
	return(0);
}
		
//#############################################################################################################  
// Bei Programmende
int dcf77_end()
{
	digitalWrite(LedBlu,0);	// LED-Ausschalten
	digitalWrite(LedRot,0);
	digitalWrite(LedGrn,0);
	return(0);
}
