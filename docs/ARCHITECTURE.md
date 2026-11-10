# Arquitetura

```text
Player / jogo / navegador
          |
          v
Mixer do Windows ---------> Primeiro dispositivo (caminho original)
          |
          v
WASAPI loopback -> decode/downmix -> resampler 48 kHz
          |
          v
Ring SPSC -> leitura adaptativa -> ganho suave -> WASAPI -> Segundo dispositivo
```

## Motor

`AudioEngine` possui uma thread de captura e uma thread de renderização. Cada thread inicializa seu apartamento COM e registra prioridade MMCSS. Um evento compartilhado encerra ambas; `stop()` sinaliza, aguarda e libera os recursos antes de outra sessão começar. As interfaces COM são destruídas antes do encerramento do apartamento.

A captura abre o dispositivo pelo identificador estável, em WASAPI compartilhado com loopback e eventos. Formatos desconhecidos falham de forma explícita. O decoder trata PCM inteiro e float, amostras não finitas, mono e downmix de surround. O resampler mantém a fase entre pacotes.

O ring SPSC tem capacidade de dois segundos; a ocupação-alvo normal é de 20–80 ms no modo automático. Manualmente é possível selecionar até 160 ms. O produtor nunca sobrescreve dados ainda não consumidos. Overflow pede recuperação; underrun faz fade para silêncio e rearma a leitura. Não há repetição de blocos antigos para esconder falhas.

O consumidor aplica correção PI suave limitada a ±1200 ppm para controlar a ocupação frente à diferença de relógio dos dispositivos. Essa correção não sincroniza a latência física dos dois fones.

A renderização tenta o menor período compartilhado de IAudioClient3 para 48 kHz estéreo float nativo. O fallback usa `AUTOCONVERTPCM` e `SRC_DEFAULT_QUALITY` em IAudioClient, com buffer de 20 ms e segunda tentativa de 40 ms. O volume de destino é aplicado em software com suavização por amostra.

## Interface

Win32, controles nativos, Segoe UI e desenho compartilhado em GDI+. A tela principal contém os dois dispositivos, volume, compartilhar/parar e Ajustes. A segunda tela reúne estabilidade, diagnóstico, som do Windows, bandeja e ajuda. O executável inclui ícone em múltiplas resoluções, recurso de versão, manifesto sem elevação e DPI PerMonitorV2. A janela permite redimensionar e rolar quando a área disponível é menor que o conteúdo.

A interface consulta o estado a cada 250 ms e enumera dispositivos a cada dois segundos. Enquanto a sessão roda, preserva os IDs escolhidos. Se uma saída desaparecer, encerra a sessão; não seleciona outra automaticamente para continuar tocando. Alterar o dispositivo padrão do Windows não muda a origem já escolhida. A enumeração fica suspensa enquanto um menu de seleção está aberto.

`UiRenderer` concentra cores, tipografia, geometria e desenho. O app usa o mesmo renderizador de `ui_preview`, que exporta imagens sem janelas. Realce de ponteiro e troca de tela usam 120/180 ms; o temporizador cessa ao terminar. A preferência de animação do Windows e ações de teclado suprimem a transição. Veja [decisões de interface](DESIGN.md).

`ControlSurface` preserva entrada e acessibilidade dos botões/barra nativos, mas intercepta a pintura completa e o apagamento do fundo. Cada controle copia sua região do mesmo quadro em memória. A janela usa composição dupla; os objetos de desenho são liberados antes de `EndPaint`. A sincronização só solicita novo quadro quando o estado visual muda, e só habilita/desabilita controles quando necessário. `UiState` concentra troca de endpoints e conversão da posição do volume.

## Dados

Preferências usam um INI local UTF-16 com IDs, volume e índice do buffer. Não há autosstart, serviço, alterações de registro, instalador de driver, gravação de áudio ou componente de rede. Diagnósticos são gerados apenas no clique do usuário e copiados para a área de transferência; incluem nomes amigáveis, não IDs de endpoint.

## Testes

`audio_tests` valida formatos, limites, continuidade de resampling, ring, nomes e ciclo de parada. Usa verificações explícitas que continuam ativas em Release. `audio_probe` é uma ferramenta manual: sem argumentos apenas enumera dispositivos; com dois índices inicia uma sessão por 12 segundos. Ela não gera som nem muda o padrão do Windows. Execute somente durante um teste autorizado e com o player usando a origem selecionada.

`ui_native_tests` exercita o adaptador de pintura com botões e barra reais sob um pai somente de mensagens. Os controles ficam invisíveis. Verifica pixels, validação da região de pintura, ausência de cancelamentos na ociosidade, estado nativo, volume e troca da rota. `ui_preview --test` também mede o título nas três larguras suportadas.

Referências: [flags WASAPI](https://learn.microsoft.com/windows/win32/coreaudio/audclnt-streamflags-xxx-constants), [IAudioClient3](https://learn.microsoft.com/windows/win32/api/audioclient/nn-audioclient-iaudioclient3).
