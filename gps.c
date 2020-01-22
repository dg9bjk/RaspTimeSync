#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define GPSArray	1023	// Zeichenpuffer GPS RX



//###################################################################################################################################
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

//###################################################################################################################################
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

//###################################################################################################################################
// Zeichenfolge zerlegen (Zeichen ausschneiden)
int strcut(char StrIn[],char StrOut[],int start,int stop)
{
	int n = 0; // Laufzeitvariablen
	int m = 0;
	
	for(n=start,m=0;n<=stop;n++,m++)
		StrOut[m] = StrIn[n];
	StrOut[m]=0x0;
}

//###################################################################################################################################
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

//###################################################################################################################################
// Hauptfunktion
int gps_init(int *fdserial,char gpschararray[GPSArray],struct Zeitstempel GPSData)
{
	int gpsinitstatus = 0;	// GPS-Mouse aktiv =1
	int gpsstatus = 0;	// GPS gültig =1

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

	//Serial-Schnittstelle (USB-GPS-Mouse) aktivieren
	fdserial=gpsmouseinit();
	if(fdserial < 0)
	{
		printf("GPS not OK!\n");
		gpsinitstatus = -1;
	}
	else
	{
		gpsinitstatus = 1;
	}
}

//###################################################################################################################################
// GPSZeit lesen
int gps_run(int fdserial,struct Zeitstempel GPSData)
{
	char gpschararray[GPSArray];
	int gpsrxcnt;
		
	// GPS-Daten-Array löschen
	for(n=0;n<GPSArray;n++)
		gpschararray[GPSArray] = 0x0; 				// RX_buffer GPS

	gpsrxcnt = gpsread(fdserial,gpschararray);
	if(gpsrxcnt > 0) // Wenn Daten da sind
	{
		if(debug==2)
			printf("Debug:%s\n",gpschararray);
		if(gpsdecodeGPRMC(gpschararray,gpsrxcnt,&GPSData)) // Decoder
		{
			printf("GPS Decoderfehler\n");
			return(0);
		}
		else
			return(-1); // Daten sind gültig
	}
	else
		return(-1);
}


//###################################################################################################################################
// Debuginformation Zusammenfasseung Zeit-Decoder
int gps_debug(struct Zeitstempel GPSData)
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
	return(0);
}

//###################################################################################################################################
int gps_end(int fdserial)
{
	close(fdserial);
}
