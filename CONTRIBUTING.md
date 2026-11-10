# Contribuir

Use uma issue para descrever um defeito ou propor uma mudança. Para problemas de áudio, informe versão do Windows, modelos, conexões, buffer e passos reproduzíveis. Não publique gravações, caminhos pessoais ou IDs de dispositivos.

Compile com `scripts/Build.ps1`, mantenha `ctest` passando e use o estilo de `.clang-format`. Testes de lógica precisam executar também em Release; não dependa de `assert` desativado por `NDEBUG`.

Mantenha o processamento de áudio fora da thread da interface. Não introduza bloqueios, acesso a disco ou rede no laço de áudio. Prefira IDs estáveis a regras por nome/marca. Novos formatos precisam de testes com amostras conhecidas; novas promessas de hardware precisam de evidência física documentada.

Em pull requests, explique problema, comportamento resultante, testes e limitações. Mudanças de áudio exigem também avaliação auditiva em hardware real, além dos testes automatizados.
