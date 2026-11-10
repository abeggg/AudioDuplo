# Interface 1.1

A tela principal foi reduzida a cinco controles: seleção de origem, seleção de destino, volume, compartilhar/parar e Ajustes. Buffer, diagnóstico, bandeja e ajuda ficam em uma segunda tela. O motor de áudio permanece igual ao da edição 1.0.0.

## Direção visual

Uma ferramenta compacta de escuta, com fundo grafite, hierarquia tipográfica curta, um único bloco para o par de dispositivos e ação principal clara. Os nomes usam reticências quando excedem o espaço. Os controles têm alvos de pelo menos 40 pixels lógicos e foco visível para teclado.

Os seletores usam menus nativos com desenho próprio. O volume mantém um controle nativo com valor acessível, suporte a teclado e arraste com captura do ponteiro. Não há um formulário web dentro do executável.

## Movimento

| Interação | Duração | Propósito |
|---|---:|---|
| Realce ao passar o ponteiro | 120 ms | Confirmar que o elemento pode ser acionado |
| Troca entre principal e Ajustes por clique | 180 ms | Suavizar a mudança de contexto |
| Teclado e movimento reduzido do Windows | Imediata | Preservar resposta e preferência de acessibilidade |
| Volume e valores funcionais | Imediata | Mostrar o valor que está sendo aplicado |

A curva é `cubic-bezier(0.23, 1, 0.32, 1)`. Transições interrompidas partem do quadro atual; nenhuma animação bloqueia a interação. O temporizador de animação para ao terminar. Não há animações decorativas em loop. A composição nativa usa bitmaps e mistura alfa, não CSS nem uma alegação de aceleração por GPU.

## Referências consultadas

Na revisão 1.1.4, foi consultada a [skill oficial Craft Anything](https://github.com/storytold/craft-anything/blob/main/crates/setup/src/skill.md). Sua orientação de ler diretamente o código disponível foi aplicada à investigação da pintura. Hipóteses, resultados e limites estão em [UI_FIXES](UI_FIXES.md). Não foi necessário instalar ou reconfigurar o servidor de engenharia reversa.

A descoberta foi feita com o [find-skills oficial da Vercel](https://github.com/vercel-labs/skills/tree/main/skills/find-skills), após baixar seu repositório e executar a busca por interface/animação.

- [Frontend Design, Anthropic](https://github.com/anthropics/skills/tree/main/skills/frontend-design): direção visual, tipografia, conteúdo e autocrítica.
- [Web Design Guidelines, Vercel](https://github.com/vercel-labs/agent-skills/tree/main/skills/web-design-guidelines): foco, nomes acessíveis, estados, conteúdo longo e movimento reduzido. As orientações pertinentes foram adaptadas ao aplicativo nativo.
- [Emil Kowalski Skills](https://github.com/emilkowalski/skills): decisões de movimento, resposta ao clique, duração, interrupção e moderação.

Esses repositórios são referências de desenvolvimento; o programa não os executa ou acessa pela rede. Não há afiliação comercial com seus autores.

## Comparação da revisão

| Antes | Depois | Motivo |
|---|---|---|
| Métricas e seis ações no rodapé | Uma ação principal e acesso a Ajustes | Reduzir escolhas antes de começar a ouvir |
| Dois cartões separados com numeração | Um bloco contendo a rota de áudio | Tornar a relação entre os dispositivos visível |
| Campos de buffer na tela inicial | Estabilidade automática, ajustável em Ajustes | Um valor técnico não deve impedir a primeira reprodução |
| Interface fixa extensa | Conteúdo de 620 × 650 pixels lógicos, adaptável em largura | Diminuir a janela e preservar legibilidade |
| Sem resposta visual ao ponteiro | Realce breve, foco de teclado e transição de contexto | Mostrar resposta sem distração contínua |

## Verificação sem abrir a janela

`ui_preview` usa `UiRenderer`, o mesmo desenho compilado no app, para gerar imagens de estados com dados ilustrativos. Não cria janela, não enumera dispositivos e não inicia áudio. As imagens conferem geometria e estilo; não substituem teste interativo de menus, foco e hardware.

```powershell
build/Release/ui_preview.exe preview-output
```


## Interface 1.1.4

Embedded Lato Regular/Bold fonts; no system font installation required. Clicking an open selector closes it. Selector arrows animate for 180 ms; native menus follow Windows animation preferences. Reduced motion and keyboard actions are immediate. Source and destination selectors use the same behavior. Visible click/toggle testing remains pending; automated audio, layout and native-control suites passed.



## 1.1.4: Montserrat and motion

Montserrat Regular/Bold is embedded from the official JulietaUla/Montserrat repository. No font installation is required. The typography reference was provided by the project owner.

The [Prismic button animation collection](https://prismic.io/blog/css-button-animations) inspired two native adaptations: directional background fill on hover (180 ms) and button compression/release (120 ms). Settings transitions add a 12 px directional slide with the existing 180 ms fade. Selector outlines and arrows indicate open state. Keyboard actions remain immediate; Windows reduced motion is respected. These are GDI+ implementations, not CSS or copied CodePen source. Interactive screen validation remains pending.


## 1.1.4 rounded typography

Nunito Regular/Bold replaces Montserrat in the interface, including menus and headings. The rounded terminals follow the requested softer typography. Fonts are embedded and registered only inside the process; no system font installation. Existing motion and controls remain.
