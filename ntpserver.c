#include "ntpserver.h"

#ifdef dcf77
#include "dcf77.c"
#endif

#ifdef gps
#include "gps.c"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>

// Debug-Information für Entwicklung
// 0 = Auslieferung - Keine Informationen
// 1 = Zeitmessung direkt (nur DCF77)
// 2 = Protokoll rahmen (DCF77 und GPS)
// 3 = Dekodierte Daten (DCF77 und GPS)
// 4 = Zeitstring (Epche-Time)
int debug = 0;

// Hauptprogramm
int main(int VarCnt, char[] *Parameter, char[] *Env)
{
	int gpsinitstatus = 0;	// GPS-Mouse aktiv =1
	int gpsstatus = 0;	// GPS gültig =1
	int dcfstatus = 0;	// DCF77 gültig =1
	int update = 0;		// Sekundenschalter für Display-Information
	int n = 0;		// lokaler Zähler
	int m = 0;
	int PRGabbruch = 1;	// Schleife wird beendet, wegen groben Fehler. 0 bei Fehler
	int ntpstart = 0;	// Zum eimaligen starten des NTP-Servers
	
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

	struct timex timebuf;
	int timeretur;
	int dl = 0;
	time_t temptime;
		
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
	ntpstart = 0;
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

		if((update) & (akttime > startzeit + DelaySetTime))
		{
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
					{
						printf("\n Fehler: Keine gültige Uhrzeit empfangbar!");
					}
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
				{
					printf("\n Fehler: Keine Uhrzeit empfangbar!");
				}

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

		if((akttime > startzeit + DelaySetTime)& (! ntpstart))
		{
			system("service ntp start &");
			ntpstart = 1;
		}
				
		delay(3); // Entlastung der CPU
		
		if(update)		
		{
			timeretur=adjtimex(&timebuf);
		
			printf("\nAdjtimex: %d Status %d\n",dl,timeretur);
			printf("Mode: %d\n",timebuf.modes);
			printf("offset: %ld\n",timebuf.offset);
			printf("frequency: %ld\n",timebuf.freq);
			printf("maxerror: %ld\n",timebuf.maxerror);
			printf("esterror: %ld\n",timebuf.esterror);
			printf("status: %d\n",timebuf.status);
			printf("PLL: %ld\n",timebuf.constant);
			printf("precicion: %ld\n",timebuf.precision);
			printf("tolerance: %ld\n",timebuf.tolerance);
			printf("tick: %ld\n",timebuf.tick);
			printf("timeval: %ld , %ld\n",timebuf.time.tv_sec,timebuf.time.tv_usec);
			temptime=timebuf.time.tv_sec;
			printf("Zeit System: %s",ctime(&temptime));
			printf("Zeit DCF: %s",ctime(&set_of_time_dcf));
			printf("Zeit GPS: %s",ctime(&set_of_time_gps));
		}
		
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
