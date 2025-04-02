/**
 * Cliente HTTP para Raspberry Pi Pico W
 * Envia mensagens dinâmicas periodicamente para um servidor Flask
 */
#include <stdio.h>
#include "pico/stdio.h"
#include "pico/cyw43_arch.h"
#include "pico/async_context.h"
#include "lwip/altcp_tls.h"
#include "example_http_client_util.h"

// ======= CONFIGURAÇÕES ======= //
#define HOST "192.168.0.58"  // Substitua pelo IP do servidor
#define PORT 5000
#define INTERVALO_MS 3000     // Intervalo entre mensagens (3 segundos)
// ============================= //

int main() {
    // Inicializa hardware
    stdio_init_all();
    printf("\nIniciando cliente HTTP...\n");

    // Configura Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar Wi-Fi\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    // Conecta à rede Wi-Fi
    printf("Conectando a %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                                          CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha na conexão Wi-Fi\n");
        return 1;
    }
    printf("Conectado! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    int counter = 0;
    char url[128];  // Buffer para URL dinâmica

    // Loop principal
    while(1) {
        // Cria mensagem dinâmica
        sprintf(url, "/mensagem?msg=PicoW_%d_Temp=%.1fC", 
                counter++, 25.0 + (counter % 10) * 0.5);  // Exemplo com valor simulado

        // Configura requisição
        EXAMPLE_HTTP_REQUEST_T req = {0};
        req.hostname = HOST;
        req.url = url;
        req.port = PORT;
        req.headers_fn = http_client_header_print_fn;
        req.recv_fn = http_client_receive_print_fn;

        // Envia requisição
        printf("[%d] Enviando: %s\n", counter, url);
        int result = http_client_request_sync(cyw43_arch_async_context(), &req);
        
        // Verifica resultado
        if (result == 0) {
            printf("Sucesso!\n");
        } else {
            printf("Erro %d - Verifique conexão\n", result);
            
            // Tenta reconectar se houver falha
            cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                                             CYW43_AUTH_WPA2_AES_PSK, 10000);
        }

        // Aguarda antes de enviar novamente
        sleep_ms(INTERVALO_MS);
    }

    // Nunca chegará aqui devido ao while(1)
}