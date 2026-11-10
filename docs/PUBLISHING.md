# Publicar no GitHub

Este projeto foi preparado localmente. Antes de criar um repositório público, confirme que a autorização do proprietário do **Áudio Duplo V4 QCY** permite publicar a arquitetura e o código derivados sob os termos da licença MIT incluída. O aviso atual registra a origem, mas não substitui essa confirmação.

## Primeiro envio

1. Após confirmar os direitos da base derivada, crie um repositório vazio no GitHub, por exemplo `audioduplo`.
2. Coloque o conteúdo desta pasta na raiz do repositório. Inclua `.github`, `.gitignore`, `.gitattributes` e `.clang-format`.
3. Mantenha `build/`, configurações pessoais, diagnósticos e executáveis fora do código versionado. O EXE é distribuído em Releases.
4. Revise a licença MIT e a atribuição em `NOTICE.md`; ajuste a autoria pública conforme desejado e mantenha os avisos de licenças de fontes.
5. Envie a branch principal, confira **Windows CI** e **CodeQL** em Actions e resolva alertas antes de distribuir.

Para usar Git no terminal, substitua os campos entre colchetes antes de executar:

```powershell
git init -b main
git add .
git commit -m "Initial AudioDuplo desktop release"
git remote add origin https://github.com/[SEU-USUARIO]/[SEU-REPOSITORIO].git
git push -u origin main
```

## Criar um lançamento

Depois de testar o executável com seus fones e conferir a CI remota:

```powershell
git tag v1.1.4
git push origin v1.1.4
```

O workflow **Windows release** compila, testa, cria o ZIP e o SHA256SUMS e abre um **rascunho** de release. Revise os arquivos e as notas na página Releases, então publique quando estiver satisfeito. O workflow não publica um lançamento final automaticamente.

Tags existentes não devem ser reaproveitadas. Para outra versão, atualize de forma consistente CMakeLists, versão do executável, interface, scripts, README e CHANGELOG, e crie uma tag nova. Os scripts e workflows desta edição estão preparados para `v1.1.4`.

## Assinatura do executável

O pacote local é **não assinado digitalmente**. Ícone, manifesto e informações de versão não equivalem a um certificado. A distribuição não inclui certificado, reputação SmartScreen ou aprovação da Microsoft Store. Assinatura Authenticode exige um certificado apropriado e processo de assinatura do mantenedor. Não desative as proteções do Windows para distribuir o app.

## Antes de publicar como estável

- Execute os testes automatizados e confirme o SHA-256 dos arquivos.
- Faça teste auditivo prolongado com os dispositivos que pretende declarar como testados.
- Verifique início/parada, reconexão, suspensão do PC, mudança de perfil do Bluetooth e fechamento pela bandeja.
- Confira interface, navegação por teclado, escalas 100/125/150/200% e uma instalação limpa do Windows.
- Registre resultados reais em `docs/VALIDATION.md` e na matriz de compatibilidade.

As automações prontas no repositório só são executadas após o envio ao GitHub. A validação local não comprova que um workflow remoto já executou.
