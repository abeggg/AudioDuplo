## AudioDuplo 1.1.4

Um som. Dois destinos. Compartilhe o áudio entre duas saídas do Windows, sem restrição de marca e sem driver virtual.

Esta revisão trata o relato de piscadas ao abrir, unifica a pintura dos controles e melhora o volume, Enter e seleção de dispositivos. O título passa a ser **Ouça simultaneamente**. Três suítes automatizadas cobrem áudio, layout e controles nativos isolados. A confirmação visual em tela física permanece pendente; consulte `docs/UI_FIXES.md`.

- Interface refeita: grafite, dois seletores e uma ação principal. Buffer, diagnóstico, bandeja e ajuda ficam em Ajustes.
- Movimento discreto, teclado e preferência de movimento reduzido respeitados.
- Preferências locais, ícone na bandeja e tratamento de desconexões.
- Pacote portátil para Windows x64. Extraia o ZIP e abra AudioDuplo.exe.

O executável é não assinado. Compatibilidade depende do endpoint compartilhado, do driver e do hardware. Bluetooth pode acrescentar atraso ou mudar para modo de chamadas ao usar o microfone. Não há garantia de sincronismo perfeito ou suporte testado a todos os modelos.

Este rascunho deve ser revisado e testado pelo mantenedor antes da publicação. Consulte README, COMPATIBILITY e VALIDATION incluídos no pacote.
