# Changelog

## 1.1.4 — 2026-11-08

- Nunito rounded typography, embedded Regular/Bold fonts.
- Existing motion and selector behavior retained.

## 1.1.3 — 2026-11-08

- Montserrat Regular/Bold embedded from the official JulietaUla/Montserrat repository.
- Animated 120 ms button press/release, 180 ms directional page transition and animated open-selector outline.
- Windows reduced motion respected. Live interaction verification remains pending.

## 1.1.2 — 2026-11-08

- Embedded Lato Regular/Bold typography (SIL OFL).
- Clicking an open device selector closes its menu.
- Animated selector arrows (180 ms), native menu entrance, reduced-motion and keyboard support.
- Visible interaction remains pending manual verification.

## 1.1.1 — 2026-11-08

- Pintura unificada da janela e dos controles para tratar o relato de piscadas ao abrir; composição dupla, sem apagamento intermediário do fundo.
- Removidos cancelamentos repetidos de controles desativados durante a atualização de estado.
- Barra de volume atualiza o modelo antes de redesenhar, evitando exibir o valor anterior durante o arraste.
- Título alterado para “Ouça simultaneamente”, dimensionado para janelas estreitas.
- Escolher o dispositivo do outro campo troca os dois automaticamente; menus respeitam a área útil do monitor.
- Enter tratado pelo controle correto, sem repetir a ação ao manter a tecla pressionada.
- Nome e valor da estabilidade em linhas separadas; instrução de parar antes de trocar os fones durante uma sessão.
- Testes de pintura e comportamento nativo com controles isolados e invisíveis, sem acessar áudio ou configurações do usuário.
- Motor de áudio preservado; confirmação visual em uma tela física ainda depende de teste manual.

## 1.1.0 — 2026-11-08

- Interface refeita: grafite, um bloco para os dois dispositivos, volume e ação principal.
- Buffer, diagnóstico, bandeja e ajuda movidos para Ajustes.
- Seletores nativos com menu escuro e opção para trocar os dois dispositivos.
- Realce de ponteiro em 120 ms e transição de telas em 180 ms; movimento reduzido e teclado respeitados.
- Renderizador compartilhado gera prévias sem criar janelas ou acessar áudio.
- Testes de limites, alvos de clique, sobreposição e curva de animação.
- Motor de áudio preservado da edição 1.0.0.

## 1.0.0 — 2026-11-07

Primeira edição independente preparada para publicação, derivada do Áudio Duplo V4 QCY.

- Seleção genérica de endpoints ativos do Windows, sem preferência ou bloqueio por marca.
- Removido bloqueio de nomes genéricos como Headset; perfil de chamadas agora recebe aviso.
- Nova interface nativa com cartões, ícone, escala por monitor, rolagem e atalhos.
- Volume do destino com suavização, preferências locais e bandeja.
- Diagnóstico e tratamento de desconexão sem troca automática de destino em reprodução.
- PCM8 e float64, validação de formatos e downmix de surround convencional.
- Correção da ordem de liberação de COM e limpeza do pedido de recuperação ao reiniciar o ring.
- Buffer manual de até 160 ms, mantendo o automático original de 20–80 ms.
- Build x64 portátil, testes ativos em Release, workflows e documentação em português/inglês.

Consulte `docs/VALIDATION.md` para distinguir o que foi verificado do que ainda depende de teste físico e visual.
