#include <stdio.h>    // Pour perror
#include <unistd.h>   // Pour close et usleep
#include <fcntl.h>    // Pour open
#include <sys/mman.h> // Pour mmap
#include <stdint.h>   // Pour uint32_t

#define GPIO_BASE_ADDR 0xFF203000
#define PAGE_SIZE 4
#define DELAY_MS 100000 // Délai en microsecondes (500ms)
#define LED_COUNT 9

int main() {
   int fd;
   volatile uint32_t *gpio_addr;

   // Ouvrir la mémoire physique
   fd = open("/dev/mem", O_RDWR | O_SYNC);
   if (fd < 0) {
       perror("Erreur lors de l'ouverture de /dev/mem");
       return -1;
   }

   // Mapper l'adresse physique
   gpio_addr = (uint32_t *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO_BASE_ADDR);
   if (gpio_addr == MAP_FAILED) {
       perror("Erreur lors du mmap");
       close(fd);
       return -1;
   }

   // Chenillard : allumer chaque LED séquentiellement
   while (1) {
       for (int i = 0; i < LED_COUNT; i++) {
           // Allumer la LED i
           printf("Allumage de la LED %d...\n", i + 1);
           *gpio_addr = (1 << i);

           // Pause pour laisser la LED allumée
           usleep(DELAY_MS);

           // Éteindre toutes les LEDs avant de passer à la suivante
           printf("Extinction de la LED %d...\n", i + 1);
           *gpio_addr = 0x00;

           // Pause pour observer l'extinction
           usleep(DELAY_MS);
       }
   }

   // Nettoyer (ce code ne sera jamais atteint dans la boucle infinie)
   munmap((void *)gpio_addr, PAGE_SIZE);
   close(fd);

   return 0;
}