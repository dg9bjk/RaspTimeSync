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

        struct timex timebuf;
        int timeretur;
        int dl = 0;
        time_t temptime;

#ifdef dcf77
        int dcfinitstatus;	// DCF 77 aktiv (= 1)
        int dcfstatus;		// DCF77 gültig =1
        struct Zeitstempel DCFData;     // Struktur der Decoderdaten
#endif

#ifdef gps
        int gpsinitstatus;	// GPS-Mouse aktiv (= 1)
        int gpsstatus;		// GPS gültig =1
        int fdserial;           	// Dateizeiger zur USB-GPS-Mouse
        char gpschararray[GPSArray]; 	// Zeichenspeicher GPS Array
        struct Zeitstempel GPSData;	// Struktur der Decoderdaten
#endif        

// Erste Initialierung

        PRGabbruch = 1;
        ntpstart = 0;
        update = 0;
        
        set_of_time_dcf = 0;
        dcfstatus = 0;
        set_of_time_gps = 0;
        gpsstatus = 0;

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
                set_of_time_dcf = set_of_time - (DCFData.ZeitZone * 3600); // Zeitzonen Rückrechnung
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

                        if((abs(akttime - set_of_time_dcf) > MaxTimeDiff) | (abs(akttime - set_of_time_gps) > MaxTimeDiff))
                        {
                                if((gpsstatus > 1) & (dcfstatus > 1))
                                {
                                        if((GPSData.Status == 0) & (DCFData.Status == 0))       // Beide Uhrwerte sind korrekt
                                        {
                                                temptime1=set_of_time_dcf;
                                                temptime2=set_of_time_gps;
                                                if(abs(temptime1-temptime2) < Diff)             // Wenn die Zeitdifferenz kleiner des Grenzwertes ist.
                                                {
                                                        timerclear(&settime);
                                                        settime.tv_sec=set_of_time_dcf;
                                                        timesetreturn = settimeofday(&settime,0);
                                                }
                                                else
                                                {
                                                        printf("\n Fehler: Zeitdifferenz zwischen beiden Zeitquellen ist zu gross\n");
                                                }
                                        }
                                        else if((GPSData.Status > 0) & (DCFData.Status > 0))    // Beide Uhrwerte werden als gestört definiert
                                        {
                                                printf("\n Fehler: Beide Quellen melden Einschränkung\n");
                                                temptime1=set_of_time_dcf;
                                                temptime2=set_of_time_gps;
                                                if(abs(temptime1-temptime2) < Diff)             // Wenn die Zeitdifferenz kleiner des Grenzwertes ist.
                                                {
                                                        timerclear(&settime);
                                                        settime.tv_sec=set_of_time_dcf;
                                                        timesetreturn = settimeofday(&settime,0);
                                                }
                                                else
                                                {
                                                        printf("\n Fehler: Zeitdifferenz zwischen beiden Zeitquellen ist zu gross\n");
                                                }
                                        }
                                        else if((GPSData.Status == 0) & (DCFData.Status != 0))  // Nur eine Zeitquelle ok
                                        {
                                                printf("\n Fehler: DCF77 Signal meldet Einschänkungen\n");
                                                timerclear(&settime);
                                                settime.tv_sec=set_of_time_gps;
                                                timesetreturn = settimeofday(&settime,0);
                                        }
                                        else if((DCFData.Status == 0) & (GPSData.Status != 0))  // Nur eine Zeitquelle ok
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
                                        printf("\n\n Fehler: Systemzeit lässt sich nicht setzen! \n");
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
// NTP Server starten - nur nach der Zeitsync nötig
                if((akttime > startzeit + DelaySetTime)& (! ntpstart))
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
