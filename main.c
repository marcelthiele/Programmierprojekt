#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/random.h>

// Konstanten und Speicherbereiche, die im gesamten Code verwendet werden
uint8_t hd_mem[4194304]; // Festplattenspeicher: 4 MB
uint8_t ra_mem[65536]; // RAM: 64 KB

// Struktur für Seitentabelleneintrag
struct seitentabellen_zeile {
	uint8_t present_bit; // 1, wenn die Seite im RAM ist; 0 sonst
	uint8_t dirty_bit; // 1, wenn die Seite seit dem Laden aus der Festplatte geändert wurde; 0 sonst
	int8_t page_frame; // Speicheradresse im RAM, wo die Seite geladen ist
}seitentabelle[1024]; // 4MB / 4KB = 1024 Seiteneinträge

// Struktur für Seitentabellen-Queue zur Implementierung des LRU-Algorithmus
struct seitentabelle_zeile_queue{
    int16_t prev; // Vorheriger Eintrag in der Queue
    int16_t next; // Nächster Eintrag in der Queue
}seitentabelle_queue[1024];

int16_t lru = -1; // Letzter Verwendeter (Least Recently Used)
int16_t mru = -1; // Meist Verwendeter (Most Recently Used)
int8_t queue_length = 0; // Anzahl der momentan im RAM befindlichen Seiten

// Ermittelt die Seitennummer aus der gegebenen virtuellen Adresse
uint16_t get_seiten_nr(uint32_t virt_address) {
    return virt_address >> 12; // 4KB-Seite -> 12 Bits für den Offset
}

// Überprüft, ob die Seite der gegebenen virtuellen Adresse im RAM ist
int8_t check_present(uint32_t virt_address) {
    return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

// Übersetzt eine virtuelle Adresse in eine RAM-Adresse
uint16_t virt_2_ram_address(uint32_t virt_address)
{
    uint32_t seitennummer = get_seiten_nr(virt_address);
    uint16_t offset = (uint16_t)(virt_address & ((1 << 12) - 1)); // Untere 12 Bit der virtuellen Adresse
    uint16_t ram_address = ((uint16_t)(seitentabelle[seitennummer].page_frame) << 12) | offset;
    return ram_address;
}

// Überprüft, ob der RAM voll ist
int8_t is_mem_full()
{
    return queue_length >= ((65536) / (1 << 12));
}

// Kopiert eine Seite von der Quell- zur Zieladresse
void copy_page_from_to(uint8_t *source, uint8_t *dest)
{
    uint8_t *source_copy = source;
    uint8_t *dest_copy = dest;

    while (source_copy < source + (1 << 12))
    {
        *(dest_copy++) = *(source_copy++);
    }
}

// Schreibt eine Seite aus dem RAM auf die Festplatte
void write_page_to_hd(uint32_t seitennummer)
{
    u_int32_t hd_address = seitennummer * (1 << 12);
    uint16_t ra_address = ((uint16_t)(seitentabelle[seitennummer].page_frame)) << 12;

    copy_page_from_to(&ra_mem[ra_address], &hd_mem[hd_address]);
}

// Initialisiert die Queue
void initqueue(int16_t seitennummer)
{
    seitentabelle_queue[seitennummer].next = -1;
    seitentabelle_queue[seitennummer].prev = -1;

    lru = seitennummer;
    mru = seitennummer;
}

// Fügt eine Seite zur Queue hinzu
void enqueue(int seitennummer)
{
    seitentabelle_queue[mru].prev = seitennummer;
    seitentabelle_queue[seitennummer].next = mru;

    mru = seitennummer;
}

// Entfernt eine Seite aus der Queue
int16_t dequeue()
{
    int16_t temp = lru;
    lru = seitentabelle_queue[lru].prev;
    seitentabelle_queue[lru].next = -1;

    return temp;
}

// Schreibt die am wenigsten zuletzt verwendete Seite (LRU) auf die Festplatte, falls sie geändert wurde
void write_lru_to_hd()
{
    int16_t oldLru = dequeue();

    if (seitentabelle[oldLru].dirty_bit)
        write_page_to_hd(oldLru);

    seitentabelle[oldLru].dirty_bit = 0;
    seitentabelle[oldLru].present_bit = 0;
    seitentabelle[oldLru].page_frame = -1;
}

/**
	 * Lädt eine Seite von der Festplatte in den RAM
     * Erstellt einen Seitentabelleneintrag.
     * Wenn der Arbeitsspeicher voll ist, muss eine Seite ausgetauscht werden.
*/
void get_page_from_hd(uint32_t virt_address)
{

    uint32_t seitennummer = get_seiten_nr(virt_address);
    seitentabelle[seitennummer].present_bit = 1;

    if (queue_length == 0)
        initqueue(seitennummer);
    else
        enqueue(seitennummer);

    if (is_mem_full())
    {
        seitentabelle[seitennummer].page_frame = seitentabelle[lru].page_frame;
        write_lru_to_hd();
    }
    else
    {
        seitentabelle[seitennummer].page_frame = queue_length++;
    }

    uint32_t hd_address = seitennummer * (1 << 12);
    uint16_t ra_address = ((uint16_t)(seitentabelle[seitennummer].page_frame)) << 12;

    copy_page_from_to(&hd_mem[hd_address], &ra_mem[ra_address]);
}

// Gibt den Wert an der gegebenen virtuellen Adresse zurück
uint8_t get_data(uint32_t virt_address)
{
	// Wenn die virtuelle Adresse nicht bereits im RAM gespeichert ist, muss sie vom HD in den RAM geladen werden
    if (!check_present(virt_address))
    {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    return ra_mem[ram_address];
}

// Schreibt den gegebenen Wert an die gegebene virtuelle Adresse
void set_data(uint32_t virt_address, uint8_t value)
{
    /**
     * Wenn die virtuelle Adresse nicht bereits im RAM gespeichert ist,
     * muss sie vom HD in den RAM geladen werden, um dann die Daten zu überschreiben
     */
    if (!check_present(virt_address))
    {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    ra_mem[ram_address] = value;
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;
}

int main(void) {
		puts("test driver_");
	static uint8_t hd_mem_expected[4194304];
	srand(1);
	fflush(stdout);
	for(int i = 0; i < 4194304; i++) {
		//printf("%d\n",i);
		uint8_t val = (uint8_t)rand();
		hd_mem[i] = val;
		hd_mem_expected[i] = val;
	}

	for (uint32_t i = 0; i < 1024;i++) {
//		printf("%d\n",i);
		seitentabelle[i].dirty_bit = 0;
		seitentabelle[i].page_frame = -1;
		seitentabelle[i].present_bit = 0;
	}


	uint32_t zufallsadresse = 4192425;
	uint8_t value = get_data(zufallsadresse);
//	printf("value: %d\n", value);

	if(hd_mem[zufallsadresse] != value) {
		printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
	}

	value = get_data(zufallsadresse);

	if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);

	}

//		printf("Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);


	srand(3);

	for (uint32_t i = 0; i <= 1000;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}
			exit(1);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
		fflush(stdout);
	}


	srand(3);

	for (uint32_t i = 0; i <= 100;i++) {
		uint32_t zufallsadresse = rand() % 4095 *7;
		uint8_t value = (uint8_t)zufallsadresse >> 1;
		set_data(zufallsadresse, value);
		hd_mem_expected[zufallsadresse] = value;
//		printf("i : %d set_data address: %d - %d value at ram: %d\n",i,zufallsadresse,(uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
	}



	srand(4);
	for (uint32_t i = 0; i <= 16;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value) {
//			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}

			exit(2);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
		fflush(stdout);
	}

	srand(3);
	for (uint32_t i = 0; i <= 2500;i++) {
		uint32_t zufallsadresse = rand() % (4095 *5);//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value ) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem_expected[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}
			exit(3);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem_expected[zufallsadresse]);
		fflush(stdout);
	}

	puts("test end");
	fflush(stdout);
	return EXIT_SUCCESS;
}
