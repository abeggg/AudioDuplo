# Compatibilidade

O AudioDuplo trabalha com **endpoints de reprodução ativos do Windows**. Não usa APIs específicas de QCY, Sony, JBL, Samsung, Apple ou outros fabricantes e não mantém uma lista de marcas permitidas. Essa abordagem amplia a compatibilidade, mas não comprova funcionamento com todos os modelos.

| Categoria | Condição de uso | Situação desta versão |
|---|---|---|
| Fone P2 / saída da placa de som | Saída ativa no Windows | Detectado endpoint Realtek no ambiente de desenvolvimento |
| Fone USB / dongle USB | Driver oferece saída WASAPI compartilhada | Implementado; teste físico pendente |
| Bluetooth A2DP | Fone pareado e conectado pelo Windows | Detectado endpoint QCY Melobuds ANC; teste auditivo desta edição pendente |
| Bluetooth HFP / chamadas | Driver aceita reprodução compartilhada | Não bloqueado; aviso por nome; qualidade limitada pelo perfil |
| Bluetooth LE Audio | Windows e hardware apresentam endpoint compatível | Sem código específico de LE Audio; teste físico pendente |
| HDMI / DisplayPort | Monitor ou receptor oferece saída de áudio | Implementado por endpoint; teste físico pendente |
| Dois dispositivos Bluetooth | Adaptador/driver sustentam conexões simultâneas | Depende do hardware; teste físico pendente |
| ASIO / modo exclusivo | Não usa o mixer compartilhado | Fora do escopo |
| Conteúdo protegido | Sistema pode impedir loopback | Não há contorno dessa proteção |

**Detectado** significa que a enumeração retornou o dispositivo. Não significa que houve teste auditivo de reprodução, sincronismo ou estabilidade.

## Formatos

Captura PCM de 8, 16, 24 ou 32 bits, ou float de 32/64 bits, de 8 a 384 kHz, com até 8 canais. Mono é duplicado em L/R; estéreo é preservado. Layouts surround com máscara convencional são reduzidos para estéreo com ganho normalizado, centro e canais traseiros/laterais; LFE é omitido. Layouts desconhecidos podem exigir configurar estéreo no Windows.

O formato interno é 48 kHz, float32 e estéreo. A saída usa IAudioClient3 quando possível; o fallback do WASAPI solicita conversão de taxa e canais pelo Windows. Isso é uma capacidade do caminho de áudio, não uma certificação de cada combinação de hardware.

## Atraso e qualidade

O buffer exibido representa o áudio aguardando dentro do aplicativo. Não mede a latência total, que também inclui o driver, rádio Bluetooth, codec e processamento do fone. A primeira saída segue o caminho original e não recebe um atraso compensatório. Dois fones podem apresentar diferença audível de tempo.

O resampler de captura usa interpolação linear com estado preservado entre blocos. É adequado ao propósito original de compartilhamento, mas não oferece uma garantia de conversão audiófila, filtragem antialias de alta ordem ou saída bit-perfect.

No Bluetooth Classic, abrir o microfone pode mudar o perfil de mídia para chamadas. No Windows 11, os dois perfis podem aparecer como um único endpoint. O aviso baseado no nome é uma heurística; não confirma o perfil ou codec em uso. [Microsoft: Bluetooth Classic Audio](https://learn.microsoft.com/windows-hardware/drivers/bluetooth/bluetooth-classic-audio).

Loopback opera em modo compartilhado, e suporte a eventos de loopback existe a partir do Windows 10 1703. [Microsoft: Loopback Recording](https://learn.microsoft.com/windows/win32/coreaudio/loopback-recording).

## Como relatar um teste

Abra uma issue com Windows, modelo dos dois dispositivos, tipo de conexão, buffer e resultado observado. Informe se há corte, eco, ruído ou desvio ao longo de pelo menos 15 minutos. Revise o texto de Diagnóstico antes de colar. Nunca inclua gravações privadas apenas para demonstrar uma falha.
