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

// Hauptfunktion
int main(void)
{
        int gpsinitstatus = 0;  // GPS-Mouse aktiv (= 1)
        int dcfinitstatus = 0;  // DCF 77 aktiv (= 1)

        int gpsstatus = 0;      // GPS gültig =1
        char gpschararray[GPSArray];    // RX_buffer GPS

        int dcfstatus = 0;      // DCF77 gültig =1
        int update = 0;         // Sekundenschalter für Display-Information
        int n = 0;              // lokaler Zähler
        int m = 0;
        int PRGabbruch = 1;     // Schleife wird beendet, wegen groben Fehler. 0 bei Fehler
        int ntpstart = 0;       // Zum eimaligen starten des NTP-Servers

        struct tm timeset;      // Zum Erstellen der Epoch-Zeiten für Systemzeitkorrektur
        unsigned long temptime1;
        unsigned long temptime2;
        time_t set_of_time_dcf; // Zeitcodes
        time_t set_of_time_gps;
        time_t set_of_time;
        struct timeval settime; // Uhrzeit zum Schreiben ins System
        int timesetreturn;      // Rückmeldung vom Zeitsetz-Mechanismus

        time_t akttime;         // Systemzeit auslesen
        time_t baktime;
        time_t startzeit;       // Systemstart

        struct Zeitstempel DCFData;     // Struktur der Decoderdaten
        struct Zeitstempel GPSData;

        int fdserial;           // Dateizeiger zur USB-GPS-Mouse
        char gpschararray[GPSArray];    // RX_buffer GPS
        int gpsrxcnt = 0;               // Anzahl der RX-Daten

        struct timex timebuf;
        int timeretur;
        int dl = 0;
        time_t temptime;

// Erste Initialierung

        PRGabbruch = 1;
        ntpstart = 0;

        time(&akttime);
        time(&startzeit);

#ifdef dcf77
// DCF77 Initialisierung
        dcfinitstatus = dcf77_init(DCFData)
#endif

#ifdef gps
// GPS Initialisierung
        gpsinitstatus = gps_init(&fdserial,gpschararray);
#endif

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

//DCF77 Empfang
        if(dcfinitstatus > 0)
                 dcfstatus = dcf77_run(&GPSData);

// GPSZeit lesen
        if((gpsinitstatus > 0)
                gpsstatus = gps_run(fdserial,&GPSData);

// Debuginformation Zusammenfasseung Zeit-Decoder
                if((debug == 3) & (update))
                {
                        if(gpsstatus)
                        {
                                gps_debug(&GPSData);
                        }
                        if(dcfstatus)
                        {
                                dcf77_debug(&DCFData);
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
                timeset.tm_mon = (DCFData.Monat-1);     // Monat ist von 0 - 11 definiert.
                timeset.tm_year = (DCFData.Jahr+2000)-1900;
                timeset.tm_isdst = 0;        // Daylightsaving / Sommerzeit nicht relevant
                set_of_time = mktime(&timeset);
                set_of_time_dcf = set_of_time - (DCFData.ZeitZone * 3600); // Zeitzonen Rückrechnung

                if((update) & (akttime > startzeit + DelaySetTime))
                {
                        if(debug==4)
                        {
                                printf("Aktuelles System: %ld\n",akttime);
                                printf("Empfangen GPS: %ld \n",set_of_time_gps);
                                printf("Empfangen DCF: %ld \n",set_of_time_dcf);
                        }

                        if((abs(akttime - set_of_time_dcf) > MaxTimeDiff) | (abs(akttime - set_of_tim
                        {
                                if((gpsstatus) & (dcfstatus))
                                {
                                        if((GPSData.Status == 0) & (DCFData.Status == 0))       // Be
                                        {
                                                temptime1=set_of_time_dcf;
                                                temptime2=set_of_time_gps;
                                                if(abs(temptime1-temptime2) < Diff)             // We
                                                {
                                                        timerclear(&settime);
                                                        settime.tv_sec=set_of_time_dcf;
                                                        timesetreturn = settimeofday(&settime,0);
                                                }
                                                else
                                                {
                                                        printf("\n Fehler: Zeitdifferenz zwischen bei
                                                }
                                        }
                                        else if((GPSData.Status > 0) & (DCFData.Status > 0))    // We
                                        {
                                                printf("\n Fehler: Beide Quellen melden Einschränkung
                                                temptime1=set_of_time_dcf;
                                                temptime2=set_of_time_gps;
                                                if(abs(temptime1-temptime2) < Diff)             // We
                                                {
                                                        timerclear(&settime);
                                                        settime.tv_sec=set_of_time_dcf;
                                                        timesetreturn = settimeofday(&settime,0);
                                                }
                                                else
                                                {
                                                        printf("\n Fehler: Zeitdifferenz zwischen bei
                                                }
                                        }
                                        else if((GPSData.Status == 0) & (DCFData.Status != 0))  // Nu
                                        {
                                                printf("\n Fehler: DCF77 Signal meldet Einschänkungen
                                                timerclear(&settime);
                                                settime.tv_sec=set_of_time_gps;
                                                timesetreturn = settimeofday(&settime,0);
                                        }
                                        else if((DCFData.Status == 0) & (GPSData.Status != 0))  // Nu
                                        {
                                                printf("\n Fehler: GPS Signal meldet Einschänkungen!"
                                                timerclear(&settime);
                                                settime.tv_sec=set_of_time_dcf;
                                                timesetreturn = settimeofday(&settime,0);
                                        }
                                        else
                                        {
                                                printf("\n Fehler: Keine gültige Uhrzeit empfangbar!"
                                        }
                                }
                                else if((gpsstatus) & (! dcfstatus))    // Nur GPS verfügbar
                                        if(GPSData.Status == 0)
                                        {
                                                printf("\n Fehler: DCF77 nicht aktiv!");
                                                timerclear(&settime);
                                                settime.tv_sec=set_of_time_gps;
                                                timesetreturn = settimeofday(&settime,0);
                                        }
                                else if((dcfstatus) & (! gpsstatus))    // Nur DCF77 verfügbar
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
                                        printf("\n\n Fehler: Systemzeit lässt sich nicht setzen! \n")
                                        printf("  ----  Root-Rechte ? ---- \n\n");
                                        PRGabbruch = 0;         // Abbruch des Programmes
                                }
                                else
                                {
                                        printf("\n Aktuelle Systemzeit %s",ctime(&akttime));    // An
                                        time(&akttime);
                                        baktime = akttime;
                                        startzeit = akttime - DelaySetTime -1; // Damit die Versögeru
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
#ifdef dcf77
                dcf77_end();
#endif

        }
// Bei Programmende
        dcf77_end();
        gps_end(fserial);

        return(-1);
}
