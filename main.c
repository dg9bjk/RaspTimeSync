#include "ntpserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#ifdef dcf77
#include "dcf77.c"
#endif

#ifdef gps
#include "gps.c"
#endif

#include <sys/timex.h>

// Hauptfunktion
int main(void)
{
        int PRGabbruch;		// Schleife wird beendet, wegen groben Fehler. 0 bei Fehler
        int ntpstart;		// Zum eimaligen starten des NTP-Servers
        int update;		// Sekundenschalter für Display-Information

        time_t akttime;         // Systemzeit auslesen
        time_t baktime;
        time_t startzeit;       // Systemstart

        unsigned long temptime1;
        unsigned long temptime2;
        unsigned long difftime;
        struct tm timeset;      // Zum Erstellen der Epoch-Zeiten für Systemzeitkorrektur
        struct timeval settime; // Uhrzeit zum Schreiben ins System
        int timesetreturn;      // Rückmeldung vom Zeitsetz-Mechanismus

        struct timex timebuf;
        int timeretur;
        int dl = 0;
        time_t temptime;

#ifdef dcf77
        int dcfinitstatus;	// DCF 77 aktiv (= 1)
        int dcfstatus;		// DCF77 gültig =1
        struct Zeitstempel DCFData;     // Struktur der Decoderdaten
        time_t set_of_time_dcf;
#else
        int ledcolor;
        int ledflash;
#endif


#ifdef gps
        int gpsinitstatus;	// GPS-Mouse aktiv (= 1)
        int gpsstatus;		// GPS gültig =1
        int fdserial;           	// Dateizeiger zur USB-GPS-Mouse
        char gpschararray[GPSArray]; 	// Zeichenspeicher GPS Array
        struct Zeitstempel GPSData;	// Struktur der Decoderdaten
        time_t set_of_time_gps;
#endif        

// Erste Initialierung

        PRGabbruch = 1;
        ntpstart = 0;
        update = 0;

#ifndef dcf77
        ledcolor = 1;		// rot
        ledflash = 0;		// Aus
#endif
       
        time(&akttime);
        time(&startzeit);

#ifdef dcf77
// DCF77 Initialisierung
        dcfinitstatus = dcf77_init(DCFData);
#endif

#ifdef gps
// GPS Initialisierung
        gpsinitstatus = gps_init(&fdserial,gpschararray,GPSData);
#endif

//Hauptprogramm
        while(PRGabbruch)
        {
                // Systemzeit - Verzögerung der Anzeige
                if (baktime != akttime)
                {
                        baktime = akttime;
                        update = 1;
#ifndef dcf77
                        ledflash = ! ledflash;
                        if(ledcolor == 1)
                                digitalWrite(LedRot,ledflash); // ledcolor = 1
                        else if(ledcolor == 2)
                                digitalWrite(LedGrn,ledflash); // ledcolor = 2
                        else if(ledcolor == 3)
                                digitalWrite(LedBlu,ledflash); // ledcolor = 3
                        else
                                digitalWrite(LedRot,1); // ledcolor = 1
#endif
                }
                else
                        update = 0;

                time(&akttime);

#ifdef dcf77
//DCF77 Empfang
        if(dcfinitstatus > 0)
                 dcfstatus = dcf77_run(DCFData);
#endif

#ifdef gps
// GPSZeit lesen
        if(gpsinitstatus > 0)
                gpsstatus = gps_run(fdserial,gpschararray,GPSData);
#endif
#ifndef dcf77
        if(gpsinitstatus < 0)
                ledcolor = 1;	// Rot
        if(gpsstatus > 0)
                ledcolor = 2;	// Grün
        if(gpsstatus < 0)
                ledcolor = 3;	// Blau
#endif

// Debuginformation Zusammenfasseung Zeit-Decoder
                if((debug == 3) & (update))
                {
#ifdef dcf77
                        if(dcfstatus > 0)
                        {
                                dcf77_debug(DCFData);
                        }
#endif
#ifdef gps
                        if(gpsstatus > 0)
                        {
                                gps_debug(GPSData);
                        }
#endif
                        printf("--------------------------\n");
                        printf("Zeitanzeige System: %ld -> %s",akttime,ctime(&akttime));
                        printf("--------------------------\n");
                }

// Setzen der Zeitinformation in das System
#ifdef dcf77
                timeset.tm_sec = DCFSekunde;
                timeset.tm_min = DCFData.Minute;
                timeset.tm_hour = DCFData.Stunde;
                timeset.tm_mday = DCFData.Tag;
                timeset.tm_mon = (DCFData.Monat-1);     // Monat ist von 0 - 11 definiert.
                timeset.tm_year = (DCFData.Jahr+2000)-1900;
                timeset.tm_isdst = 0;        // Daylightsaving / Sommerzeit nicht relevant
                set_of_time = mktime(&timeset);
                set_of_time_dcf = set_of_time_dcf - (DCFData.ZeitZone * 3600); // Zeitzonen Rückrechnung
#else
                set_of_time_dcf = akttime;	// Für die Auswertung sind beide Zeiten identisch, somit keine Reaktion vom System
#endif

#ifdef gps
                timeset.tm_sec   = GPSData.Sekunde;
                timeset.tm_min   = GPSData.Minute;
                timeset.tm_hour  = GPSData.Stunde;
                timeset.tm_mday  = GPSData.Tag;
                timeset.tm_mon   = GPSData.Monat-1;
                timeset.tm_year  = (GPSData.Jahr+2000)-1900;
                timeset.tm_isdst = 0;        // Daylightsaving / Sommerzeit nicht relevant
                set_of_time_gps = mktime(&timeset);
#else
                set_of_time_gps = akttime;	// Für die Auswertung sind beide Zeiten identisch, somit keine Reaktion vom System
#endif

                if((update) & (akttime > startzeit + DelaySetTime))
                {
                        if(debug==4)
                        {
                                printf("Aktuelles System: %ld\n",akttime);
#ifdef gps
                                printf("Empfangen GPS: %ld \n",set_of_time_gps);
#endif
#ifdef dcf77
                                printf("Empfangen DCF: %ld \n",set_of_time_dcf);
#endif
                        }

                        timerclear(&settime);
                        temptime1=set_of_time_dcf;
                        temptime2=set_of_time_gps;
                        difftime = abs(temptime1-temptime2);

                        if((abs(akttime - set_of_time_dcf) > MaxTimeDiff) | (abs(akttime - set_of_time_gps) > MaxTimeDiff))
                        {
                                if((gpsstatus > 0) & (dcfstatus > 0))
                                {
                                        if((GPSData.Status == 0) & (DCFData.Status == 0))       // Beide Uhrwerte sind korrekt
                                        {
                                                if(difftime < Diff)             // Wenn die Zeitdifferenz kleiner des Grenzwertes ist.
                                                        settime.tv_sec=set_of_time_dcf;
                                                else
                                                        printf("\n Fehler: Zeitdifferenz zwischen beiden Zeitquellen ist zu gross\n");
                                        }
                                        else if((GPSData.Status > 0) & (DCFData.Status > 0))    // Beide Uhrwerte werden als gestört definiert
                                        {
                                                printf("\n Fehler: Beide Quellen melden Einschränkung\n");
                                                if(difftime < Diff)             // Wenn die Zeitdifferenz kleiner des Grenzwertes ist.
                                                        settime.tv_sec=set_of_time_dcf;
                                                else
                                                        printf("\n Fehler: Zeitdifferenz zwischen beiden Zeitquellen ist zu gross\n");
                                        }
                                        else if((GPSData.Status == 0) & (DCFData.Status != 0))  // Nur eine Zeitquelle ok
                                        {
                                                printf("\n Fehler: DCF77 Signal meldet Einschänkungen\n");
                                                settime.tv_sec=set_of_time_gps;
                                        }
                                        else if((DCFData.Status == 0) & (GPSData.Status != 0))  // Nur eine Zeitquelle ok
                                        {
                                                printf("\n Fehler: GPS Signal meldet Einschänkungen!");
                                                settime.tv_sec=set_of_time_dcf;
                                        }
                                        else
                                                printf("\n Fehler: Keine gültige Uhrzeit empfangbar!");
                                }
                                else if((gpsstatus > 0) & (dcfstatus < 0))    // Nur GPS verfügbar
                                        if(GPSData.Status == 0)
                                        {
                                                printf("\n Fehler: DCF77 nicht aktiv!");
                                                settime.tv_sec=set_of_time_gps;
                                        }
                                else if((dcfstatus > 0) & (gpsstatus < 0))    // Nur DCF77 verfügbar
                                        if(DCFData.Status == 0)
                                        {
                                                printf("\n Fehler: GPS nicht aktiv!");
                                                settime.tv_sec=set_of_time_dcf;
                                        }
                                else
                                        printf("\n Fehler: Keine Uhrzeit empfangbar!");

                                timesetreturn = settimeofday(&settime,0);

                                if(timesetreturn)
                                {
                                        printf("\n\n Fehler: Systemzeit lässt sich nicht setzen! \n");
                                        printf("  ----  Root-Rechte ? ---- \n\n");
                                        PRGabbruch = 0;         // Abbruch des Programmes
                                }
                                else
                                {
                                        printf("\n Aktuelle Systemzeit %s",ctime(&akttime));    // Anzeige
                                        time(&akttime);
                                        baktime = akttime;
                                        startzeit = akttime - DelaySetTime -1; // Damit die Versögerung nicht noch einmal greift
                                        printf(" - neue Systemzeit: %s\n",ctime(&akttime));
                                }
                        }
                }
// NTP Server starten - nur nach der Zeitsync nötig
                if((akttime > startzeit + DelaySetTime) & (! ntpstart))
                {
                        system("service ntp start &");
                        ntpstart = 1;
                }

                delay(3); // Entlastung der CPU

// samfte Zeitkorrektur
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
#ifdef dcf77
                dcf77_end();
#endif

        }
// Bei Programmende
#ifdef dcf77
        dcf77_end();
#endif
#ifdef gps
        gps_end(fdserial);
#endif

        return(-1);
}
