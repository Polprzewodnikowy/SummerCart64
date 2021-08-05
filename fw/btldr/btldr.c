#define LED     *((volatile unsigned int *) 0x80000000)

volatile int counter = 0;

int main (void) {
    while (1) {
        if (counter++ == 0x000FFFFF) {
            LED ^= 1;
            counter = 0;
        }
    }
}

void irq_handler(void) {
    LED = 1;
    while (1);
}
