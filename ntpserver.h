#define MaxTimeDiff     30      // Maximale Abweichtung der Systemzeit
#define Diff            5       // Maximale Abweichtung zwischen DCF77 und GPS-Zeiten
#define DelaySetTime    300     // Verzögerung der Zeitkorrektur in Sekunden, bis alle Zeiten erfasst sind - Minimum 5 Minuten

#define dcf77
#define gps

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
