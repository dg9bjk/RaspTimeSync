#include "ntpserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <time.h>
#include "timesync.c"

#define LedRot    1
#define LedGrn    2
#define LedBlu    3

#ifdef dcf77
#include "dcf77.c"
#endif

#ifdef gps
#include "gps.c"
#endif

#ifndef wiringlib
#include <wiringPi.h>
#endif

#include <sys/timex.h>

// Hauptfunktion
int main(void)
{
        int PRGabbruch;		// Schleife wird beendet, wegen groben Fehler. 0 bei Fehler
        int ntpstart;		// Zum eimaligen starten des NTP-Servers
        int update;		// Sekundenschalter für Display-Information
        int firstsync;		// Nur ein harter Sync mit Uhrzeit im System
        int funkreturn; 	// temp. Rückgabe
        time_t akttime;         // Systemzeit auslesen
        time_t baktime;
        time_t startzeit;       // Systemstart

        struct Zeitstempel DCFData;     // Struktur der Decoderdaten
        struct Zeitstempel GPSData;	// Struktur der Decoderdaten
        time_t set_of_time_dcf;		// Zeitstempel des DCF77 Empfängers
        time_t set_of_time_gps;		// Zeitstempel des GPS Empfängers
        struct tm timeset;      // Zum Erstellen der Epoch-Zeiten für Systemzeitkorrektur

        struct timex timebuf;
        int timeretur;
        int dl = 0;
        time_t temptime;

        int dcfinitstatus;	// DCF 77 aktiv (= 1)
        int dcfstatus;		// DCF77 gültig =1

        int ledcolor;		// Lichtfarbe
        int ledflash;		// Blickflag

        int gpsinitstatus;	// GPS-Mouse aktiv (= 1)
        int gpsstatus;		// GPS gültig =1

#ifdef gps
        int fdserial;           	// Dateizeiger zur USB-GPS-Mouse
        char gpschararray[GPSArray]; 	// Zeichenspeicher GPS Array
#endif

// Erste Initialierung

        PRGabbruch = 1;
        ntpstart = 0;
        update = 0;
        firstsync = 1;
        
        ledcolor = 1;		// rot
        ledflash = 0;		// Aus
       
        time(&akttime);
        time(&startzeit);

#ifdef dcf77
// DCF77 Initialisierung
        dcfinitstatus = dcf77_init(DCFData);
#else
        dcfinitstatus = -1;
#endif

#ifdef gps
// GPS Initialisierung
        gpsinitstatus = gps_init(&fdserial,gpschararray,GPSData);
#else
        gpsinitstatus = -1;
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
                        ledflash = ! ledflash;			// Die LED blinkt im Sekundentakt
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
#else
                // Wenn kein DCF77 Modul aktiv ist, wird das Modul gesperrt
                dcfstatus = -1;
#endif

#ifdef gps
                // GPSZeit lesen
                if(gpsinitstatus > 0)
                        gpsstatus = gps_run(fdserial,gpschararray,GPSData);
#else
                // Wenn kein GPS Modul aktiv ist, wird das Modul gesperrt
                gpsstatus = -1;
#endif

                // Wenn kein DCF77 Empfänger vorhanden ist, wird die LED für Betriebszustand des GPS-Sensors benutzt.
                if(gpsinitstatus < 0)
                        ledcolor = 1;	// Rot
                if(gpsstatus > 0)
                        ledcolor = 2;	// Grün
                if(gpsstatus < 0)
                        ledcolor = 3;	// Blau

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
                set_of_time_dcf = mktime(&timeset);
                set_of_time_dcf = set_of_time_dcf - (DCFData.ZeitZone * 3600); // Zeitzonen Rückrechnung
#else
                set_of_time_dcf = akttime;	// Wenn Empfänger nicht aktiv, dann wird die aktuelle Zeit angenommen
                DCFData.Status = -1;
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
                set_of_time_gps = akttime;	// Wenn Empfänger nicht aktiv, dann wird die aktuelle Zeit angenommen
                GPSData.Status = -1;
#endif

                if((debug==4) & update)	// Anzeige der Uhrzeiten im Zusammenhang
                {
                        printf("Aktuelles System: %ld\n",akttime);
#ifdef gps
                        printf("Empfangen GPS: %ld \n",set_of_time_gps);
#endif
#ifdef dcf77
                        printf("Empfangen DCF: %ld \n",set_of_time_dcf);
#endif
                }

                // Die Uhrzeit einmal hart setzten
                if((firstsync) & (akttime > startzeit + DelaySetTime))
                {
                        funkreturn=time_set_once(akttime,set_of_time_dcf,set_of_time_gps,GPSData,DCFData,dcfstatus,gpsstatus);
                        if(funkreturn < 0)
                                PRGabbruch = 0;	// Ende des Programmes wegen Root-Rechte
                        if(funkreturn > 0)
                        {
                                time(&akttime);
                                baktime = akttime;
                                startzeit = akttime - DelaySetTime -1; // Damit die Versögerung nicht noch einmal greift
                                
                                // NTP Server starten - nur nach der Zeitsync nötig
                                if(! ntpstart)
                                {
                                        system("service ntp start &");
                                        ntpstart = 1;
                                }
                                firstsync = 0;
                        }
                }

#ifdef gps
                delay(3); // Entlastung der CPU
#else
                delay(50);
#endif

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
