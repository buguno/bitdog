#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "inc/ssd1306.h"
#include "ws2818b.pio.h"

// Matriz de LEDs
#define LED_COUNT 25
#define LED_PIN 7

// Pinos dos LEDs individuais
const uint LED_CT_RED = 13;
const uint LED_CT_GREEN = 11;
const uint LED_CT_BLUE = 12;

// Joystick - EIXOS INVERTIDOS CONFORME SUA PLACA
const uint JOY_X_PHYSICAL = 27;  // Este é o Y lógico
const uint JOY_Y_PHYSICAL = 26;  // Este é o X lógico

// OLED
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

typedef struct {
    bool GR;
    bool HO;
    bool DI;
    bool PT;
    uint8_t selected_input;
    bool CT;
} SystemState;

SystemState current_state = {0, 0, 0, 1, 0, 0};
SystemState previous_state = {1, 1, 1, 0, 4, 1};

// Configurações da matriz de LEDs
struct pixel_t {
    uint8_t GREEN, RED, BLUE;
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;
npLED_t leds[LED_COUNT];
PIO np_pio;
uint sm;
const uint status_leds[4] = {4, 3, 2, 1};

// Assinaturas de funções
void np_init(uint pin);
void np_set_led(const uint index, const uint8_t red, const uint8_t green, const uint8_t blue);
void np_clear();
void np_write();
void update_matrix_leds();
bool state_changed();
void update_ct_led();
void update_oled();
int8_t read_joystick_direction();
void process_inputs();
bool get_ct_state();

// Inicializa a máquina PIO para controle da matriz de LEDs
void np_init(uint pin) {
    // Cria programa PIO
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    
    // Toma posse de uma máquina PIO
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    
    // Limpa buffer de pixels
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].RED = leds[i].GREEN = leds[i].BLUE = 0;
    }
}

// Atribui uma cor RGB a um LED
void np_set_led(const uint index, const uint8_t red, const uint8_t green, const uint8_t blue) {
    if (index < LED_COUNT) {
        leds[index].RED = red;
        leds[index].GREEN = green;
        leds[index].BLUE = blue;
    }
}

// Limpa o buffer de pixels
void np_clear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        np_set_led(i, 0, 0, 0);
}

// Escreve os dados do buffer nos LEDs
void np_write() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].GREEN);
        pio_sm_put_blocking(np_pio, sm, leds[i].RED);
        pio_sm_put_blocking(np_pio, sm, leds[i].BLUE);
    }
}

// Atualiza os LEDs da matriz de acordo com o estado atual
void update_matrix_leds() {
    for (int i = 0; i < 4; i++) {
        bool state = 0;
        switch(i) {
            case 0: state = current_state.GR; break;
            case 1: state = current_state.HO; break;
            case 2: state = current_state.DI; break;
            case 3: state = !current_state.PT; break;
        }

        if (state)
            np_set_led(status_leds[i], 0, 255, 0);  // Verde se habilitado
        else
            np_set_led(status_leds[i], 255, 0, 0);  // Vermelho se desabilitado
    }

    np_write();  // Envia dados para os LEDs da matriz
}

// Verifica se o estado atual mudou em relação ao estado anterior
bool state_changed() {
    return memcmp(&current_state, &previous_state, sizeof(SystemState)) != 0;
}

// Atualiza o LED de controle de catraca
void update_ct_led() {
    gpio_put(LED_CT_RED, !current_state.CT);
    gpio_put(LED_CT_GREEN, current_state.CT);
    gpio_put(LED_CT_BLUE, 0);
}

// Atualiza o OLED com as informações do sistema
void update_oled() {
    static struct render_area frame_area = {
        start_column: 0,
        end_column: ssd1306_width - 1,
        start_page: 0,
        end_page: ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    ssd1306_draw_string(ssd, 5, 0, "Controle Catraca");

    const char* labels[] = {"GR", "HO", "DI", "PT"};
    bool states[] = {current_state.GR, current_state.HO, current_state.DI, !current_state.PT};

    for (int i = 0; i < 4; i++) {
        char line[20];
        snprintf(line, sizeof(line), "%s %s: %s",
                (current_state.selected_input == i) ? "X" : " ",
                labels[i],
                states[i] ? "ON " : "OFF");

        ssd1306_draw_string(ssd, 5, 16 + (i * 8), line);
    }

    char status[20];
    snprintf(status, sizeof(status), "CATRACA: %s", current_state.CT ? "ABERTA" : "FECHADA");
    ssd1306_draw_string(ssd, 5, 56, status);

    render_on_display(ssd, &frame_area);
}

// Lê a direção do joystick e retorna um valor correspondente
int8_t read_joystick_direction() {
    static uint32_t last_input_time = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (current_time - last_input_time < 200) return 0;

    adc_select_input(1); // Fisicamente Y → agora X lógico
    uint16_t x_logical = adc_read();
    adc_select_input(0); // Fisicamente X → agora Y lógico
    uint16_t y_logical = adc_read();

    int16_t x = (x_logical - 2048) / 20;
    int16_t y = -((y_logical - 2048) / 20);

    const int threshold = 50;

    if (x > threshold) return 1;      // Direita
    if (x < -threshold) return -1;    // Esquerda
    if (y > threshold) return 2;      // Baixo
    if (y < -threshold) return -2;    // Cima

    last_input_time = current_time;
    return 0;
}

// Processa as entradas do joystick e atualiza o estado do sistema
void process_inputs() {
    int8_t dir = read_joystick_direction();

    switch(dir) {
        case -2: current_state.selected_input = (current_state.selected_input + 3) % 4; sleep_ms(150); break;
        case  2: current_state.selected_input = (current_state.selected_input + 1) % 4; sleep_ms(150); break;
        case  1:
            if (current_state.selected_input == 3) current_state.PT = 0;
            else ((bool*)&current_state)[current_state.selected_input] = 1;
            sleep_ms(150); break;
        case -1:
            if (current_state.selected_input == 3) current_state.PT = 1;
            else ((bool*)&current_state)[current_state.selected_input] = 0;
            sleep_ms(150); break;
    }
}

// Verifica o estado da catraca
bool get_ct_state() {
    return !current_state.PT || (current_state.GR && current_state.HO && current_state.DI);
}

void setup() {
    stdio_init_all();

    adc_init();
    adc_gpio_init(JOY_X_PHYSICAL);
    adc_gpio_init(JOY_Y_PHYSICAL);

    gpio_init(LED_CT_RED);
    gpio_init(LED_CT_GREEN);
    gpio_init(LED_CT_BLUE);
    gpio_set_dir(LED_CT_RED, GPIO_OUT);
    gpio_set_dir(LED_CT_GREEN, GPIO_OUT);
    gpio_set_dir(LED_CT_BLUE, GPIO_OUT);

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

    np_init(LED_PIN);
    np_clear();
    np_write();

    update_ct_led();
    update_oled();
    update_matrix_leds();
}

int main() {
    setup();

    while(1) {
        process_inputs();
        current_state.CT = get_ct_state();

        if (state_changed()) {
            update_ct_led();
            update_oled();
            update_matrix_leds();
            previous_state = current_state;
        }

        sleep_ms(10);
    }

    return 0;
}
