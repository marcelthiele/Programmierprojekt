#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/random.h>

uint8_t hd_mem[4194303];
uint8_t ra_mem[65535];

struct seitentabellen_zeile
{
    uint8_t present_bit;
    uint8_t dirty_bit;
    int8_t page_frame;

    int16_t prev;
    int16_t next;
} seitentabelle[1024]; // 4194303 >> 12 = 1023

int16_t lru = -1;
int16_t mru = -1;
int8_t queue_length = 0;

uint16_t get_seiten_nr(uint32_t virt_address)
{
    /**
     *
     */
    return virt_address >> 12;
}

int8_t check_present(uint32_t virt_address)
{
    /**
     * Wenn eine Seite im Arbeitsspeicher ist, gibt die Funktion "check_present" 1 zurück, sonst 0
     */
    return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

uint16_t virt_2_ram_address(uint32_t virt_address)
{
    uint32_t seitennummer = get_seiten_nr(virt_address);
    uint16_t offset = (uint16_t)(virt_address & ((1 << 12) - 1)); // Untere 12 Bit der virtuellen Adresse
    uint16_t ram_address = ((uint16_t)(seitentabelle[seitennummer].page_frame) << 12) | offset;
    return ram_address;
}

int8_t is_mem_full()
{
    return queue_length >= ((65535 + 1) / (1 << 12));
}

void copy_page_from_to(uint8_t *source, uint8_t *dest){
    uint8_t *source_copy = source;
    uint8_t *dest_copy = dest;

    while(source_copy < source + (1 << 12)){
        *(dest_copy++) = *(source_copy++);
    }
}

int8_t write_page_to_hd(uint32_t seitennummer)
{
    u_int32_t hd_address = seitennummer * (1 << 12);
    uint16_t ra_address = ((uint16_t)(seitentabelle[seitennummer].page_frame)) << 12;

    copy_page_from_to(&ra_mem[ra_address],&hd_mem[hd_address]);
}

//Queue initialisieren
void initqueue(int16_t seitennummer){
    seitentabelle[seitennummer].next = -1;
    seitentabelle[seitennummer].prev = -1;

    lru = seitennummer;
    mru = seitennummer;
}

// Fügt eine Seite zur Queue hinzu
void enqueue(int seitennummer) {
    seitentabelle[mru].prev = seitennummer;
    seitentabelle[seitennummer].next = mru;

    mru = seitennummer;
}


// Entfernt eine Seite aus der Queue
int16_t dequeue() {
    int16_t temp = lru;
    lru = seitentabelle[lru].prev;
    seitentabelle[lru].next = -1;

    return temp;
}


void write_lru_to_hd(){
    int16_t oldLru = dequeue();

    if(seitentabelle[oldLru].dirty_bit)
        write_page_to_hd(oldLru);
    
    seitentabelle[oldLru].dirty_bit = 0;
    seitentabelle[oldLru].present_bit = 0;
    seitentabelle[oldLru].page_frame = -1;
    
}




int8_t get_page_from_hd(uint32_t virt_address)
{
    /**
	 * Lädt eine Seite von der Festplatte und speichert diese Daten im ra_mem (Arbeitsspeicher).
	 * Erstellt einen Seitentabelleneintrag.
	 * Wenn der Arbeitsspeicher voll ist, muss eine Seite ausgetauscht werden.
	 */

    uint32_t seitennummer = get_seiten_nr(virt_address);
    seitentabelle[seitennummer].present_bit = 1;

    if(queue_length == 0)
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

    u_int32_t hd_address = seitennummer * (1 << 12);
    uint16_t ra_address = ((uint16_t)(seitentabelle[seitennummer].page_frame)) << 12;

    copy_page_from_to(&hd_mem[hd_address], &ra_mem[ra_address]);
}


uint8_t get_data(uint32_t virt_address)
{
    if (!check_present(virt_address))
    {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    return ra_mem[ram_address];
}

void set_data(uint32_t virt_address, uint8_t value)
{
    /**
	 * Schreibt ein Byte in den Arbeitsspeicher zurück.
	 * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
	 * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
	 */
    if (!check_present(virt_address))
    {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    ra_mem[ram_address] = value;
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;
}

int main(void) // Vollständig unverändert
{
    puts("test driver_");
    uint8_t hd_mem_expected[4194303];
    srand(1);
    fflush(stdout);
    for (int i = 0; i <= 4194303; i++)
    {
        uint8_t val = (uint8_t)rand();
        hd_mem[i] = val;
        hd_mem_expected[i] = val;
    }

    for (uint32_t i = 0; i < 1023; i++)
    {
        seitentabelle[i].dirty_bit = 0;
        seitentabelle[i].page_frame = -1;
        seitentabelle[i].present_bit = 0;
    }

    uint32_t zufallsadresse = 4192426;
    uint8_t value = get_data(zufallsadresse);

    if (hd_mem[zufallsadresse] != value)
    {
        printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
    }

    value = get_data(zufallsadresse);

    if (hd_mem[zufallsadresse] != value)
    {
        printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
    }
    printf("Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);

    srand(3);

    for (uint32_t i = 0; i <= 1000; i++)
    {
        uint32_t zufallsadresse = rand() % 4194304; // i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem[zufallsadresse] != value)
        {
            printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1023; i++)
            {
                printf("%d,%d-",i,seitentabelle[i].present_bit);
                if (seitentabelle[i].present_bit)
                {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }
            exit(1);
        }
        printf("i: %d data @ %u: %d hd value: %d\n", i, zufallsadresse, value, hd_mem[zufallsadresse]);
        fflush(stdout);
    }

    srand(3);

    for (uint32_t i = 0; i <= 100; i++)
    {
        uint32_t zufallsadresse = rand() % 4095 * 7;
        uint8_t value = (uint8_t)zufallsadresse >> 1;
        set_data(zufallsadresse, value);
        hd_mem_expected[zufallsadresse] = value;
        printf("i : %d set_data address: %d - %d value at ram: %d\n", i, zufallsadresse, (uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
    }

    srand(4);
    for (uint32_t i = 0; i <= 16; i++)
    {
        uint32_t zufallsadresse = rand() % 4194304; // i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem_expected[zufallsadresse] != value)
        {
            printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1023; i++)
            {
                printf("%d,%d-",i,seitentabelle[i].present_bit);
                if (seitentabelle[i].present_bit)
                {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }

            exit(2);
        }
        printf("i: %d data @ %u: %d hd value: %d\n", i, zufallsadresse, value, hd_mem[zufallsadresse]);
        fflush(stdout);
    }

    srand(3);
    for (uint32_t i = 0; i <= 2500; i++)
    {
        uint32_t zufallsadresse = rand() % (4095 * 5); // i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem_expected[zufallsadresse] != value)
        {
            printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem_expected[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1023; i++)
            {
                printf("%d,%d-",i,seitentabelle[i].present_bit);
                if (seitentabelle[i].present_bit)
                {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }
            exit(3);
        }
        printf("i: %d data @ %u: %d hd value: %d\n", i, zufallsadresse, value, hd_mem_expected[zufallsadresse]);
        fflush(stdout);
    }

    puts("test end");
    fflush(stdout);
    return EXIT_SUCCESS;
}