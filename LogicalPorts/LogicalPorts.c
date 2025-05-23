#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

// Pinos do I2C
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Pinos dos LEDs
const uint red_pin = 13;
const uint green_pin = 11;
const uint blue_pin = 12;

// Botões A e B
const uint button_a = 5;
const uint button_b = 6;

// Joystick
const uint joy_x = 26; // ADC0
const uint joy_y = 27; // ADC1

// Definições para as portas lógicas
enum LogicGate {AND, OR, NOT, NAND, NOR, XOR, XNOR};
enum LogicGate current_gate = AND; // Inicializa a porta lógica como AND

// Função para atualizar o LED RGB
void set_led_color(bool R, bool G, bool B) {
    gpio_put(red_pin, R);
    gpio_put(green_pin, G);
    gpio_put(blue_pin, B);
}

// Função para desenhar o nome da porta no OLED
void draw_gate_name(enum LogicGate gate) {
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    // Portas lógicas
    char *gate_names[] = {
        "AND", "OR", "NOT", "NAND", "NOR", "XOR", "XNOR"
    };

    ssd1306_draw_string(ssd, 20, 24, gate_names[gate]);
    render_on_display(ssd, &frame_area);
}

// Função para ler os botões (retorna 1 se não pressionado, 0 se pressionado)
int read_button(uint gpio) {
    return gpio_get(gpio);
}

// Função para ler o joystick no eixo X
int read_joystick_x() {
    adc_select_input(0);
    return adc_read();
}

// Função para mudar a porta lógica
void update_gate() {
    int x_value = read_joystick_x();
    
    if (x_value > 4000) { // direita
        current_gate = (current_gate + 1) % 7;
        draw_gate_name(current_gate);
        sleep_ms(300); // debounce
    }
    else if (x_value < 1000) { // esquerda
        current_gate = (current_gate + 6) % 7; // decrementa e faz loop
        draw_gate_name(current_gate);
        sleep_ms(300); // debounce
    }
}

// Função para aplicar a lógica da porta
int apply_gate(enum LogicGate gate, int a, int b) {
    switch(gate) {
        case AND: return a & b;
        case OR:  return a | b;
        case NOT: return !a;
        case NAND: return !(a & b);
        case NOR: return !(a | b);
        case XOR: return a ^ b;
        case XNOR: return !(a ^ b);
        default: return 0;
    }
}

void setup() {
    stdio_init_all(); // Inicializa a comunicação serial

    adc_init(); // Inicializa o ADC
    adc_gpio_init(26); // ADC0
    adc_gpio_init(27); // ADC1

    // Configuração dos pinos GPIO para os LEDs
    gpio_init(red_pin);
    gpio_init(green_pin);
    gpio_init(blue_pin);
    gpio_set_dir(red_pin, GPIO_OUT);
    gpio_set_dir(green_pin, GPIO_OUT);
    gpio_set_dir(blue_pin, GPIO_OUT);

    // Inicialmente, desligar o LED RGB
    gpio_init(button_a);
    gpio_init(button_b);
    gpio_set_dir(button_a, GPIO_IN);
    gpio_set_dir(button_b, GPIO_IN);
    gpio_pull_up(button_a);
    gpio_pull_up(button_b);

    // Inicializar I2C e display
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display e renderização
    ssd1306_init();
    draw_gate_name(current_gate);
}

int main() {
    setup(); // Configuração inicial

    while (true) {
        update_gate(); // Atualiza a porta lógica com o joystick

        int a = read_button(button_a);
        int b = read_button(button_b);

        int result;
        if (current_gate == NOT) {
            result = apply_gate(current_gate, a, 0); // só botão A
        } else {
            result = apply_gate(current_gate, a, b);
        }

        if (result) {
            set_led_color(0, 1, 0); // Verde para 1
        } else {
            set_led_color(1, 0, 0); // Vermelho para 0
        }

        sleep_ms(100);
    }

    return 0;
}
