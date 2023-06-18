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
} seitentabelle[1023]; // 4194303 >> 12 = 1023

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
    uint32_t offset = virt_address & 0xFFF; // Untere 12 Bit der virtuellen Adresse
    return (seitentabelle[seitennummer].page_frame << 12) | offset;
}

int8_t is_mem_full()
{
    for (uint32_t i = 0; i < 1023; i++)
    {
        if (seitentabelle[i].page_frame == -1)
        {
            return 0; // Found an empty page frame
        }
    }
    return 1; // All page frames are used
}

int8_t write_page_to_hd(uint32_t seitennummer)
{
    uint32_t start_address = seitennummer << 12; // convert page number to address
    for (uint32_t i = 0; i < 4096; i++)
    {
        hd_mem[start_address + i] = ra_mem[(seitentabelle[seitennummer].page_frame << 12) + i];
    }
    return 0;
}

uint16_t swap_page(uint32_t virt_address)
{
    static uint32_t next_page_to_swap = 0; // This is just a simple round-robin strategy
    if (seitentabelle[next_page_to_swap].dirty_bit)
    {
        write_page_to_hd(next_page_to_swap);
    }
    uint16_t swapped_page_frame = seitentabelle[next_page_to_swap].page_frame;
    seitentabelle[next_page_to_swap].page_frame = -1;
    seitentabelle[next_page_to_swap].present_bit = 0;
    next_page_to_swap = (next_page_to_swap + 1) % 1023;
    return swapped_page_frame;
}

int8_t get_page_from_hd(uint32_t virt_address)
{
    /**
	 * Lädt eine Seite von der Festplatte und speichert diese Daten im ra_mem (Arbeitsspeicher).
	 * Erstellt einen Seitentabelleneintrag.
	 * Wenn der Arbeitsspeicher voll ist, muss eine Seite ausgetauscht werden.
	 */
    uint32_t seitennummer = get_seiten_nr(virt_address);
    if (is_mem_full())
    {
        seitentabelle[seitennummer].page_frame = swap_page(virt_address);
    }
    else
    {
        // Find the first free page frame
        for (uint32_t i = 0; i < 1023; i++)
        {
            if (seitentabelle[i].page_frame == -1)
            {
                seitentabelle[seitennummer].page_frame = i;
                break;
            }
        }
    }
    uint32_t start_address = seitennummer << 12; // convert page number to address
    for (uint32_t i = 0; i < 4096; i++)
    {
        ra_mem[(seitentabelle[seitennummer].page_frame << 12) + i] = hd_mem[start_address + i];
    }
    seitentabelle[seitennummer].present_bit = 1;
    seitentabelle[seitennummer].dirty_bit = 0;
    return 0;
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

int main(void)
{
    printf("starting...");
    puts("test driver_");
    uint8_t hd_mem_expected[4194303];
    srand(1);
    fflush(stdout);
    for (int i = 0; i < 4194303; i++)
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
    // printf("Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);

    printf("PASSED First Tests \n");

    srand(3);

    for (uint32_t i = 0; i <= 1000; i++)
    {
        uint32_t zufallsadresse = rand() % 4095 * 7;
        uint8_t value = (uint8_t)zufallsadresse >> 1;
        set_data(zufallsadresse, value);
        hd_mem_expected[zufallsadresse] = value;
        printf("i : %d set_data address: %d - %d value at ram: %d\n", i, zufallsadresse, (uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
    }

    for (uint32_t i = 0; i <= 1000; i++)
    {
        uint32_t zufallsadresse = i * 4095 + 1; // i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem[zufallsadresse] != value)
        {
            printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i < 1023; i++)
            {
                // printf("%d,%d-",i,seitentabelle[i].present_bit);
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

    printf("PASSED 2. Tests \n");

    srand(3);

    

    srand(4);
    for (uint32_t i = 0; i <= 16; i++)
    {
        uint32_t zufallsadresse = rand() % 4194303; // i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem_expected[zufallsadresse] != value)
        {
            printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i < 1023; i++)
            {
                // printf("%d,%d-",i,seitentabelle[i].present_bit);
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
            for (uint32_t i = 0; i < 1023; i++)
            {
                // printf("%d,%d-",i,seitentabelle[i].present_bit);
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
