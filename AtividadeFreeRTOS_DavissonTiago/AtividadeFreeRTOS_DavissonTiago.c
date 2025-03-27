#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "semphr.h"
#include "queue.h"

// Definição dos pinos
#define PIN_BUTTON_A 5    // Pino do botão (GPIO5)
#define PIN_LED_RED 13    // Pino do LED (GPIO13)

// Variáveis globais para comunicação entre tarefas
QueueHandle_t xButtonQueue;         // Fila para enviar o estado do botão
SemaphoreHandle_t xLedSemaphore;    // Semáforo para controlar o LED

// Tarefa 1: Leitura do botão
void vReadingButtonA(void *pvParameters) {
    bool button_state;  // Armazena o estado atual do botão
    
    for (;;) {  // Loop infinito da tarefa
        // Lê o estado do botão (0 = pressionado, 1 = solto, devido ao pull-up)
        button_state = (gpio_get(PIN_BUTTON_A) == 0);
        
        // Envia o estado do botão para a fila
        xQueueSend(xButtonQueue, &button_state, 0);
        
        // Aguarda 100ms antes da próxima leitura
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Tarefa 2: Processamento do estado do botão
void vProcessingButtonA(void *pvParameters) {
    bool received_state;  // Armazena o estado recebido da fila
    
    for (;;) {
        // Recebe o estado do botão da fila (espera indefinidamente)
        if (xQueueReceive(xButtonQueue, &received_state, portMAX_DELAY) == pdTRUE) {
            // Se o botão estiver pressionado (estado = true)
            if (received_state) {
                // Libera o semáforo para acionar o LED
                xSemaphoreGive(xLedSemaphore); 
            }
        }
    }
}

// Tarefa 3: Controle do LED
void vLedControlTask(void *pvParameters) {
    for (;;) {
        // Aguarda o semáforo ser liberado (botão pressionado)
        if (xSemaphoreTake(xLedSemaphore, portMAX_DELAY)) {
            // Acende o LED
            gpio_put(PIN_LED_RED, 1);
            printf("LED Red: ON\n");
            
            // Mantém o LED aceso por 500ms
            vTaskDelay(500 / portTICK_PERIOD_MS); 
            
            // Apaga o LED
            gpio_put(PIN_LED_RED, 0);
            printf("LED Red: OFF\n");
        }
    }
}

// Função para configurar o pino do LED
void config_led_pin(uint gpio) {
    gpio_init(gpio);              // Inicializa o pino
    gpio_set_dir(gpio, GPIO_OUT); // Configura como saída
}

// Função para configurar o pino do botão
void config_button_pin(uint gpio) {
    gpio_init(gpio);              // Inicializa o pino
    gpio_set_dir(PIN_BUTTON_A, GPIO_IN);  // Configura como entrada
    gpio_pull_up(PIN_BUTTON_A);   // Habilita resistor de pull-up
}

// Função para configuração inicial do hardware
void hardware_setup() {
    config_button_pin(PIN_BUTTON_A);  // Configura o botão
    config_led_pin(PIN_LED_RED);     // Configura o LED
    gpio_put(PIN_LED_RED, 0);        // Garante que o LED comece apagado
}

// Função principal
int main() {
    stdio_init_all();  // Inicializa a comunicação serial (para printf)

    // Cria a fila para comunicação entre tarefas (tamanho: 10 itens)
    xButtonQueue = xQueueCreate(10, sizeof(bool));
    
    // Cria o semáforo binário para controle do LED
    xLedSemaphore = xSemaphoreCreateBinary();

    // Configura os pinos do hardware
    hardware_setup();

    // Cria as tarefas do FreeRTOS:
    // 1. Tarefa de leitura do botão (prioridade 1)
    xTaskCreate(vReadingButtonA, "Button Reading", 128, NULL, 1, NULL);
    
    // 2. Tarefa de processamento do botão (prioridade 2)
    xTaskCreate(vProcessingButtonA, "Button Processing", 128, NULL, 2, NULL);
    
    // 3. Tarefa de controle do LED (prioridade 3)
    xTaskCreate(vLedControlTask, "LED Control", 128, NULL, 3, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    // Nunca deverá chegar aqui (o escalonador controla o programa)
    while (1);
    return 0;
}