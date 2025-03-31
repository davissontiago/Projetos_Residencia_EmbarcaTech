/***************************************************************
 * INCLUSÕES DE BIBLIOTECAS
 **************************************************************/
#include "pico/stdlib.h"          // Biblioteca padrão do Pico
#include "hardware/adc.h"         // Para leitura ADC (temperatura)
#include "pico/cyw43_arch.h"      // Para controle Wi-Fi
#include <stdio.h>                // Para funções de I/O
#include <string.h>               // Para manipulação de strings
#include <stdlib.h>               // Para alocação de memória
#include "lwip/pbuf.h"            // Para buffers de rede
#include "lwip/tcp.h"             // Para protocolo TCP
#include "lwip/netif.h"           // Para interface de rede
#include "hardware/pio.h"         // Para controle PIO
#include "hardware/clocks.h"      // Para controle de clocks
#include "ws2818b.pio.h"          // Para LEDs NeoPixel
#include "hardware/pwm.h"         // Para controle PWM (buzzer)
#include "pico/binary_info.h"     // Para informações binárias
#include "inc/ssd1306_i2c.h"      // Para display OLED
#include "hardware/i2c.h"         // Para comunicação I2C

/***************************************************************
 * DEFINIÇÕES DE CONSTANTES E CONFIGURAÇÕES
 **************************************************************/
// Configurações de Wi-Fi
#define WIFI_SSID "XXXXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXXX"

// Definição dos pinos
#define LED_PIN CYW43_WL_GPIO_LED_PIN  // LED onboard
#define LED_BLUE_PIN 12                // LED azul
#define LED_GREEN_PIN 11               // LED verde
#define LED_RED_PIN 13                 // LED vermelho
#define LED_COUNT 25                   // Número de LEDs na matriz
#define LED_NEO_PIN 7                  // Pino dos LEDs NeoPixel
#define BUZZER_PIN 21                  // Pino do buzzer
#define I2C_SDA 14                     // Pino SDA I2C (display)
#define I2C_SCL 15                     // Pino SCL I2C (display)
#define BUZZER_FREQUENCY 6000          // Frequência do buzzer (6kHz)

/***************************************************************
 * VARIÁVEIS GLOBAIS
 **************************************************************/
// Controle do display OLED
char mensagem_display[50] = "";    // Buffer para mensagem do display
bool atualizar_display = false;    // Flag para atualização do display

// Controle do buzzer
static absolute_time_t next_buzzer_toggle;  // Próximo momento para alternar buzzer
static bool buzzer_state = false;           // Estado atual do buzzer
static bool buzzer_active = false;          // Flag de ativação do buzzer

// Estrutura e variáveis para LEDs NeoPixel
typedef struct {
    uint8_t G, R, B;  // Componentes de cor (Green, Red, Blue)
} pixel_t;
typedef pixel_t npLED_t;

npLED_t leds[LED_COUNT];  // Buffer para cores dos LEDs
PIO np_pio;               // Instância do PIO
uint sm;                  // Máquina de estado do PIO

/***************************************************************
 * PROTÓTIPOS DE FUNÇÕES
 **************************************************************/
// Funções para LEDs NeoPixel
void npInit(uint pin);
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b);
void npClear();
void npWrite();
int getIndex(int x, int y);
void updateLEDs(int matriz[5][5][3]);

// Funções para display
void exibir_mensagem_centralizada(const char *mensagem);

// Funções para buzzer
void pwm_init_buzzer(uint pin);
void buzzer_on(uint pin);
void buzzer_off(uint pin);
void update_buzzer();

// Funções para servidor web
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

/***************************************************************
 * MATRIZES DE CORES PARA OS LEDs
 **************************************************************/
// Matriz para olhos acesos (azul com detalhes verdes)
int matriz_olhos_acesos[5][5][3] = {
    {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
    {{0, 0, 128}, {0, 255, 0}, {0, 0, 128}, {0, 255, 0}, {0, 0, 128}},
    {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
    {{0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}},
    {{0, 0, 128}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 128}}
};

// Matriz para olhos apagados (azul com detalhes brancos)
int matriz_olhos_apagados[5][5][3] = {
    {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
    {{0, 0, 128}, {128, 128, 128}, {0, 0, 128}, {128, 128, 128}, {0, 0, 128}},
    {{0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}, {0, 0, 128}},
    {{0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}},
    {{0, 0, 128}, {0, 0, 128}, {0, 0, 100}, {0, 0, 128}, {0, 0, 128}}
};

// Matriz com todos LEDs apagados
int matriz_apagada[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
};

/***************************************************************
 * FUNÇÕES PARA CONTROLE DO DISPLAY OLED
 **************************************************************/
/**
 * Exibe uma mensagem centralizada no display OLED
 * @param mensagem Texto a ser exibido (máx 49 caracteres)
 */
void exibir_mensagem_centralizada(const char *mensagem) {
    strncpy(mensagem_display, mensagem, sizeof(mensagem_display)-1);
    atualizar_display = true;  // Sinaliza para atualizar o display
}

/***************************************************************
 * FUNÇÕES PARA CONTROLE DOS LEDs NEOPIXEL
 **************************************************************/
/**
 * Inicializa a matriz de LEDs NeoPixel
 * @param pin Pino de controle dos LEDs
 */
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    npClear();
}

/**
 * Define a cor de um LED específico
 * @param index Índice do LED (0 a LED_COUNT-1)
 * @param r Componente vermelha (0-255)
 * @param g Componente verde (0-255)
 * @param b Componente azul (0-255)
 */
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

/**
 * Desliga todos os LEDs
 */
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        npSetLED(i, 0, 0, 0);
    }
}

/**
 * Envia os dados do buffer para os LEDs
 */
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);  // Aguarda o sinal de reset
}

/**
 * Converte coordenadas (x,y) para índice linear
 * @param x Posição horizontal (0-4)
 * @param y Posição vertical (0-4)
 * @return Índice correspondente (0-24)
 */
int getIndex(int x, int y) {
    if (y % 2 == 0) {
        return 24 - (y * 5 + x);  // Linhas pares: esquerda para direita
    } else {
        return 24 - (y * 5 + (4 - x));  // Linhas ímpares: direita para esquerda
    }
}

/**
 * Atualiza os LEDs com base em uma matriz de cores
 * @param matriz Matriz 5x5x3 com as cores RGB
 */
void updateLEDs(int matriz[5][5][3]) {
    for (int linha = 0; linha < 5; linha++) {
        for (int coluna = 0; coluna < 5; coluna++) {
            int posicao = getIndex(linha, coluna);
            npSetLED(posicao, matriz[coluna][linha][0], matriz[coluna][linha][1], matriz[coluna][linha][2]);
        }
    }
    npWrite();
}

/***************************************************************
 * FUNÇÕES PARA CONTROLE DO BUZZER
 **************************************************************/
/**
 * Inicializa o PWM para o buzzer
 * @param pin Pino do buzzer
 */
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096));
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);  // Inicia desligado
}

/**
 * Liga o buzzer com frequência configurada
 * @param pin Pino do buzzer
 */
void buzzer_on(uint pin) {
    buzzer_active = true;
    buzzer_state = true;
    pwm_set_gpio_level(pin, 1024);  // 25% duty cycle (volume médio)
    next_buzzer_toggle = make_timeout_time_ms(500);  // Próximo toggle em 500ms
}

/**
 * Desliga o buzzer
 * @param pin Pino do buzzer
 */
void buzzer_off(uint pin) {
    buzzer_active = false;
    pwm_set_gpio_level(pin, 0);
}

/**
 * Atualiza o estado do buzzer (chamada no loop principal)
 */
void update_buzzer() {
    if (buzzer_active && time_reached(next_buzzer_toggle)) {
        buzzer_state = !buzzer_state;
        pwm_set_gpio_level(BUZZER_PIN, buzzer_state ? 1024 : 0);
        next_buzzer_toggle = make_timeout_time_ms(500);  // Novo timeout
    }
}

/***************************************************************
 * FUNÇÕES DO SERVIDOR WEB
 **************************************************************/
/**
 * Callback para recebimento de dados TCP
 */
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Processa a requisição HTTP
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Controle dos LEDs e display baseado na requisição
    if (strstr(request, "GET /robo_on") != NULL) {
        updateLEDs(matriz_olhos_acesos);
        buzzer_on(BUZZER_PIN);
        exibir_mensagem_centralizada("Bip Bip Bip");
    }
    else if (strstr(request, "GET /robo_off") != NULL) {
        updateLEDs(matriz_olhos_apagados);
        buzzer_off(BUZZER_PIN);
        exibir_mensagem_centralizada("ZzZ ZzZ ZzZ");
    }
    else if (strstr(request, "GET /matriz_off") != NULL) {
        updateLEDs(matriz_apagada);
        buzzer_off(BUZZER_PIN);
        exibir_mensagem_centralizada("");
    }

    // Leitura da temperatura interna
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;

    // Resposta HTTP
    char html[1024];
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Controlador do Robo</title>\n"
             "<style>\n"
             "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 64px; margin-bottom: 30px; }\n"
             "h2 { font-size: 16px; margin-bottom: 8px; }\n"
             "button { font-size: 36px; margin: 10px; padding: 20px 40px; border-radius: 10px; }\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: #333; }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Controlador do Robo</h1>\n"
             "<form action=\"./robo_on\"><button>Robo Acordado</button></form>\n"
             "<form action=\"./robo_off\"><button>Robo Dormindo</button></form>\n"
             "<form action=\"./matriz_off\"><button>Apagar Leds</button></form>\n"
             "<p class=\"temperature\">Temperatura Interna: %.2f &deg;C</p>\n"
             "<h2>Davisson Tiago</h2>\n"
             "</body>\n"
             "</html>\n",
             temperature);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    pbuf_free(p);

    return ERR_OK;
}

/**
 * Callback para aceitação de novas conexões TCP
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

/***************************************************************
 * FUNÇÃO PRINCIPAL
 **************************************************************/
int main() {
    // Inicializações básicas
    stdio_init_all();

    // Inicializa matriz de LEDs NeoPixel
    npInit(LED_NEO_PIN);
    npClear();

    // Inicializa buzzer
    pwm_init_buzzer(BUZZER_PIN);

    // Inicializa display OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

    // Configura LEDs GPIO simples
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);

    // Conecta ao Wi-Fi
    while (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    printf("Conectado ao Wi-Fi\n");

    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura servidor TCP na porta 80
    struct tcp_pcb *server = tcp_new();
    if (!server) {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    printf("Servidor ouvindo na porta 80\n");

    // Inicializa ADC para leitura de temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Loop principal
    while (true) {
        cyw43_arch_poll();  // Necessário para manter conexão Wi-Fi
        
        // Atualiza o estado do buzzer
        update_buzzer();

        // Atualiza display quando necessário
        if (atualizar_display) {
            struct render_area frame_area = {
                .start_column = 0,
                .end_column = ssd1306_width - 1,
                .start_page = 0,
                .end_page = ssd1306_n_pages - 1
            };
            calculate_render_area_buffer_length(&frame_area);
            
            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            
            int pos_x = (ssd1306_width - strlen(mensagem_display)*8)/2;
            int pos_y = (ssd1306_height - 8)/2;
            ssd1306_draw_string(ssd, pos_x, pos_y, mensagem_display);
            
            render_on_display(ssd, &frame_area);
            atualizar_display = false;
        }
        
        sleep_ms(10);  // Pequeno delay para reduzir carga da CPU
    }

    cyw43_arch_deinit();
    return 0;
}