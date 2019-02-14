#include <sys/time.h>
#include <sys/timex.h>
#include <errno.h>

//#############################################################################################################
int time_set_once(time_t akttime,time_t set_of_time_dcf,time_t set_of_time_gps,struct Zeitstempel *DCFData,struct Zeitstempel *GPSData,int dcfstatus,int gpsstatus)
{
    unsigned long temptime1;        // laufzeitvariablen
    unsigned long temptime2;
    unsigned long difftime;
    time_t set_of_time_prio;
    struct timeval settime; // Uhrzeit zum Schreiben ins System
    int timesetreturn;      // Rückmeldung vom Zeitsetz-Mechanismus

    timerclear(&settime);                   // Setup-Muster zum Setzten der Uhrzeit säubern
    temptime1=set_of_time_dcf;
    temptime2=set_of_time_gps;
    difftime = abs(temptime1-temptime2);    // Differenz zwischen DCF und GPS
    
#ifdef priogps
    set_of_time_prio = set_of_time_gps;
#else
    set_of_time_prio = set_of_time_dcf;
#endif

    
    if(difftime < Diff)
    {
        if((abs(akttime - set_of_time_dcf) > MaxTimeDiff) | (abs(akttime - set_of_time_gps) > MaxTimeDiff))
        {
            if((gpsstatus > 0) & (dcfstatus > 0))
            {
            
                if((GPSData->Status == 0) & (DCFData->Status == 0))       // Beide Uhrwerte sind korrekt
                {
                    settime.tv_sec=set_of_time_prio;
                }
                else if((GPSData->Status > 0) & (DCFData->Status > 0))    // Beide Uhrwerte werden als gestört definiert
                {
                    printf("\n Fehler: Beide Quellen melden Einschränkung\n");
                    settime.tv_sec=set_of_time_prio;
                }
                else if((GPSData->Status == 0) & (DCFData->Status != 0))  // Nur eine Zeitquelle ok
                {
                    printf("\n Fehler: DCF77 Signal meldet Einschänkungen\n");
                    settime.tv_sec=set_of_time_gps;
                }
                else if((DCFData->Status == 0) & (GPSData->Status != 0))  // Nur eine Zeitquelle ok
                {
                    printf("\n Fehler: GPS Signal meldet Einschänkungen!");
                    settime.tv_sec=set_of_time_dcf;
                }
                else
                    printf("\n Fehler: Keine gültige Uhrzeit empfangbar!");
            }
            else if((gpsstatus > 0) & (dcfstatus < 0))    // Nur GPS verfügbar
                if(GPSData->Status == 0)
                {
                    printf("\n Fehler: DCF77 nicht aktiv!");
                    settime.tv_sec=set_of_time_gps;
                }
            else if((dcfstatus > 0) & (gpsstatus < 0))    // Nur DCF77 verfügbar
                if(DCFData->Status == 0)
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
        return(1);
    }
    else
    {
        printf("\n Fehler: Zeitdifferenz zwischen beiden Zeitquellen ist zu gross\n");
        return(0);
    }

    return(0);
}

//#############################################################################################################
int time_set_debug(struct timex *timebuf,time_t set_of_time_dcf,time_t set_of_time_gps)
{
    time_t temptime;

    printf("Mode: 0x%04x\n",timebuf->modes);
    printf("offset: %ld\n",timebuf->offset);
    printf("frequency: %ld\n",timebuf->freq);
    printf("maxerror: %ld\n",timebuf->maxerror);
    printf("esterror: %ld\n",timebuf->esterror);
    printf("status: %d\n",timebuf->status);
    printf("PLL: %ld\n",timebuf->constant);
    printf("precicion: %ld\n",timebuf->precision);
    printf("tolerance: %ld\n",timebuf->tolerance);
    printf("tick: %ld\n",timebuf->tick);
    // Read only
//    printf("ppsfreq: %ld\n",timebuf->ppsfreq);
//    printf("jitter: %ld\n",timebuf->jitter);
//    printf("shift: %d\n",timebuf->shift);
//    printf("stabil: %ld\n",timebuf->stabil);
//    printf("jitcnt: %ld\n",timebuf->jitcnt);
//    printf("calcnt: %ld\n",timebuf->calcnt);
//    printf("errcnt: %ld\n",timebuf->errcnt);
//    printf("stbcnt: %ld\n",timebuf->stbcnt);
//    printf("tai: %d\n",timebuf->tai);
    // Zeiten
    printf("timeval: %ld , %ld\n",timebuf->time.tv_sec,timebuf->time.tv_usec);
    temptime=timebuf->time.tv_sec;
    printf("Zeit System: %s",ctime(&temptime));
    printf("Zeit DCF: %s",ctime(&set_of_time_dcf));
    printf("Zeit GPS: %s",ctime(&set_of_time_gps));
    printf("-------------------------------------------\n");
    return(0);    
}

//#############################################################################################################
int time_set_cyclic(time_t set_of_time_dcf,time_t set_of_time_gps)
{
    static struct timex timebuf;            // Struktur muss Global sein ???
    int timeretur;

    timeretur=adjtimex(&timebuf);
    if(debug==5)
    {
        printf("\nDebug Errno: %d\n",errno);
        printf("Adjtimex 1: Status %d\n",timeretur);
        time_set_debug(&timebuf,set_of_time_dcf,set_of_time_gps);
    }

    timebuf.status = 0;
    timebuf.modes = 0;
    timebuf.modes  = ADJ_STATUS;
    
    timeretur=adjtimex(&timebuf);
    if(debug==5)
    {
        printf("\nDebug: %d\n",errno);
        printf("Adjtimex 2: Status %d\n",timeretur);
        time_set_debug(&timebuf,set_of_time_dcf,set_of_time_gps);
    }

    return(0);
}

