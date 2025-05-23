#include "pico/stdlib.h"
#include "hardware/pio.h"

// Pinos dos LEDs individuais
const uint LED_CT_RED = 13;
const uint LED_CT_GREEN = 11;
const uint LED_CT_BLUE = 12;

// Função para atualizar os estados dos LEDs
void set_led_color(bool red, bool green, bool blue) {
    gpio_put(LED_CT_RED, red);   // Configura o estado do LED vermelho
    gpio_put(LED_CT_GREEN, green); // Configura o estado do LED verde
    gpio_put(LED_CT_BLUE, blue);  // Configura o estado do LED azul
}

void setup() {
    stdio_init_all();

    gpio_init(LED_CT_RED);
    gpio_init(LED_CT_GREEN);
    gpio_init(LED_CT_BLUE);
    gpio_set_dir(LED_CT_RED, GPIO_OUT);
    gpio_set_dir(LED_CT_GREEN, GPIO_OUT);
    gpio_set_dir(LED_CT_BLUE, GPIO_OUT);
}

int main() {
    setup();

    set_led_color(0, 0, 0);

    uint8_t current_color = 0; // 0: azul, 1: vermelho, 2: verde

    while(1) {
        set_led_color(0, 0, 0);

        // Acende a cor atual
        switch(current_color) {
            case 0: // Azul
                set_led_color(0, 0, 1);
                break;
            case 1: // Vermelho
                set_led_color(1, 0, 0);
                break;
            case 2: // Verde
                set_led_color(0, 1, 0);
                break;
        }

        // Avança para a próxima cor
        current_color = (current_color + 1) % 3;

        // Aguarda 30 segundos (30000 ms)
        sleep_ms(30000);
    }

    return 0;
}
