/***NOTE:
   This is an interrupt service TSR system
   You can NOT compile this program with Test
   Stack Overflow turned on and get an
   executable file which will operate
   correctly.
*/
#pragma inline
#if !defined(__TINY__)
#error Mozna kompilowac TYLKO w modelu TINY
#endif

#include <dos.h>

#define NULL 0L
/* The clock tick interrupt */
#define INTR 0x1C
#define INTDOS 0x21
/* Screen attribute (blue on grey) */
#define ATTR 0x0F00
#define CARRY 0x01
#define NO_CARRY 0xFE
#define ATTR_MASK 0xE7


/* reduce heaplength and stacklength
to make a smaller program in memory */
extern unsigned _heaplen = 1;/* Nie potrzebna ?*/
extern unsigned _stklen  = 1024;

/*Flagi prywatnego Cashu */
extern unsigned char read_req; /* nieaktualnosci informacji */
extern unsigned char write_req;/* koniecznosci zapisu */
extern unsigned char User_Id;/* Identyfikator aktualnie pracujacego uzytkownika */

int Cerber21H(void);/* Funkcja modyfikujaca serwis systemowy */
int writedirsec(void);/* Aktualizuje obraz dyskowy sektora z prywatnego Cach'u - zwraca 1 jesli nie wykonal*/


unsigned int (far *screen)[80];
void interrupt ( *OrginalTimerHandle)()=NULL;
void interrupt ( *OrginalDOS21h)()=NULL;

/* Przekazanie sterowania pod adres orginalny przerwania*/
#define ChainIntr(P) asm{pop bp;     \
			 pop di;     \
			 pop si;     \
			 pop ds;     \
			 pop es;     \
			 pop dx;     \
			 pop cx;     \
			 pop bx;     \
			 pop ax;     \
			 jmp cs:[P]; /* Tylko model TINY !!! */\
			 }

unsigned char far* InDos=0;
unsigned int error=0;
unsigned int MySS=0;
unsigned int MySP=0;
unsigned int CallerSS=0;
unsigned int CallerSP=0;

/* STRUKTURA DO PRZECHOWYWANIA REJESTROW WYWOLANIA PO ZMIANIE STOSOW */
struct REGPACK r;

const char blip[]="!/-\�/-\!";
const int lblip=sizeof(blip)-1;
int pblip=0;

#define U unsigned
void interrupt DOShandler(U bp,U di,U si,U ds,U es,U dx,U cx,U bx,U ax,U ip,U cs,U flags)
#undef U
{
pblip=0;
flags&=NO_CARRY;
if(ax==0x4301 || ax>=0xff00)
	{
	screen[0][79] ='�' + ATTR;
	/* Zapamientanie rejestrow w struktorze REGPACK*/
	r.r_ax=ax;
	r.r_bx=bx;
	r.r_cx=cx;
	r.r_dx=dx;
	r.r_si=si;
	r.r_di=di;
	r.r_ds=ds;
	r.r_es=es;
	r.r_flags=flags;
	r.r_bp=bp;
	disable();
	CallerSS=_SS;
	CallerSP=_SP;
	/* Przelanczanie stosu do stanu z MAIN */
	_SS=MySS;
	_SP=MySP;
	enable();
	/* Funkcja obslugujaca z casem w srodku */
	Cerber21H();
	/* Przywrocenie rejestrow stosu */
	disable();
	_SP=CallerSP;
	_SS=CallerSS;
	enable();
	/* Odtworzenie rejestrow na stosie by funkcje mogly cos przekazac*/
	ax=r.r_ax;
	bx=r.r_bx;
	cx=r.r_cx;
	dx=r.r_dx;
	si=r.r_si;
	di=r.r_di;
	ds=r.r_ds;
	es=r.r_es;
	flags=r.r_flags;
	bp=r.r_bp;
	return;
	}
	else
	{
	if(ax<0x0D00) goto fast;
	screen[0][79] =User_Id + ATTR;
	if(
	   ax==0x0D00 || /* oproznienie buforow */
	   ax==0x1000 || /* zamkniecie pliku */
	   ax==0x1300 || /* skasowanie pliku */
	   ax==0x1300 || /* skasowanie pliku */
	   ax==0x1500 || /* zapis sekwencyjny*/
	   ax==0x1600 || /* utworzenie pliku */
	   ax==0x1700 || /* zmiana nazwy pliku */
	   ax==0x1800 || /* ??? reserved */
	   ax==0x2200 || /* zapis swobodny */
	   ax==0x2800 || /* blokowy zapis swobodny */
	   ax==0x3900 || /* utworzenie podkatalogu */
	   ax==0x3A00 || /* usuniecie  podkatalogu */
	   ax==0x3C00 || /* utworzenie pliku */
	   ax==0x3E00 || /* zamkniecie pliku */
	   ax==0x4000 || /* zapisanie do pliku */
	   ax==0x4100 || /* skasowanie   pliku */
	   ax==0x4401 || /* zapisanie informacji o urzadzeniu */
	   ax==0x5600 || /* zmiana nazwy   pliku */
	   ax==0x5701 || /* zmiana nazwy daty i czasu  pliku */
	   ax==0x5A00 || /* utworzenie pliku tymczasowego */
	   ax==0x5B00 || /* utworzenie pliku */
	   ax==0x6800 || /* zapisanie buforow pliku*/
	   ax==0x6C00    /* rozszerzona funkcja otwarcia pliku*/
	   )
	   read_req=1;   /* Zawartosc w buforze moze zostac zaraz uniewazniona */
	}
fast:
ChainIntr(OrginalDOS21h);
}

void interrupt handler()
{
pblip++;
pblip%=lblip;
/* put the number on the screen */
 if(*InDos>=2)
   screen[0][79] = blip[pblip] + ATTR;
   else
   {
   if(pblip==0)
	screen[0][79]=' ' + ATTR;
   }
/* jump to old interrupt handler */
ChainIntr(OrginalTimerHandle);
}

int main(void)
{
/* Save actual top of stack */
MySS=_SS;
MySP=_SP;
/* For a color screen the video memory  is at B800:0000.
   For a monochrome system use B000:000 */
_AH=0x0F;
geninterrupt(0x10);
if( _AL !=7)
(void far* )screen = MK_FP(0xB800,0);
else
(void far* )screen = MK_FP(0xB000,0);

_AH=0x34;geninterrupt(0x21);
InDos=MK_FP(_ES,_BX);
{
/* Zwolnienie bloku srodowiska */
unsigned int environ_seg=*((unsigned int far*)MK_FP(_psp,0x002C));/* Adres segmentu srodowiska */
asm push es;
_ES=environ_seg;		/* Wpisanie tej wartosci do _ES */
_AH=0x49;
geninterrupt(0x21);
asm pop es;
asm jc blad;
/* Powoduje uszkodzenie lancucha blokow !? */
}

/* get the addreses of interrups */
OrginalTimerHandle = getvect(INTR);
OrginalDOS21h = getvect(INTDOS);

/* install the new interrupt handler */
setvect(INTR, handler);
setvect(INTDOS, DOShandler);

/* _psp is the starting address of the
   program in memory.  The top of the stack
   is the end of the program.  Using _SS and
   _SP together we can get the end of the
   stack.  You may want to allow a bit of
   saftey space to insure that enough room
   is being allocated ie:
   (_SS + ((_SP + safety space)/16) - _psp)
*/
keep(0, (MySS + ((MySP+16)/16) - _psp));/* Juz nie wraca z tego wywolania */
blad:
	return 1;
}

/* BLOK DEFINICJI FUNKCJI IMPLEMENTUJACYCH DZIALANIA TSR-a */

unsigned char User_Id=0x0;/* Identyfikator aktualnie pracujacego uzytkownika */


/* Struktury systemowe */
/*---------------------*/
struct ffblk {  /* DOS file control block structure. */
    char      ff_reserved[21];
    char      ff_attrib;
    unsigned  ff_ftime;
    unsigned  ff_fdate;
    long      ff_fsize;
    char      ff_name[13];
    };

struct ffprivate /* Rekord transmisyjny miedzy funkcja FindFirst i FindNext */
	{
	unsigned  char disk;/*Numer  przeszukiwanego dysku*/
	char       mask[11];/*Maska przeszukiwania */
	unsigned char m_att;/*Atrybuty przeszukiwania */
	unsigned int  posit;/*Pozycja w katalogu*/
	unsigned int  JAP_k;/*JAP czytanego katalogu*/
	unsigned long LZero;/*Cztery bajty zawsze zerowe*/
	};

struct f_attrib /* Bajt atrybutow pliku */
	{
	unsigned readonly:1;
	unsigned hidden:1;
	unsigned system:1;
	unsigned label:1;
	unsigned subdir:1;
	unsigned archive:1;
	unsigned free6:1;
	unsigned free7:1;
	};

struct dir_entry /* Rekord katalogu */
	{
	char name[8];
	char ext[3];
	struct f_attrib attr;
	unsigned char user_id;
	char free[9];
	unsigned int  ftime;
	unsigned int  fdate;
	unsigned int   fJAP;
	unsigned long fsize;
	};

struct DPB /* Blok parametrow dysku */
	{
	unsigned char DiscNum;	/* Numer dysku */
	unsigned char InObsNum;	/* Numer jednostki w programie obslugi */
	unsigned int  SecSize;	/* Wielkosc sektora w bajtach */
	unsigned char SecJAP_1;	/* Liczba sektorow w JAP-1 */
	unsigned char logSecJAP;/* log2 z liczby sektorow w JAP */
	unsigned int  SecReserved;/* Liczba sektorow zarezarwowanych przed FAT-em */
	unsigned char FATnum; 	/* Liczba kopi FAT */
	unsigned int  FileMax;	/* Maksymalna liczba plikow w katalogu glownym */
	unsigned int  FirstData;/*Pierwszy sektor danych */
	unsigned int  MaxJAP; 	/* Najwiekszy numer JAP - liczba JAP przeznaczonych na dane +1*/
	unsigned int  FATSize; 	/* Liczba sektorow zajetych przez tablice FAT */
	unsigned int  RootFirst;/* Numer pierwszego sektora katalogu */
	void far *    Procedure;/* Wskaznik do programu obslugi dysku */
	unsigned char MediaBayt;/* Bajt identyfikacji nosnika */
	unsigned char AccesBayt;/* Bajt dostepu do dysku - 0xFF - nie zadano */
	struct DPB far*  Follow;/* Wskaznik do nastepnego DPB - 0xffff -ostatni */
	unsigned int  FistUnUse;/* Pierwsza wolna JAP na dysku */
	unsigned int  UnUsedJAP;/* Liczba wolnych JAP na dysku - 0xffff - nie znana */
	};

/* Funkcja  pobierania bloku parametrow dysku */
/*-------------------------------------------*/
struct DPB far* GetDPB(unsigned char D)
{
struct DPB far* retval;
unsigned char blad;

_DL=D;
_AH=0x32;
asm push ds;/* Save DATA segment */
OrginalDOS21h();
blad=_AL;
retval=(struct DPB far*)MK_FP(_DS,_BX);
asm pop ds;/* Restore DATA segment */

if(blad!=0) return NULL;
   else   return retval;
}

void SetDTA(void far* NewDTA)
{
asm push ds;
_DX=FP_OFF(NewDTA);
_DS=FP_SEG(NewDTA);
_AH=0x1A;
asm pushf
asm call cs:[OrginalDOS21h];/* Bo DS moze miec inna wartosc ! */
asm pop ds;
}

void far* GetDTA(void)
{
_AH=0x2F;
OrginalDOS21h();
return MK_FP(_ES,_BX);
}

int FindFirst(char far * path,void far* LocalDTA,unsigned char attr)
{
unsigned int ecode,flag;
void far* OldDTA=GetDTA();/* Bufor transmisji wywolujacego */
SetDTA(LocalDTA);	  /* Bufor tramsmisji TSR-a */

asm push ds;
asm mov cx,attr;
_DX=FP_OFF(path);
_DS=FP_SEG(path);
asm mov ah,0x4E;
asm mov al,0;
asm pushf
asm call cs:[OrginalDOS21h];/* Bo DS juz ma inna wartosc ! */
asm pop ds;
ecode=_AX;
flag=_FLAGS;

SetDTA(OldDTA);		/* Z powrotem bufor transmisji wywolujacego */
if( flag & CARRY )
	{ error=ecode;return -1; }/* Kod bledu zwrocony programowi wywolujacemu */
	else return 0;/*OK ! opis w obszarze DTA .*/
}

/* Funkcja czytania sektora z dysku */
/*----------------------------------*/
int AbsRead(int drive, int nsects,unsigned long lsect, void *buffer)
{
struct par { unsigned long lsect;
		       int nsect;
	       void far * buffer;
	   }  ;
struct par param;
int bladv;

param.lsect=lsect;
param.nsect=nsects;
param.buffer=buffer;
_BX=FP_OFF(&param);
_DS=FP_SEG(&param);
_CX=-1;
_AL=drive;
asm{	int 25H
	jc blad
	popf}
return 0;
blad:
bladv=_AX;
asm popf     ;
error=bladv;
return -1;
}

int AbsWrite(int drive, int nsects,unsigned long lsect, void *buffer)
{
struct par { unsigned long lsect;
		       int nsect;
	       void far * buffer;
	   }  ;
struct par param;
int bladv;
param.lsect=lsect;
param.nsect=nsects;
param.buffer=buffer;
_BX=FP_OFF(&param);
_DS=FP_SEG(&param);
_CX=-1;
_AL=drive;
asm{	int 26H
	jc blad
	popf}
return 0;

blad:
bladv=_AX;
asm popf     ;
error=bladv;
return -1;
}

/* Cache do czytania FAT-u */
/*-------------------------*/
unsigned FATbuf[128];	/* Bufor do wczytywania sektorow FATu */
char diskFAT=0xff;  	/* Z ktorego dysku jest aktualnie wczytany sektor */
unsigned long old_sectorFAT=0xffffffff;/* Numer aktualnie wczytanego sektora */
extern unsigned char read_req; /* Flaga nieaktualnosci informacji, ta co dla katalogow*/

unsigned NextJAP(struct DPB far* ThisDPB,unsigned  JAP)
{
unsigned sector;
unsigned poz;
if(ThisDPB->MediaBayt==0xF8/*Dysk twardy FAT16 */)
{
sector=(JAP/256)+ThisDPB->SecReserved;
poz=(JAP%256);
if(ThisDPB->DiscNum !=diskFAT || sector!=old_sectorFAT || read_req)
	{
	if( AbsRead(ThisDPB->DiscNum, 1, sector, &FATbuf)!= 0)
	       return 0x0FF1;/* Blad odczytu */
	       else
	       {
	       diskFAT=ThisDPB->DiscNum;
	       old_sectorFAT=sector;
	       }
	}
return FATbuf[poz];
}
else /* Najpewniej dyskietka - FAT12*/
{
unsigned pom;
poz=(JAP*3)/2; /* <=> Trunc(JAP*1.5) */
sector=(poz/512)+ThisDPB->SecReserved;
if(ThisDPB->DiscNum !=diskFAT || sector!=old_sectorFAT || read_req)
	{
	if( AbsRead(ThisDPB->DiscNum, 1, sector, &FATbuf)!= 0)
	       return 0x0FF1;/* Blad odczytu */
	       else
	       {
	       diskFAT=ThisDPB->DiscNum;
	       old_sectorFAT=sector;
	       }
	}
poz%=512;/* In read sector */
pom=JAP;
JAP=*((unsigned*)( ((char*)FATbuf+poz) ));
if(pom&1!=0)
	{
	JAP&=0xFFF0;
	JAP>>=4;
	return JAP;
	}
	else
	{
	JAP&=0x0FFF;
	return JAP;
	}
}
}

int ChainDirFAT(struct DPB far* ThisDPB,unsigned  *SJAP,unsigned  *SEnt)
{
#define JAP (*SJAP)
#define Ent (*SEnt)
unsigned i,n=(Ent/16)/(ThisDPB->SecJAP_1+1); /* Okresla ile FATentries trzeba przeczytac zeby dotrzec do wlasciwej */
for(i=1;i<=n;i++)/* Moze nie wykonac sie ani razu jesli sektor jest w podanej JAP */
	{
	JAP=NextJAP(ThisDPB,JAP);
	if((JAP&0x0FFF)>=0x0FF0 || JAP==0) /* Blad odczytu lub parametrow */
		return 0;
	Ent-=(16*(ThisDPB->SecJAP_1+1));
	}
return 1;
#undef JAP
#undef Ent
}

/* Cache do pobierania sektorow katalogow */
/*----------------------------------------*/
unsigned char read_req=1; /* Flaga nieaktualnosci informacji */
unsigned char write_req=0;/* Flaga koniecznosci zapisu */
unsigned char disk=0xff;  /* Z ktorego dysku jest aktualnie wczytany sektor */
unsigned long old_sector=0xffffffffL;/* Numer aktualnie wczytanego sektora */
unsigned char bufor[512]; /* Bufor na wczytywanie segmentow katalogow */

int writedirsec(void)/* Aktualizuje obraz dyskowy sektora z Cach'u - zwraca 1 jesli nie wykonal*/
{
if(write_req && old_sector!=0xffffffffL && disk!=0xff)
	{/* Jesli bylo cos zmieniane to trzeba zapisac */
	//sound(100);
	 if( AbsWrite(disk-1,1,old_sector,&bufor) !=0 )
		{
	  //	nosound();
	      return 1;
	      }
	//nosound();
	}
write_req=0;
return 0;
}

int  loaddirsec(unsigned char Disc,unsigned JAP,unsigned *Ent)/* Ladowanie - zwraca 1 jesli blad */
{
struct DPB far* ThisDPB=GetDPB(Disc);
unsigned long sector=0xffffffffL;

if(ThisDPB==NULL || (ThisDPB->SecSize>512) )
	return  1;

if(JAP==0)
 sector=ThisDPB->RootFirst;
 else
 {
 if(ChainDirFAT(ThisDPB,&JAP,Ent)==0)
		return  1;
 sector=((unsigned long)JAP-2)*(ThisDPB->SecJAP_1+1)+ThisDPB->FirstData;
 }
 sector+=((*Ent)/16); /* Powieksza jesli nie w pierwszym sektorze katalogu */
 *Ent=((*Ent)%16);    /* Pozycja we wczytanym sektorze */

if(sector!=old_sector || read_req)/* Czytanie gdy inny sektor lub mozliwosc dezaktualizacji */
	{
	if(writedirsec()) return 1;/* Aktualizacja jesli trzeba - wypada jesli blad ! */
	if( AbsRead(Disc-1, 1, sector, &bufor)!= 0)
	       return 1;
	       else{
		disk=Disc;/* Zapamientanie parametrow wczytanego sektora */
		old_sector=sector;
		read_req=0;
		write_req=0;
		}
	}
return 0;
}

struct dir_entry* pass_to_entry(char far* path)
/* Funkcja sprawdza czy aktualny "user" ma prawo grzebac w tym pliku */
/* Jesli tak to zwraca wskaznik do CASHU - jesli nie to NULL */
{
struct dir_entry* dent=NULL;
  /* Szukanie pozycji katalogu */
char myDTA[sizeof(struct ffblk)];

if(FindFirst(path,myDTA,0xFF)==0) /* OK -opis w myDTA */
   {
    struct ffprivate* ffp=(struct ffprivate* )myDTA;
    unsigned int posit=ffp->posit;	/* Pozycja w katalogu */
    if(loaddirsec(ffp->disk,ffp->JAP_k,&posit)!=0)/* Ladowanie - zwraca 1 jesli blad */
				goto ERROR;
    dent=((struct dir_entry*)bufor+posit); /* Obliczenie adresu w buforze */
    if(!(            /* D O S T E P N Y   D L A */
       User_Id==1	||          /* SUPERUSER */
       dent->user_id==0	||	    /* jest ogolnie dostepny */
       dent->user_id==User_Id       /* ten user */
       ) )
       goto NO_PASS;    /* TO NIE WOLNO ! */
   return dent;	/* WOLNO ! */
   }
   else goto ERROR;

	/* Zmiana danych w uchwycie zabroniona !!! */
NO_PASS:		/* Kod bledu do rejestru powrotnego struktury r */
r.r_ax=5 ;
r.r_flags|=CARRY;
return NULL;
ERROR:			/* Wartosci bledow do  struktory rejestrowej r - zwracane do wywolujacego */
r.r_ax=error;
r.r_flags|=CARRY;	/* Bledy systemowe uniemozliwiajace odczyt lub zapis */
return NULL;
}

/* Glowna funkcja sterujaca praca TSR-a */
int Cerber21H()
{
switch(r.r_ax){
case 0xff00:r.r_ax=0x0f0f;r.r_cx=User_Id;	/*FF00H - Get User_ID 	*/
	break;					/* returned in CX    	*/
						/*and 0F0F in AX for 	*/
						/* detected TSR */
case 0xff01:if(r.r_cx==HiPass|| r.r_dx==LoPass) /* FF01H - Settings User_Id 	*/
			{		        /* Jesli wywolywacz podal	*/
			r.r_ax=0x0;             /* prawidlowa wartosc Pass	*/
			User_Id=r.r_bx;		/* To za Id wstawia wartosc z bx*/
			}
			else			/* Jesli nie to zwraca blad 5	*/
			{
			r.r_ax=5;		/* Kod bledu do rejestru powrotnego */
			r.r_flags|=CARRY;       /* I flaga bledu  ustawione ! 	    */
			} break;
case 0xff02:					/* FF02H - Set file owner */
	{
	struct dir_entry *this=NULL;
	if( (this=pass_to_entry(MK_FP(r.r_ds,r.r_dx)))!=NULL)/* Odszukanie pozycji wg nazwy */
		/* Zmiana user'a DOZWOLONA wiec zmieniamy */
		{
		this->user_id=r.r_cx;
		write_req=1;	/* Zmieniono wiec wymaga zapisu */
		if(writedirsec()!=0)
				{
				r.r_ax=29;
				r.r_flags|=CARRY;
				break;
				}
		r.r_ax=0;
		r.r_flags&=NO_CARRY;
		}
	}break;
case 0x4301:
	{
	struct dir_entry *this=NULL;
	if( (this=pass_to_entry(MK_FP(r.r_ds,r.r_dx)))!=NULL)/* Odszukanie pozycji wg nazwy */
		/* Zmiana atrybutow DOZWOLONA wiec zmieniamy */
		{
		/* Nie mozna nadac attr. katalogu ani etykiety */
		struct f_attrib pom;
		asm{ mov ax,r.r_cx;
		     and ax,ATTR_MASK;
		     mov pom,ax; }
		this->attr=pom;
		this->user_id=User_Id;
		write_req=1;	/* Zmieniono wiec wymaga zapisu */
		if(writedirsec()!=0)
				{
				r.r_ax=29;
				r.r_flags|=CARRY;
				break;
				}
		r.r_ax=0;
		r.r_flags&=NO_CARRY;
		}
	}break;
}
return 0;
}
