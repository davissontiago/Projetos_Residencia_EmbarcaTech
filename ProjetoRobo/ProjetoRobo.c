#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/binary_info.h"
#include "inc/ssd1306_i2c.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "ws2818b.pio.h"

// Definições
#define LED_COUNT 25          // Número total de LEDs na matriz
#define LED_PIN 7             // Pino de controle dos LEDs NeoPixel
#define BUTTON_A 5            // Pino do botão A
#define BUTTON_B 6            // Pino do botão B
#define I2C_SDA 14            // Pino SDA do I2C para o display SSD1306
#define I2C_SCL 15            // Pino SCL do I2C para o display SSD1306
#define BUZZER_PIN 21         // Pino do buzzer

// Configuração da frequência do buzzer (em Hz)
#define BUZZER_FREQUENCY 1000 // 1 kHz (frequência audível)

// Estrutura para um pixel GRB (usada para controlar os LEDs NeoPixel)
typedef struct {
    uint8_t G, R, B; // Componentes de cor: verde, vermelho e azul
} pixel_t;

typedef pixel_t npLED_t; // Define um tipo para os LEDs NeoPixel

// Variáveis globais
npLED_t leds[LED_COUNT]; // Buffer para armazenar as cores dos LEDs
PIO np_pio;              // Instância do PIO para controle dos LEDs
uint sm;                 // Máquina de estado do PIO

// Protótipos de funções
void npInit(uint pin);   // Inicializa a matriz de LEDs
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b); // Define a cor de um LED
void npClear();          // Limpa o buffer de LEDs
void npWrite();          // Envia os dados do buffer para os LEDs
int getIndex(int x, int y); // Calcula o índice do LED na matriz
void updateLEDs(int matriz[5][5][3]); // Atualiza os LEDs com base em uma matriz de cores
void exibir_mensagem_centralizada(uint8_t *ssd, struct render_area *frame_area, const char *mensagem); // Exibe uma mensagem no display
void pwm_init_buzzer(uint pin); // Inicializa o PWM para o buzzer
void buzzer_on(uint pin);       // Ativa o buzzer
void buzzer_off(uint pin);      // Desativa o buzzer

// Implementação das funções

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 * @param pin Pino de controle dos LEDs NeoPixel.
 */
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program); // Adiciona o programa PIO
    np_pio = pio0; // Usa o PIO 0
    sm = pio_claim_unused_sm(np_pio, false); // Obtém uma máquina de estado livre
    if (sm < 0) {
        np_pio = pio1; // Se não houver máquinas livres no PIO 0, usa o PIO 1
        sm = pio_claim_unused_sm(np_pio, true); // Obtém uma máquina de estado no PIO 1
    }
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f); // Inicializa o programa PIO
    npClear(); // Limpa o buffer de LEDs
}

/**
 * Atribui uma cor RGB a um LED.
 * @param index Índice do LED na matriz.
 * @param r Valor da componente vermelha (0-255).
 * @param g Valor da componente verde (0-255).
 * @param b Valor da componente azul (0-255).
 */
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

/**
 * Limpa o buffer de pixels (desliga todos os LEDs).
 */
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        npSetLED(i, 0, 0, 0); // Define todas as cores como 0 (apagado)
    }
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G); // Envia o componente verde
        pio_sm_put_blocking(np_pio, sm, leds[i].R); // Envia o componente vermelho
        pio_sm_put_blocking(np_pio, sm, leds[i].B); // Envia o componente azul
    }
    sleep_us(100); // Aguarda 100us (sinal de RESET do datasheet)
}

/**
 * Calcula o índice do LED na matriz com base nas coordenadas (x, y).
 * @param x Coordenada X (0-4).
 * @param y Coordenada Y (0-4).
 * @return Índice do LED na matriz.
 */
int getIndex(int x, int y) {
    if (y % 2 == 0) {
        return 24 - (y * 5 + x); // Linha par (esquerda para direita)
    } else {
        return 24 - (y * 5 + (4 - x)); // Linha ímpar (direita para esquerda)
    }
}

/**
 * Atualiza os LEDs com base em uma matriz de cores.
 * @param matriz Matriz 5x5x3 contendo as cores dos LEDs.
 */
void updateLEDs(int matriz[5][5][3]) {
    for (int linha = 0; linha < 5; linha++) {
        for (int coluna = 0; coluna < 5; coluna++) {
            int posicao = getIndex(linha, coluna); // Obtém o índice do LED
            npSetLED(posicao, matriz[coluna][linha][0], matriz[coluna][linha][1], matriz[coluna][linha][2]); // Define a cor do LED
        }
    }
    npWrite(); // Envia os dados para os LEDs
}

/**
 * Exibe uma mensagem centralizada no display SSD1306.
 * @param ssd Buffer do display.
 * @param frame_area Área de renderização.
 * @param mensagem Mensagem a ser exibida.
 */
void exibir_mensagem_centralizada(uint8_t *ssd, struct render_area *frame_area, const char *mensagem) {
    int largura_texto = strlen(mensagem) * 8; // Calcula a largura do texto em pixels
    int pos_x = (ssd1306_width - largura_texto) / 2; // Calcula a posição X para centralizar
    int pos_y = (ssd1306_height - 8) / 2; // Calcula a posição Y para centralizar
    memset(ssd, 0, ssd1306_buffer_length); // Limpa o buffer do display
    ssd1306_draw_string(ssd, pos_x, pos_y, mensagem); // Desenha a mensagem
    render_on_display(ssd, frame_area); // Atualiza o display
}

/**
 * Inicializa o PWM no pino do buzzer.
 * @param pin Pino do buzzer.
 */
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM); // Configura o pino como saída de PWM
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice do PWM
    pwm_config config = pwm_get_default_config(); // Obtém a configuração padrão do PWM
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Configura o divisor de clock
    pwm_init(slice_num, &config, true); // Inicializa o PWM
    pwm_set_gpio_level(pin, 0); // Inicia com o buzzer desligado
}

/**
 * Ativa o buzzer.
 * @param pin Pino do buzzer.
 */
void buzzer_on(uint pin) {
    pwm_set_gpio_level(pin, 2048); // Define 50% de duty cycle (som audível)
}

/**
 * Desativa o buzzer.
 * @param pin Pino do buzzer.
 */
void buzzer_off(uint pin) {
    pwm_set_gpio_level(pin, 0); // Define 0% de duty cycle (silêncio)
}

// Função principal
int main() {
    stdio_init_all(); // Inicializa as entradas e saídas padrão

    // Inicialização do I2C para o display SSD1306
    i2c_init(i2c1, ssd1306_i2c_clock * 1000); // Inicializa o I2C com a frequência especificada
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura o pino SDA
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino SCL
    gpio_pull_up(I2C_SDA); // Habilita pull-up no pino SDA
    gpio_pull_up(I2C_SCL); // Habilita pull-up no pino SCL

    // Inicializa o display SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area); // Calcula o tamanho do buffer de renderização

    // Zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length); // Limpa o buffer do display
    render_on_display(ssd, &frame_area); // Atualiza o display

    // Configura o botão A como entrada com pull-up.
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    // Configura o botão B como entrada com pull-up.
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Inicializa matriz de LEDs NeoPixel.
    npInit(LED_PIN);
    npClear(); // Limpa os LEDs

    // Inicializar o PWM no pino do buzzer
    pwm_init_buzzer(BUZZER_PIN);

    // Matrizes de cores para os LEDs.
    int matriz_pressionado[5][5][3] = {
        {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
        {{0, 0, 128}, {0, 255, 0}, {0, 0, 128}, {0, 255, 0}, {0, 0, 128}},
        {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
        {{0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}},
        {{0, 0, 128}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 128}}
    };

    int matriz_solto[5][5][3] = {
        {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
        {{0, 0, 128}, {128, 128, 128}, {0, 0, 128}, {128, 128, 128}, {0, 0, 128}},
        {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
        {{0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}},
        {{0, 0, 128}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 128}}
    };

    int matriz_apagada[5][5][3] = {
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    };

    bool botao_a_pressionado = false; // Estado do botão A

    while (true) {
        if (gpio_get(BUTTON_A) == 0) { // Botão A pressionado
            if (!botao_a_pressionado) { // Verifica se o estado mudou
                updateLEDs(matriz_pressionado); // Atualiza os LEDs
                botao_a_pressionado = true; // Atualiza o estado do botão
                exibir_mensagem_centralizada(ssd, &frame_area, "Bip Bip Bip"); // Exibe a mensagem
                buzzer_on(BUZZER_PIN); // Ativa o buzzer
            }
        } else { // Botão A não pressionado
            if (botao_a_pressionado) { // Verifica se o estado mudou
                updateLEDs(matriz_solto); // Atualiza os LEDs
                botao_a_pressionado = false; // Atualiza o estado do botão
                exibir_mensagem_centralizada(ssd, &frame_area, "ZzZ ZzZ ZzZ"); // Exibe a mensagem
                buzzer_off(BUZZER_PIN); // Desativa o buzzer
            }
        }
        
        if (gpio_get(BUTTON_B) == 0) { // Botão B pressionado
            updateLEDs(matriz_apagada); // Apaga os LEDs
            exibir_mensagem_centralizada(ssd, &frame_area, " "); // Limpa o display
        }
        
        sleep_ms(10); // Evita leitura excessiva do botão.
    }

    return 0;
}