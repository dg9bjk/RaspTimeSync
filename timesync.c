#include <sys/time.h>

//#############################################################################################################
int time_set_once(time_t akttime,time_t set_of_time_dcf,time_t set_of_time_gps,struct Zeitstempel DCFData,struct Zeitstempel GPSData,int dcfstatus,int gpsstatus)
{
    unsigned long temptime1;        // laufzeitvariablen
    unsigned long temptime2;
    unsigned long difftime;
    struct timeval settime; // Uhrzeit zum Schreiben ins System
    int timesetreturn;      // Rückmeldung vom Zeitsetz-Mechanismus

    timerclear(&settime);                   // Setup-Muster zum Setzten der Uhrzeit säubern
    if((abs(akttime - set_of_time_dcf) > MaxTimeDiff) | (abs(akttime - set_of_time_gps) > MaxTimeDiff))
    {
        if((gpsstatus > 0) & (dcfstatus > 0))
        {
            temptime1=set_of_time_dcf;
            temptime2=set_of_time_gps;
            difftime = abs(temptime1-temptime2);    // Differenz zwischen DCF und GPS

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

        if(settime.tv_sec > 0)
        {
            timesetreturn = settimeofday(&settime,0);

            if(timesetreturn)
            {
                printf("\n\n Fehler: Systemzeit lässt sich nicht setzen! \n");
                printf("  ----  Root-Rechte ? ---- \n\n");
                return(-1);         // Abbruch des Programmes
            }
            else
            {
                printf("\n Aktuelle Systemzeit %s",ctime(&akttime));    // Anzeige
                time(&akttime);
                printf(" - neue Systemzeit: %s\n",ctime(&akttime));
                return(1);
            }
        }
    }
    return(0);
}

