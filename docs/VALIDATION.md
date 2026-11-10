# Validação da entrega 1.1.4

Data da entrega: **8 de novembro de 2026**.

## Verificado localmente

| Verificação | Resultado |
|---|---|
| Compilação nativa Release x64 | Passou, MSVC 19.51 / Visual Studio 2026, SDK 10.0.26100 |
| Testes automatizados (`ctest`) | Passou: 3 suítes; áudio com 1.948 verificações, layout/animação e controles nativos |
| Formatos de áudio | PCM8/16/24/32, float32/64, mono, silêncio, dados não finitos, formato inválido, centro/LFE em 5.1 |
| Resampling | Continuidade entre blocos e equivalência entre processamento inteiro e dividido |
| Ring SPSC | Escrita/leitura, falta de dados, recuperação, reset e estresse concorrente |
| Validação de sessão | IDs vazios/iguais e buffer inválido recusados; parada repetida segura |
| Classificação por nome | Headset USB genérico não é tratado como perfil de chamada |
| Enumeração de dispositivos | Foram detectadas saídas QCY Melobuds ANC e Realtek; somente detecção |
| Executável | Recurso de versão 1.1.4, ícone multirresolução, manifesto válido e dependências apenas de DLLs do Windows |
| Layout e animação | Limites em larguras 420/480/620, cinco controles por tela, alvos de 40 px ou mais, ausência de sobreposição e curva monotônica com extremos exatos |
| Revisão visual em arquivo | Seis imagens geradas pelo mesmo renderizador: principal, compartilhando, ajustes, vazio compacto, erro com nomes longos e ajustes compactos; revisadas em 125/150/200% |
| Pintura e uso dos controles | Controles isolados invisíveis: fundo sem apagamento intermediário, quadros completos, região validada, desativação sem cancelamento repetido, estado do botão, volume nativo e troca de dispositivos |
| Assinatura digital | Não assinado; não há certificado incluído |
| Preservação do original | SHA-256 dos 78 arquivos conferido, nenhuma alteração |

As verificações por amostra não representam 1.948 cenários independentes nem testes de 1.948 dispositivos.

## Ainda depende de validação manual

- Escuta real nas duas saídas e estabilidade durante uma sessão prolongada.
- Latência percebida, troca de perfil Bluetooth, reconexão e retorno de suspensão.
- Interação real com menus, foco, teclado, leitor de tela, rolagem, bandeja e mudança de monitor. As prévias em arquivo conferem o desenho, não esses comportamentos.
- Confirmar em uma tela física que as piscadas relatadas na versão 1.1.0 não ocorrem na 1.1.4; os testes de mensagens cobrem mecanismos de pintura, não a composição final do monitor.
- Instalação limpa do Windows 10/11 e variedade de adaptadores, USB, HDMI e dois fones Bluetooth.
- Execução dos workflows em um repositório remoto e publicação de um release.

Nenhuma janela do aplicativo foi aberta na tela para os testes desta revisão. O teste nativo cria controles com pai somente de mensagens, sem superfície visível, foco global, acesso a preferências ou dispositivos de áudio. O gerador de prévias também não acessa áudio. As verificações interativas permanecem pendentes. Veja [investigação e correções](UI_FIXES.md).
