# Revisão de uso e pintura — 1.1.4

O relato que motivou a revisão foi “ao abrir está piscando”. A investigação usou o código da edição 1.1.0 e a [skill Craft Anything oficial](https://github.com/storytold/craft-anything/blob/main/crates/setup/src/skill.md), consultada no commit `56aa31976b4ff0950e6b34ba020b0a568d2462fa`. Como o fonte estava disponível, foi seguida a recomendação de análise direta. Não foi executada uma sessão de engenharia reversa nem usado acesso à tela do usuário.

## Evidências e mudanças

| Observado no código anterior | Mudança | Verificação |
|---|---|---|
| Pai, botões e volume tinham caminhos de pintura diferentes; os filhos não interceptavam o apagamento de fundo | Todos os controles copiam o mesmo quadro completo; fundo nativo não é apagado; composição dupla na janela | Mensagens de pintura em controles reais invisíveis verificam pixels completos, ausência de escrita intermediária e validação da região |
| Controles desativados recebiam EnableWindow(FALSE) a cada sincronização | Aplicar somente quando muda o estado | Quarenta sincronizações sem alteração não produzem cancelamentos nem mudanças de habilitação |
| A barra solicitava pintura antes de atualizar o valor do modelo | Atualizar primeiro; redesenhar depois | Limites e centro do volume em quatro escalas; estado e teclado nativos preservados |
| Enter podia ser consumido pelo gerenciador de diálogo antes do tratamento do botão | Solicitar Enter em WM_GETDLGCODE e ignorar repetição da tecla | Revisão estática; interação visível ainda pendente |
| Seleção do endpoint no outro campo ficava desativada | Escolhê-lo troca origem e destino | Cenários de troca, terceiro dispositivo e opção vazia |
| Nome e valor da estabilidade dividiam a mesma linha | Valor em segunda linha | Prévia compacta revisada |

A [documentação da Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enablewindow) confirma a mensagem de cancelamento ao desativar um controle. A [composição dupla](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles) evita a apresentação separada das pinturas dos filhos. O [gerenciamento de diálogo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-isdialogmessagew) usa WM_GETDLGCODE para determinar quais teclas são processadas pelo controle.

## Limite da conclusão

As condições estruturais acima foram corrigidas e cobertas por testes locais. A piscada relatada não foi reproduzida visualmente nesta sessão e não é possível afirmar que todos os fatores de tela/driver foram eliminados. A confirmação na janela visível e a escuta nos dois dispositivos continuam pendentes. O motor de áudio permanece idêntico ao da versão 1.1.0.
