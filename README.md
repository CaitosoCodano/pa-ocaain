# Ocaain — CS 1.6 / Windows

Implementação de cliente GoldSrc de 32 bits com menu Windows Forms renderizado
como textura OpenGL dentro do jogo, inclusive em tela cheia. O processo Forms
não abre uma janela sobre a área de trabalho. Insert abre/fecha o menu dentro
de um mapa; somente as caixas de seleção controlam aimbot, boxes e skeleton.
Todos começam desativados. O botão **Desativar tudo** limpa as três opções.

**Estado:** DLL x86 e menu compilados em Linux; 17 verificações do núcleo passaram.
Outras 12 verificações de instalação/restauração passaram com arquivos temporários.
Não houve execução no Windows nem no CS 1.6. A compatibilidade e o comportamento
na build Steam **10210, protocolo 48, 1.1.2.7/Stdio** informada ainda precisam
ser validados. Não é possível afirmar que o conjunto funciona nessa build
apenas com a compilação. Veja o roteiro de validação abaixo.

## Baixar o pacote pronto

Abra [downloads/Ocaain-CS16-win-x86.zip](downloads/Ocaain-CS16-win-x86.zip)
no GitHub e clique em **Download raw file**. O ZIP tem aproximadamente 62 MB
e inclui a DLL, o menu com runtime .NET, os scripts de instalação/restauração
e as instruções. Extraia o ZIP antes de executar `Install.ps1`.

Os hashes SHA-256 dos arquivos estão em [downloads/SHA256SUMS.txt](downloads/SHA256SUMS.txt).
Para instalar o pacote extraído, execute o `Install.ps1` que fica na raiz dele;
o caminho `scripts/Install.ps1` usado abaixo se aplica ao checkout do código.

## Requisitos e escopo

- Windows 10/11, CS 1.6 Steam de 32 bits e renderizador OpenGL (`-gl`).
- Execução local com `-insecure`. O cliente recusa iniciar sem esse parâmetro.
  Não há mecanismo de evasão de VAC. Restaure o cliente original antes de
  voltar a servidores protegidos; modificações de cliente podem resultar em banimento.
- Menu e renderização utilizam os callbacks públicos do SDK GoldSrc, recebidos
  por uma DLL intermediária que encaminha a interface para `client.original.dll`.
- A mira escolhe o inimigo vivo mais próximo da direção atual e aponta para
  o centro do hitbox da cabeça; não exige tecla de ativação e não dispara sozinha.
- Skeleton usa transformações reais de ossos e seus pais, não um desenho
  humano genérico. Boxes envolvem os ossos projetados na tela.
- Só há rastreamento de entidades que o servidor envia e o motor renderiza.
  Entidades fora do PVS, omitidas pelo servidor ou com ossos desatualizados são
  descartadas. Não há garantia de ESP atrás de todas as paredes nem de alvos
  fora da tela. Modelos sem hitbox de cabeça não recebem mira presumida.

## Compilar no Windows

Instale .NET SDK 8, Git e Visual Studio 2022 / Build Tools com **Desenvolvimento
para desktop com C++**, Windows SDK e CMake. Em PowerShell, na raiz do projeto:

```powershell
.\scripts\Build.ps1
```

O CMake baixa o SDK oficial Valve em um commit fixo:
`b1b5cf5892918535619b2937bb927e46cb097ba1`. Para usar um checkout local, configure
`-DHLSDK_ROOT=C:\caminho\halflife`. A DLL precisa ser **Win32/x86**.
O menu é publicado com runtime .NET incluído; o computador de destino não
precisa do SDK. `dist/` contém o pacote a instalar.
O script também executa um autoteste Windows dos controles ocultos, cliques e
pixels BGRA; seu resultado fica em `dist/ocaain-menu/self-test-results.json`.
Esse autoteste foi escrito, mas não executado neste ambiente Linux.

## Instalar e abrir

1. Feche o CS 1.6.
2. Execute, ajustando o caminho da biblioteca Steam:

   ```powershell
   .\scripts\Install.ps1 -GameDirectory 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life'
   ```

3. Nas propriedades do jogo na Steam, adicione `-insecure -gl` às opções de
   inicialização. Mantenha tela cheia nas configurações de vídeo.
4. Abra pela Steam e entre num mapa local com adversários.
5. Pressione **Insert**. Clique nas caixas para habilitar/desabilitar recursos;
   pressione Insert novamente para fechar o menu.

A instalação preserva a DLL original como `cstrike/cl_dlls/client.original.dll`
e registra os hashes SHA-256. Ela não sobrescreve backups existentes. Para
atualizar este projeto instalado, desinstale antes e instale o novo pacote.

```powershell
.\scripts\Uninstall.ps1 -GameDirectory 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life'
```

Depois de restaurar, remova `-insecure` se quiser o cliente padrão novamente.
Se a Steam atualizar a DLL enquanto o projeto estiver instalado, o script
recusa substituir o arquivo alterado: utilize **Verificar integridade dos
arquivos** na Steam para recuperar o cliente atual.

## Validação no jogo — pendente

Este roteiro requer Windows e a build 10210 instalada. Ainda não foi executado:

1. Entre num mapa local em tela cheia/OpenGL; confirme que o menu começa fechado.
2. Abra com Insert: verifique desenho, cursor, cliques e que a câmera não se move
   enquanto o menu está aberto. Confirme as três caixas inicialmente desmarcadas.
3. Ative boxes e skeleton separadamente. Compare com inimigos visíveis; aliados,
   jogadores mortos e entidades antigas não devem ser selecionados.
4. Ative aimbot pelo checkbox, feche o menu e confira a mira na cabeça de um
   inimigo. Desative pelo checkbox e confirme o retorno ao controle manual.
5. Teste agachar, movimento, troca de rodada, troca de equipe, desconexão,
   Alt+Tab e mudança de resolução. Menu e ESP não devem aparecer fora do jogo.
6. Finalize o processo do menu: todos os recursos devem parar em até 1,5 s.
7. Feche o jogo, desinstale e confira o hash da DLL original restaurada.

Se o menu não abrir, procure `Ocaain: menu executable unavailable` no console
do jogo ou `%TEMP%\Ocaain.Menu-<PID>.log`. Confirme `-insecure -gl`, o diretório
`cstrike/cl_dlls/ocaain-menu/` completo e se existe uma partida carregada.

## Estrutura e testes

- `native/`: DLL intermediária, coleta dos ossos, mira, ESP e entrada do menu.
- `menu/`: controles Windows Forms reais, renderizados fora da tela; comunicação
  por memória compartilhada local ao processo. Pixels protegidos por mutex e
  comandos/heartbeats com operações atômicas. Não há servidor de rede.
- `tests/core_tests.cpp`: cálculo de ângulos, correção do movimento, filtros de
  alvo, valores inválidos e layout do protocolo. Esses testes não comprovam a
  integração com o jogo ou a renderização Windows.
- `tests/install_roundtrip.ps1`: preservação e restauração byte a byte, bloqueio
  com jogo em execução e recusa de sobrescrever backups/arquivos alterados.
- `scripts/`: compilação, instalação com backup e restauração.

O núcleo pode ser testado em Linux, sem SDK Windows:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I native tests/core_tests.cpp -o /tmp/ocaain-core-tests
/tmp/ocaain-core-tests
```

Código do SDK Valve tem licença própria para melhorias não comerciais. Consulte
[a licença oficial](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/LICENSE)
antes de redistribuir ou comercializar derivados.
